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

#ifndef STG_BTF_READER_H_
#define STG_BTF_READER_H_

#include <string>
#include <string_view>

#include "graph.h"
#include "reader_options.h"
#include "runtime.h"

namespace stg {
namespace btf {

Id ReadSection(Runtime& runtime, Graph& graph, std::string_view data);
Id ReadFile(Runtime& runtime, Graph& graph, const std::string& path,
            ReadOptions options);

}  // namespace btf
}  // namespace stg

#endif  // STG_BTF_READER_H_
