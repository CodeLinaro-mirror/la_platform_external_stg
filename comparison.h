// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -*- mode: C++ -*-
//
// Copyright 2020-2024 Google LLC
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

#ifndef STG_COMPARISON_H_
#define STG_COMPARISON_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "graph.h"
#include "runtime.h"

namespace stg {
namespace diff {

struct Ignore {
  enum Value {
    // noise reduction
    SYMBOL_TYPE_PRESENCE,
    TYPE_DECLARATION_STATUS,
    PRIMITIVE_TYPE_ENCODING,
    MEMBER_SIZE,
    ENUM_UNDERLYING_TYPE,
    QUALIFIER,
    SYMBOL_CRC,
    // ABI compatibility testing
    INTERFACE_ADDITION,
    TYPE_DEFINITION_ADDITION,
  };

  using Bitset = uint16_t;

  Ignore() = default;
  template <typename... Values>
  explicit Ignore(Values... values) {
    for (auto value : {values...}) {
      Set(value);
    }
  }

  void Set(Value other) {
    bitset = bitset | (1 << other);
  }
  bool Test(Value other) const {
    return (bitset & (1 << other)) != 0;
  }

  Bitset bitset = 0;
};

std::optional<Ignore::Value> ParseIgnore(std::string_view ignore);

struct IgnoreUsage {};
std::ostream& operator<<(std::ostream& os, IgnoreUsage);

using Comparison = std::pair<std::optional<Id>, std::optional<Id>>;

struct DiffDetail {
  DiffDetail(const std::string& text, const std::optional<Comparison>& edge)
      : text(text), edge(edge) {}
  std::string text;
  std::optional<Comparison> edge;
};

struct Diff {
  // This diff node corresponds to an entity that is reportable, if it or any of
  // its children (excluding reportable ones) has changed.
  bool holds_changes = false;
  // This diff node contains a local (non-recursive) change.
  bool has_changes = false;
  std::vector<DiffDetail> details;

  void Add(const std::string& text,
           const std::optional<Comparison>& comparison) {
    details.emplace_back(text, comparison);
  }
};

struct HashComparison {
  size_t operator()(const Comparison& comparison) const {
    size_t seed = 0;
    const std::hash<std::optional<Id>> h;
    combine_hash(seed, h(comparison.first));
    combine_hash(seed, h(comparison.second));
    return seed;
  }
  static void combine_hash(size_t& seed, size_t hash) {
    seed ^= hash + 0x9e3779b97f4a7c15 + (seed << 12) + (seed >> 4);
  }
};

using Outcomes = std::unordered_map<Comparison, Diff, HashComparison>;

std::pair<Id, std::vector<std::string>> ResolveTypedefs(
    const Graph& graph, Id id);

std::pair<bool, std::optional<Comparison>>
    Compare(Runtime& runtime, Ignore ignore, const Graph& graph,
            Id root1, Id root2, Outcomes& outcomes);

}  // namespace diff
}  // namespace stg

#endif  // STG_COMPARISON_H_
