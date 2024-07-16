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

#ifndef STG_ELF_DWARF_HANDLE_H_
#define STG_ELF_DWARF_HANDLE_H_

#include <elf.h>
#include <elfutils/libdw.h>
#include <elfutils/libdwfl.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

namespace stg {

class ElfDwarfHandle {
 public:
  explicit ElfDwarfHandle(const std::string& path);
  ElfDwarfHandle(char* data, size_t size);

  Elf& GetElf();
  Dwarf& GetDwarf();

 private:
  struct DwflDeleter {
    void operator()(Dwfl* dwfl) {
      dwfl_end(dwfl);
    }
  };
  using DwflUniquePtr = std::unique_ptr<Dwfl, DwflDeleter>;

  ElfDwarfHandle(const char* module_name,
                 const std::function<Dwfl_Module*()>& add_module);

  DwflUniquePtr dwfl_;
  // Lifetime of Dwfl_Module is controlled by Dwfl.
  Dwfl_Module* dwfl_module_ = nullptr;
};

}  // namespace stg


#endif  // STG_ELF_DWARF_HANDLE_H_
