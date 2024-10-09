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
// Author: Ignes Simeonova
// Author: Aleksei Vetrov

#ifndef STG_SCOPE_H_
#define STG_SCOPE_H_

#include <cstddef>
#include <string>

namespace stg {

struct Scope {
  std::string name;
  bool named = true;
};

class PushScopeName {
 public:
  template <typename Kind>
  PushScopeName(Scope& scope, Kind&& kind, const std::string& name)
      : scope_(scope), old_size_(scope_.name.size()),
        old_named_(scope_.named) {
    if (name.empty()) {
      scope_.name += "<unnamed ";
      scope_.name += kind;
      scope_.name += ">::";
      scope_.named = false;
    } else {
      scope_.name += name;
      scope_.name += "::";
    }
  }

  PushScopeName(const PushScopeName& other) = delete;
  PushScopeName& operator=(const PushScopeName& other) = delete;
  ~PushScopeName() {
    scope_.name.resize(old_size_);
    scope_.named = old_named_;
  }

 private:
  Scope& scope_;
  const size_t old_size_;
  const bool old_named_;
};

}  // namespace stg

#endif  // STG_SCOPE_H_
