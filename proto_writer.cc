// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -*- mode: C++ -*-
//
// Copyright 2022-2024 Google LLC
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
// Author: Siddharth Nayyar

#include "proto_writer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <ostream>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/repeated_ptr_field.h>
#include <google/protobuf/text_format.h>
#include "error.h"
#include "graph.h"
#include "naming.h"
#include "stable_hash.h"
#include "stg.pb.h"

namespace stg {
namespace proto {

namespace {

class StableId {
 public:
  explicit StableId(const Graph& graph) : stable_hash_(graph) {}

  uint32_t operator()(Id id) {
    return stable_hash_(id).value;
  }

 private:
  StableHash stable_hash_;
};

template <typename MapId>
struct Transform {
  Transform(const Graph& graph, proto::STG& stg, MapId& map_id)
      : graph(graph), stg(stg), map_id(map_id) {}

  uint32_t operator()(Id);

  void operator()(const stg::Special&, uint32_t);
  void operator()(const stg::PointerReference&, uint32_t);
  void operator()(const stg::PointerToMember&, uint32_t);
  void operator()(const stg::Typedef&, uint32_t);
  void operator()(const stg::Qualified&, uint32_t);
  void operator()(const stg::Primitive&, uint32_t);
  void operator()(const stg::Array&, uint32_t);
  void operator()(const stg::BaseClass&, uint32_t);
  void operator()(const stg::Method&, uint32_t);
  void operator()(const stg::Member&, uint32_t);
  void operator()(const stg::VariantMember&, uint32_t);
  void operator()(const stg::StructUnion&, uint32_t);
  void operator()(const stg::Enumeration&, uint32_t);
  void operator()(const stg::Variant&, uint32_t);
  void operator()(const stg::Function&, uint32_t);
  void operator()(const stg::ElfSymbol&, uint32_t);
  void operator()(const stg::Interface&, uint32_t);

  Special::Kind operator()(stg::Special::Kind);
  PointerReference::Kind operator()(stg::PointerReference::Kind);
  Qualified::Qualifier operator()(stg::Qualifier);
  Primitive::Encoding operator()(stg::Primitive::Encoding);
  BaseClass::Inheritance operator()(stg::BaseClass::Inheritance);
  StructUnion::Kind operator()(stg::StructUnion::Kind);
  ElfSymbol::SymbolType operator()(stg::ElfSymbol::SymbolType);
  ElfSymbol::Binding operator()(stg::ElfSymbol::Binding);
  ElfSymbol::Visibility operator()(stg::ElfSymbol::Visibility);

  std::unordered_map<uint32_t, Id> GetInternalIdByExternalIdMap() {
    std::unordered_map<uint32_t, Id> internal_id_map;
    for (const auto& [id, ext_id] : external_id_by_internal_id) {
      internal_id_map.emplace(ext_id, id);
    }
    return internal_id_map;
  }

  const Graph& graph;
  proto::STG& stg;
  std::unordered_map<Id, uint32_t> external_id_by_internal_id;
  std::unordered_set<uint32_t> used_ids;

  // Function object: Id -> uint32_t
  MapId& map_id;
};

template <typename MapId>
uint32_t Transform<MapId>::operator()(Id id) {
  auto [it, inserted] = external_id_by_internal_id.emplace(id, 0);
  if (inserted) {
    uint32_t mapped_id = map_id(id);

    // Ensure uniqueness of external ids. It is best to probe here since id
    // generators will not in general guarantee that the mapping from internal
    // ids to external ids will be injective.
    while (!used_ids.insert(mapped_id).second) {
      ++mapped_id;
    }
    it->second = mapped_id;
    graph.Apply(*this, id, mapped_id);
  }
  return it->second;
}

template <typename MapId>
void Transform<MapId>::operator()(const stg::Special& x, uint32_t id) {
  auto& special = *stg.add_special();
  special.set_id(id);
  special.set_kind((*this)(x.kind));
}

template <typename MapId>
void Transform<MapId>::operator()(const stg::PointerReference& x, uint32_t id) {
  auto& pointer_reference = *stg.add_pointer_reference();
  pointer_reference.set_id(id);
  pointer_reference.set_kind((*this)(x.kind));
  pointer_reference.set_pointee_type_id((*this)(x.pointee_type_id));
}

template <typename MapId>
void Transform<MapId>::operator()(const stg::PointerToMember& x, uint32_t id) {
  auto& pointer_to_member = *stg.add_pointer_to_member();
  pointer_to_member.set_id(id);
  pointer_to_member.set_containing_type_id((*this)(x.containing_type_id));
  pointer_to_member.set_pointee_type_id((*this)(x.pointee_type_id));
}

template <typename MapId>
void Transform<MapId>::operator()(const stg::Typedef& x, uint32_t id) {
  auto& typedef_ = *stg.add_typedef_();
  typedef_.set_id(id);
  typedef_.set_name(x.name);
  typedef_.set_referred_type_id((*this)(x.referred_type_id));
}

template <typename MapId>
void Transform<MapId>::operator()(const stg::Qualified& x, uint32_t id) {
  auto& qualified = *stg.add_qualified();
  qualified.set_id(id);
  qualified.set_qualifier((*this)(x.qualifier));
  qualified.set_qualified_type_id((*this)(x.qualified_type_id));
}

template <typename MapId>
void Transform<MapId>::operator()(const stg::Primitive& x, uint32_t id) {
  auto& primitive = *stg.add_primitive();
  primitive.set_id(id);
  primitive.set_name(x.name);
  if (x.encoding) {
    primitive.set_encoding((*this)(*x.encoding));
  }
  primitive.set_bytesize(x.bytesize);
}

template <typename MapId>
void Transform<MapId>::operator()(const stg::Array& x, uint32_t id) {
  auto& array = *stg.add_array();
  array.set_id(id);
  array.set_number_of_elements(x.number_of_elements);
  array.set_element_type_id((*this)(x.element_type_id));
}

template <typename MapId>
void Transform<MapId>::operator()(const stg::BaseClass& x, uint32_t id) {
  auto& base_class = *stg.add_base_class();
  base_class.set_id(id);
  base_class.set_type_id((*this)(x.type_id));
  base_class.set_offset(x.offset);
  base_class.set_inheritance((*this)(x.inheritance));
}

template <typename MapId>
void Transform<MapId>::operator()(const stg::Method& x, uint32_t id) {
  auto& method = *stg.add_method();
  method.set_id(id);
  method.set_mangled_name(x.mangled_name);
  method.set_name(x.name);
  method.set_vtable_offset(x.vtable_offset);
  method.set_type_id((*this)(x.type_id));
}

template <typename MapId>
void Transform<MapId>::operator()(const stg::Member& x, uint32_t id) {
  auto& member = *stg.add_member();
  member.set_id(id);
  member.set_name(x.name);
  member.set_type_id((*this)(x.type_id));
  member.set_offset(x.offset);
  member.set_bitsize(x.bitsize);
}

template <typename MapId>
void Transform<MapId>::operator()(const stg::VariantMember& x, uint32_t id) {
  auto& variant_member = *stg.add_variant_member();
  variant_member.set_id(id);
  variant_member.set_name(x.name);
  if (x.discriminant_value) {
    variant_member.set_discriminant_value(*x.discriminant_value);
  }
  variant_member.set_type_id((*this)(x.type_id));
}

template <typename MapId>
void Transform<MapId>::operator()(const stg::StructUnion& x, uint32_t id) {
  auto& struct_union = *stg.add_struct_union();
  struct_union.set_id(id);
  struct_union.set_kind((*this)(x.kind));
  struct_union.set_name(x.name);
  if (x.definition) {
    auto& definition = *struct_union.mutable_definition();
    definition.set_bytesize(x.definition->bytesize);
    for (const auto id : x.definition->base_classes) {
      definition.add_base_class_id((*this)(id));
    }
    for (const auto id : x.definition->methods) {
      definition.add_method_id((*this)(id));
    }
    for (const auto id : x.definition->members) {
      definition.add_member_id((*this)(id));
    }
  }
}

template <typename MapId>
void Transform<MapId>::operator()(const stg::Enumeration& x, uint32_t id) {
  auto& enumeration = *stg.add_enumeration();
  enumeration.set_id(id);
  enumeration.set_name(x.name);
  if (x.definition) {
    auto& definition = *enumeration.mutable_definition();
    definition.set_underlying_type_id(
        (*this)(x.definition->underlying_type_id));
    for (const auto& [name, value] : x.definition->enumerators) {
      auto& enumerator = *definition.add_enumerator();
      enumerator.set_name(name);
      enumerator.set_value(value);
    }
  }
}

template <typename MapId>
void Transform<MapId>::operator()(const stg::Variant& x, uint32_t id) {
  auto& variant = *stg.add_variant();
  variant.set_id(id);
  variant.set_name(x.name);
  variant.set_bytesize(x.bytesize);
  if (x.discriminant.has_value()) {
    variant.set_discriminant((*this)(x.discriminant.value()));
  }
  for (const auto id : x.members) {
    variant.add_member_id((*this)(id));
  }
}

template <typename MapId>
void Transform<MapId>::operator()(const stg::Function& x, uint32_t id) {
  auto& function = *stg.add_function();
  function.set_id(id);
  function.set_return_type_id((*this)(x.return_type_id));
  for (const auto id : x.parameters) {
    function.add_parameter_id((*this)(id));
  }
}

template <typename MapId>
void Transform<MapId>::operator()(const stg::ElfSymbol& x, uint32_t id) {
  auto& elf_symbol = *stg.add_elf_symbol();
  elf_symbol.set_id(id);
  elf_symbol.set_name(x.symbol_name);
  if (x.version_info) {
    auto& version_info = *elf_symbol.mutable_version_info();
    version_info.set_is_default(x.version_info->is_default);
    version_info.set_name(x.version_info->name);
  }
  elf_symbol.set_is_defined(x.is_defined);
  elf_symbol.set_symbol_type((*this)(x.symbol_type));
  elf_symbol.set_binding((*this)(x.binding));
  elf_symbol.set_visibility((*this)(x.visibility));
  if (x.crc) {
    elf_symbol.set_crc(x.crc->number);
  }
  if (x.ns) {
    elf_symbol.set_namespace_(*x.ns);
  }
  if (x.type_id) {
    elf_symbol.set_type_id((*this)(*x.type_id));
  }
  if (x.full_name) {
    elf_symbol.set_full_name(*x.full_name);
  }
}

template <typename MapId>
void Transform<MapId>::operator()(const stg::Interface& x, uint32_t id) {
  auto& interface = *stg.add_interface();
  interface.set_id(id);
  for (const auto& [_, id] : x.symbols) {
    interface.add_symbol_id((*this)(id));
  }
  for (const auto& [_, id] : x.types) {
    interface.add_type_id((*this)(id));
  }
}

template <typename MapId>
PointerReference::Kind Transform<MapId>::operator()(
    stg::PointerReference::Kind x) {
  switch (x) {
    case stg::PointerReference::Kind::POINTER:
      return PointerReference::POINTER;
    case stg::PointerReference::Kind::LVALUE_REFERENCE:
      return PointerReference::LVALUE_REFERENCE;
    case stg::PointerReference::Kind::RVALUE_REFERENCE:
      return PointerReference::RVALUE_REFERENCE;
  }
}

template <typename MapId>
Special::Kind Transform<MapId>::operator()(
    stg::Special::Kind x) {
  switch (x) {
    case stg::Special::Kind::VOID:
      return Special::VOID;
    case stg::Special::Kind::VARIADIC:
      return Special::VARIADIC;
    case stg::Special::Kind::NULLPTR:
      return Special::NULLPTR;
  }
}

template <typename MapId>
Qualified::Qualifier Transform<MapId>::operator()(stg::Qualifier x) {
  switch (x) {
    case stg::Qualifier::CONST:
      return Qualified::CONST;
    case stg::Qualifier::VOLATILE:
      return Qualified::VOLATILE;
    case stg::Qualifier::RESTRICT:
      return Qualified::RESTRICT;
    case stg::Qualifier::ATOMIC:
      return Qualified::ATOMIC;
  }
}

template <typename MapId>
Primitive::Encoding Transform<MapId>::operator()(stg::Primitive::Encoding x) {
  switch (x) {
    case stg::Primitive::Encoding::BOOLEAN:
      return Primitive::BOOLEAN;
    case stg::Primitive::Encoding::SIGNED_INTEGER:
      return Primitive::SIGNED_INTEGER;
    case stg::Primitive::Encoding::UNSIGNED_INTEGER:
      return Primitive::UNSIGNED_INTEGER;
    case stg::Primitive::Encoding::SIGNED_CHARACTER:
      return Primitive::SIGNED_CHARACTER;
    case stg::Primitive::Encoding::UNSIGNED_CHARACTER:
      return Primitive::UNSIGNED_CHARACTER;
    case stg::Primitive::Encoding::REAL_NUMBER:
      return Primitive::REAL_NUMBER;
    case stg::Primitive::Encoding::COMPLEX_NUMBER:
      return Primitive::COMPLEX_NUMBER;
    case stg::Primitive::Encoding::UTF:
      return Primitive::UTF;
  }
}

template <typename MapId>
BaseClass::Inheritance Transform<MapId>::operator()(
    stg::BaseClass::Inheritance x) {
  switch (x) {
    case stg::BaseClass::Inheritance::NON_VIRTUAL:
      return BaseClass::NON_VIRTUAL;
    case stg::BaseClass::Inheritance::VIRTUAL:
      return BaseClass::VIRTUAL;
  }
}

template <typename MapId>
StructUnion::Kind Transform<MapId>::operator()(stg::StructUnion::Kind x) {
  switch (x) {
    case stg::StructUnion::Kind::STRUCT:
      return StructUnion::STRUCT;
    case stg::StructUnion::Kind::UNION:
      return StructUnion::UNION;
  }
}

template <typename MapId>
ElfSymbol::SymbolType Transform<MapId>::operator()(
    stg::ElfSymbol::SymbolType x) {
  switch (x) {
    case stg::ElfSymbol::SymbolType::NOTYPE:
      return ElfSymbol::NOTYPE;
    case stg::ElfSymbol::SymbolType::OBJECT:
      return ElfSymbol::OBJECT;
    case stg::ElfSymbol::SymbolType::FUNCTION:
      return ElfSymbol::FUNCTION;
    case stg::ElfSymbol::SymbolType::COMMON:
      return ElfSymbol::COMMON;
    case stg::ElfSymbol::SymbolType::TLS:
      return ElfSymbol::TLS;
    case stg::ElfSymbol::SymbolType::GNU_IFUNC:
      return ElfSymbol::GNU_IFUNC;
  }
}

template <typename MapId>
ElfSymbol::Binding Transform<MapId>::operator()(stg::ElfSymbol::Binding x) {
  switch (x) {
    case stg::ElfSymbol::Binding::GLOBAL:
      return ElfSymbol::GLOBAL;
    case stg::ElfSymbol::Binding::LOCAL:
      return ElfSymbol::LOCAL;
    case stg::ElfSymbol::Binding::WEAK:
      return ElfSymbol::WEAK;
    case stg::ElfSymbol::Binding::GNU_UNIQUE:
      return ElfSymbol::GNU_UNIQUE;
  }
}

template <typename MapId>
ElfSymbol::Visibility Transform<MapId>::operator()(
    stg::ElfSymbol::Visibility x) {
  switch (x) {
    case stg::ElfSymbol::Visibility::DEFAULT:
      return ElfSymbol::DEFAULT;
    case stg::ElfSymbol::Visibility::PROTECTED:
      return ElfSymbol::PROTECTED;
    case stg::ElfSymbol::Visibility::HIDDEN:
      return ElfSymbol::HIDDEN;
    case stg::ElfSymbol::Visibility::INTERNAL:
      return ElfSymbol::INTERNAL;
  }
}

template <typename ProtoNode>
void SortNodesById(google::protobuf::RepeatedPtrField<ProtoNode>& nodes) {
  const auto compare = [](const auto* lhs, const auto* rhs) {
    return lhs->id() < rhs->id();
  };
  std::sort(nodes.pointer_begin(), nodes.pointer_end(), compare);
}

template <typename ProtoNode>
void SortNodesByName(google::protobuf::RepeatedPtrField<ProtoNode>& nodes) {
  const auto compare = [](const auto* lhs, const auto* rhs) {
    return std::forward_as_tuple(lhs->name(), lhs->id())
        < std::forward_as_tuple(rhs->name(), rhs->id());
  };
  std::sort(nodes.pointer_begin(), nodes.pointer_end(), compare);
}

void SortMethodsByMangledName(google::protobuf::RepeatedPtrField<Method>& methods) {
  const auto compare = [](const Method* lhs, const Method* rhs) {
    return std::forward_as_tuple(lhs->mangled_name(), lhs->id())
        < std::forward_as_tuple(rhs->mangled_name(), rhs->id());
  };
  std::sort(methods.pointer_begin(), methods.pointer_end(), compare);
}

void SortElfSymbolsByVersionedName(
    google::protobuf::RepeatedPtrField<ElfSymbol>& elf_symbols) {
  const auto compare = [](const ElfSymbol* lhs, const ElfSymbol* rhs) {
    // Sorting by:
    //
    // name
    // version name
    // ID as tie-breaker
    //
    // Note: symbols without version information will be ordered before
    // versioned symbols of the same name.
    return std::forward_as_tuple(lhs->name(), lhs->version_info().name(),
                                 lhs->id())
        < std::forward_as_tuple(rhs->name(), rhs->version_info().name(),
                                rhs->id());
  };
  std::sort(elf_symbols.pointer_begin(), elf_symbols.pointer_end(), compare);
}

void SortNodes(STG& stg) {
  SortNodesById(*stg.mutable_void_());
  SortNodesById(*stg.mutable_variadic());
  SortNodesById(*stg.mutable_pointer_reference());
  SortNodesById(*stg.mutable_pointer_to_member());
  SortNodesByName(*stg.mutable_typedef_());
  SortNodesById(*stg.mutable_qualified());
  SortNodesById(*stg.mutable_primitive());
  SortNodesById(*stg.mutable_array());
  SortNodesById(*stg.mutable_base_class());
  SortMethodsByMangledName(*stg.mutable_method());
  SortNodesByName(*stg.mutable_member());
  SortNodesByName(*stg.mutable_struct_union());
  SortNodesByName(*stg.mutable_enumeration());
  SortNodesById(*stg.mutable_function());
  SortElfSymbolsByVersionedName(*stg.mutable_elf_symbol());
}

class HexPrinter : public google::protobuf::TextFormat::FastFieldValuePrinter {
  void PrintUInt32(
      uint32_t value,
      google::protobuf::TextFormat::BaseTextGenerator* generator) const final {
    std::ostringstream os;
    // 0x01234567
    os << "0x" << std::hex << std::setfill('0') << std::setw(8) << value;
    generator->PrintString(os.str());
  }
};

class AnnotationHexPrinter : public google::protobuf::TextFormat::FastFieldValuePrinter {
 public:
  AnnotationHexPrinter(
      Describe& describe,
      const std::unordered_map<uint32_t, Id>& internal_id_by_external_id)
      : describe_(describe),
        internal_id_by_external_id_(internal_id_by_external_id) {}

 private:
  void PrintUInt32(
      uint32_t value,
      google::protobuf::TextFormat::BaseTextGenerator* generator) const final {
    std::ostringstream os;
    // 0x01234567  # Describe(0x01234567)
    os << "0x" << std::hex << std::setfill('0') << std::setw(8) << value
       << "  # " << describe_(internal_id_by_external_id_.at(value));
    generator->PrintString(os.str());
  }

  Describe& describe_;
  const std::unordered_map<uint32_t, Id>& internal_id_by_external_id_;
};

const uint32_t kWrittenFormatVersion = 2;

// Collection of fields which represent edges in the STG proto.
//
// This collection is used to register the AnnotationHexPrinter for each of the
// fields, which will print a description of the node in STG to which the edge
// points.
const std::array<const google::protobuf::FieldDescriptor*, 19> edge_descriptors = {
    PointerReference::descriptor()->FindFieldByNumber(3),
    PointerToMember::descriptor()->FindFieldByNumber(3),
    Typedef::descriptor()->FindFieldByNumber(3),
    Qualified::descriptor()->FindFieldByNumber(3),
    Array::descriptor()->FindFieldByNumber(3),
    BaseClass::descriptor()->FindFieldByNumber(2),
    Method::descriptor()->FindFieldByNumber(5),
    Member::descriptor()->FindFieldByNumber(3),
    VariantMember::descriptor()->FindFieldByNumber(4),
    StructUnion::Definition::descriptor()->FindFieldByNumber(2),
    StructUnion::Definition::descriptor()->FindFieldByNumber(3),
    StructUnion::Definition::descriptor()->FindFieldByNumber(4),
    Enumeration::Definition::descriptor()->FindFieldByNumber(1),
    Function::descriptor()->FindFieldByNumber(2),
    Function::descriptor()->FindFieldByNumber(3),
    ElfSymbol::descriptor()->FindFieldByNumber(10),
    Interface::descriptor()->FindFieldByNumber(2),
    Interface::descriptor()->FindFieldByNumber(3),
    STG::descriptor()->FindFieldByNumber(2),
};

}  // namespace

void Writer::Write(const Id& root, google::protobuf::io::ZeroCopyOutputStream& os,
                   bool annotate) {
  proto::STG stg;
  StableId stable_id(graph_);
  Transform<StableId> transform(graph_, stg, stable_id);
  stg.set_root_id(transform(root));
  SortNodes(stg);
  stg.set_version(kWrittenFormatVersion);

  // Print
  google::protobuf::TextFormat::Printer printer;
  printer.SetDefaultFieldValuePrinter(new HexPrinter());
  if (annotate) {
    NameCache names;
    Describe describe(graph_, names);
    auto internal_id_by_external_id = transform.GetInternalIdByExternalIdMap();
    for (const auto* descriptor : edge_descriptors) {
      Check(printer.RegisterFieldValuePrinter(
          descriptor,
          new AnnotationHexPrinter(describe, internal_id_by_external_id)))
          << "Failed to register annotation printer for descriptor: "
          << descriptor->name();
    }
    Check(printer.Print(stg, &os)) << "Failed to write STG";
  } else {
    Check(printer.Print(stg, &os)) << "Failed to write STG";
  }
}

}  // namespace proto
}  // namespace stg
