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
#include <sstream>

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

}  // namespace
