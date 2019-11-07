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
 * Configures build modes for CyberCache and its libraries.
 *
 * This file must be the VERY FIRST included by all modules, before any other
 * system, library, or application include file. Neither include files nor
 * modules should include either `assert.h` or `cassert` directly.
 *
 * The following modes are supported (listed in order from least safe/most
 * optimized to safest/least optimized; each mode defines C3_BUILD_MODE_NAME
 * string that should be used as part of application version/build number):
 *
 * - C3_FASTEST: removes even array bounds checks; buffer pointers are checked
 *   for `NULL`s only immediately after memory [re]allocations,
 *
 * - C3_FASTER: defines `NDEBUG` and thus removes all regular `assert()`s,
 *
 * - C3_NORMAL: defined if no other mode is specified explicitly; no extra
 *   checks stipulated by "safe" modes are done,
 *
 * - C3_SAFER: defines `c3_assert()` macro that is used throughout the code to
 *   do some extra checks,
 *
 * - C3_SAFEST: allows to perform extra, potentially [very] time-consuming
 *   checks to ensure data integrity, which may include walking through arrays
 *   and other such operations; should only be used for debugging purposes.
 *
 * Additionally, `C3_FASTER` and `C3_FASTEST` modes define `C3_FAST`, and
 * `C3_SAFER` and `C3_SAFEST` modes define `C3_SAFE`. This allows for one-macro
 * checks in any circumstances; for instance, it is possible to see if current
 * mode is normal, fast, or fastest by simply testing if `C3_SAFE` is undefined.
 */
#ifndef _C3_BUILD_H
#define _C3_BUILD_H

#include "c3_build_defs.h"

///////////////////////////////////////////////////////////////////////////////
// FIGURE OUT PLATFORM
///////////////////////////////////////////////////////////////////////////////

#ifdef __CYGWIN__
  #define C3_CYGWIN 1
#elif defined(__linux__)
  #undef C3_CYGWIN
#else
  #error "Unsupported platform (neither Linux nor Cygwin)"
#endif

#ifdef C3_CYGWIN
  #define C3_CDECL __cdecl
#else
  #define C3_CDECL
#endif

// this should only be included *after* we figure out if it's native Linux or Cygwin
#include "c3_build_assert.h"

///////////////////////////////////////////////////////////////////////////////
// CONFIGURE EDITIONS
///////////////////////////////////////////////////////////////////////////////

namespace CyberCache {

#ifndef C3_ENTERPRISE
  // disable Enterprise edition features and settings
  #define C3_ENTERPRISE 0
#endif // C3_ENTERPRISE

#if C3_ENTERPRISE
  #define C3_EDITION "Enterprise"
  #define BUILD_MODE_EDITION_ID   BUILD_MODE_ID_ENTERPRISE
  #define BUILD_MODE_EDITION_NAME BUILD_MODE_STRING_ENTERPRISE
  constexpr unsigned int MAX_NUM_TABLES_PER_STORE = 256;
  constexpr unsigned int MAX_NUM_INTERNAL_TAG_REFS = 64;
  constexpr bool LIMITED_MEMORY_QUOTA = false; // actual limit per store is 128Tb
  constexpr unsigned int MAX_CONFIG_INCLUDE_LEVEL = 8; // base config + 7 nested
  constexpr unsigned int MAX_NUM_CONNECTION_THREADS = 48; // worker threads
  constexpr unsigned int MAX_IPS_PER_SERVICE = 16; // IPs per sistener/replicator/etc.
#else
  #define C3_EDITION "Community"
  #define BUILD_MODE_EDITION_ID   BUILD_MODE_ID_COMMUNITY
  #define BUILD_MODE_EDITION_NAME BUILD_MODE_STRING_COMMUNITY
  constexpr unsigned int MAX_NUM_TABLES_PER_STORE = 4;
  constexpr unsigned int MAX_NUM_INTERNAL_TAG_REFS = 1;
  constexpr bool LIMITED_MEMORY_QUOTA = true; // actual limit per store is 32Gb
  constexpr unsigned int MAX_CONFIG_INCLUDE_LEVEL = 2; // base config + 1 nested
  constexpr unsigned int MAX_NUM_CONNECTION_THREADS = 6; // worker threads
  constexpr unsigned int MAX_IPS_PER_SERVICE = 2; // IPs per sistener/replicator/etc.
#endif

///////////////////////////////////////////////////////////////////////////////
// CONFIGURE INSTRUMENTATION
///////////////////////////////////////////////////////////////////////////////

#ifndef C3_INSTRUMENTED
  // disable instrumentation by default
  #define C3_INSTRUMENTED 0
#endif // C3_INSTRUMENTED

#if C3_INSTRUMENTED
  #define BUILD_MODE_INSTRUMENTATION_ID   BUILD_MODE_ID_IS_INSTRUMENTED
  #define BUILD_MODE_INSTRUMENTATION_NAME BUILD_MODE_STRING_IS_INSTRUMENTED
#else
  #define BUILD_MODE_INSTRUMENTATION_ID   BUILD_MODE_ID_NOT_INSTRUMENTED
  #define BUILD_MODE_INSTRUMENTATION_NAME BUILD_MODE_STRING_NOT_INSTRUMENTED
#endif // C3_INSTRUMENTED

///////////////////////////////////////////////////////////////////////////////
// FIGURE OUT BUILD MODE
///////////////////////////////////////////////////////////////////////////////

#undef C3_BUILD_MODE_NAME

#ifdef C3_FASTEST
  #if defined(C3_FASTER) || defined(C3_NORMAL) || defined(C3_SAFER) || defined(C3_SAFEST)
    #error C3_FASTEST cannot be specified along with other build modes
  #endif
  #define C3_BUILD_MODE_NAME BUILD_MODE_EDITION_NAME BUILD_MODE_STRING_FAST BUILD_MODE_STRING_EXT \
    BUILD_MODE_INSTRUMENTATION_NAME
  #define C3_BUILD_MODE_ID (BUILD_MODE_EDITION_ID | BUILD_MODE_INSTRUMENTATION_ID | BUILD_MODE_ID_FASTEST)
#endif // C3_FASTEST

#ifdef C3_FASTER
  #if defined(C3_NORMAL) || defined(C3_SAFER) || defined(C3_SAFEST)
    #error C3_FASTER cannot be specified along with other build modes
  #endif
  #define C3_BUILD_MODE_NAME BUILD_MODE_EDITION_NAME BUILD_MODE_STRING_FAST BUILD_MODE_STRING_NO_EXT \
    BUILD_MODE_INSTRUMENTATION_NAME
  #define C3_BUILD_MODE_ID (BUILD_MODE_EDITION_ID | BUILD_MODE_INSTRUMENTATION_ID | BUILD_MODE_ID_FASTER)
#endif // C3_FASTER

#ifdef C3_NORMAL
  #if defined(C3_SAFER) || defined(C3_SAFEST)
    #error C3_NORMAL cannot be specified along with other build modes
  #endif
  #define C3_BUILD_MODE_NAME BUILD_MODE_EDITION_NAME BUILD_MODE_STRING_NORMAL BUILD_MODE_STRING_EXT \
    BUILD_MODE_INSTRUMENTATION_NAME
  #define C3_BUILD_MODE_ID (BUILD_MODE_EDITION_ID | BUILD_MODE_INSTRUMENTATION_ID | \
    BUILD_MODE_ID_NORMAL_EXPLICIT)
#endif // C3_NORMAL

#ifdef C3_SAFER
  #if defined(C3_SAFEST)
    #error C3_SAFER cannot be specified along with other build modes
  #endif
  #define C3_BUILD_MODE_NAME BUILD_MODE_EDITION_NAME BUILD_MODE_STRING_SAFE BUILD_MODE_STRING_NO_EXT \
    BUILD_MODE_INSTRUMENTATION_NAME
  #define C3_BUILD_MODE_ID (BUILD_MODE_EDITION_ID | BUILD_MODE_INSTRUMENTATION_ID | BUILD_MODE_ID_SAFER)
#endif // C3_SAFER

#ifdef C3_SAFEST
  #define C3_BUILD_MODE_NAME BUILD_MODE_EDITION_NAME BUILD_MODE_STRING_SAFE BUILD_MODE_STRING_EXT \
    BUILD_MODE_INSTRUMENTATION_NAME
  #define C3_BUILD_MODE_ID (BUILD_MODE_EDITION_ID | BUILD_MODE_INSTRUMENTATION_ID | BUILD_MODE_ID_SAFEST)
#endif // C3_SAFEST

#ifndef C3_BUILD_MODE_NAME
  #define C3_NORMAL
  #define C3_BUILD_MODE_NAME BUILD_MODE_EDITION_NAME BUILD_MODE_STRING_NORMAL BUILD_MODE_STRING_NO_EXT \
    BUILD_MODE_INSTRUMENTATION_NAME
  #define C3_BUILD_MODE_ID (BUILD_MODE_EDITION_ID | BUILD_MODE_INSTRUMENTATION_ID | \
    BUILD_MODE_ID_NORMAL_IMPLICIT)
#endif // C3_BUILD_MODE_NAME

///////////////////////////////////////////////////////////////////////////////
// CONFIGURE SELECTED BUILD MODE
///////////////////////////////////////////////////////////////////////////////

#if defined(C3_SAFEST) || defined(C3_SAFER)
  #define C3_SAFE
  #undef NDEBUG
  #define C3_DEBUG_ON 1
  #define C3_DEBUG(stmt) stmt
  #define C3_DEBUG_LOG(args) log(LL_DEBUG, args)
  #define C3_STACKDUMP_ENABLED 1
#else // !(C3_SAFEST || C3_SAFER)
  #if defined(C3_FASTEST) || defined(C3_FASTER)
    #define C3_FAST
    #define NDEBUG
  #endif // C3_FASTEST || C3_FASTER
  #undef  C3_DEBUG_ON
  #define C3_DEBUG(stmt) ((void)0)
  #define C3_DEBUG_LOG(args) ((void)0)
  #define C3_STACKDUMP_ENABLED 0
#endif // C3_SAFEST || C3_SAFER

#ifdef C3_SAFEST
  #if C3_CYGWIN
    #define c3_assert(expr) ((expr)? (void)0 : __assert_func(__FILE__, __LINE__, __ASSERT_FUNC, #expr))
    #define c3_assert_failure() __assert_func(__FILE__, __LINE__, __ASSERT_FUNC, "c3_assert_failure()")
  #else
    #define c3_assert(expr) ((expr)? (void)0 : C3_ASSERT_HANDLER(#expr, __FILE__, __LINE__, __ASSERT_FUNCTION))
    #define c3_assert_failure() C3_ASSERT_HANDLER("c3_assert_failure()", __FILE__, __LINE__, __ASSERT_FUNCTION)
  #endif
  #define c3_assert_def(def) def =
#else
  #define c3_assert(expr) ((void)0)
  #define c3_assert_failure() ((void)0)
  #define c3_assert_def(def)
#endif

#ifndef C3_FAST
  #if C3_CYGWIN
    #define assert_failure() __assert_func(__FILE__, __LINE__, __ASSERT_FUNC, "assert_failure()")
  #else
    #define assert_failure() C3_ASSERT_HANDLER("assert_failure()", __FILE__, __LINE__, __ASSERT_FUNCTION)
  #endif
#else
  #define assert_failure(msg) ((void)0)
#endif

///////////////////////////////////////////////////////////////////////////////
// CONFIGURE ACCESS TO UNALIGNED AND BE/LE DATA
///////////////////////////////////////////////////////////////////////////////

#ifdef C3_ALIGNED_ACCESS
  #ifdef C3_UNALIGNED_ACCESS
    #error Aligned and unaligned access cannot be specified simultaneously
  #endif
#elif !defined(C3_UNALIGNED_ACCESS)
  #define C3_UNALIGNED_ACCESS
#endif

#ifdef C3_BIG_ENDIAN
  #ifdef C3_LITTLE_ENDIAN
    #error Big-endian and little-endian layouts cannot be specified simultaneously
  #endif
#elif !defined(C3_LITTLE_ENDIAN)
  #define C3_LITTLE_ENDIAN
#endif

///////////////////////////////////////////////////////////////////////////////
// FUNCTION ATTRIBUTES
///////////////////////////////////////////////////////////////////////////////

/*
 * See: https://gcc.gnu.org/onlinedocs/gcc-5.4.0/gcc/Function-Attributes.html#Function-Attributes
 *
 * Some potentially useful function attributes such as `__attribute__((nothrow))` are not represented with
 * `C3_xxx` macros because there are standard C++11 keywords (e.g. `noexcept`) for what they do.
 */

/// Function never returns (e.g always calls abort(), extit() etc. internally)
#define C3_FUNC_NORETURN __attribute__((noreturn))

/// Function uses printf()-style format, format string is argument `fmt_index`, followed by data arguments;
/// argument indices start from 1; for non-static member functions, `this` counts as argument 1
#define C3_FUNC_PRINTF(fmt_index) __attribute__((format (printf, fmt_index, (fmt_index) + 1)))

/// Function returns memory ptr that doesn't overlap with any other address (not applicable to realloc())
#define C3_FUNC_MALLOC __attribute__((malloc))

/// All pointer arguments to the function are (must be) non-NULL
#define C3_FUNC_NONNULL_ARGS __attribute__((nonnull))

/// Function always returns non-NULL pointer
#define C3_FUNC_NONNULL_RETURN __attribute__((returns_nonnull))

/// Function is a pure function whose result depends on arguments only; may reference global memory
#define C3_FUNC_PURE __attribute__((pure))

/// Function is a pure function whose result depends on arguments only; can NOT reference global memory
#define C3_FUNC_CONST __attribute__((const))

/// Function is a "hot spot", very likely to be called; optimized [more] aggressively for speed
#define C3_FUNC_HOT __attribute__((hot))

/// Function is a "cold spot", unlikely to be called; optimized for size
#define C3_FUNC_COLD __attribute__((cold))

} // CyberCache

// this must be included after `NDEBUG` is configured (defined or undefined...)
#include <cassert>

#endif // _C3_BUILD_H
