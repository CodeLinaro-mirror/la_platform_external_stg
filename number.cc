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

#include "number.h"

#include <cstdint>
#include <limits>
#include <ostream>
#include <type_traits>

#include "error.h"

namespace stg {

Number::Number() : value_(0) {}

template<typename T>
Number::Number(T n) : value_(n) {
  static_assert(std::is_integral_v<T>);
  if constexpr (std::is_signed_v<T>) {
    if (std::numeric_limits<int64_t>::min() <= n
        && n <= std::numeric_limits<int64_t>::max()) {
      return;
    }
  } else {
    if (0 <= n
        && n <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
      return;
    }
  }
  Warn() << "number " << n << " misrepresented as " << value_;
}

int64_t Number::AsInt64() const {
  return value_;
}

std::ostream& Number::Print(std::ostream& os) const {
  return os << value_;
}

int64_t Number::HashValue() const {
  return value_;
}

template Number::Number(int8_t);
template Number::Number(int16_t);
template Number::Number(int32_t);
template Number::Number(int64_t);

template Number::Number(uint8_t);
template Number::Number(uint16_t);
template Number::Number(uint32_t);
template Number::Number(uint64_t);

}  // namespace stg
