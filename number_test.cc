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

#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>
#include <vector>

#include <catch2/catch.hpp>

namespace {

using stg::Number;

template<typename T>
void CheckStrings(T n) {
  const Number number(n);
  std::ostringstream os1;
  os1 << (n + 0);  // promote char to int
  const auto n_as_string = os1.str();
  std::ostringstream os2;
  os2 << number;
  CHECK(!os2.fail());
  const auto number_as_string = os2.str();
  CHECK(n_as_string == number_as_string);
}

template<typename T>
void CheckAllStrings() {
  auto min = std::numeric_limits<T>::min();
  auto max = std::numeric_limits<T>::max();
  for (auto n = min; n < max; ++n) {
    CheckStrings(n);
  }
  CheckStrings(max);
}

template<typename T>
void CheckLimitStrings() {
  auto min = std::numeric_limits<T>::min();
  auto max = std::numeric_limits<T>::max();
  CheckStrings(min);
  CheckStrings(max);
}

TEST_CASE("all int8_t via Number to string") {
  CheckAllStrings<int8_t>();
}

TEST_CASE("all uint8_t via Number to string") {
  CheckAllStrings<uint8_t>();
}

TEST_CASE("all int16_t via Number to string") {
  CheckAllStrings<int16_t>();
}

TEST_CASE("all uint16_t via Number to string") {
  CheckAllStrings<uint16_t>();
}

TEST_CASE("min/max int32_t via Number to string") {
  CheckLimitStrings<int32_t>();
}

TEST_CASE("min/max uint32_t via Number to string") {
  CheckLimitStrings<uint32_t>();
}

TEST_CASE("min/max int64_t via Number to string") {
  CheckLimitStrings<int64_t>();
}

TEST_CASE("min/max uint64_t via Number to string") {
  CheckLimitStrings<uint64_t>();
}

template<typename T>
void CheckZeroChunks() {
  const Number zero;
  const Number four(4);
  for (size_t ix = 0; ix < 12; ++ix) {
    std::vector<T> chunks;
    chunks.resize(ix);
    const auto number = Number::FromChunks(chunks);
    CHECK(number == zero);
  }
  for (size_t ix = 1; ix < 12; ++ix) {
    std::vector<T> chunks;
    chunks.resize(ix);
    chunks[0] = 4;
    const auto number = Number::FromChunks(chunks);
    CHECK(number == four);
  }
}

TEST_CASE("zero chunks") {
  CheckZeroChunks<uint8_t>();
  CheckZeroChunks<uint16_t>();
  CheckZeroChunks<uint32_t>();
  CheckZeroChunks<uint64_t>();
}

TEST_CASE("Number to chunks and back") {
  for (int64_t a = 0; a < 2; ++a) {
    for (int64_t b = 0; b < 2; ++b) {
      for (int64_t c = 0; c < 2; ++c) {
        for (int64_t d = 0; d < 2; ++d) {
          for (int64_t e = 0; e < 2; ++e) {
            for (int64_t f = 0; f < 2; ++f) {
              for (int64_t g = 0; g < 2; ++g) {
                const int64_t n = (a << 56) + (b << 55) + (c << 32)
                                  + (d << 31) + (e << 8) + (f << 7) + g;
                GIVEN(n) {
                  const Number number(n);
                  const auto chunks8 = Number::ToChunks<uint8_t>(number);
                  const auto chunks16 = Number::ToChunks<uint16_t>(number);
                  const auto chunks32 = Number::ToChunks<uint32_t>(number);
                  const auto chunks64 = Number::ToChunks<uint64_t>(number);
                  const Number number8 = Number::FromChunks(chunks8);
                  const Number number16 = Number::FromChunks(chunks16);
                  const Number number32 = Number::FromChunks(chunks32);
                  const Number number64 = Number::FromChunks(chunks64);
                  CHECK(number8 == number);
                  CHECK(number16 == number);
                  CHECK(number32 == number);
                  CHECK(number64 == number);
                  CHECK(chunks64.size() == (n ? 1 : 0));
                  if (!chunks64.empty()) {
                    CHECK(static_cast<int64_t>(chunks64[0]) == n);
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

template<typename T>
void CheckChunks(const std::vector<T>& chunks) {
  const Number number = Number::FromChunks(chunks);
  std::ostringstream os;
  os << number;
  auto chunks1 = Number::ToChunks<T>(number);
  const T zeros = 0;
  const T ones = ~zeros;
  const T top = 1ULL << (sizeof(T) * 8 - 1);
  const T extension
      = chunks1.empty() || (chunks1.back() & top) == 0 ? zeros : ones;
  while (chunks1.size() < chunks.size()) {
    chunks1.push_back(extension);
  }
  CHECK(chunks1 == chunks);
}

TEST_CASE("1-byte chunks to Number and back") {
  for (size_t length = 0; length <= 10; ++length) {
    std::vector<uint8_t> chunks;
    chunks.resize(length);
    for (size_t bits = 0; bits < (1ULL << (2 * length)); ++bits) {
      for (size_t probe = 0; probe < length; ++probe) {
        uint8_t chunk = 0;
        if (bits & (1ULL << (2 * probe))) {
          chunk = chunk | 0x7f;
        }
        if (bits & (1ULL << (2 * probe + 1))) {
          chunk = chunk | 0x80;
        }
        chunks[probe] = chunk;
      }
      CheckChunks(chunks);
    }
  }
}

TEST_CASE("2-byte chunk to Number and back") {
  for (size_t length = 0; length <= 5; ++length) {
    std::vector<uint16_t> chunks;
    chunks.resize(length);
    for (size_t bits = 0; bits < (1ULL << (4 * length)); ++bits) {
      for (size_t probe = 0; probe < length; ++probe) {
        uint16_t chunk = 0;
        if (bits & (1ULL << (4 * probe))) {
          chunk = chunk | 0x7f;
        }
        if (bits & (1ULL << (4 * probe + 1))) {
          chunk = chunk | 0x80;
        }
        if (bits & (1ULL << (4 * probe + 2))) {
          chunk = chunk | 0x7f00;
        }
        if (bits & (1ULL << (4 * probe + 3))) {
          chunk = chunk | 0x8000;
        }
        chunks[probe] = chunk;
      }
      CheckChunks(chunks);
    }
  }
}

TEST_CASE("4-byte chunk to Number and back") {
  for (size_t length = 0; length <= 3; ++length) {
    std::vector<uint32_t> chunks;
    chunks.resize(length);
    for (size_t bits = 0; bits < (1ULL << (8 * length)); ++bits) {
      for (size_t probe = 0; probe < length; ++probe) {
        uint32_t chunk = 0;
        if (bits & (1ULL << (8 * probe))) {
          chunk = chunk | 0x7f;
        }
        if (bits & (1ULL << (8 * probe + 1))) {
          chunk = chunk | 0x80;
        }
        if (bits & (1ULL << (8 * probe + 2))) {
          chunk = chunk | 0x7f00;
        }
        if (bits & (1ULL << (8 * probe + 3))) {
          chunk = chunk | 0x8000;
        }
        if (bits & (1ULL << (8 * probe + 4))) {
          chunk = chunk | 0x7f0000;
        }
        if (bits & (1ULL << (8 * probe + 5))) {
          chunk = chunk | 0x800000;
        }
        if (bits & (1ULL << (8 * probe + 6))) {
          chunk = chunk | 0x7f000000;
        }
        if (bits & (1ULL << (8 * probe + 7))) {
          chunk = chunk | 0x80000000;
        }
        chunks[probe] = chunk;
      }
      CheckChunks(chunks);
    }
  }
}

TEST_CASE("8-byte chunk to Number and back") {
  for (size_t length = 0; length <= 2; ++length) {
    std::vector<uint64_t> chunks;
    chunks.resize(length);
    for (size_t bits = 0; bits < (1ULL << (8 * length)); ++bits) {
      for (size_t probe = 0; probe < length; ++probe) {
        uint64_t chunk = 0;
        if (bits & (1ULL << (8 * probe))) {
          chunk = chunk | 0x7f;
        }
        if (bits & (1ULL << (8 * probe + 1))) {
          chunk = chunk | 0x80;
        }
        if (bits & (1ULL << (8 * probe + 2))) {
          chunk = chunk | 0x7f00;
        }
        if (bits & (1ULL << (8 * probe + 3))) {
          chunk = chunk | 0x8000;
        }
        if (bits & (1ULL << (8 * probe + 4))) {
          chunk = chunk | 0x7fffffffff0000;
        }
        if (bits & (1ULL << (8 * probe + 5))) {
          chunk = chunk | 0x80000000000000;
        }
        if (bits & (1ULL << (8 * probe + 6))) {
          chunk = chunk | 0x7f00000000000000;
        }
        if (bits & (1ULL << (8 * probe + 7))) {
          chunk = chunk | 0x8000000000000000;
        }
        chunks[probe] = chunk;
      }
      CheckChunks(chunks);
    }
  }
}

}  // namespace
