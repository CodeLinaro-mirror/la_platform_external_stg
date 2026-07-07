// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -*- mode: C++ -*-
//
// Copyright 2021-2026 Google LLC
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

#include "order.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <catch2/catch.hpp>

namespace Test {

using Sequence = std::vector<std::string>;

namespace {

// Safe for small k!
size_t Factorial(size_t k) {
  size_t count = 1;
  for (size_t i = 1; i <= k; ++i) {
    count *= i;
  }
  return count;
}

template <typename G>
std::vector<size_t> MakePermutation(size_t k, size_t n, G& gen) {
  std::vector<size_t> result(n);
  std::iota(result.begin(), result.end(), k);
  std::shuffle(result.begin(), result.end(), gen);
  return result;
}

template <typename T>
std::vector<T> CombineOrders(const std::vector<T>& items1,
                             const std::vector<T>& items2) {
  std::vector<T> combined;
  const auto extract = [](const T& x) { return x; };
  const auto removed = [&](const T& x) { combined.push_back(x); };
  const auto added = [&](const T& x) { combined.push_back(x); };
  const auto in_both = [&](const T& x, const T&) { combined.push_back(x); };
  stg::MatchReorderForEach(items1, items2, extract, removed, added, in_both);
  return combined;
}

template <typename Extract>
Sequence GetMappingCalls(const Sequence& items1, const Sequence& items2,
                         Extract extract) {
  Sequence calls;
  const auto removed = [&](const std::string& left) {
    calls.push_back(left + ">");
  };
  const auto added = [&](const std::string& right) {
    calls.push_back("<" + right);
  };
  const auto in_both = [&](const std::string& left, const std::string& right) {
    calls.push_back(left + "=" + right);
  };
  stg::MatchReorderForEach(items1, items2, extract, removed, added, in_both);
  return calls;
}

Sequence GetMappingCalls(const Sequence& items1, const Sequence& items2) {
  const auto identity = [](const std::string& s) { return s; };
  return GetMappingCalls(items1, items2, identity);
}

}  // namespace

TEST_CASE("hand-curated permutation") {
  Sequence data = {"emily", "george", "rose", "ted"};
  std::vector<size_t> permutation = {2, 1, 3, 0};
  const Sequence expected = {"rose", "george", "ted", "emily"};
  const std::vector<size_t> identity = {0, 1, 2, 3};
  stg::Permute(data, permutation);
  CHECK(data == expected);
  CHECK(permutation == identity);
}

TEST_CASE("randomly-generated permutations") {
  std::ranlux48 gen;
  auto seed = gen();
  // NOTES:
  //   Permutations of size 6 are plenty big enough to shake out bugs.
  //   There are k! permutations of size k. Testing costs are O(k).
  for (size_t k = 0; k < 7; ++k) {
    const auto count = Factorial(k);
    INFO("testing with " << count << " permutations of size " << k);
    std::vector<size_t> identity(k);
    for (size_t i = 0; i < k; ++i) {
      identity[i] = i;
    }
    for (size_t n = 0; n < count; ++n, ++seed) {
      gen.seed(seed);
      auto permutation = MakePermutation(0, k, gen);
      std::ostringstream os;
      os << "permutation of " << k << " numbers generated using seed " << seed;
      GIVEN(os.str()) {
        // NOTE: We could test with something other than [0, k) as the data, but
        // let's just say "parametric polymorphism" and move on.
        auto permutation_copy = permutation;
        auto identity_copy = identity;
        stg::Permute(identity_copy, permutation_copy);
        for (size_t i = 0; i < k; ++i) {
          // permutation_copy should be the identity
          CHECK(permutation_copy[i] == i);
          // identity_copy should now be the same as permutation
          CHECK(identity_copy[i] == permutation[i]);
        }
      }
    }
  }
}

TEST_CASE("randomly-generated ordering sequences, fully-matching") {
  std::ranlux48 gen;
  auto seed = gen();
  // NOTES:
  //   Permutations of size 6 are plenty big enough to shake out bugs.
  //   There are k! permutations of size k. Testing costs are O(k^2).
  for (size_t k = 0; k < 7; ++k) {
    const auto count = Factorial(k);
    INFO("testing with " << count << " random orderings of size " << k);
    for (size_t n = 0; n < count; ++n, ++seed) {
      gen.seed(seed);
      const auto order1 = MakePermutation(0, k, gen);
      const auto order2 = MakePermutation(0, k, gen);
      std::ostringstream os;
      os << "orderings of " << k << " numbers generated using seed " << seed;
      GIVEN(os.str()) {
        const auto combined = CombineOrders(order1, order2);
        // combined should be identical to order2
        CHECK(combined == order2);
      }
    }
  }
}

TEST_CASE("randomly-generated ordering sequences, no overlap") {
  std::ranlux48 gen;
  auto seed = gen();
  // NOTES:
  //   Orderings of size 4 are plenty big enough to shake out bugs.
  //   There are k! permutations of size k. Testing costs are O(k^2).
  for (size_t k = 0; k < 5; ++k) {
    const auto count = Factorial(k);
    INFO("testing with " << count << " random orderings of size " << k);
    for (size_t n = 0; n < count; ++n, ++seed) {
      gen.seed(seed);
      const auto order1 = MakePermutation(0, k, gen);
      const auto order2 = MakePermutation(k, k, gen);
      std::ostringstream os;
      os << "orderings of " << k << " numbers generated using seed " << seed;
      GIVEN(os.str()) {
        const auto combined = CombineOrders(order1, order2);
        for (size_t i = 0; i < k; ++i) {
          // order1 should appear as the first part
          CHECK(combined[i] == order1[i]);
          // order2 should appear as the second part
          CHECK(combined[i + k] == order2[i]);
        }
      }
    }
  }
}

TEST_CASE("randomly-generated ordering sequences, single overlap") {
  std::ranlux48 gen;
  auto seed = gen();
  // NOTES:
  //   Orderings of size 4 are plenty big enough to shake out bugs.
  //   There are k! permutations of size k. Testing costs are O(k^2).
  for (size_t k = 1; k < 5; ++k) {
    const auto count = Factorial(k);
    INFO("testing with " << count << " random orderings of size " << k);
    for (size_t n = 0; n < count; ++n, ++seed) {
      gen.seed(seed);
      const auto order1 = MakePermutation(0, k, gen);
      const auto order2 = MakePermutation(k - 1, k, gen);
      const auto pivot = k - 1;
      std::ostringstream os;
      os << "orderings of " << k << " numbers generated using seed " << seed;
      GIVEN(os.str()) {
        const auto combined = CombineOrders(order1, order2);
        CHECK(combined.size() == 2 * k - 1);
        // order1 pre, order2 pre, pivot, order1 post, order2 post
        size_t ix = 0;
        size_t ix1 = 0;
        size_t ix2 = 0;
        while (order1[ix1] != pivot) {
          CHECK(combined[ix++] == order1[ix1++]);
        }
        while (order2[ix2] != pivot) {
          CHECK(combined[ix++] == order2[ix2++]);
        }
        ++ix1;
        ++ix2;
        CHECK(combined[ix++] == pivot);
        while (ix1 < k) {
          CHECK(combined[ix++] == order1[ix1++]);
        }
        while (ix2 < k) {
          CHECK(combined[ix++] == order2[ix2++]);
        }
      }
    }
  }
}

TEST_CASE("hand-curated ordering sequences") {
  // NOTES:
  //   The output sequence MUST include the second sequence as a subsequence.
  //   The first sequence's ordering is respected as far as possible.
  const std::vector<std::tuple<Sequence, Sequence, Sequence>> cases = {
      // these cases have a unique ordering that respects both inputs
      {{"rose", "george", "emily"},
       {"george", "ted", "emily"},
       {"rose", "george", "ted", "emily"}},
      {{}, {}, {}},
      {{"a"}, {}, {"a"}},
      {{}, {"a"}, {"a"}},
      {{"a", "z"}, {}, {"a", "z"}},
      {{}, {"a", "z"}, {"a", "z"}},
      {{"a", "b", "c"}, {"c", "d"}, {"a", "b", "c", "d"}},
      {{"a", "b", "d"}, {"b", "c", "d"}, {"a", "b", "c", "d"}},
      {{"a", "c", "d"}, {"a", "b", "c"}, {"a", "b", "c", "d"}},
      {{"b", "c", "d"}, {"a", "b"}, {"a", "b", "c", "d"}},

      // this case has no ordering that respects both inputs
      // * q has to come after a
      // * z and a could go either way, but the current algorithm always prefers
      //     the second sequence's order
      {{"z", "a", "q"}, {"a", "z"}, {"a", "z", "q"}},

      // this case has two orderings that respect both inputs
      // * Emily could come before Zoë or vice versa, but the current algorithm
      //   always prefers the second sequence's order so Emily comes before Zoë
      // * Steven comes last regardless
      {{"emily", "steven"}, {"zoë", "steven"}, {"emily", "zoë", "steven"}},
      // the same case permutated to show that only position and identity matter
      {{"zoë", "steven"}, {"emily", "steven"}, {"zoë", "emily", "steven"}},
      {{"steven", "zoë"}, {"emily", "zoë"}, {"steven", "emily", "zoë"}},
      {{"emily", "zoë"}, {"steven", "zoë"}, {"emily", "steven", "zoë"}},
      {{"zoë", "emily"}, {"steven", "emily"}, {"zoë", "steven", "emily"}},
      {{"steven", "emily"}, {"zoë", "emily"}, {"steven", "zoë", "emily"}},
  };
  for (const auto& [order1, order2, expected] : cases) {
    const auto combined = CombineOrders(order1, order2);
    CHECK(combined == expected);
  }
}

TEST_CASE("hand-curated reorderings with input order randomisation") {
  using Constraint = std::pair<std::optional<size_t>, std::optional<size_t>>;
  using Constraints = std::vector<Constraint>;
  // NOTES:
  //   item removed at position x: {x}, {}
  //   item added at position y: {}, {y}
  //   item modified at positions x and y: {x}, {y}
  //   input item order should be irrelevant to output order
  const std::vector<std::pair<Constraints, Constraints>> cases = {
    {
      {
        {{2}, {2}},  // emily
        {{1}, {0}},  // george
        {{0}, {}},   // rose
        {{}, {1}},   // ted
      },
      {
        {{0}, {}},   // rose
        {{1}, {0}},  // george
        {{}, {1}},   // ted
        {{2}, {2}},  // emily
      },
    },
    {{}, {}},
    {{{{0}, {0}}}, {{{0}, {0}}}},
    {{{{0}, {}}}, {{{0}, {}}}},
    {{{{}, {0}}}, {{{}, {0}}}},
    {{{{}, {2}}, {{}, {1}}, {{}, {0}}},
     {{{}, {0}}, {{}, {1}}, {{}, {2}}}},
    {{{{2}, {}}, {{1}, {}}, {{0}, {}}},
     {{{0}, {}}, {{1}, {}}, {{2}, {}}}},
    // int b; int c; -> int b; int a; int c
    {{{{}, {1}}, {{0}, {0}}, {{1}, {2}}},
     {{{0}, {0}}, {{}, {1}}, {{1}, {2}}}},
  };
  std::ranlux48 gen;
  auto seed = gen();
  for (const auto& [given, expected] : cases) {
    const auto k = given.size();
    const auto count = Factorial(k);
    INFO("testing with " << count << " random input orderings");
    for (size_t n = 0; n < count; ++n, ++seed) {
      gen.seed(seed);
      std::ostringstream os;
      os << "permutation of " << k << " items generated using seed " << seed;
      GIVEN(os.str()) {
        auto permutation = MakePermutation(0, k, gen);
        auto copy = given;
        stg::Permute(copy, permutation);
        stg::Reorder(copy);
        CHECK(copy == expected);
      }
    }
  }
}

TEST_CASE("MatchReorderForEach with collections") {
  const Sequence items1 = {"rose", "george", "emily"};
  const Sequence items2 = {"george", "ted", "emily"};

  const auto calls = GetMappingCalls(items1, items2);

  const Sequence expected = {
      "rose>",
      "george=george",
      "<ted",
      "emily=emily",
  };
  CHECK(calls == expected);
}

TEST_CASE("MatchReorderForEach with duplicate keys") {
  const Sequence unique = {"a", "b", "c"};
  const Sequence duplicate = {"a", "b", "a"};

  const auto extract = [](const std::string& s) { return s; };
  const auto ignore = [](auto&&...) {};

  // duplicate in first container (items1)
  CHECK_THROWS(stg::MatchReorderForEach(duplicate, unique, extract, ignore,
                                        ignore, ignore));

  // duplicate in second container (items2)
  CHECK_THROWS(stg::MatchReorderForEach(unique, duplicate, extract, ignore,
                                        ignore, ignore));
}

TEST_CASE("MatchReorderForEach with anonymous items") {
  const Sequence items1 = {"", "", "namedA"};
  const Sequence items2 = {"", "", "namedB"};

  size_t anonymous_ix = 0;
  auto extract = [anonymous_ix](const std::string& s) mutable {
    if (s.empty()) {
      return "#anon#" + std::to_string(anonymous_ix++);
    }
    return s;
  };

  const auto calls = GetMappingCalls(items1, items2, extract);

  // We expect anonymous items to match by position:
  // anon0 (index 0) matches anon0 (index 0) -> "="
  // anon1 (index 1) matches anon1 (index 1) -> "="
  // namedA (index 2) is removed -> "namedA>"
  // namedB (index 2) is added -> "<namedB"
  const Sequence expected = {
      "=",
      "=",
      "namedA>",
      "<namedB",
  };
  CHECK(calls == expected);
}

}  // namespace Test
