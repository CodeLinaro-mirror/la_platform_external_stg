// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -*- mode: C++ -*-
//
// Copyright 2021-2024 Google LLC
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
// Author: Ignes Simeonova

#ifndef STG_ABIGAIL_READER_H_
#define STG_ABIGAIL_READER_H_

#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

#include <libxml/tree.h>
#include "graph.h"
#include "runtime.h"

namespace stg {
namespace abixml {

Id ProcessDocument(Graph& graph, xmlDocPtr document);
Id Read(Runtime& runtime, Graph& graph, const std::string& path);
Id ReadFromString(Graph& graph, std::string_view xml);

// Exposed for testing.
void Clean(xmlNodePtr root);
bool EqualTree(xmlNodePtr left, xmlNodePtr right);
bool SubTree(xmlNodePtr left, xmlNodePtr right);
using Document =
    std::unique_ptr<std::remove_pointer_t<xmlDocPtr>, void(*)(xmlDocPtr)>;
Document Read(Runtime& runtime, const std::string& path);

}  // namespace abixml
}  // namespace stg

#endif  // STG_ABIGAIL_READER_H_
