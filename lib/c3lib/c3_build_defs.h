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
 * Constants making it possible to reliably encode and decode build mode IDs.
 */
#ifndef _C3_BUILD_DEFS_H
#define _C3_BUILD_DEFS_H

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// BUILD MODE IDS
///////////////////////////////////////////////////////////////////////////////

// subtypes
constexpr unsigned char BUILD_MODE_ID_FAST   = 0x01;
constexpr unsigned char BUILD_MODE_ID_NORMAL = 0x02;
constexpr unsigned char BUILD_MODE_ID_SAFE   = 0x03;
constexpr unsigned char BUILD_MODE_ID_EXT    = 0x04;

// types
constexpr unsigned char BUILD_MODE_ID_FASTER          = BUILD_MODE_ID_FAST;
constexpr unsigned char BUILD_MODE_ID_FASTEST         = BUILD_MODE_ID_FAST | BUILD_MODE_ID_EXT;
constexpr unsigned char BUILD_MODE_ID_NORMAL_IMPLICIT = BUILD_MODE_ID_NORMAL;
constexpr unsigned char BUILD_MODE_ID_NORMAL_EXPLICIT = BUILD_MODE_ID_NORMAL | BUILD_MODE_ID_EXT;
constexpr unsigned char BUILD_MODE_ID_SAFER           = BUILD_MODE_ID_SAFE;
constexpr unsigned char BUILD_MODE_ID_SAFEST          = BUILD_MODE_ID_SAFE | BUILD_MODE_ID_EXT;

// instrumentation
constexpr unsigned char BUILD_MODE_ID_NOT_INSTRUMENTED = 0x00;
constexpr unsigned char BUILD_MODE_ID_IS_INSTRUMENTED  = 0x08;

// editions
constexpr unsigned char BUILD_MODE_ID_COMMUNITY  = 0xC0;
constexpr unsigned char BUILD_MODE_ID_ENTERPRISE = 0xE0;

// masks
constexpr unsigned char BUILD_MODE_SUBTYPE_MASK         = 0x03;
constexpr unsigned char BUILD_MODE_EXT_MASK             = 0x04;
constexpr unsigned char BUILD_MODE_INSTRUMENTATION_MASK = 0x08;
constexpr unsigned char BUILD_MODE_EDITION_MASK         = 0xF0;

///////////////////////////////////////////////////////////////////////////////
// BUILD MODE NAMES
///////////////////////////////////////////////////////////////////////////////

#define BUILD_MODE_CHAR_FAST     'o'
#define BUILD_MODE_STRING_FAST   "o"
#define BUILD_MODE_CHAR_NORMAL   'n'
#define BUILD_MODE_STRING_NORMAL "n"
#define BUILD_MODE_CHAR_SAFE     's'
#define BUILD_MODE_STRING_SAFE   "s"

#define BUILD_MODE_CHAR_NO_EXT   '1'
#define BUILD_MODE_STRING_NO_EXT "1"
#define BUILD_MODE_CHAR_EXT      '2'
#define BUILD_MODE_STRING_EXT    "2"

#define BUILD_MODE_CHAR_IS_INSTRUMENTED    'i'
#define BUILD_MODE_STRING_IS_INSTRUMENTED  "i"
#define BUILD_MODE_CHAR_NOT_INSTRUMENTED   'r'
#define BUILD_MODE_STRING_NOT_INSTRUMENTED "r"

#define BUILD_MODE_CHAR_COMMUNITY    'C'
#define BUILD_MODE_STRING_COMMUNITY  "C"
#define BUILD_MODE_CHAR_ENTERPRISE   'E'
#define BUILD_MODE_STRING_ENTERPRISE "E"

} // CyberCache

#endif // _C3_BUILD_DEFS_H
