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
// Author: Giuliano Procida

#ifndef STG_SUBSTITUTION_H_
#define STG_SUBSTITUTION_H_

#include <map>
#include <vector>

#include "graph.h"
#include "runtime.h"

namespace stg {

// This is a single-node node id substitution function that updates all node
// references according to a given mapping. The caller is responsible for
// determining the nodes to which substitution should apply (e.g., excluding
// orphaned nodes).
//
// The caller must provide a reference to a callable object which maps an Id to
// a new Id.
//
// The Update helpers may be used to update external node id references.
template <typename Updater>
struct Substitute {
  Substitute(Graph& graph, const Updater& updater)
      : graph(graph), updater(updater) {}

  void Update(Id& id) const {
    // update id to representative id, avoiding silent stores
    const Id new_id = updater(id);
    if (new_id != id) {
      id = new_id;
    }
  }

  void Update(std::vector<Id>& ids) const {
    for (auto& id : ids) {
      Update(id);
    }
  }

  template <typename Key>
  void Update(std::map<Key, Id>& ids) const {
    for (auto& [key, id] : ids) {
      Update(id);
    }
  }

  void operator()(Id id) const {
    return graph.Apply(*this, id);
  }

  void operator()(Special&) const {}

  void operator()(PointerReference& x) const {
    Update(x.pointee_type_id);
  }

  void operator()(PointerToMember& x) const {
    Update(x.containing_type_id);
    Update(x.pointee_type_id);
  }

  void operator()(Typedef& x) const {
    Update(x.referred_type_id);
  }

  void operator()(Qualified& x) const {
    Update(x.qualified_type_id);
  }

  void operator()(Primitive&) const {}

  void operator()(Array& x) const {
    Update(x.element_type_id);
  }

  void operator()(BaseClass& x) const {
    Update(x.type_id);
  }

  void operator()(Method& x) const {
    Update(x.type_id);
  }

  void operator()(Member& x) const {
    Update(x.type_id);
  }

  void operator()(VariantMember& x) const {
    Update(x.type_id);
  }

  void operator()(StructUnion& x) const {
    if (x.definition.has_value()) {
      auto& definition = x.definition.value();
      Update(definition.base_classes);
      Update(definition.methods);
      Update(definition.members);
    }
  }

  void operator()(Enumeration& x) const {
    if (x.definition.has_value()) {
      auto& definition = x.definition.value();
      Update(definition.underlying_type_id);
    }
  }

  void operator()(Variant& x) const {
    if (x.discriminant.has_value()) {
      Update(x.discriminant.value());
    }
    Update(x.members);
  }

  void operator()(Function& x) const {
    Update(x.parameters);
    Update(x.return_type_id);
  }

  void operator()(ElfSymbol& x) const {
    if (x.type_id) {
      Update(*x.type_id);
    }
  }

  void operator()(Interface& x) const {
    Update(x.symbols);
    Update(x.types);
  }

  Graph& graph;
  const Updater& updater;
};

template <class Updater>
Substitute(Graph&, const Updater& updater) -> Substitute<decltype(updater)>;

// Applies node ID substitutions and removes redundant nodes from a graph.
//
// find - map a node ID to its canonical representative ID.
// for_each - invoke a function for each candidate node ID.
void Rewrite(Graph& graph, auto&& find, auto&& for_each, Counter& retained,
             Counter& removed) {
  const Substitute substitute(graph, find);
  for_each([&](Id id) {
    if (find(id) == id) {
      substitute(id);
      ++retained;
    } else {
      graph.Remove(id);
      ++removed;
    }
  });
}

}  // namespace stg

#endif  // STG_SUBSTITUTION_H_
