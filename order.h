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

// Returns a permutation that reorders the data array according to its implicit
// ordering constraints.
//
// At least one of each pair of positions must be present.
//
// Each pair gives 1 or 2 abstract positions for the corresponding data item.
//
// The first and second positions are interpreted separately, with the second
// implied ordering having precedence over the first in the event of a conflict.
//
// The real work is done by CombineOrders.
//
// In practice the input data are the output of a matching process, consider:
//
// sequence1: rose, george, emily
// sequence2: george, ted, emily
//
// These have the corresponding matches (here just ordered by the matching key;
// this algorithm gives the same result independent of this ordering):
//
// emily:  {{2}, {2}}
// george: {{1}, {0}}
// rose:   {{0}, {} }
// ted:    {{},  {1}}
//
// Now ignore the matching keys.
//
// This function processes the matches into intermediate data structures:
//
// positions1: {{2, 0}, {1, 1}, {0, 2},        }
// positions2: {{2, 0}, {0, 1},         {1, 3},}
//
// The indexes (.second) are sorted by the positions (.first):
//
// positions1: {{0, 2}, {1, 1}, {2, 0}}
// positions2: {{0, 1}, {1, 3}, {2, 0}}
//
// And the positions are discarded:
//
// indexes1: 2, 1, 0
// indexes2: 1, 3, 0
//
// Finally a consistent ordering is made and returned:
//
// 2, 1, 3, 0
template <typename T>
std::vector<size_t> Reorder(
    const std::vector<std::pair<std::optional<T>, std::optional<T>>>& data) {
  const auto size = data.size();
  // Split out the ordering constraints as position-index pairs.
  std::vector<std::pair<T, size_t>> positions1;
  positions1.reserve(size);
  std::vector<std::pair<T, size_t>> positions2;
  positions2.reserve(size);
  for (size_t index = 0; index < size; ++index) {
    const auto& [position1, position2] = data[index];
    Check(position1 || position2)
        << "internal error: Reorder constraint with no positions";
    if (position1) {
      positions1.push_back({*position1, index});
    }
    if (position2) {
      positions2.push_back({*position2, index});
    }
  }
  // Order the indexes by the desired positions.
  std::stable_sort(positions1.begin(), positions1.end());
  std::stable_sort(positions2.begin(), positions2.end());
  std::vector<size_t> indexes1;
  indexes1.reserve(positions1.size());
  std::vector<size_t> indexes2;
  indexes2.reserve(positions2.size());
  for (const auto& ordered_index : positions1) {
    indexes1.push_back(ordered_index.second);
  }
  for (const auto& ordered_index : positions2) {
    indexes2.push_back(ordered_index.second);
  }
  // Merge the two orderings of indexes, giving preference to the second.
  return CombineOrders(indexes1, indexes2, size);
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

  for (size_t ix1 = 0; ix1 < size1; ++ix1) {
    const auto key = extract_key(items1[ix1]);
    const auto it = key_to_index2.find(key);
    if (it != key_to_index2.end()) {
      auto& match = it->second;
      Check(!match.matched) << "MatchReorderForEach: duplicate key in items1";
      match.matched = true;
      pairs.emplace_back(ix1, match.index);
    } else {
      pairs.emplace_back(ix1, std::nullopt);
    }
  }

  for (const auto& [key, match] : key_to_index2) {
    if (!match.matched) {
      pairs.emplace_back(std::nullopt, match.index);
    }
  }

  const auto permutation = Reorder(pairs);

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
