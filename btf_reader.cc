// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -*- mode: C++ -*-
//
// Copyright 2020-2025 Google LLC
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
// Author: Maria Teguiani
// Author: Giuliano Procida
// Author: Ignes Simeonova
// Author: Aleksei Vetrov

#include "btf_reader.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <linux/btf.h>
#include "elf_dwarf_handle.h"
#include "elf_loader.h"
#include "error.h"
#include "graph.h"
#include "reader_options.h"
#include "runtime.h"
#include "unification.h"

namespace stg {

namespace btf {

namespace {

// BTF Specification: https://www.kernel.org/doc/html/latest/bpf/btf.html
class Structs {
 public:
  Structs(Runtime& runtime, Graph& graph);
  Id Process(std::string_view data);

 private:
  struct MemoryRange {
    const char* start;
    const char* limit;
    bool Empty() const;
    template <typename T> const T* Pull(size_t count = 1);
  };

  MemoryRange string_section_;

  Maker<uint32_t> maker_;
  std::optional<Id> void_;
  std::optional<Id> variadic_;
  std::map<std::string, Id> btf_symbols_;

  Id ProcessAligned(std::string_view data);

  Id GetVoid();
  Id GetVariadic();
  Id GetIdRaw(uint32_t btf_index);
  Id GetId(uint32_t btf_index);
  Id GetParameterId(uint32_t btf_index);
  template <typename Node, typename... Args>
  void Set(uint32_t id, Args&&... args);

  Id BuildTypes(MemoryRange memory);
  void BuildOneType(const btf_type* t, uint32_t btf_index,
                    MemoryRange& memory);
  Id BuildSymbols();
  std::vector<Id> BuildMembers(
      bool kflag, const btf_member* members, size_t vlen);
  Enumeration::Enumerators BuildEnums(
      bool is_signed, const struct btf_enum* enums, size_t vlen);
  Enumeration::Enumerators BuildEnums64(
      bool is_signed, const struct btf_enum64* enums, size_t vlen);
  std::vector<Id> BuildParams(const struct btf_param* params, size_t vlen);
  Id BuildEnumUnderlyingType(size_t size, bool is_signed);
  std::string GetName(uint32_t name_off);

  Unification unification_;
};

bool Structs::MemoryRange::Empty() const {
  return start == limit;
}

template <typename T>
const T* Structs::MemoryRange::Pull(size_t count) {
  const char* saved = start;
  start += sizeof(T) * count;
  Check(start <= limit) << "type data extends past end of type section";
  return reinterpret_cast<const T*>(saved);
}

Structs::Structs(Runtime& runtime, Graph& graph)
    : maker_(graph), unification_(runtime, graph, Id{0}, Id{0}) {}

// Get the index of the void type, creating one if needed.
Id Structs::GetVoid() {
  if (!void_) {
    void_ = {maker_.Add<Special>(Special::Kind::VOID)};
  }
  return *void_;
}

// Get the index of the variadic parameter type, creating one if needed.
Id Structs::GetVariadic() {
  if (!variadic_) {
    variadic_ = {maker_.Add<Special>(Special::Kind::VARIADIC)};
  }
  return *variadic_;
}

// Map BTF type index to node ID.
Id Structs::GetIdRaw(uint32_t btf_index) {
  return maker_.Get(btf_index);
}

// Translate BTF type index to node ID, for non-parameters.
Id Structs::GetId(uint32_t btf_index) {
  return btf_index ? GetIdRaw(btf_index) : GetVoid();
}

// Translate BTF type index to node ID, for parameters.
Id Structs::GetParameterId(uint32_t btf_index) {
  return btf_index ? GetIdRaw(btf_index) : GetVariadic();
}

// For a BTF type index, populate the node with the corresponding ID.
template <typename Node, typename... Args>
void Structs::Set(uint32_t id, Args&&... args) {
  maker_.Set<Node>(id, std::forward<Args>(args)...);
}

Id Structs::Process(std::string_view btf_data) {
  // TODO: Remove this hack once the upstream binaries have proper
  // alignment.
  //
  // Copy the data to aligned heap-allocated memory, if needed.
  return reinterpret_cast<uintptr_t>(btf_data.data()) % alignof(btf_header) > 0
      ? ProcessAligned(std::string(btf_data))
      : ProcessAligned(btf_data);
}

Id Structs::ProcessAligned(std::string_view btf_data) {
  Check(sizeof(btf_header) <= btf_data.size())
      << "BTF section too small for header";
  const btf_header* header =
      reinterpret_cast<const btf_header*>(btf_data.data());
  Check(header->magic == 0xEB9F) << "Magic field must be 0xEB9F for BTF";

  const char* header_limit = btf_data.begin() + header->hdr_len;
  const char* type_start = header_limit + header->type_off;
  const char* type_limit = type_start + header->type_len;
  const char* string_start = header_limit + header->str_off;
  const char* string_limit = string_start + header->str_len;

  Check(btf_data.begin() + sizeof(btf_header) <= header_limit)
      << "header exceeds length";
  Check(header_limit <= type_start) << "type section overlaps header";
  Check(type_start <= type_limit) << "type section ill-formed";
  Check(reinterpret_cast<uintptr_t>(type_start) % alignof(btf_type) == 0)
      << "misaligned type section";
  Check(type_limit <= string_start)
      << "string section does not follow type section";
  Check(string_start <= string_limit) << "string section ill-formed";
  Check(string_limit <= btf_data.end())
      << "string section extends beyond end of BTF data";

  const MemoryRange type_section{type_start, type_limit};
  string_section_ = MemoryRange{string_start, string_limit};
  return BuildTypes(type_section);
}

// vlen: vector length, the number of struct/union members
std::vector<Id> Structs::BuildMembers(
    bool kflag, const btf_member* members, size_t vlen) {
  std::vector<Id> result;
  for (size_t i = 0; i < vlen; ++i) {
    const auto& raw_member = members[i];
    const auto name = GetName(raw_member.name_off);
    const auto raw_offset = raw_member.offset;
    const auto offset = kflag ? BTF_MEMBER_BIT_OFFSET(raw_offset) : raw_offset;
    const auto bitfield_size = kflag ? BTF_MEMBER_BITFIELD_SIZE(raw_offset) : 0;
    result.push_back(
        maker_.Add<Member>(name, GetId(raw_member.type),
                           static_cast<uint64_t>(offset), bitfield_size));
  }
  return result;
}

// vlen: vector length, the number of enum values
Enumeration::Enumerators Structs::BuildEnums(
    bool is_signed, const struct btf_enum* enums, size_t vlen) {
  Enumeration::Enumerators result;
  for (size_t i = 0; i < vlen; ++i) {
    const auto name = GetName(enums[i].name_off);
    const uint32_t unsigned_value = enums[i].val;
    if (is_signed) {
      const int32_t signed_value = unsigned_value;
      result.emplace_back(name, static_cast<int64_t>(signed_value));
    } else {
      result.emplace_back(name, static_cast<int64_t>(unsigned_value));
    }
  }
  return result;
}

Enumeration::Enumerators Structs::BuildEnums64(
    bool is_signed, const struct btf_enum64* enums, size_t vlen) {
  Enumeration::Enumerators result;
  for (size_t i = 0; i < vlen; ++i) {
    const auto name = GetName(enums[i].name_off);
    const uint32_t low = enums[i].val_lo32;
    const uint32_t high = enums[i].val_hi32;
    const uint64_t unsigned_value = (static_cast<uint64_t>(high) << 32) | low;
    if (is_signed) {
      const int64_t signed_value = unsigned_value;
      result.emplace_back(name, signed_value);
    } else {
      // TODO: very large unsigned values are stored as negative numbers
      result.emplace_back(name, static_cast<int64_t>(unsigned_value));
    }
  }
  return result;
}

// vlen: vector length, the number of parameters
std::vector<Id> Structs::BuildParams(const struct btf_param* params,
                                     size_t vlen) {
  std::vector<Id> result;
  result.reserve(vlen);
  for (size_t i = 0; i < vlen; ++i) {
    const auto name = GetName(params[i].name_off);
    const auto type = params[i].type;
    result.push_back(GetParameterId(type));
  }
  return result;
}

Id Structs::BuildEnumUnderlyingType(size_t size, bool is_signed) {
  std::ostringstream os;
  os << (is_signed ? "enum-underlying-signed-" : "enum-underlying-unsigned-")
     << (8 * size);
  const auto encoding = is_signed ? Primitive::Encoding::SIGNED_INTEGER
                                  : Primitive::Encoding::UNSIGNED_INTEGER;
  return maker_.Add<Primitive>(os.str(), encoding, size);
}

Id Structs::BuildTypes(MemoryRange memory) {
  // Alas, BTF overloads type id 0 to mean both void (for everything but
  // function parameters) and variadic (for function parameters). We determine
  // which is intended and create void and variadic types on demand.

  // The type section is parsed sequentially and each type's index is its id.
  uint32_t btf_index = 1;
  while (!memory.Empty()) {
    const auto* t = memory.Pull<struct btf_type>();
    BuildOneType(t, btf_index, memory);
    ++btf_index;
  }

  return BuildSymbols();
}

void Structs::BuildOneType(const btf_type* t, uint32_t btf_index,
                           MemoryRange& memory) {
  const auto kind = BTF_INFO_KIND(t->info);
  const auto vlen = BTF_INFO_VLEN(t->info);
  Check(kind < NR_BTF_KINDS) << "Unknown BTF kind: " << static_cast<int>(kind);

  switch (kind) {
    case BTF_KIND_INT: {
      const auto info = *memory.Pull<uint32_t>();
      const auto name = GetName(t->name_off);
      const auto raw_encoding = BTF_INT_ENCODING(info);
      const auto offset = BTF_INT_OFFSET(info);
      const auto bits = BTF_INT_BITS(info);
      const auto is_bool = raw_encoding & BTF_INT_BOOL;
      const auto is_signed = raw_encoding & BTF_INT_SIGNED;
      const auto is_char = raw_encoding & BTF_INT_CHAR;
      Primitive::Encoding encoding =
          is_bool ? Primitive::Encoding::BOOLEAN
                : is_char ? is_signed ? Primitive::Encoding::SIGNED_CHARACTER
                                      : Primitive::Encoding::UNSIGNED_CHARACTER
                          : is_signed ? Primitive::Encoding::SIGNED_INTEGER
                                      : Primitive::Encoding::UNSIGNED_INTEGER;
      if (offset) {
        Die() << "BTF INT non-zero offset " << offset;
      }
      if (bits != 8 * t->size) {
        Die() << "BTF INT bits != 8 * size";
      }
      Set<Primitive>(btf_index, name, encoding, t->size);
      break;
    }
    case BTF_KIND_FLOAT: {
      const auto name = GetName(t->name_off);
      const auto encoding = Primitive::Encoding::REAL_NUMBER;
      Set<Primitive>(btf_index, name, encoding, t->size);
      break;
    }
    case BTF_KIND_PTR: {
      Set<PointerReference>(btf_index, PointerReference::Kind::POINTER,
                            GetId(t->type));
      break;
    }
    case BTF_KIND_TYPEDEF: {
      const auto name = GetName(t->name_off);
      Set<Typedef>(btf_index, name, GetId(t->type));
      break;
    }
    case BTF_KIND_VOLATILE:
    case BTF_KIND_CONST:
    case BTF_KIND_RESTRICT: {
      const auto qualifier = kind == BTF_KIND_CONST
                             ? Qualifier::CONST
                             : kind == BTF_KIND_VOLATILE
                             ? Qualifier::VOLATILE
                             : Qualifier::RESTRICT;
      Set<Qualified>(btf_index, qualifier, GetId(t->type));
      break;
    }
    case BTF_KIND_TYPE_TAG: {
      // Type tags are stringly-kinded qualifiers of specific relevance to the
      // Linux kernel. The current tags include "rcu" and "user". As yet, they
      // have no ABI significance, so just replace references to them with their
      // referants.
      //
      // Set with a dummy value, to avoid upsetting Maker's undefined node
      // tracking. Refactoring ownership and lifetime of the union mapping would
      // be one way of avoiding this.
      Set<Special>(btf_index, Special::Kind::VOID);
      unification_.Union(GetId(btf_index), GetId(t->type));
      break;
    }
    case BTF_KIND_ARRAY: {
      const auto* array = memory.Pull<struct btf_array>();
      Set<Array>(btf_index, array->nelems, GetId(array->type));
      break;
    }
    case BTF_KIND_STRUCT:
    case BTF_KIND_UNION: {
      const auto struct_union_kind = kind == BTF_KIND_STRUCT
                                     ? StructUnion::Kind::STRUCT
                                     : StructUnion::Kind::UNION;
      const auto name = GetName(t->name_off);
      const bool kflag = BTF_INFO_KFLAG(t->info);
      const auto* btf_members = memory.Pull<struct btf_member>(vlen);
      const auto members = BuildMembers(kflag, btf_members, vlen);
      Set<StructUnion>(btf_index, struct_union_kind, name, t->size,
                       std::vector<Id>(), std::vector<Id>(), members);
      break;
    }
    case BTF_KIND_ENUM: {
      const auto name = GetName(t->name_off);
      const bool is_signed = BTF_INFO_KFLAG(t->info);
      const auto* enums = memory.Pull<struct btf_enum>(vlen);
      const auto enumerators = BuildEnums(is_signed, enums, vlen);
      // BTF only considers structs and unions as forward-declared types, and
      // does not include forward-declared enums. They are treated as
      // BTF_KIND_ENUMs with vlen set to zero.
      if (vlen) {
        // create a synthetic underlying type
        const Id underlying = BuildEnumUnderlyingType(t->size, is_signed);
        Set<Enumeration>(btf_index, name, underlying, enumerators);
      } else {
        // BTF actually provides size (4), but it's meaningless.
        Set<Enumeration>(btf_index, name);
      }
      break;
    }
    case BTF_KIND_ENUM64: {
      const auto name = GetName(t->name_off);
      const bool is_signed = BTF_INFO_KFLAG(t->info);
      const auto* enums = memory.Pull<struct btf_enum64>(vlen);
      const auto enumerators = BuildEnums64(is_signed, enums, vlen);
      // create a synthetic underlying type
      const Id underlying = BuildEnumUnderlyingType(t->size, is_signed);
      Set<Enumeration>(btf_index, name, underlying, enumerators);
      break;
    }
    case BTF_KIND_FWD: {
      const auto name = GetName(t->name_off);
      const auto struct_union_kind = BTF_INFO_KFLAG(t->info)
                                     ? StructUnion::Kind::UNION
                                     : StructUnion::Kind::STRUCT;
      Set<StructUnion>(btf_index, struct_union_kind, name);
      break;
    }
    case BTF_KIND_FUNC: {
      const auto name = GetName(t->name_off);
      // TODO: map linkage (vlen) to symbol properties
      Set<ElfSymbol>(btf_index, name, std::nullopt, true,
                     ElfSymbol::SymbolType::FUNCTION,
                     ElfSymbol::Binding::GLOBAL,
                     ElfSymbol::Visibility::DEFAULT,
                     std::nullopt,
                     std::nullopt,
                     GetId(t->type),
                     std::nullopt);
      const bool inserted =
          btf_symbols_.insert({name, GetIdRaw(btf_index)}).second;
      Check(inserted) << "duplicate symbol " << name;
      break;
    }
    case BTF_KIND_FUNC_PROTO: {
      const auto* params = memory.Pull<struct btf_param>(vlen);
      const auto parameters = BuildParams(params, vlen);
      Set<Function>(btf_index, GetId(t->type), parameters);
      break;
    }
    case BTF_KIND_VAR: {
      // NOTE: global variables are not yet emitted by pahole -J
      const auto* variable = memory.Pull<struct btf_var>();
      const auto name = GetName(t->name_off);
      // TODO: map variable->linkage to symbol properties
      (void) variable;
      Set<ElfSymbol>(btf_index, name, std::nullopt, true,
                     ElfSymbol::SymbolType::OBJECT,
                     ElfSymbol::Binding::GLOBAL,
                     ElfSymbol::Visibility::DEFAULT,
                     std::nullopt,
                     std::nullopt,
                     GetId(t->type),
                     std::nullopt);
      const bool inserted =
          btf_symbols_.insert({name, GetIdRaw(btf_index)}).second;
      Check(inserted) << "duplicate symbol " << name;
      break;
    }
    case BTF_KIND_DATASEC: {
      // Just skip BTF DATASEC entries. They partially duplicate ELF symbol
      // table information, if they exist at all.
      memory.Pull<struct btf_var_secinfo>(vlen);
      break;
    }
    default: {
      Die() << "Unhandled BTF kind: " << static_cast<int>(kind);
      break;
    }
  }
}

std::string Structs::GetName(uint32_t name_off) {
  const char* name_begin = string_section_.start + name_off;
  const char* const limit = string_section_.limit;
  Check(name_begin < limit) << "name offset exceeds string section length";
  const char* name_end = std::find(name_begin, limit, '\0');
  Check(name_end < limit) << "name continues past the string section limit";
  return {name_begin, static_cast<size_t>(name_end - name_begin)};
}

Id Structs::BuildSymbols() {
  return maker_.Add<Interface>(btf_symbols_);
}

}  // namespace

Id ReadSection(Runtime& runtime, Graph& graph, std::string_view data) {
  return Structs(runtime, graph).Process(data);
}

Id ReadFile(Runtime& runtime, Graph& graph, const std::string& path,
            ReadOptions) {
  ElfDwarfHandle handle(path);
  const elf::ElfLoader loader(handle.GetElf());
  return ReadSection(runtime, graph, loader.GetSectionRawData(".BTF"));
}

}  // namespace btf

}  // namespace stg
