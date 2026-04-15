// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -*- mode: C++ -*-
//
// Copyright 2022 Google LLC
//
// Licensed under the Apache License v2.0 with LLVM Exceptions (the
// "License"); you may not use this file except in compliance with the
// License.  You may obtain a copy of the License at
//
//     https://llvm.org/LICENSE.txt
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: Aleksei Vetrov

#include "elf_reader.h"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "dwarf_processor.h"
#include "dwarf_wrappers.h"
#include "elf_dwarf_handle.h"
#include "elf_loader.h"
#include "error.h"
#include "filter.h"
#include "graph.h"
#include "hex.h"
#include "reader_options.h"
#include "runtime.h"
#include "type_normalisation.h"
#include "type_resolution.h"
#include "unification.h"

namespace stg {
namespace elf {
namespace internal {

namespace {

template <typename M, typename K>
std::optional<typename M::mapped_type> MaybeGet(const M& map, const K& key) {
  const auto it = map.find(key);
  if (it == map.end()) {
    return {};
  }
  return {it->second};
}

}  // namespace

ElfSymbol::SymbolType ConvertSymbolType(
    SymbolTableEntry::SymbolType symbol_type) {
  switch (symbol_type) {
    case SymbolTableEntry::SymbolType::NOTYPE:
      return ElfSymbol::SymbolType::NOTYPE;
    case SymbolTableEntry::SymbolType::OBJECT:
      return ElfSymbol::SymbolType::OBJECT;
    case SymbolTableEntry::SymbolType::FUNCTION:
      return ElfSymbol::SymbolType::FUNCTION;
    case SymbolTableEntry::SymbolType::COMMON:
      return ElfSymbol::SymbolType::COMMON;
    case SymbolTableEntry::SymbolType::TLS:
      return ElfSymbol::SymbolType::TLS;
    case SymbolTableEntry::SymbolType::GNU_IFUNC:
      return ElfSymbol::SymbolType::GNU_IFUNC;
    default:
      Die() << "Unsupported ELF symbol type: " << symbol_type;
  }
}

SymbolNameList GetKsymtabSymbols(const SymbolTable& symbols) {
  constexpr std::string_view kKsymtabPrefix = "__ksymtab_";
  SymbolNameList result;
  result.reserve(symbols.size() / 2);
  for (const auto& symbol : symbols) {
    if (symbol.name.substr(0, kKsymtabPrefix.size()) == kKsymtabPrefix) {
      result.emplace(symbol.name.substr(kKsymtabPrefix.size()));
    }
  }
  return result;
}

CRCValuesMap GetCRCValuesMap(const SymbolTable& symbols, const ElfLoader& elf) {
  constexpr std::string_view kCRCPrefix = "__crc_";

  CRCValuesMap crc_values;

  for (const auto& symbol : symbols) {
    const std::string_view name = symbol.name;
    if (name.substr(0, kCRCPrefix.size()) == kCRCPrefix) {
      const std::string_view name_suffix = name.substr(kCRCPrefix.size());
      if (!crc_values.emplace(name_suffix, elf.GetElfSymbolCRC(symbol))
               .second) {
        Die() << "Multiple CRC values for symbol '" << name_suffix << '\'';
      }
    }
  }

  return crc_values;
}

NamespacesMap GetNamespacesMap(const SymbolTable& symbols,
                               const ElfLoader& elf) {
  constexpr std::string_view kNSPrefix = "__kstrtabns_";

  NamespacesMap namespaces;

  for (const auto& symbol : symbols) {
    const std::string_view name = symbol.name;
    if (name.substr(0, kNSPrefix.size()) == kNSPrefix) {
      const std::string_view name_suffix = name.substr(kNSPrefix.size());
      const std::string_view ns = elf.GetElfSymbolNamespace(symbol);
      if (ns.empty()) {
        // The global namespace is explicitly represented as the empty string,
        // but the common interpretation is that such symbols lack an export
        // namespace.
        continue;
      }
      if (!namespaces.emplace(name_suffix, ns).second) {
        Die() << "Multiple namespaces for symbol '" << name_suffix << '\'';
      }
    }
  }

  return namespaces;
}

AddressMap GetCFIAddressMap(const SymbolTable& symbols, const ElfLoader& elf) {
  AddressMap name_to_address;
  for (const auto& symbol : symbols) {
    const std::string_view name_prefix = UnwrapCFISymbolName(symbol.name);
    const size_t address = elf.GetAbsoluteAddress(symbol);
    if (!name_to_address.emplace(name_prefix, address).second) {
      Die() << "Multiple CFI symbols referring to symbol '" << name_prefix
            << '\'';
    }
  }
  return name_to_address;
}

bool IsPublicFunctionOrVariable(const SymbolTableEntry& symbol) {
  const auto symbol_type = symbol.symbol_type;
  // Reject symbols that are not functions or variables.
  if (symbol_type != SymbolTableEntry::SymbolType::FUNCTION &&
      symbol_type != SymbolTableEntry::SymbolType::OBJECT &&
      symbol_type != SymbolTableEntry::SymbolType::TLS &&
      symbol_type != SymbolTableEntry::SymbolType::GNU_IFUNC) {
    return false;
  }

  // Function or variable of ValueType::ABSOLUTE is not expected in any binary,
  // but GNU `ld` adds object of such type for every version name defined in
  // file. Such symbol should be rejected, because in fact it is not variable.
  if (symbol.value_type == SymbolTableEntry::ValueType::ABSOLUTE) {
    Check(symbol_type == SymbolTableEntry::SymbolType::OBJECT)
        << "Unexpected function or variable with ABSOLUTE value type";
    return false;
  }

  // Undefined symbol is dependency of the binary but is not part of ABI
  // provided by binary and should be rejected.
  if (symbol.value_type == SymbolTableEntry::ValueType::UNDEFINED) {
    return false;
  }

  // Common symbols can only be seen in .o files emitted by old compilers.
  if (symbol.value_type == SymbolTableEntry::ValueType::COMMON) {
    Die() << "unexpected COMMON symbol: '" << symbol.name << '\'';
  }

  // Local symbol is not visible outside the binary, so it is not public
  // and should be rejected.
  if (symbol.binding == SymbolTableEntry::Binding::LOCAL) {
    return false;
  }

  // "Hidden" and "internal" visibility values mean that symbol is not public
  // and should be rejected.
  if (symbol.visibility == SymbolTableEntry::Visibility::HIDDEN ||
      symbol.visibility == SymbolTableEntry::Visibility::INTERNAL) {
    return false;
  }

  return true;
}

bool IsLinuxKernelFunctionOrVariable(const SymbolNameList& ksymtab,
                                     const SymbolTableEntry& symbol) {
  // We use symbol name extracted from __ksymtab_ symbols as a proxy for the
  // real symbol in the ksymtab. Such names can still be duplicated by LOCAL
  // symbols so drop them to avoid false matches.
  if (symbol.binding == SymbolTableEntry::Binding::LOCAL) {
    return false;
  }

  // TODO: handle undefined ksymtab symbols
  if (symbol.value_type == SymbolTableEntry::ValueType::UNDEFINED) {
    return false;
  }

  // Common symbols can only be seen in .o files emitted by old compilers.
  if (symbol.value_type == SymbolTableEntry::ValueType::COMMON) {
    Die() << "unexpected COMMON symbol: '" << symbol.name << '\'';
  }

  // Symbol linkage is determined by the ksymtab.
  if (!ksymtab.contains(symbol.name)) {
    return false;
  }

  const auto symbol_type = symbol.symbol_type;
  // Keep function and object symbols, but not GNU indirect function or TLS ones
  // as the module loader does not expect them.
  if (symbol_type != SymbolTableEntry::SymbolType::FUNCTION
      && symbol_type != SymbolTableEntry::SymbolType::OBJECT) {
    // TODO: upgrade to Die after more testing / fixing
    Warn() << "ignoring Linux kernel symbol '" << symbol.name << "' in section "
           << Hex(symbol.section_index) << " of type " << symbol_type;
    return false;
  }

  return true;
}

namespace {

class Reader {
 public:
  Reader(Runtime& runtime, Graph& graph, ElfDwarfHandle& elf_dwarf_handle,
         ReadOptions options, const std::unique_ptr<Filter>& file_filter)
      : graph_(graph),
        elf_dwarf_handle_(elf_dwarf_handle),
        elf_(elf_dwarf_handle_.GetElf()),
        options_(options),
        file_filter_(file_filter),
        runtime_(runtime) {}

  Id Read();

 private:
  using SymbolIndex =
      std::map<std::pair<dwarf::Location, std::string>, std::vector<size_t>>;

  void GetLinuxKernelSymbols(
      const std::vector<SymbolTableEntry>& all_symbols,
      std::vector<std::pair<ElfSymbol, dwarf::Location>>& symbols) const;
  void GetUserspaceSymbols(
      const std::vector<SymbolTableEntry>& all_symbols,
      std::vector<std::pair<ElfSymbol, dwarf::Location>>& symbols) const;

  Id BuildRoot(
      const std::vector<std::pair<ElfSymbol, dwarf::Location>>& symbols) {
    // On destruction, the unification object will remove or rewrite each graph
    // node for which it has a mapping.
    //
    // Graph rewriting is expensive so an important optimisation is to restrict
    // the nodes in consideration to the ones allocated by the DWARF processor
    // here and any symbol or type roots that follow. This is done by setting
    // the starting node ID to be the current graph limit.
    const Id start = graph_.Limit();

    const dwarf::Types types =
        dwarf::Process(elf_dwarf_handle_.GetDwarf(),
                       elf_.IsLittleEndianBinary(), file_filter_, graph_);

    // A less important optimisation is avoiding copying the mapping array as it
    // is populated. This is done by reserving space to the new graph limit.
    Unification unification(runtime_, graph_, start, graph_.Limit());

    // Replace incomplete types with the full type.
    for (const auto [incomplete_type_id, full_type_id] :
         types.incomplete_to_full_types) {
      unification.Unify(incomplete_type_id, full_type_id);
    }

    // fill location to id
    //
    // In general, we want to handle as many of the following cases as possible.
    // In practice, determining the correct ELF-DWARF match may be impossible.
    //
    // * compiler-driven aliasing - multiple symbols with same address
    // * zero-size symbol false aliasing - multiple symbols and types with same
    //   address
    // * weak/strong linkage symbols - multiple symbols and types with same
    //   address
    // * assembly symbols - multiple declarations but no definition and no
    //   address in DWARF.
    SymbolIndex location_and_name_to_index;
    for (size_t i = 0; i < types.symbols.size(); ++i) {
      const auto& s = types.symbols[i];
      for (const auto& location : s.locations) {
        location_and_name_to_index[{location, s.linkage_name}].push_back(i);
      }
    }

    std::map<std::string, Id> symbols_map;
    for (auto [symbol, address] : symbols) {
      // TODO: add VersionInfoToString to SymbolKey name
      // TODO: check for uniqueness of SymbolKey in map after
      // support for version info
      MaybeAddTypeInfo(location_and_name_to_index, types.symbols, address,
                       symbol, unification);
      symbols_map.emplace(VersionedSymbolName(symbol),
                          graph_.Add<ElfSymbol>(symbol));
    }

    std::map<std::string, Id> types_map;
    if (options_.Test(ReadOptions::TYPE_ROOTS)) {
      const InterfaceKey get_key(graph_);
      for (const auto id : types.named_type_ids) {
        const auto [it, inserted] = types_map.emplace(get_key(id), id);
        if (!inserted && !unification.Unify(id, it->second)) {
          Die() << "found conflicting interface type: " << it->first;
        }
      }
    }

    const Id root =
        graph_.Add<Interface>(std::move(symbols_map), std::move(types_map));

    // Use all named types and DWARF declarations as roots for type resolution.
    std::vector<Id> roots;
    roots.reserve(types.named_type_ids.size() + types.symbols.size() + 1);
    for (const auto& symbol : types.symbols) {
      roots.push_back(symbol.type_id);
    }
    for (const auto id : types.named_type_ids) {
      roots.push_back(id);
    }
    roots.push_back(root);

    stg::ResolveTypes(runtime_, graph_, unification, {roots});

    return unification.Find(root);
  }

  static bool IsEqual(Unification& unification, const dwarf::Types::Symbol& lhs,
                      const dwarf::Types::Symbol& rhs) {
    return lhs.scoped_name == rhs.scoped_name
           && lhs.linkage_name == rhs.linkage_name
           && lhs.locations == rhs.locations
           && unification.Unify(lhs.type_id, rhs.type_id);
  }

  static ElfSymbol SymbolTableEntryToElfSymbol(const CRCValuesMap& crc_values,
                                               const NamespacesMap& namespaces,
                                               const SymbolTableEntry& symbol) {
    return {/* symbol_name = */ std::string(symbol.name),
            /* version_info = */ std::nullopt,
            /* is_defined = */
            symbol.value_type != SymbolTableEntry::ValueType::UNDEFINED,
            /* symbol_type = */ ConvertSymbolType(symbol.symbol_type),
            /* binding = */ symbol.binding,
            /* visibility = */ symbol.visibility,
            /* crc = */ MaybeGet(crc_values, std::string(symbol.name)),
            /* ns = */ MaybeGet(namespaces, std::string(symbol.name)),
            /* type_id = */ std::nullopt,
            /* full_name = */ std::nullopt};
  }

  static void MaybeAddTypeInfo(
      const SymbolIndex& location_and_name_to_index,
      const std::vector<dwarf::Types::Symbol>& dwarf_symbols,
      dwarf::Location location, ElfSymbol& node, Unification& unification) {
    // try to find the first symbol with given location
    const auto start_it = location_and_name_to_index.lower_bound(
        std::make_pair(location, std::string()));
    auto best_symbols_it = location_and_name_to_index.end();
    bool matched_by_name = false;
    size_t candidates = 0;
    for (auto it = start_it;
         it != location_and_name_to_index.end() && it->first.first == location;
         ++it) {
      ++candidates;
      // We have at least matching locations.
      if (it->first.second == node.symbol_name) {
        // If we have also matching names we can stop looking further.
        matched_by_name = true;
        best_symbols_it = it;
        break;
      }
      if (best_symbols_it == location_and_name_to_index.end()) {
        // Otherwise keep the first match.
        best_symbols_it = it;
      }
    }
    if (best_symbols_it != location_and_name_to_index.end()) {
      const auto& best_symbols = best_symbols_it->second;
      Check(!best_symbols.empty()) << "best_symbols.empty()";
      const auto& best_symbol = dwarf_symbols[best_symbols[0]];
      for (size_t i = 1; i < best_symbols.size(); ++i) {
        const auto& other = dwarf_symbols[best_symbols[i]];
        // TODO: allow "compatible" duplicates, for example
        // "void foo(int bar)" vs "void foo(const int bar)"
        if (!IsEqual(unification, best_symbol, other)) {
          Die() << "Duplicate DWARF symbol: location="
                << best_symbols_it->first.first
                << ", name=" << best_symbols_it->first.second;
        }
      }
      if (best_symbol.scoped_name.empty()) {
        Die() << "Anonymous DWARF symbol: location="
              << best_symbols_it->first.first
              << ", name=" << best_symbols_it->first.second;
      }
      // There may be multiple DWARF symbols with same address (zero-length
      // arrays), or ELF symbol has different name from DWARF symbol (aliases).
      // But if we have both situations at once, we can't match ELF to DWARF and
      // it should be fixed in analysed binary source code.
      Check(matched_by_name || candidates == 1)
          << "Multiple candidate symbols without matching name: location="
          << best_symbols_it->first.first
          << ", name=" << best_symbols_it->first.second;
      node.type_id = best_symbol.type_id;
      node.full_name = best_symbol.scoped_name;
    }
  }

  Graph& graph_;
  ElfDwarfHandle& elf_dwarf_handle_;
  ElfLoader elf_;
  ReadOptions options_;
  const std::unique_ptr<Filter>& file_filter_;
  Runtime& runtime_;
};

void Reader::GetLinuxKernelSymbols(
    const std::vector<SymbolTableEntry>& all_symbols,
    std::vector<std::pair<ElfSymbol, dwarf::Location>>& symbols) const {
  const auto crcs = GetCRCValuesMap(all_symbols, elf_);
  const auto namespaces = GetNamespacesMap(all_symbols, elf_);
  const auto ksymtab_symbols = GetKsymtabSymbols(all_symbols);
  for (const auto& symbol : all_symbols) {
    if (IsLinuxKernelFunctionOrVariable(ksymtab_symbols, symbol)) {
      const size_t address = elf_.GetAbsoluteAddress(symbol);
      symbols.emplace_back(
          SymbolTableEntryToElfSymbol(crcs, namespaces, symbol),
          dwarf::Location{dwarf::Location::Kind::ADDRESS, address});
    }
  }
}

void Reader::GetUserspaceSymbols(
    const std::vector<SymbolTableEntry>& all_symbols,
    std::vector<std::pair<ElfSymbol, dwarf::Location>>& symbols) const {
  const auto cfi_address_map = GetCFIAddressMap(elf_.GetCFISymbols(), elf_);
  for (const auto& symbol : all_symbols) {
    if (IsPublicFunctionOrVariable(symbol)) {
      if (symbol.symbol_type == SymbolTableEntry::SymbolType::TLS) {
        // TLS symbols offsets may be incorrect because of unsupported
        // relocations. Resetting it to zero the same way as it is done in
        // dwarf::Entry::GetLocationFromExpression.
        // TODO: match TLS variables by offset
        symbols.emplace_back(SymbolTableEntryToElfSymbol({}, {}, symbol),
                             dwarf::Location{dwarf::Location::Kind::TLS, 0});
      } else {
        const auto cfi_it = cfi_address_map.find(std::string(symbol.name));
        const size_t absolute = cfi_it != cfi_address_map.end()
                                    ? cfi_it->second
                                    : elf_.GetAbsoluteAddress(symbol);
        symbols.emplace_back(
            SymbolTableEntryToElfSymbol({}, {}, symbol),
            dwarf::Location{dwarf::Location::Kind::ADDRESS, absolute});
      }
    }
  }
}

Id Reader::Read() {
  const auto all_symbols = elf_.GetElfSymbols();
  const auto get_symbols = elf_.IsLinuxKernelBinary()
                               ? &Reader::GetLinuxKernelSymbols
                               : &Reader::GetUserspaceSymbols;
  std::vector<std::pair<ElfSymbol, dwarf::Location>> symbols;
  symbols.reserve(all_symbols.size());
  (this->*get_symbols)(all_symbols, symbols);
  symbols.shrink_to_fit();

  const Id root = BuildRoot(symbols);

  // Types produced by ELF/DWARF readers may require removing useless
  // qualifiers.
  return RemoveUselessQualifiers(graph_, root);
}

}  // namespace
}  // namespace internal

Id Read(Runtime& runtime, Graph& graph, ElfDwarfHandle& elf_dwarf_handle,
        ReadOptions options, const std::unique_ptr<Filter>& file_filter) {
  return internal::Reader(runtime, graph, elf_dwarf_handle, options,
                          file_filter)
      .Read();
}

}  // namespace elf
}  // namespace stg
