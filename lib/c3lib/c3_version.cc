/**
 * This file is a part of the implementation of the CyberCache Cluster.
 * Written by Vadim Sytnikov.
 * Copyright (C) 2016-2019 CyberHULL. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include "c3_version.h"
#include "c3_errors.h"
#include "c3_system.h"

#include <sys/utsname.h>
#include <cstring>
#include <cstdio>

namespace CyberCache {

const char c3lib_version_build_string[] = C3_VERSION_STRING " [" C3_BUILD_MODE_NAME "]";
const char c3lib_full_version_string[] =
  "CyberCache Cluster (" C3_EDITION " edition) " C3_VERSION_STRING " [" C3_BUILD_MODE_NAME "]";

const char* c3_get_build_mode_name(char* buffer, c3_byte_t id) {
  switch (id & BUILD_MODE_EDITION_MASK) {
    case BUILD_MODE_ID_COMMUNITY:
      buffer[0] = BUILD_MODE_CHAR_COMMUNITY;
      break;
    case BUILD_MODE_ID_ENTERPRISE:
      buffer[0] = BUILD_MODE_CHAR_ENTERPRISE;
    default:
      buffer[0] = '?';
  }
  switch (id & BUILD_MODE_SUBTYPE_MASK) {
    case BUILD_MODE_ID_FAST:
      buffer[1] = BUILD_MODE_CHAR_FAST;
      break;
    case BUILD_MODE_ID_NORMAL:
      buffer[1] = BUILD_MODE_CHAR_NORMAL;
      break;
    case BUILD_MODE_ID_SAFE:
      buffer[1] = BUILD_MODE_CHAR_SAFE;
      break;
    default:
      buffer[1] = '?';
  }
  buffer[2] = (id & BUILD_MODE_EXT_MASK) != 0? BUILD_MODE_CHAR_EXT: BUILD_MODE_CHAR_NO_EXT;
  buffer[3] = (id & BUILD_MODE_INSTRUMENTATION_MASK) != 0?
    BUILD_MODE_CHAR_IS_INSTRUMENTED: BUILD_MODE_CHAR_NOT_INSTRUMENTED;
  buffer[4] = 0;
  return buffer;
}

#if INCLUDE_C3_GET_OS_INFO
const char* c3_get_os_info(char* buffer, size_t size) {
  if (buffer != nullptr && size > 0) {
    struct utsname info;
    if (uname(&info) == 0) {
      std::snprintf(buffer, size, "System: %s - %s [%s]", info.sysname, info.version, info.release);
      return buffer;
    } else {
      c3_set_stdlib_error_message();
    }
  } else {
    c3_set_einval_error_message();
  }
  return nullptr;
}
#endif // INCLUDE_C3_GET_OS_INFO

const char* c3_get_system_info(char* buffer, size_t size) {
  if (buffer != nullptr && size > 0) {
    struct utsname info;
    if (uname(&info) == 0) {
      c3_uint_t num_cores = c3_get_num_cpus();
      if (num_cores > 0) {
        std::snprintf(buffer, size, "%s - %s [%s] - %s (%d cores)",
          info.sysname, info.version, info.release, info.machine, num_cores);
      } else {
        std::snprintf(buffer, size, "%s - %s [%s] - %s",
          info.sysname, info.version, info.release, info.machine);
      }
      return buffer;
    } else {
      c3_set_stdlib_error_message();
    }
  } else {
    c3_set_einval_error_message();
  }
  return nullptr;
}

}
