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

#ifndef STG_UNION_FIND_H_
#define STG_UNION_FIND_H_

#include "graph.h"
#include "runtime.h"

namespace stg {

// generic Union-Find (Disjoint Set Union) class with path halving
template <typename Mapping>
class UnionFind {
 public:
  UnionFind(Runtime& runtime, Mapping& mapping)
      : mapping_(mapping),
        find_query_(runtime, "dsu.find_query"),
        find_halved_(runtime, "dsu.find_halved"),
        union_known_(runtime, "dsu.union_known"),
        union_unknown_(runtime, "dsu.union_unknown") {}

  Id Find(Id id) {
    ++find_query_;
    // path halving - tiny performance gain
    while (true) {
      // note: safe to take a reference as mapping cannot grow after this
      auto& parent = mapping_.Get(id);
      if (parent == id) {
        return id;
      }
      const auto parent_parent = mapping_.Get(parent);
      if (parent_parent == parent) {
        return parent;
      }
      id = parent = parent_parent;
      ++find_halved_;
    }
  }

  // id2 will always be preferred as a parent node; interpreted as a
  // substitution, id1 will be replaced by id2
  void Union(Id id1, Id id2) {
    const Id fid1 = Find(id1);
    const Id fid2 = Find(id2);
    if (fid1 == fid2) {
      ++union_known_;
      return;
    }
    mapping_.Add(fid1, fid2);
    ++union_unknown_;
  }

 private:
  Mapping& mapping_;
  Counter find_query_;
  Counter find_halved_;
  Counter union_known_;
  Counter union_unknown_;
};

}  // namespace stg

#endif  // STG_UNION_FIND_H_
