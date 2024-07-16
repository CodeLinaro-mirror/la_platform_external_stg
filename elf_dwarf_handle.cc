// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// -*- mode: C++ -*-
//
// Copyright 2022-2024 Google LLC
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
// Author: Aleksei Vetrov

#include "elf_dwarf_handle.h"

#include <elfutils/libdw.h>
#include <elfutils/libdwfl.h>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>

#include <cstddef>
#include <functional>
#include <string>

#include "error.h"

namespace stg {

namespace {

const Dwfl_Callbacks kDwflCallbacks = {
    .find_elf = nullptr,
    .find_debuginfo = dwfl_standard_find_debuginfo,
    .section_address = dwfl_offline_section_address,
    .debuginfo_path = nullptr};

constexpr int kReturnOk = 0;

void CheckOrDwflError(bool condition, const char* caller) {
  if (!condition) {
    const int dwfl_error = dwfl_errno();
    const char* errmsg = dwfl_errmsg(dwfl_error);
    if (errmsg == nullptr) {
      // There are some cases when DWFL fails to produce an error message.
      Die() << caller << " returned error code " << Hex(dwfl_error);
    }
    Die() << caller << " returned error: " << errmsg;
  }
}

}  // namespace

ElfDwarfHandle::ElfDwarfHandle(
    const char* module_name, const std::function<Dwfl_Module*()>& add_module) {
  dwfl_ = DwflUniquePtr(dwfl_begin(&kDwflCallbacks));
  CheckOrDwflError(dwfl_ != nullptr, "dwfl_begin");
  // Add data to process to dwfl
  dwfl_module_ = add_module();
  CheckOrDwflError(dwfl_module_ != nullptr, module_name);
  // Finish adding files to dwfl and process them
  CheckOrDwflError(dwfl_report_end(dwfl_.get(), nullptr, nullptr) == kReturnOk,
                   "dwfl_report_end");
}

ElfDwarfHandle::ElfDwarfHandle(const std::string& path)
    : ElfDwarfHandle("dwfl_report_offline", [&] {
        return dwfl_report_offline(dwfl_.get(), path.c_str(), path.c_str(), -1);
      }) {}

ElfDwarfHandle::ElfDwarfHandle(char* data, size_t size)
    : ElfDwarfHandle("dwfl_report_offline_memory", [&] {
        return dwfl_report_offline_memory(dwfl_.get(), "<memory>", "<memory>",
                                          data, size);
      }) {}

Elf& ElfDwarfHandle::GetElf() {
  GElf_Addr loadbase = 0;  // output argument for dwfl, unused by us
  Elf* elf = dwfl_module_getelf(dwfl_module_, &loadbase);
  CheckOrDwflError(elf != nullptr, "dwfl_module_getelf");
  return *elf;
}

Dwarf& ElfDwarfHandle::GetDwarf() {
  GElf_Addr loadbase = 0;  // output argument for dwfl, unused by us
  Dwarf* dwarf = dwfl_module_getdwarf(dwfl_module_, &loadbase);
  CheckOrDwflError(dwarf != nullptr, "dwfl_module_getdwarf");
  return *dwarf;
}

}  // namespace stg
