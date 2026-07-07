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
#include <numeric>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "error.h"

namespace stg {

// Combines two orderings of unique items, eliminating duplicates between the
// sequences, preserving the relative positions of the items in the second
// ordering and incorporating as much of the first's order as is compatible.
//
// The two orderings are reconciled by examining each item from the first
// sequence in turn. If it is not present in the second sequence, it is greedily
// appended to the combined sequence. If it is present but hasn't yet been
// appended, then all items from the current position in the second sequence up
// to and including it are appended in bulk. Otherwise it is skipped. Finally,
// all items from the current position in the second sequence are appended.
//
// This guarantees that the second sequence is a subsequence of the combined
// sequence and that items unique to the first subsequence are output as early
// as possible and only out of order if they are one of the extra items appended
// in bulk.
//
// Example, before and after:
//
// indexes1: rose, george, emily
// indexes2: george, ted, emily
//
// combined: rose, george, ted, emily
template <typename T>
std::vector<T> CombineOrders(const std::vector<T>& indexes1,
                             const std::vector<T>& indexes2,
                             size_t combined_size) {
  std::vector<T> combined;
  combined.reserve(combined_size);
  // keep track of where we are up to in indexes2
  auto position = indexes2.begin();
  for (const auto& value : indexes1) {
    auto found = std::find(indexes2.begin(), indexes2.end(), value);
    if (found == indexes2.end()) {
      // value not found in the second ordering, append immediately
      combined.push_back(value);
    } else {
      // copy up to and including found value, if not yet copied
      for (; position <= found; ++position) {
        combined.push_back(*position);
      }
    }
  }
  // copy any remaining values unique to indexes2
  for (; position < indexes2.end(); ++position) {
    combined.push_back(*position);
  }
  return combined;
}

// Match and reorder two collections using a key-extraction lambda.
// Invokes callbacks on-the-fly for unmatched and matched elements.
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

  // build index pairs: (removed, _), (_, added), (in, both)
  std::vector<std::pair<std::optional<size_t>, std::optional<size_t>>> pairs;
  pairs.reserve(std::max(size1, size2));

  std::vector<size_t> indexes2(size2);

  for (size_t ix1 = 0; ix1 < size1; ++ix1) {
    const auto key = extract_key(items1[ix1]);
    const auto it = key_to_index2.find(key);
    if (it != key_to_index2.end()) {
      auto& match = it->second;
      Check(!match.matched) << "MatchReorderForEach: duplicate key in items1";
      match.matched = true;
      indexes2[match.index] = pairs.size();
      pairs.emplace_back(ix1, match.index);
    } else {
      pairs.emplace_back(ix1, std::nullopt);
    }
  }

  for (const auto& [key, match] : key_to_index2) {
    if (!match.matched) {
      indexes2[match.index] = pairs.size();
      pairs.emplace_back(std::nullopt, match.index);
    }
  }

  std::vector<size_t> indexes1(size1);
  std::iota(indexes1.begin(), indexes1.end(), 0);

  const auto permutation = CombineOrders(indexes1, indexes2, pairs.size());

  for (const size_t index : permutation) {
    const auto& [ix1, ix2] = pairs[index];
    if (ix1 && !ix2) {
      removed(items1[*ix1]);
    } else if (!ix1 && ix2) {
      added(items2[*ix2]);
    } else if (ix1 && ix2) {
      in_both(items1[*ix1], items2[*ix2]);
    }
  }
}

}  // namespace stg

#endif  // STG_ORDER_H_
