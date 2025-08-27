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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace stg {

namespace {

bool Negative(const std::string& limbs) {
  return !limbs.empty() && (limbs.back() & 0x80) != 0;
}

void Negate(std::string& limbs) {
  // negate in 2's complement
  bool carry = true;
  for (char& limb : limbs) {
    limb = ~limb;
    if (carry) {
      limb = static_cast<unsigned char>(limb) + 1;
      carry = limb == 0;
    }
  }
}

void Trim(std::string& limbs) {
  const bool negative = Negative(limbs);
  while (!limbs.empty()
         && (limbs.back() == 0 || limbs.back() == static_cast<char>(-1))) {
    const bool new_negative = limbs.size() > 1
                              && (limbs[limbs.size() - 2] & 0x80) != 0;
    if (new_negative != negative) {
      break;
    }
    limbs.pop_back();
  }
}

}  // namespace

template<typename T>
Number::Number(T n) {
  static_assert(std::is_integral_v<T>);
  if constexpr (std::is_signed_v<T>) {
    // This works for both signed and unsigned T, but with explicit template
    // instantiation it triggers Clang signed / unsigned comparison warnings.
    while (n != 0 && n != -1) {
      limbs_.push_back(n);
      n = n >> 8;
    }
    // sign extend, if needed
    if (Negative(limbs_) != (n < 0)) {
      limbs_.push_back(n);
    }
  } else {
    while (n != 0) {
      limbs_.push_back(n);
      n = n >> 8;
    }
    // zero extend, if needed
    if (Negative(limbs_)) {
      limbs_.push_back(0);
    }
  }
}

std::ostream& Number::Print(std::ostream& os) const {
  std::string limbs = limbs_;
  if (Negative(limbs)) {
    Negate(limbs);
    os << '-';
  }
  std::string digits;
  while (!limbs.empty()) {
    char carry = 0;
    for (auto it = limbs.rbegin(); it != limbs.rend(); ++it) {
      auto& value = *it;
      const unsigned int intermediate
          = static_cast<uint8_t>(value) | (static_cast<uint8_t>(carry) << 8);
      value = intermediate / 10;
      carry = intermediate % 10;
    }
    digits.push_back('0' + carry);
    if (limbs.back() == 0) {
      limbs.pop_back();
    }
  }
  if (digits.empty()) {
    return os << '0';
  }
  std::reverse(digits.begin(), digits.end());
  return os << digits;
}

int64_t Number::HashValue() const {
  // abbreviated ToChunks<uint64_t>, for backwards compatibility
  uint64_t result = 0;
  const char extension = Negative(limbs_) ? -1 : 0;
  for (size_t ix = 0; ix < 8; ++ix) {
    const uint8_t value = ix < limbs_.size() ? limbs_[ix] : extension;
    result |= static_cast<uint64_t>(value) << (8 * ix);
  }
  return static_cast<int64_t>(result);
}

template<typename T>
std::vector<T> Number::ToChunks(const Number& number) {
  static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>);
  std::vector<T> chunks;

  const auto& limbs = number.limbs_;
  const char extension = Negative(limbs) ? -1 : 0;
  for (size_t ix = 0; ix < limbs.size(); ix += sizeof(T)) {
    T chunk = 0;
    for (size_t iy = 0; iy < sizeof(T); ++iy) {
      const size_t byte = ix + iy;
      const uint8_t limb = byte < limbs.size() ? limbs[byte] : extension;
      chunk |= static_cast<T>(limb) << (8 * iy);
    }
    chunks.push_back(chunk);
  }

  return chunks;
}

template<typename T>
Number Number::FromChunks(const std::vector<T>& chunks) {
  static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>);

  Number number;

  auto& limbs = number.limbs_;
  for (size_t ix = 0; ix < chunks.size(); ++ix) {
    T value = chunks[ix];
    for (size_t iy = 0; iy < sizeof(T); ++iy) {
      limbs.push_back(value);
      value = value >> 8;
    }
  }

  // remove excess sign extension
  Trim(limbs);

  return number;
}

Number Number::FromBytes(bool is_signed, std::string_view bytes) {
  Number number;

  auto& limbs = number.limbs_;
  limbs.reserve(bytes.size());
  for (auto byte : bytes) {
    limbs.push_back(byte);
  }
  if (is_signed) {
    Trim(limbs);
  } else {
    // need to avoid a false negative if the highest bit is set
    if (Negative(limbs)) {
      limbs.push_back(0);
    } else {
      Trim(limbs);
    }
  }

  return number;
}

template Number::Number(int8_t);
template Number::Number(int16_t);
template Number::Number(int32_t);
template Number::Number(int64_t);

template Number::Number(uint8_t);
template Number::Number(uint16_t);
template Number::Number(uint32_t);
template Number::Number(uint64_t);

template std::vector<uint8_t> Number::ToChunks(const Number&);
template std::vector<uint16_t> Number::ToChunks(const Number&);
template std::vector<uint32_t> Number::ToChunks(const Number&);
template std::vector<uint64_t> Number::ToChunks(const Number&);

template Number Number::FromChunks(const std::vector<uint8_t>&);
template Number Number::FromChunks(const std::vector<uint16_t>&);
template Number Number::FromChunks(const std::vector<uint32_t>&);
template Number Number::FromChunks(const std::vector<uint64_t>&);

}  // namespace stg
