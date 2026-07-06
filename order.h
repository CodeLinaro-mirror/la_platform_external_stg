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
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "error.h"

namespace stg {

// Match and reorder two collections using a key-extraction lambda.
// Invokes callbacks on-the-fly for unmatched and matched elements in a
// greedy, order-preserving manner.
//
// The two orderings are reconciled by examining each item from the first
// sequence in turn. If it is not present in the second sequence, it is greedily
// output as removed. If it is present but hasn't yet been output, then all
// items from the current position in the second sequence up to and including it
// are output in bulk (as added or in-both). Otherwise it is skipped. Finally,
// all remaining items from the second sequence are output as added.
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

  std::vector<size_t> indexes2(size2);
  size_t unmatched2_id = size1;

  for (size_t ix1 = 0; ix1 < size1; ++ix1) {
    const auto key = extract_key(items1[ix1]);
    const auto it = key_to_index2.find(key);
    if (it != key_to_index2.end()) {
      auto& match = it->second;
      Check(!match.matched) << "MatchReorderForEach: duplicate key in items1";
      match.matched = true;
      indexes2[match.index] = ix1;
    }
  }

  for (const auto& [key, match] : key_to_index2) {
    if (!match.matched) {
      indexes2[match.index] = unmatched2_id++;
    }
  }

  // Reconcile orderings greedily, invoking callbacks on-the-fly.
  // indexes1 is implicitly [0, size1).
  auto position = indexes2.begin();
  for (size_t ix1 = 0; ix1 < size1; ++ix1) {
    const auto found = std::find(indexes2.begin(), indexes2.end(), ix1);
    if (found == indexes2.end()) {
      // ix1 not found in the second ordering (unique to items1)
      removed(items1[ix1]);
    } else {
      // output items in items2 up to and including found value (if not already
      // output)
      for (; position <= found; ++position) {
        const size_t ix2 = position - indexes2.begin();
        if (*position < size1) {
          in_both(items1[*position], items2[ix2]);
        } else {
          added(items2[ix2]);
        }
      }
    }
  }
  // output any remaining items in items2
  for (; position < indexes2.end(); ++position) {
    const size_t ix2 = position - indexes2.begin();
    if (*position < size1) {
      in_both(items1[*position], items2[ix2]);
    } else {
      added(items2[ix2]);
    }
  }
}

}  // namespace stg

#endif  // STG_ORDER_H_
