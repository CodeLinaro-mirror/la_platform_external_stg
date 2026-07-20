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

#ifndef STG_GRAPH_H_
#define STG_GRAPH_H_

#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <map>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "error.h"
#include "number.h"

namespace stg {

// A wrapped (for type safety) array index.
struct Id {
  // defined in graph.cc as maximum value for index type
  static const Id kInvalid;
  explicit Id(uint32_t ix) : ix_(ix) {}
  auto operator<=>(const Id&) const = default;
  uint32_t ix_;
};

std::ostream& operator<<(std::ostream& os, Id id);

using Pair = std::pair<Id, Id>;

}  // namespace stg

namespace std {

template <>
struct hash<stg::Id> {
  size_t operator()(const stg::Id& id) const {
    return hash<decltype(id.ix_)>()(id.ix_);
  }
};

template <>
struct hash<stg::Pair> {
  size_t operator()(const stg::Pair& comparison) const {
    size_t seed = 0;
    HashCombine(seed, comparison.first);
    HashCombine(seed, comparison.second);
    return seed;
  }
  static void HashCombine(size_t& seed, const stg::Id& id) {
    const std::hash<stg::Id> h;
    // assumes 64-bit size_t, would be better if std::hash_combine existed
    seed ^= h(id) + 0x9e3779b97f4a7c15 + (seed << 12) + (seed >> 4);
  }
};

}  // namespace std

namespace stg {

struct Special {
  enum class Kind {
    VOID,
    VARIADIC,
    NULLPTR,
  };
  explicit Special(Kind kind)
      : kind(kind) {}

  Kind kind;
};

struct PointerReference {
  enum class Kind {
    POINTER,
    LVALUE_REFERENCE,
    RVALUE_REFERENCE,
  };
  PointerReference(Kind kind, Id pointee_type_id)
      : kind(kind), pointee_type_id(pointee_type_id) {}

  Kind kind;
  Id pointee_type_id;
};

struct PointerToMember {
  PointerToMember(Id containing_type_id, Id pointee_type_id)
      : containing_type_id(containing_type_id), pointee_type_id(pointee_type_id)
  {}

  Id containing_type_id;
  Id pointee_type_id;
};

struct Typedef {
  Typedef(const std::string& name, Id referred_type_id)
      : name(name), referred_type_id(referred_type_id) {}

  std::string name;
  Id referred_type_id;
};

enum class Qualifier { CONST, VOLATILE, RESTRICT, ATOMIC };

std::ostream& operator<<(std::ostream& os, Qualifier qualifier);

struct Qualified {
  Qualified(Qualifier qualifier, Id qualified_type_id)
      : qualifier(qualifier), qualified_type_id(qualified_type_id) {}

  Qualifier qualifier;
  Id qualified_type_id;
};

struct Primitive {
  enum class Encoding {
    BOOLEAN,
    SIGNED_INTEGER,
    UNSIGNED_INTEGER,
    SIGNED_CHARACTER,
    UNSIGNED_CHARACTER,
    REAL_NUMBER,
    COMPLEX_NUMBER,
    UTF,
  };
  Primitive(const std::string& name, std::optional<Encoding> encoding,
            uint32_t bytesize)
      : name(name), encoding(encoding), bytesize(bytesize) {}

  std::string name;
  std::optional<Encoding> encoding;
  uint32_t bytesize;
};

struct Array {
  Array(uint64_t number_of_elements, Id element_type_id)
      : number_of_elements(number_of_elements),
        element_type_id(element_type_id)  {}

  uint64_t number_of_elements;
  Id element_type_id;
};

struct BaseClass {
  enum class Inheritance { NON_VIRTUAL, VIRTUAL };
  BaseClass(Id type_id, uint64_t offset, Inheritance inheritance)
      : type_id(type_id), offset(offset), inheritance(inheritance) {}

  Id type_id;
  uint64_t offset;
  Inheritance inheritance;
};

std::ostream& operator<<(std::ostream& os, BaseClass::Inheritance inheritance);

struct Method {
  Method(const std::string& mangled_name, const std::string& name,
         uint64_t vtable_offset, Id type_id)
      : mangled_name(mangled_name), name(name),
        vtable_offset(vtable_offset), type_id(type_id) {}

  std::string mangled_name;
  std::string name;
  uint64_t vtable_offset;
  Id type_id;
};

struct Member {
  Member(const std::string& name, Id type_id, uint64_t offset, uint64_t bitsize)
      : name(name), type_id(type_id), offset(offset), bitsize(bitsize) {}

  std::string name;
  Id type_id;
  uint64_t offset;
  uint64_t bitsize;
};

struct VariantMember {
  VariantMember(const std::string& name,
                std::optional<Number> discriminant_value, Id type_id)
      : name(name), discriminant_value(discriminant_value), type_id(type_id) {}

  std::string name;
  std::optional<Number> discriminant_value;
  Id type_id;
};

struct StructUnion {
  enum class Kind { STRUCT, UNION };
  struct Definition {
    uint64_t bytesize;
    std::vector<Id> base_classes;
    std::vector<Id> methods;
    std::vector<Id> members;
  };
  StructUnion(Kind kind, const std::string& name) : kind(kind), name(name) {}
  StructUnion(Kind kind, const std::string& name, uint64_t bytesize,
              const std::vector<Id>& base_classes,
              const std::vector<Id>& methods, const std::vector<Id>& members)
      : kind(kind), name(name),
        definition({bytesize, base_classes, methods, members}) {}

  Kind kind;
  std::string name;
  std::optional<Definition> definition;
};

std::ostream& operator<<(std::ostream& os, StructUnion::Kind kind);
std::string& operator+=(std::string& os, StructUnion::Kind kind);

struct Enumeration {
  using Enumerators = std::vector<std::pair<std::string, Number>>;
  struct Definition {
    Id underlying_type_id;
    Enumerators enumerators;
  };
  explicit Enumeration(const std::string& name) : name(name) {}
  Enumeration(const std::string& name, Id underlying_type_id,
              const Enumerators& enumerators)
      : name(name), definition({underlying_type_id, enumerators}) {}

  std::string name;
  std::optional<Definition> definition;
};

struct Variant {
  Variant(const std::string& name, uint64_t bytesize,
          std::optional<Id> discriminant, const std::vector<Id>& members)
      : name(name),
        bytesize(bytesize),
        discriminant(discriminant),
        members(members) {}

  std::string name;
  uint64_t bytesize;
  std::optional<Id> discriminant;
  std::vector<Id> members;
};

struct Function {
  Function(Id return_type_id, const std::vector<Id>& parameters)
      : return_type_id(return_type_id), parameters(parameters) {}

  Id return_type_id;
  std::vector<Id> parameters;
};

struct ElfSymbol {
  enum class SymbolType { NOTYPE, OBJECT, FUNCTION, COMMON, TLS, GNU_IFUNC };
  enum class Binding { GLOBAL, LOCAL, WEAK, GNU_UNIQUE };
  enum class Visibility { DEFAULT, PROTECTED, HIDDEN, INTERNAL };
  struct VersionInfo {
    auto operator<=>(const VersionInfo&) const = default;
    bool is_default;
    std::string name;
  };
  struct CRC {
    explicit CRC(uint32_t number) : number(number) {}
    auto operator<=>(const CRC&) const = default;
    uint32_t number;
  };
  ElfSymbol(const std::string& symbol_name,
            std::optional<VersionInfo> version_info,
            bool is_defined,
            SymbolType symbol_type,
            Binding binding,
            Visibility visibility,
            std::optional<CRC> crc,
            std::optional<std::string> ns,
            std::optional<Id> type_id,
            const std::optional<std::string>& full_name)
      : symbol_name(symbol_name),
        version_info(version_info),
        is_defined(is_defined),
        symbol_type(symbol_type),
        binding(binding),
        visibility(visibility),
        crc(crc),
        ns(ns),
        type_id(type_id),
        full_name(full_name) {}

  std::string symbol_name;
  std::optional<VersionInfo> version_info;
  bool is_defined;
  SymbolType symbol_type;
  Binding binding;
  Visibility visibility;
  std::optional<CRC> crc;
  std::optional<std::string> ns;
  std::optional<Id> type_id;
  std::optional<std::string> full_name;
};

std::ostream& operator<<(std::ostream& os, ElfSymbol::SymbolType);
std::ostream& operator<<(std::ostream& os, ElfSymbol::Binding);
std::ostream& operator<<(std::ostream& os, ElfSymbol::Visibility);

std::string VersionInfoToString(const ElfSymbol::VersionInfo& version_info);
std::string VersionedSymbolName(const ElfSymbol&);

std::ostream& operator<<(std::ostream& os, ElfSymbol::CRC crc);

struct Interface {
  explicit Interface(const std::map<std::string, Id>& symbols)
      : symbols(symbols) {}
  Interface(const std::map<std::string, Id>& symbols,
            const std::map<std::string, Id>& types)
      : symbols(symbols), types(types) {}

  std::map<std::string, Id> symbols;
  std::map<std::string, Id> types;
};

std::ostream& operator<<(std::ostream& os, Primitive::Encoding encoding);

// Concrete graph type.
class Graph {
 public:
  Id Limit() const {
    return Id(indirection_.size());
  }

  bool Is(Id id) const {
    return indirection_[id.ix_].first != Which::ABSENT;
  }

  Id Allocate() {
    const auto id = Limit();
    indirection_.emplace_back(Which::ABSENT, 0);
    return id;
  }

  template <typename Node, typename... Args>
  void Set(Id id, Args&&... args) {
    auto& reference = indirection_[id.ix_];
    if (reference.first != Which::ABSENT) {
      Die() << "node value already set";
    }
    if constexpr (std::is_same_v<Node, Special>) {
      reference = {Which::SPECIAL, special_.size()};
      special_.emplace_back(std::forward<Args>(args)...);
    } else if constexpr (std::is_same_v<Node, PointerReference>) {
      reference = {Which::POINTER_REFERENCE, pointer_reference_.size()};
      pointer_reference_.emplace_back(std::forward<Args>(args)...);
    } else if constexpr (std::is_same_v<Node, PointerToMember>) {
      reference = {Which::POINTER_TO_MEMBER, pointer_to_member_.size()};
      pointer_to_member_.emplace_back(std::forward<Args>(args)...);
    } else if constexpr (std::is_same_v<Node, Typedef>) {
      reference = {Which::TYPEDEF, typedef_.size()};
      typedef_.emplace_back(std::forward<Args>(args)...);
    } else if constexpr (std::is_same_v<Node, Qualified>) {
      reference = {Which::QUALIFIED, qualified_.size()};
      qualified_.emplace_back(std::forward<Args>(args)...);
    } else if constexpr (std::is_same_v<Node, Primitive>) {
      reference = {Which::PRIMITIVE, primitive_.size()};
      primitive_.emplace_back(std::forward<Args>(args)...);
    } else if constexpr (std::is_same_v<Node, Array>) {
      reference = {Which::ARRAY, array_.size()};
      array_.emplace_back(std::forward<Args>(args)...);
    } else if constexpr (std::is_same_v<Node, BaseClass>) {
      reference = {Which::BASE_CLASS, base_class_.size()};
      base_class_.emplace_back(std::forward<Args>(args)...);
    } else if constexpr (std::is_same_v<Node, Method>) {
      reference = {Which::METHOD, method_.size()};
      method_.emplace_back(std::forward<Args>(args)...);
    } else if constexpr (std::is_same_v<Node, Member>) {
      reference = {Which::MEMBER, member_.size()};
      member_.emplace_back(std::forward<Args>(args)...);
    } else if constexpr (std::is_same_v<Node, VariantMember>) {
      reference = {Which::VARIANT_MEMBER, variant_member_.size()};
      variant_member_.emplace_back(std::forward<Args>(args)...);
    } else if constexpr (std::is_same_v<Node, StructUnion>) {
      reference = {Which::STRUCT_UNION, struct_union_.size()};
      struct_union_.emplace_back(std::forward<Args>(args)...);
    } else if constexpr (std::is_same_v<Node, Enumeration>) {
      reference = {Which::ENUMERATION, enumeration_.size()};
      enumeration_.emplace_back(std::forward<Args>(args)...);
    } else if constexpr (std::is_same_v<Node, Variant>) {
      reference = {Which::VARIANT, variant_.size()};
      variant_.emplace_back(std::forward<Args>(args)...);
    } else if constexpr (std::is_same_v<Node, Function>) {
      reference = {Which::FUNCTION, function_.size()};
      function_.emplace_back(std::forward<Args>(args)...);
    } else if constexpr (std::is_same_v<Node, ElfSymbol>) {
      reference = {Which::ELF_SYMBOL, elf_symbol_.size()};
      elf_symbol_.emplace_back(std::forward<Args>(args)...);
    } else if constexpr (std::is_same_v<Node, Interface>) {
      reference = {Which::INTERFACE, interface_.size()};
      interface_.emplace_back(std::forward<Args>(args)...);
    } else {
      // unfortunately we cannot static_assert(false, "missing case")
      static_assert(std::is_same<Node, Node*>::value, "missing case");
    }
  }

  template <typename Node, typename... Args>
  Id Add(Args&&... args) {
    auto id = Allocate();
    Set<Node>(id, std::forward<Args>(args)...);
    return id;
  }

  void Deallocate(Id) {
    // don't actually do anything, not supported
  }

  void Unset(Id id) {
    auto& reference = indirection_[id.ix_];
    if (reference.first == Which::ABSENT) {
      Die() << "node value already unset";
    }
    reference = {Which::ABSENT, 0};
  }

  void Remove(Id id) {
    Unset(id);
    Deallocate(id);
  }

  template <typename... Args>
  decltype(auto) Apply(auto&& function, Id id, Args&&... args) const {
    const auto& [which, ix] = indirection_[id.ix_];
    return WithVector(*this, which, [&](const auto& vector) -> decltype(auto) {
      return function(vector[ix], std::forward<Args>(args)...);
    });
  }

  template <typename... Args>
  decltype(auto) Apply(auto&& function, Id id, Args&&... args) {
    const auto& [which, ix] = indirection_[id.ix_];
    return WithVector(*this, which, [&](auto& vector) -> decltype(auto) {
      return function(vector[ix], std::forward<Args>(args)...);
    });
  }

  template <typename... Args>
  decltype(auto) Apply2(auto&& function, Id id1, Id id2, Args&&... args) const {
    const auto& [which1, ix1] = indirection_[id1.ix_];
    const auto& [which2, ix2] = indirection_[id2.ix_];
    if (which1 != which2) {
      return function.Mismatch(std::forward<Args>(args)...);
    }
    return WithVector(*this, which1, [&](const auto& vector) -> decltype(auto) {
      return function(vector[ix1], vector[ix2], std::forward<Args>(args)...);
    });
  }

  void ForEach(Id start, Id limit, auto&& function) const {
    for (size_t ix = start.ix_; ix < limit.ix_; ++ix) {
      const Id id(ix);
      if (Is(id)) {
        function(id);
      }
    }
  }

 private:
  enum class Which {
    ABSENT,
    SPECIAL,
    POINTER_REFERENCE,
    POINTER_TO_MEMBER,
    TYPEDEF,
    QUALIFIED,
    PRIMITIVE,
    ARRAY,
    BASE_CLASS,
    METHOD,
    MEMBER,
    VARIANT_MEMBER,
    STRUCT_UNION,
    ENUMERATION,
    VARIANT,
    FUNCTION,
    ELF_SYMBOL,
    INTERFACE,
  };

  static decltype(auto) WithVector(auto& self, Which which, auto&& function) {
    switch (which) {
      case Which::ABSENT:
        Die() << "undefined node";
      case Which::SPECIAL:
        return function(self.special_);
      case Which::POINTER_REFERENCE:
        return function(self.pointer_reference_);
      case Which::POINTER_TO_MEMBER:
        return function(self.pointer_to_member_);
      case Which::TYPEDEF:
        return function(self.typedef_);
      case Which::QUALIFIED:
        return function(self.qualified_);
      case Which::PRIMITIVE:
        return function(self.primitive_);
      case Which::ARRAY:
        return function(self.array_);
      case Which::BASE_CLASS:
        return function(self.base_class_);
      case Which::METHOD:
        return function(self.method_);
      case Which::MEMBER:
        return function(self.member_);
      case Which::VARIANT_MEMBER:
        return function(self.variant_member_);
      case Which::STRUCT_UNION:
        return function(self.struct_union_);
      case Which::ENUMERATION:
        return function(self.enumeration_);
      case Which::VARIANT:
        return function(self.variant_);
      case Which::FUNCTION:
        return function(self.function_);
      case Which::ELF_SYMBOL:
        return function(self.elf_symbol_);
      case Which::INTERFACE:
        return function(self.interface_);
    }
  }

  std::vector<std::pair<Which, uint32_t>> indirection_;

  std::vector<Special> special_;
  std::vector<PointerReference> pointer_reference_;
  std::vector<PointerToMember> pointer_to_member_;
  std::vector<Typedef> typedef_;
  std::vector<Qualified> qualified_;
  std::vector<Primitive> primitive_;
  std::vector<Array> array_;
  std::vector<BaseClass> base_class_;
  std::vector<Method> method_;
  std::vector<Member> member_;
  std::vector<VariantMember> variant_member_;
  std::vector<StructUnion> struct_union_;
  std::vector<Enumeration> enumeration_;
  std::vector<Variant> variant_;
  std::vector<Function> function_;
  std::vector<ElfSymbol> elf_symbol_;
  std::vector<Interface> interface_;
};

struct InterfaceKey {
  explicit InterfaceKey(const Graph& graph) : graph(graph) {}

  std::string operator()(Id id) const {
    return graph.Apply(*this, id);
  }

  std::string operator()(const stg::Typedef& x) const {
    return x.name;
  }

  std::string operator()(const stg::StructUnion& x) const {
    if (x.name.empty()) {
      Die() << "anonymous struct/union interface type";
    }
    std::ostringstream os;
    os << x.kind << ' ' << x.name;
    return os.str();
  }

  std::string operator()(const stg::Enumeration& x) const {
    if (x.name.empty()) {
      Die() << "anonymous enum interface type";
    }
    return "enum " + x.name;
  }

  std::string operator()(const stg::Variant& x) const {
    if (x.name.empty()) {
      Die() << "anonymous variant interface type";
    }
    return "variant " + x.name;
  }

  std::string operator()(const stg::ElfSymbol& x) const {
    return VersionedSymbolName(x);
  }

  std::string operator()(const auto&) const {
    Die() << "unexpected interface type";
  }

  const Graph& graph;
};

// Roughly equivalent to std::set<Id> but with constant time operations and
// key set limited to allocated Ids.
class DenseIdSet {
 public:
  DenseIdSet(Id start, Id limit) : offset_(start.ix_) {
    ids_.reserve(limit.ix_ - offset_);
  }
  bool Insert(Id id) {
    const auto ix = id.ix_;
    if (ix < offset_) {
      Die() << "DenseIdSet: out of range access";
    }
    const auto offset_ix = ix - offset_;
    if (offset_ix >= ids_.size()) {
      ids_.resize(offset_ix + 1, false);
    }
    if (ids_[offset_ix]) {
      return false;
    }
    ids_[offset_ix] = true;
    return true;
  }

 private:
  size_t offset_;
  std::vector<bool> ids_;
};

// Roughly equivalent to std::map<Id, Id>, defaulted to the identity mapping,
// but with constant time operations and key set limited to allocated Ids.
class DenseIdMapping {
 public:
  DenseIdMapping(Id start, Id limit) : offset_(start.ix_) {
    ids_.reserve(limit.ix_ - offset_);
  }

  // returns an updatable reference
  Id& Get(Id& id) {
    return At(id);
  }

  // records a new mapping
  void Add(Id child, Id parent) {
    At(child) = parent;
  }

 private:
  Id& At(Id id) {
    const auto ix = id.ix_;
    if (ix < offset_) {
      Die() << "DenseIdMapping: out of range access";
    }
    Populate(ix + 1);
    return ids_[ix - offset_];
  }

  void Populate(size_t size) {
    for (size_t ix = offset_ + ids_.size(); ix < size; ++ix) {
      ids_.emplace_back(ix);
    }
  }

  size_t offset_;
  std::vector<Id> ids_;
};

template <typename ExternalId>
class Maker {
 public:
  explicit Maker(Graph& graph) : graph_(graph) {}

  ~Maker() noexcept(false) {
    if (std::uncaught_exceptions() == 0) {
      if (undefined_ > 0) {
        Die die;
        die << "undefined nodes:";
        for (const auto& [external_id, id] : map_) {
          if (!graph_.Is(id)) {
            die << ' ' << external_id;
          }
        }
      }
    }
  }

  Id Get(const ExternalId& external_id) {
    auto [it, inserted] = map_.emplace(external_id, 0);
    if (inserted) {
      it->second = graph_.Allocate();
      ++undefined_;
    }
    return it->second;
  }

  template <typename Node, typename... Args>
  Id Set(const ExternalId& external_id, Args&&... args) {
    return Set<Node>(DieDuplicate, external_id, std::forward<Args>(args)...);
  }

  template <typename Node, typename... Args>
  Id MaybeSet(const ExternalId& external_id, Args&&... args) {
    return Set<Node>(WarnDuplicate, external_id, std::forward<Args>(args)...);
  }

  template <typename Node, typename... Args>
  Id Add(Args&&... args) {
    return graph_.Add<Node>(std::forward<Args>(args)...);
  }

 private:
  Graph& graph_;
  size_t undefined_ = 0;
  std::unordered_map<ExternalId, Id> map_;

  template <typename Node, typename... Args>
  Id Set(void(& fail)(const ExternalId&), const ExternalId& external_id,
         Args&&... args) {
    const Id id = Get(external_id);
    if (graph_.Is(id)) {
      fail(external_id);
    } else {
      graph_.Set<Node>(id, std::forward<Args>(args)...);
      --undefined_;
    }
    return id;
  }

  // These helpers should probably not be inlined.
  [[noreturn]] static void DieDuplicate(const ExternalId& external_id) {
    Die() << "duplicate definition of node: " << external_id;
  }
  static void WarnDuplicate(const ExternalId& external_id) {
    Warn() << "ignoring duplicate definition of node: " << external_id;
  }
};

}  // namespace stg

#endif  // STG_GRAPH_H_
