// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -*- mode: C++ -*-
//
// Copyright 2022-2025 Google LLC
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

#ifndef STG_DWARF_WRAPPERS_H_
#define STG_DWARF_WRAPPERS_H_

#include <elfutils/libdw.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "number.h"

namespace stg {
namespace dwarf {

struct Location {
  // ADDRESS - relocated, section-relative offset
  // TLS - broken (elfutils bug), TLS-relative offset
  //       TODO: match TLS variables by offset
  enum class Kind { ADDRESS, TLS };

  Location(Kind kind, uint64_t value) : kind(kind), value(value) {}
  auto operator<=>(const Location&) const = default;

  Kind kind;
  uint64_t value;
};

std::ostream& operator<<(std::ostream& os, const Location& location);

// C++ wrapper over Dwarf_Die, providing interface for its various properties.
struct Entry {
  // All methods in libdw take Dwarf_Die by non-const pointer as libdw caches
  // in it a link to the associated abbreviation table. Updating this link is
  // not thread-safe and so we cannot, for example, hold a std::shared_ptr to a
  // heap-allocated Dwarf_Die.
  //
  // The only options left are holding a std::unique_ptr or storing a value.
  // Unique pointers will add one more level of indirection to a hot path.
  // So we choose to store Dwarf_Die values.
  //
  // Each Entry only contains references to DWARF file memory and is fairly
  // small (32 bytes), so copies can be easily made if necessary. However,
  // within one thread it is preferable to pass it by reference.
  Dwarf_Die die{};

  // Get list of direct descendants of an entry in the DWARF tree.
  std::vector<Entry> GetChildren();

  // All getters are non-const as libdw may need to modify Dwarf_Die.
  int GetTag();
  Dwarf_Off GetOffset();
  std::optional<std::string> MaybeGetString(uint32_t attribute);
  std::optional<std::string> MaybeGetDirectString(uint32_t attribute);
  std::optional<uint64_t> MaybeGetUnsignedConstant(uint32_t attribute);
  uint64_t MustGetUnsignedConstant(uint32_t attribute);
  std::optional<Number> MaybeGetConstant(uint32_t attribute);
  bool GetFlag(uint32_t attribute);
  std::optional<Entry> MaybeGetReference(uint32_t attribute);
  std::optional<Location> MaybeGetLocation(uint32_t attribute);
  std::vector<Location> MaybeGetRangeStarts();
  std::optional<uint64_t> MaybeGetMemberByteOffset();
  std::optional<uint64_t> MaybeGetVtableOffset();
  // Returns value of subrange element count if it is constant or nullopt if it
  // is not defined or cannot be represented as constant.
  std::optional<uint64_t> MaybeGetCount();
};

// Metadata and top-level entry of a compilation unit.
struct CompilationUnit {
  int version;
  Entry entry;
};

std::vector<CompilationUnit> GetCompilationUnits(Dwarf& dwarf);

class Files {
 public:
  Files() = default;
  explicit Files(Entry& compilation_unit);
  std::optional<std::string> MaybeGetFile(Entry& entry,
                                          uint32_t attribute) const;

 private:
  Dwarf_Files* files_ = nullptr;
  size_t files_count_ = 0;
};

}  // namespace dwarf
}  // namespace stg

#endif  // STG_DWARF_WRAPPERS_H_
