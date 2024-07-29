// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -*- mode: C++ -*-
//
// Copyright 2023-2024 Google LLC
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

#ifndef STG_HEX_H_
#define STG_HEX_H_

#include <ios>
#include <ostream>

namespace stg {

template <typename T>
struct Hex {
  explicit Hex(const T& value) : value(value) {}
  T value;
};

template <typename T> Hex(const T&) -> Hex<T>;

template <typename T>
std::ostream& operator<<(std::ostream& os, const Hex<T>& hex_value) {
  // not quite right if an exception is thrown
  const auto flags = os.flags();
  os << "0x" << std::hex << hex_value.value;
  os.flags(flags);
  return os;
}

}  // namespace stg

#endif  // STG_HEX_H_
