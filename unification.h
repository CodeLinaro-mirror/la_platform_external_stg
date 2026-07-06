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

#ifndef STG_UNIFICATION_H_
#define STG_UNIFICATION_H_

#include "graph.h"
#include "runtime.h"

namespace stg {

// Keep track of which nodes are pending substitution and rewrite the graph on
// destruction.
class UnifyingGraph {
 public:
  UnifyingGraph(Runtime& runtime, Graph& graph, Id start, Id limit);
  ~UnifyingGraph() noexcept(false);

  const Graph& graph() const {
    return graph_;
  }

  // id2 will always be preferred as a parent node; interpreted as a
  // substitution, id1 will be replaced by id2
  void Union(Id id1, Id id2);

  Id Find(Id id);

  // attempt to unify, recursively, allowing type declarations to be replaced
  // by definitions
  bool Unify(Id id1, Id id2);

  template <typename FunctionObject, typename... Args>
  decltype(auto) Apply(FunctionObject&& function, Id id, Args&&... args) {
    return graph_.Apply(function, Find(id), std::forward<Args>(args)...);
  }

  template <typename FunctionObject, typename... Args>
  decltype(auto) Apply2(FunctionObject&& function, Id id1, Id id2,
                        Args&&... args) {
    return graph_.Apply2(function, Find(id1), Find(id2),
                         std::forward<Args>(args)...);
  }

 private:
  Graph& graph_;
  Id start_;
  DenseIdMapping mapping_;
  Runtime& runtime_;
  Counter find_query_;
  Counter find_halved_;
  Counter union_known_;
  Counter union_unknown_;
};

}  // namespace stg

#endif  // STG_UNIFICATION_H_
