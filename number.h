// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -*- mode: C++ -*-
//
// Copyright 2025 Google LLC
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

#ifndef STG_NUMBER_H_
#define STG_NUMBER_H_

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

namespace stg {

class Number {
 public:
  Number() = default;

  template<typename T>
  explicit Number(T n);

  bool operator==(const Number&) const = default;

  std::ostream& Print(std::ostream& os) const;

  int64_t HashValue() const;

  template<typename T>
  static std::vector<T> ToChunks(const Number& number);
  template<typename T>
  static Number FromChunks(const std::vector<T>& chunks);

 private:
  // little endian, 2's complement, minimal sign bits
  std::string limbs_;
};

inline std::ostream& operator<<(std::ostream& os, const Number& number) {
  return number.Print(os);
}

}  // namespace stg

#endif  // STG_NUMBER_H_
