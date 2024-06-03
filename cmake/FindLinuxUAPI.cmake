# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright 2024 Google LLC
#
# Licensed under the Apache License v2.0 with LLVM Exceptions (the "License");
# you may not use this file except in compliance with the License.  You may
# obtain a copy of the License at
#
# https://llvm.org/LICENSE.txt
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
# Author: Giuliano Procida

#[=======================================================================[.rst:
FindLinuxUAPI
-------------

Finds the Linux UAPI headers.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``LinuxUAPI_FOUND``
  True if the system has the Linux UAPI headers.
``LinuxUAPI_INCLUDE_DIR``
  The Linux UAPI include directory.
``LinuxUAPI_VERSION``
  The version of the Linux UAPI headers which were found.

#]=======================================================================]

find_path(
  LinuxUAPI_INCLUDE_DIR
  linux/version.h
)
mark_as_advanced(LinuxUAPI_INCLUDE_DIR)

if(LinuxUAPI_INCLUDE_DIR)
  file(READ "${LinuxUAPI_INCLUDE_DIR}/linux/version.h" _version_header)
  string(REGEX REPLACE ".*#define LINUX_VERSION_MAJOR ([0-9]+).*#define LINUX_VERSION_PATCHLEVEL ([0-9]+).*#define LINUX_VERSION_SUBLEVEL ([0-9]+).*" "\\1.\\2.\\3" LinuxUAPI_VERSION "${_version_header}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  LinuxUAPI
  REQUIRED_VARS LinuxUAPI_INCLUDE_DIR
  VERSION_VAR LinuxUAPI_VERSION
)
