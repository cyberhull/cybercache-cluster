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
 * Definitions of base types having certain numbers of bits.
 */
#ifndef _C3_TYPES_H
#define _C3_TYPES_H

#include "c3_build.h"

#include <sys/types.h> // for `size_t` and `ssize_t`
#include <cstdint> // for `intptr_t` and `uintptr_t`

namespace CyberCache {

#ifndef NULL
  #define NULL nullptr
#endif

///////////////////////////////////////////////////////////////////////////////
// HELPER MACROS
///////////////////////////////////////////////////////////////////////////////

/**
 * offsetof() is undefined for non-POD types, so we have to do some homebrew pointer math to achieve the
 * same effect without any warnings
 */
#define C3_OFFSETOF(C, F) ((char*)&((C*)0)->F - (char*)0)

/**
 * The macro to be used to stringify non-string values is `C3_STRINGIFY()`; the `C3_STRINGIFY_HELPER()` macro
 * is there to supply correct string to the `#` operator; without it, `C3_STRINGIFY_DEFINE(name) #name` would
 * always return string "name".
 */
#define C3_STRINGIFY(name) C3_STRINGIFY_HELPER(name)
#define C3_STRINGIFY_HELPER(str) #str

///////////////////////////////////////////////////////////////////////////////
// SIGNED INTEGER TYPES
///////////////////////////////////////////////////////////////////////////////

typedef signed char c3_char_t;
typedef short c3_short_t;
typedef int c3_int_t;
typedef long long c3_long_t;

constexpr c3_char_t CHAR_MAX_VAL = 127;
constexpr c3_char_t CHAR_MIN_VAL = -128;
constexpr c3_short_t SHORT_MAX_VAL = 32767;
constexpr c3_short_t SHORT_MIN_VAL = -32768;
constexpr c3_int_t INT_MAX_VAL = 2147483647;
constexpr c3_int_t INT_MIN_VAL = -2147483648;
constexpr c3_long_t LONG_MAX_VAL = 9223372036854775807LL;
constexpr c3_long_t LONG_MIN_VAL = -LONG_MAX_VAL - 1LL; // -9223372036854775808

///////////////////////////////////////////////////////////////////////////////
// UNSIGNED INTEGER TYPES
///////////////////////////////////////////////////////////////////////////////

typedef unsigned char c3_byte_t;
typedef unsigned short c3_ushort_t;
typedef unsigned int c3_uint_t;
typedef unsigned long long c3_ulong_t;

constexpr c3_byte_t BYTE_MAX_VAL = 255;
constexpr c3_ushort_t USHORT_MAX_VAL = 65535;
constexpr c3_uint_t UINT_MAX_VAL = 4294967295;
constexpr c3_ulong_t ULONG_MAX_VAL = 18446744073709551615ULL;

///////////////////////////////////////////////////////////////////////////////
// POINTER-RELATED TYPES
///////////////////////////////////////////////////////////////////////////////

// integer types having the size of a pointer; we provide our own since similar types available
// from `<cstdint>` are optional; workaround might be necessary; see http://en.cppreference.com/w/cpp/types/integer

typedef intptr_t c3_intptr_t;
typedef uintptr_t c3_uintptr_t;

/**
 * Simple wrapper around a pointer; meant to be used for storing pointers in queues and vectors, as well
 * as similar applications (whenever default `nullptr` value is required).
 */
template <class T> class Pointer {
  T* p_pointer; // data

public:
  Pointer() { p_pointer = nullptr; }
  explicit Pointer(T* pointer) { p_pointer = pointer; }

  bool is_valid() const { return p_pointer != nullptr; }
  T* get() const { return p_pointer; }
};

///////////////////////////////////////////////////////////////////////////////
// FILE SYSTEM-RELATED LIMITS
///////////////////////////////////////////////////////////////////////////////

/**
 * GNU system documentation claims that on systems not imposing any limits on file path length standard
 * `FILENAME_MAX` macro can be a "very large number", so they say dynamic memory allocation should be
 * used (see https://www.gnu.org/software/libc/manual/html_node/Limits-for-Files.html); we do need to
 * make some simple manipulations with paths in stack buffers, hence the below constant.
 */
constexpr size_t MAX_FILE_PATH_LENGTH = 4096;

/**
 * Maximum length of *one* command line option; each option gets truncated to the size of
 * MAX_COMMAND_LINE_OPTION_LENGTH-1.
 */
constexpr size_t MAX_COMMAND_LINE_OPTION_LENGTH = 2048;

///////////////////////////////////////////////////////////////////////////////
// APPLICATION-SPECIFIC TYPES
///////////////////////////////////////////////////////////////////////////////

/// Hash codes of hash table elements, passwords, etc.
typedef c3_ulong_t c3_hash_t;

/// Invalid hash code for *passwords*
constexpr c3_hash_t INVALID_HASH_VALUE = 0;

/**
 * Domains are server subsystems for which memory allocation quotas are tracked separately. If more
 * exactly, quotas are only tracked (and enforced!) for domains other than `DOMAIN_GLOBAL`; for the
 * global domain, the server monitors memory usage and can report it upon request, but it cannot enforce
 * any quota because, say, buffers for objects created while servicing incoming connections must be
 * created no matter what -- while there is any free memory at all.
 *
 * Only a handful of types of memory allocations belong to to session and FPC domains, but they are
 * nonetheless major consumers (it can be said that, in fact, they *are* the cache); specifically:
 *
 * - data buffers owned by `LockableHashObject`s (memory quota changes happen upon payload "transfers";
 *   transferring a buffer to a `LockableHashObject` is the only possible domain quota change [global ->
 *   session or FPC], it never happens the other way round),
 * - hash tables and all `HashObject`-derived classes,
 * - various temporary buffers used by optimization threads,
 * - file and socket readers/writers doing replication/binlogging/recovery for a domain,
 * - `String` objects holding auxiliary data in *some* pipelines,
 * - nothing else (not even any queues in non-global domains, they are statically-allocated).
 *
 * Domain IDs can be used in a variety of other objects, but they are mostly unused while
 * allocating/deallocating memory; for instance, all sync objects except queues use memory domains for
 * control/recovery purposes only rather than for memory management.
 */
enum domain_t: c3_byte_t {
  DOMAIN_INVALID = 0, // error value (placeholder)
  DOMAIN_GLOBAL,      // server data that does not clearly belong to either session or FPC, or client data
  DOMAIN_SESSION,     // server session storage
  DOMAIN_FPC,         // server full page cache storage
  DOMAIN_NUMBER_OF_ELEMENTS
};

///////////////////////////////////////////////////////////////////////////////
// NETWORKING-RELATED TYPES
///////////////////////////////////////////////////////////////////////////////

typedef c3_uint_t c3_ipv4_t;
typedef unsigned __int128 c3_ipv6_t;
typedef union {
  c3_ipv6_t ip_v6;
  c3_ipv4_t ip_v4[4];
} c3_ip_t;

constexpr c3_ipv4_t INVALID_IPV4_ADDRESS = 0xFFFFFFFF;

} // namespace CyberCache

#endif // _C3_TYPES_H
