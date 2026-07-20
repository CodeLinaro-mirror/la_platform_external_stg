// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -*- mode: C++ -*-
//
// Copyright 2022-2026 Google LLC
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

#include "deduplication.h"

#include <cstddef>
#include <unordered_map>
#include <utility>
#include <vector>

#include "equality.h"
#include "graph.h"
#include "hashing.h"
#include "runtime.h"
#include "substitution.h"
#include "union_find.h"

namespace stg {
namespace {

// Roughly equivalent to std::map<Id, Id>, defaulted to the identity mapping,
// backed by an unordered map.
class SparseIdMapping {
 public:
  Id& Get(Id& id) {
    const auto it = mapping_.find(id);
    if (it == mapping_.end()) {
      return id;
    }
    return it->second;
  }

  void Add(Id child, Id parent) {
    mapping_.emplace(child, parent);
  }

 private:
  std::unordered_map<Id, Id> mapping_;
};

// Equality cache - for use with the Equals function object
//
// It caches equalities (symmetrically) using union-find with path halving.
struct EqualityCache {
  explicit EqualityCache(Runtime& runtime)
      : dsu(runtime, mapping),
        query_count(runtime, "cache.query_count"),
        query_equal_ids(runtime, "cache.query_equal_ids"),
        query_equal_representatives(runtime,
                                    "cache.query_equal_representatives"),
        query_not_found(runtime, "cache.query_not_found") {}

  bool Query(const Pair& comparison) {
    ++query_count;
    const auto& [id1, id2] = comparison;
    if (id1 == id2) {
      ++query_equal_ids;
      return true;
    }
    const Id fid1 = Find(id1);
    const Id fid2 = Find(id2);
    if (fid1 == fid2) {
      ++query_equal_representatives;
      return true;
    }
    ++query_not_found;
    return false;
  }

  void Record(const Pair& comparison) {
    const auto& [id1, id2] = comparison;
    dsu.Union(id1, id2);
  }

  Id Find(Id id) {
    return dsu.Find(id);
  }

  void Union(Id id1, Id id2) {
    dsu.Union(id1, id2);
  }

  SparseIdMapping mapping;
  UnionFind<SparseIdMapping> dsu;

  Counter query_count;
  Counter query_equal_ids;
  Counter query_equal_representatives;
  Counter query_not_found;
};

}  // namespace

Id Deduplicate(Runtime& runtime, Graph& graph, Id root, const Hashes& hashes) {
  // Partition the nodes by hash.
  std::unordered_map<HashValue, std::vector<Id>> partitions;
  {
    const Time x(runtime, "partition nodes");
    for (const auto& [id, fp] : hashes) {
      partitions[fp].push_back(id);
    }
  }
  Counter(runtime, "deduplicate.nodes") = hashes.size();
  Counter(runtime, "deduplicate.hashes") = partitions.size();

  Histogram hash_partition_size(runtime, "deduplicate.hash_partition_size");
  Counter min_comparisons(runtime, "deduplicate.min_comparisons");
  Counter max_comparisons(runtime, "deduplicate.max_comparisons");
  for (const auto& [fp, ids] : partitions) {
    const auto n = ids.size();
    hash_partition_size.Add(n);
    min_comparisons += n - 1;
    max_comparisons += n * (n - 1) / 2;
  }

  // Refine partitions of nodes with the same fingerprints.
  EqualityCache cache(runtime);
  Equals<EqualityCache> equals(graph, cache);
  Counter equalities(runtime, "deduplicate.equalities");
  Counter inequalities(runtime, "deduplicate.inequalities");
  {
    const Time x(runtime, "find duplicates");
    for (auto& [fp, ids] : partitions) {
      while (ids.size() > 1) {
        std::vector<Id> todo;
        const Id candidate = ids[0];
        for (size_t i = 1; i < ids.size(); ++i) {
          if (equals(ids[i], candidate)) {
            ++equalities;
          } else {
            todo.push_back(ids[i]);
            ++inequalities;
          }
        }
        std::swap(todo, ids);
      }
    }
  }

  // Keep one representative of each set of duplicates.
  Counter unique(runtime, "deduplicate.unique");
  Counter duplicate(runtime, "deduplicate.duplicate");
  const auto remap = [&cache](Id id) { return cache.Find(id); };
  const Substitute substitute(graph, remap);
  {
    const Time x(runtime, "rewrite");
    for (const auto& [id, fp] : hashes) {
      const Id fid = cache.Find(id);
      if (fid != id) {
        graph.Remove(id);
        ++duplicate;
      } else {
        substitute(id);
        ++unique;
      }
    }
  }

  // In case the root node was remapped.
  substitute.Update(root);
  return root;
}

}  // namespace stg
