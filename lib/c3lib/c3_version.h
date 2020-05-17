/**
 * CyberCache Cluster
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
 * ----------------------------------------------------------------------------
 *
 * Version of the library and tools built with it, represented as numbers and strings.
 */
#ifndef _C3_VERSION_H
#define _C3_VERSION_H

#include "c3_types.h"

namespace CyberCache {

// whether to compile `c3_get_free_disk_space(const char*)`
#define INCLUDE_C3_GET_OS_INFO 0

///////////////////////////////////////////////////////////////////////////////
// SERVER VERSION
///////////////////////////////////////////////////////////////////////////////

#define C3_VERSION_MAJOR 1 // protocol change or other such major changes
#define C3_VERSION_MINOR 3 // feature set change, performance improvements, etc.
#define C3_VERSION_PATCH 6 // bug fixes and other minor changes

#define C3_VERSION_STRING C3_STRINGIFY(C3_VERSION_MAJOR) "." \
  C3_STRINGIFY(C3_VERSION_MINOR) "." C3_STRINGIFY(C3_VERSION_PATCH)

/*
 * ID of the CyberCache warmer, which is used during user agent detection; includes full version number,
 * so that to make the life of impostors more difficult.
 */
#define C3_CACHE_WARMER_ID "CyberCache-Warmer-" C3_VERSION_STRING

/// ID that fully identifies the build
constexpr c3_uint_t c3lib_version_id = ((c3_uint_t) C3_VERSION_MAJOR << 24) |
  ((c3_uint_t) C3_VERSION_MINOR << 16) |
  ((c3_uint_t) C3_VERSION_PATCH << 8) |
  (c3_uint_t) C3_BUILD_MODE_ID;

#define C3_GET_MAJOR_VERSION(version) ((c3_byte_t)((version) >> 24))
#define C3_GET_MINOR_VERSION(version) ((c3_byte_t)((version) >> 16))
#define C3_GET_PATCH_VERSION(version) ((c3_byte_t)((version) >> 8))
#define C3_GET_BUILD_MODE_ID(version) ((c3_byte_t)(version))

/// C3_VERSION_STRING plus build ID (safest/safer/normal/faster/fastest)
extern const char c3lib_version_build_string[];

/// Full application name and version
extern const char c3lib_full_version_string[];

///////////////////////////////////////////////////////////////////////////////
// ENVIRONMENT
///////////////////////////////////////////////////////////////////////////////

// '<edition>' '<type>' '<subtype>' '<instrumentation>' '\0'
constexpr size_t C3_BUILD_NAME_BUFFER_SIZE = 5;
// returns three fields that are up to 64 chars each
constexpr size_t C3_OS_INFO_BUFFER_SIZE = 64 * 3 + 16;
// returns four fields that are up to 64 chars each + 12 chars for CPU cores
constexpr size_t C3_SYSTEM_INFO_BUFFER_SIZE = 64 * 4 + 16 + 12;

const char* c3_get_build_mode_name(char* buffer, c3_byte_t id) C3_FUNC_COLD;
#if INCLUDE_C3_GET_OS_INFO
const char* c3_get_os_info(char* buffer, size_t size) C3_FUNC_COLD;
#endif // INCLUDE_C3_GET_OS_INFO
const char* c3_get_system_info(char* buffer, size_t size) C3_FUNC_COLD;

}

#endif // _C3_VERSION_H
