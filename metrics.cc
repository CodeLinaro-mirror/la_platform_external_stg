// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -*- mode: C++ -*-
//
// Copyright 2021-2023 Google LLC
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

#include "metrics.h"

#include <cstddef>
#include <iomanip>
#include <map>
#include <ostream>

namespace stg {

std::ostream& operator<<(std::ostream& os, const Frequencies& frequencies) {
  bool separate = false;
  for (const auto& [item, frequency] : frequencies.counts) {
    if (separate) {
      os << ' ';
    } else {
      separate = true;
    }
    os << '[' << item << "]=" << frequency;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const Nanoseconds& value) {
  const auto millis = value.ns / 1'000'000;
  const auto nanos = value.ns % 1'000'000;
  // fill needs to be reset; width is reset automatically
  return os << millis << '.' << std::setfill('0') << std::setw(6) << nanos
            << std::setfill(' ') << " ms";
}

Time::Time(Metrics& metrics, const char* name)
    : metrics_(metrics), name_(name) {
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_);
}

Time::~Time() {
  struct timespec finish;
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &finish);
  const auto seconds = finish.tv_sec - start_.tv_sec;
  const auto nanos = finish.tv_nsec - start_.tv_nsec;
  const Nanoseconds value(seconds * 1'000'000'000 + nanos);
  metrics_.PrintMetric(name_, value);
}

Counter::Counter(Metrics& metrics, const char* name)
    : metrics_(metrics), name_(name), value_(0) {
}

Counter::~Counter() {
  metrics_.PrintMetric(name_, value_);
}

Histogram::Histogram(Metrics& metrics, const char* name)
    : metrics_(metrics), name_(name) {
}

Histogram::~Histogram() {
  metrics_.PrintMetric(name_, frequencies_);
}

}  // namespace stg
