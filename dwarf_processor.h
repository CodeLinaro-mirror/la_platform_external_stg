// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -*- mode: C++ -*-
//
// Copyright 2022 Google LLC
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
// Author: Aleksei Vetrov

#ifndef STG_DWARF_PROCESSOR_H_
#define STG_DWARF_PROCESSOR_H_

#include <elfutils/libdw.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "dwarf_wrappers.h"
#include "filter.h"
#include "graph.h"

namespace stg {
namespace dwarf {

struct Types {
  struct Symbol {
    std::string scoped_name;
    std::string linkage_name;
    std::vector<Location> locations;
    Id type_id;
  };

  size_t processed_entries = 0;
  // Container for all named type IDs allocated during DWARF processing.
  std::vector<Id> named_type_ids;
  std::vector<Symbol> symbols;
};

// Process every compilation unit from DWARF and returns processed STG along
// with information needed for matching to ELF symbols.
// If DWARF is missing, returns empty result.
Types Process(Dwarf* dwarf, bool is_little_endian_binary,
              const std::unique_ptr<Filter>& file_filter, Graph& graph);

}  // namespace dwarf
}  // namespace stg

#endif  // STG_DWARF_PROCESSOR_H_
