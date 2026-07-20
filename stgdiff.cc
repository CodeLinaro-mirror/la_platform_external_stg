// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -*- mode: C++ -*-
//
// Copyright 2020-2023 Google LLC
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
// Author: Maria Teguiani
// Author: Giuliano Procida
// Author: Siddharth Nayyar

#include <getopt.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <ostream>
#include <unordered_set>
#include <utility>
#include <vector>

#include "comparison.h"
#include "equality.h"
#include "error.h"
#include "fidelity.h"
#include "graph.h"
#include "input.h"
#include "naming.h"
#include "reader_options.h"
#include "reporting.h"
#include "runtime.h"

namespace {

const int kAbiChange = 4;
const int kFidelityChange = 8;

using Inputs = std::vector<std::pair<stg::InputFormat, const char*>>;
using Outputs =
    std::vector<std::pair<stg::reporting::OutputFormat, const char*>>;

std::vector<stg::Id> Read(stg::Runtime& runtime, const Inputs& inputs,
                          stg::Graph& graph, stg::ReadOptions options) {
  std::vector<stg::Id> roots;
  for (const auto& [format, filename] : inputs) {
    roots.push_back(stg::Read(runtime, graph, format, filename, options,
                              nullptr));
  }
  return roots;
}

int RunFidelity(const char* filename, const stg::Graph& graph,
                const std::vector<stg::Id>& roots) {
  const auto write = [&](std::ostream& output) {
    const auto fidelity_diff =
        stg::GetFidelityTransitions(graph, roots[0], roots[1]);
    const bool diffs_reported =
        stg::reporting::FidelityDiff(fidelity_diff, output);
    output << std::flush;
    if (!output) {
      stg::Die() << "error writing to " << '\'' << filename << '\'';
    }
    return diffs_reported ? kFidelityChange : 0;
  };
  if (strcmp(filename, "-") == 0) {
    return write(std::cout);
  }
  std::ofstream output(filename);
  return write(output);
}

int RunExact(stg::Runtime& runtime, const stg::Graph& graph,
             const std::vector<stg::Id>& roots) {
  struct PairCache {
    bool Query(const stg::Pair& comparison) const {
      return equalities.find(comparison) != equalities.end();
    }
    void AllSame(const std::vector<stg::Pair>& comparisons) {
      for (const auto& comparison : comparisons) {
        equalities.insert(comparison);
      }
    }
    std::unordered_set<stg::Pair> equalities;
  };

  const stg::Time compute(runtime, "equality check");
  PairCache equalities;
  return stg::Equals<PairCache>(graph, equalities)(roots[0], roots[1])
             ? 0
             : kAbiChange;
}

int Run(stg::Runtime& runtime, const stg::Graph& graph,
        const std::vector<stg::Id>& roots, const Outputs& outputs,
        stg::diff::Ignore ignore, std::optional<const char*> fidelity) {
  // Compute differences.
  stg::diff::Outcomes outcomes;
  stg::diff::Comparison comparison;
  {
    const stg::Time compute(runtime, "compute diffs");
    comparison = stg::diff::Compare(
        runtime, ignore, graph, roots[0], roots[1], outcomes);
  }
  const bool same = comparison == stg::diff::Comparison{};
  int status = same ? 0 : kAbiChange;

  // Write reports.
  stg::NameCache names;
  for (const auto& [format, filename] : outputs) {
    const auto write = [&](std::ostream& output) {
      if (!same) {
        const stg::Time report(runtime, "report diffs");
        const stg::reporting::Options options{format};
        const stg::reporting::Reporting reporting{graph, outcomes, options,
          names};
        Report(reporting, comparison, output);
        output << std::flush;
      }
      if (!output) {
        stg::Die() << "error writing to " << '\'' << filename << '\'';
      }
    };
    if (strcmp(filename, "-") == 0) {
      write(std::cout);
    } else {
      std::ofstream output(filename);
      write(output);
    }
  }

  // Compute fidelity diff if requested.
  if (fidelity) {
    const stg::Time report(runtime, "fidelity");
    status |= RunFidelity(*fidelity, graph, roots);
  }

  return status;
}

}  // namespace

int main(int argc, char* argv[]) {
  // Process arguments.
  bool opt_metrics = false;
  bool opt_exact = false;
  stg::ReadOptions opt_read_options;
  std::optional<const char*> opt_fidelity = std::nullopt;
  stg::diff::Ignore opt_ignore;
  stg::InputFormat opt_input_format = stg::InputFormat::STG;
  stg::reporting::OutputFormat opt_output_format =
      stg::reporting::OutputFormat::SMALL;
  Inputs inputs;
  Outputs outputs;
  static const option opts[] = {
      {"metrics",        no_argument,       nullptr, 'm'},
      {"abi",            no_argument,       nullptr, 'a'},
      {"btf",            no_argument,       nullptr, 'b'},
      {"elf",            no_argument,       nullptr, 'e'},
      {"stg",            no_argument,       nullptr, 's'},
      {"exact",          no_argument,       nullptr, 'x'},
      {"types",          no_argument,       nullptr, 't'},
      {"ignore",         required_argument, nullptr, 'i'},
      {"format",         required_argument, nullptr, 'f'},
      {"output",         required_argument, nullptr, 'o'},
      {"fidelity",       required_argument, nullptr, 'F'},
      {nullptr,          0,                 nullptr, 0  },
  };
  auto usage = [&]() {
    std::cerr << "usage: " << argv[0] << '\n'
              << "  [-m|--metrics]\n"
              << "  [-a|--abi|-b|--btf|-e|--elf|-s|--stg] file1\n"
              << "  [-a|--abi|-b|--btf|-e|--elf|-s|--stg] file2\n"
              << "  [-x|--exact]\n"
              << "  [-t|--types]\n"
              << "  [{-i|--ignore} <ignore-option>] ...\n"
              << "  [{-f|--format} <output-format>] ...\n"
              << "  [{-o|--output} {filename|-}] ...\n"
              << "  [{-F|--fidelity} {filename|-}]\n"
              << "implicit defaults: --stg --format small\n"
              << "--exact (node equality) cannot be combined with --output\n"
              << stg::reporting::OutputFormatUsage()
              << stg::diff::IgnoreUsage();
    return 1;
  };
  while (true) {
    int ix;
    const int c = getopt_long(argc, argv, "-mabesxti:f:o:F:", opts, &ix);
    if (c == -1) {
      break;
    }
    const char* argument = optarg;
    switch (c) {
      case 'm':
        opt_metrics = true;
        break;
      case 'a':
        opt_input_format = stg::InputFormat::ABI;
        break;
      case 'b':
        opt_input_format = stg::InputFormat::BTF;
        break;
      case 'e':
        opt_input_format = stg::InputFormat::ELF;
        break;
      case 's':
        opt_input_format = stg::InputFormat::STG;
        break;
      case 'x':
        opt_exact = true;
        break;
      case 't':
        opt_read_options.Set(stg::ReadOptions::TYPE_ROOTS);
        break;
      case 1:
        inputs.emplace_back(opt_input_format, argument);
        break;
      case 'i':
        if (const auto ignore = stg::diff::ParseIgnore(argument)) {
          opt_ignore.Set(ignore.value());
        } else {
          std::cerr << "unknown ignore option: " << argument << '\n'
                    << stg::diff::IgnoreUsage();
          return 1;
        }
        break;
      case 'f':
        if (const auto format = stg::reporting::ParseOutputFormat(argument)) {
          opt_output_format = format.value();
        } else {
          std::cerr << "unknown output format: " << argument << '\n'
                    << stg::reporting::OutputFormatUsage();
          return 1;
        }
        break;
      case 'o':
        outputs.emplace_back(opt_output_format, argument);
        break;
      case 'F':
        opt_fidelity.emplace(argument);
        break;
      default:
        return usage();
    }
  }
  if (inputs.size() != 2 || opt_exact > outputs.empty()) {
    return usage();
  }

  try {
    stg::Runtime runtime(std::cerr, opt_metrics);
    stg::Graph graph;
    const auto roots = Read(runtime, inputs, graph, opt_read_options);
    return opt_exact ? RunExact(runtime, graph, roots)
                     : Run(runtime, graph, roots, outputs, opt_ignore,
                           opt_fidelity);
  } catch (const stg::Exception& e) {
    std::cerr << e.what();
    return 1;
  }
}
