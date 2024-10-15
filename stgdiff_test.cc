// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -*- mode: C++ -*-
//
// Copyright 2022 Google LLC
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
// Author: Siddharth Nayyar

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <catch2/catch.hpp>
#include "comparison.h"
#include "fidelity.h"
#include "graph.h"
#include "input.h"
#include "naming.h"
#include "reader_options.h"
#include "reporting.h"
#include "runtime.h"

namespace stg {
namespace {

struct IgnoreTestCase {
  const std::string name;
  const InputFormat format0;
  const std::string file0;
  const InputFormat format1;
  const std::string file1;
  const diff::Ignore ignore;
  const std::string expected_output;
  const bool expected_equals;
};

std::string filename_to_path(const std::string& f) {
  return std::filesystem::path("testdata") / f;
}

Id Read(Runtime& runtime, Graph& graph, InputFormat format,
        const std::string& input) {
  return Read(runtime, graph, format, filename_to_path(input).c_str(),
              ReadOptions(), nullptr);
}

TEST_CASE("ignore") {
  const auto test = GENERATE(
      IgnoreTestCase(
          {"symbol type presence change",
           InputFormat::ABI,
           "symbol_type_presence_0.xml",
           InputFormat::ABI,
           "symbol_type_presence_1.xml",
           diff::Ignore(),
           "symbol_type_presence_small_diff",
           false}),
      IgnoreTestCase(
          {"symbol type presence change pruned",
           InputFormat::ABI,
           "symbol_type_presence_0.xml",
           InputFormat::ABI,
           "symbol_type_presence_1.xml",
           diff::Ignore(diff::Ignore::SYMBOL_TYPE_PRESENCE),
           "empty",
           true}),
      IgnoreTestCase(
          {"type declaration status change",
           InputFormat::ABI,
           "type_declaration_status_0.xml",
           InputFormat::ABI,
           "type_declaration_status_1.xml",
           diff::Ignore(),
           "type_declaration_status_small_diff",
           false}),
      IgnoreTestCase(
          {"type declaration status change pruned",
           InputFormat::ABI,
           "type_declaration_status_0.xml",
           InputFormat::ABI,
           "type_declaration_status_1.xml",
           diff::Ignore(diff::Ignore::TYPE_DECLARATION_STATUS),
           "empty",
           true}),
      IgnoreTestCase(
          {"primitive type encoding",
           InputFormat::STG,
           "primitive_type_encoding_0.stg",
           InputFormat::STG,
           "primitive_type_encoding_1.stg",
           diff::Ignore(),
           "primitive_type_encoding_small_diff",
           false}),
      IgnoreTestCase(
          {"primitive type encoding ignored",
           InputFormat::STG,
           "primitive_type_encoding_0.stg",
           InputFormat::STG,
           "primitive_type_encoding_1.stg",
           diff::Ignore(diff::Ignore::PRIMITIVE_TYPE_ENCODING),
           "empty",
           true}),
      IgnoreTestCase(
          {"member size",
           InputFormat::STG,
           "member_size_0.stg",
           InputFormat::STG,
           "member_size_1.stg",
           diff::Ignore(),
           "member_size_small_diff",
           false}),
      IgnoreTestCase(
          {"member size ignored",
           InputFormat::STG,
           "member_size_0.stg",
           InputFormat::STG,
           "member_size_1.stg",
           diff::Ignore(diff::Ignore::MEMBER_SIZE),
           "empty",
           true}),
      IgnoreTestCase(
          {"enum underlying type",
           InputFormat::STG,
           "enum_underlying_type_0.stg",
           InputFormat::STG,
           "enum_underlying_type_1.stg",
           diff::Ignore(),
           "enum_underlying_type_small_diff",
           false}),
      IgnoreTestCase(
          {"enum underlying type ignored",
           InputFormat::STG,
           "enum_underlying_type_0.stg",
           InputFormat::STG,
           "enum_underlying_type_1.stg",
           diff::Ignore(diff::Ignore::ENUM_UNDERLYING_TYPE),
           "empty",
           true}),
      IgnoreTestCase(
          {"qualifier",
           InputFormat::STG,
           "qualifier_0.stg",
           InputFormat::STG,
           "qualifier_1.stg",
           diff::Ignore(),
           "qualifier_small_diff",
           false}),
      IgnoreTestCase(
          {"qualifier ignored",
           InputFormat::STG,
           "qualifier_0.stg",
           InputFormat::STG,
           "qualifier_1.stg",
           diff::Ignore(diff::Ignore::QUALIFIER),
           "empty",
           true}),
      IgnoreTestCase(
          {"CRC change",
           InputFormat::STG,
           "crc_change_0.stg",
           InputFormat::STG,
           "crc_change_1.stg",
           diff::Ignore(),
           "crc_change_small_diff",
           false}),
      IgnoreTestCase(
          {"CRC change ignored",
           InputFormat::STG,
           "crc_change_0.stg",
           InputFormat::STG,
           "crc_change_1.stg",
           diff::Ignore(diff::Ignore::SYMBOL_CRC),
           "empty",
           true}),
      IgnoreTestCase(
          {"interface addition",
           InputFormat::STG,
           "interface_addition_0.stg",
           InputFormat::STG,
           "interface_addition_1.stg",
           diff::Ignore(),
           "interface_addition_small_diff",
           false}),
      IgnoreTestCase(
          {"interface addition ignored",
           InputFormat::STG,
           "interface_addition_0.stg",
           InputFormat::STG,
           "interface_addition_1.stg",
           diff::Ignore(diff::Ignore::INTERFACE_ADDITION),
           "empty",
           true}),
      IgnoreTestCase(
          {"type addition",
           InputFormat::STG,
           "type_addition_0.stg",
           InputFormat::STG,
           "type_addition_1.stg",
           diff::Ignore(),
           "type_addition_small_diff",
           false}),
      IgnoreTestCase(
          {"type addition ignored",
           InputFormat::STG,
           "type_addition_0.stg",
           InputFormat::STG,
           "type_addition_1.stg",
           diff::Ignore(diff::Ignore::INTERFACE_ADDITION),
           "empty",
           true}),
      IgnoreTestCase(
          {"type definition addition",
           InputFormat::STG,
           "type_addition_1.stg",
           InputFormat::STG,
           "type_addition_2.stg",
           diff::Ignore(),
           "type_definition_addition_small_diff",
           false}),
      IgnoreTestCase(
          {"type definition addition ignored",
           InputFormat::STG,
           "type_addition_1.stg",
           InputFormat::STG,
           "type_addition_2.stg",
           diff::Ignore(diff::Ignore::TYPE_DEFINITION_ADDITION),
           "empty",
           true})
      );

  SECTION(test.name) {
    std::ostringstream os;
    Runtime runtime(os, false);

    // Read inputs.
    Graph graph;
    const Id id0 = Read(runtime, graph, test.format0, test.file0);
    const Id id1 = Read(runtime, graph, test.format1, test.file1);

    // Compute differences.
    diff::Compare compare{runtime, graph, test.ignore};
    const auto& [equals, comparison] = compare(id0, id1);

    // Write SMALL reports.
    std::ostringstream output;
    if (comparison) {
      NameCache names;
      const reporting::Options options{reporting::OutputFormat::SMALL};
      const reporting::Reporting reporting{
        graph, compare.outcomes, options, names};
      Report(reporting, *comparison, output);
    }

    // Check comparison outcome and report output.
    CHECK(equals == test.expected_equals);
    const std::ifstream expected_output_file(
        filename_to_path(test.expected_output));
    std::ostringstream expected_output;
    expected_output << expected_output_file.rdbuf();
    CHECK(output.str() == expected_output.str());
  }
}

struct ShortReportTestCase {
  const std::string name;
  const std::string xml0;
  const std::string xml1;
  const std::string expected_output;
};

TEST_CASE("short report") {
  const auto test = GENERATE(
      ShortReportTestCase(
          {"crc changes", "crc_0.xml", "crc_1.xml", "crc_changes_short_diff"}),
      ShortReportTestCase({"only crc changes", "crc_only_0.xml",
                           "crc_only_1.xml", "crc_only_changes_short_diff"}),
      ShortReportTestCase({"offset changes", "offset_0.xml", "offset_1.xml",
                           "offset_changes_short_diff"}),
      ShortReportTestCase(
          {"symbols added and removed", "added_removed_symbols_0.xml",
           "added_removed_symbols_1.xml", "added_removed_symbols_short_diff"}),
      ShortReportTestCase({"symbols added and removed only",
                           "added_removed_symbols_only_0.xml",
                           "added_removed_symbols_only_1.xml",
                           "added_removed_symbols_only_short_diff"}));

  SECTION(test.name) {
    std::ostringstream os;
    Runtime runtime(os, false);

    // Read inputs.
    Graph graph;
    const Id id0 = Read(runtime, graph, InputFormat::ABI, test.xml0);
    const Id id1 = Read(runtime, graph, InputFormat::ABI, test.xml1);

    // Compute differences.
    diff::Compare compare{runtime, graph, {}};
    const auto& [equals, comparison] = compare(id0, id1);

    // Write SHORT reports.
    std::stringstream output;
    if (comparison) {
      NameCache names;
      const reporting::Options options{reporting::OutputFormat::SHORT};
      const reporting::Reporting reporting{
        graph, compare.outcomes, options, names};
      Report(reporting, *comparison, output);
    }

    // Check comparison outcome and report output.
    CHECK(equals == false);
    const std::ifstream expected_output_file(
        filename_to_path(test.expected_output));
    std::ostringstream expected_output;
    expected_output << expected_output_file.rdbuf();
    CHECK(output.str() == expected_output.str());
  }
}

TEST_CASE("fidelity diff") {
  std::ostringstream os;
  Runtime runtime(os, false);

  // Read inputs.
  Graph graph;
  const Id id0 = Read(runtime, graph, InputFormat::STG, "fidelity_diff_0.stg");
  const Id id1 = Read(runtime, graph, InputFormat::STG, "fidelity_diff_1.stg");

  // Compute fidelity diff.
  auto fidelity_diff = GetFidelityTransitions(graph, id0, id1);

  // Write fidelity diff report.
  std::ostringstream report;
  reporting::FidelityDiff(fidelity_diff, report);

  // Check report.
  const std::ifstream expected_report_file(
      filename_to_path("fidelity_diff_report"));
  std::ostringstream expected_report;
  expected_report << expected_report_file.rdbuf();
  CHECK(report.str() == expected_report.str());
}

}  // namespace
}  // namespace stg
