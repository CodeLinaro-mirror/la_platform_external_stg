// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -*- mode: C++ -*-
//
// Copyright 2021-2026 Google LLC
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
// Author: Giuliano Procida

#ifndef STG_ORDER_H_
#define STG_ORDER_H_

#include <algorithm>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "error.h"

namespace stg {

// Match and reorder two collections using a key-extraction lambda.
// Invokes callbacks on-the-fly for unmatched and matched elements in a
// greedy, order-preserving manner.
template <typename T, typename ExtractKey, typename Removed, typename Added,
          typename InBoth>
void MatchReorderForEach(const std::vector<T>& items1,
                         const std::vector<T>& items2, ExtractKey&& extract_key,
                         Removed&& removed, Added&& added, InBoth&& in_both) {
  using Key = std::decay_t<std::invoke_result_t<ExtractKey, const T&>>;

  // Copy the key extractor to ensure that if it is stateful (e.g. anonymous
  // counter), both phases (building the map and matching) start with the same
  // state and thus generate the same keys for matching.
  auto extract_key2 = extract_key;

  const size_t size1 = items1.size();
  const size_t size2 = items2.size();

  struct IndexMatch {
    size_t index;
    bool matched;
  };

  // map Key to index in items2
  std::unordered_map<Key, IndexMatch> key_to_index2;
  key_to_index2.reserve(size2);

  for (size_t ix2 = 0; ix2 < size2; ++ix2) {
    const auto [it, inserted] = key_to_index2.emplace(extract_key2(items2[ix2]),
                                                      IndexMatch{ix2, false});
    Check(inserted) << "MatchReorderForEach: duplicate key in items2";
  }

  // map index in items1 to index in items2 (if matched)
  std::vector<std::optional<size_t>> ix1_to_ix2(size1, std::nullopt);
  // map index in items2 to index in items1 (if matched)
  std::vector<std::optional<size_t>> ix2_to_ix1(size2, std::nullopt);

  for (size_t ix1 = 0; ix1 < size1; ++ix1) {
    const auto key = extract_key(items1[ix1]);
    const auto it = key_to_index2.find(key);
    if (it != key_to_index2.end()) {
      auto& match = it->second;
      Check(!match.matched) << "MatchReorderForEach: duplicate key in items1";
      match.matched = true;
      ix1_to_ix2[ix1] = match.index;
      ix2_to_ix1[match.index] = ix1;
    }
  }

  // keep track of where we are up to in items2
  size_t position2 = 0;

  for (size_t ix1 = 0; ix1 < size1; ++ix1) {
    const auto match_ix2 = ix1_to_ix2[ix1];
    if (!match_ix2) {
      // ix1 is unmatched (removed)
      removed(items1[ix1]);
    } else {
      // ix1 is matched at match_ix2
      // If match_ix2 is already output (because we pulled it forward), do
      // nothing.
      if (*match_ix2 >= position2) {
        // output all items in items2 from position2 up to match_ix2 (inclusive)
        for (size_t i = position2; i <= *match_ix2; ++i) {
          const auto match_ix1 = ix2_to_ix1[i];
          if (!match_ix1) {
            added(items2[i]);
          } else {
            in_both(items1[*match_ix1], items2[i]);
          }
        }
        position2 = *match_ix2 + 1;
      }
    }
  }

  // output remaining items in items2 (all unmatched)
  for (size_t i = position2; i < size2; ++i) {
    added(items2[i]);
  }
}

}  // namespace stg

#endif  // STG_ORDER_H_
