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
 * Custom version of `assert` handler that dumps stack frame before aborting the application.
 */
#ifndef _C3_BUILD_ASSERT_H
#define _C3_BUILD_ASSERT_H

#ifndef C3_CYGWIN

// configure assert()/c3_assert() behavior
#if defined(C3_SAFEST) || defined(C3_SAFER)
  #define C3_USE_OWN_ASSERT 1 // overwrite assert handler with one that produces stack trace
#else
  #define C3_USE_OWN_ASSERT 0
#endif // C3_SAFEST || C3_SAFER

#define C3_USE_STACKDUMP_FILE     1 // where to write stack trace: (0) to stderr, or (1) to a file
#define C3_STACKDUMP_FILE_IN_HOME 1 // where stack trace file resides: (0) in current directory, (1) in home directory

#if C3_USE_OWN_ASSERT

#define C3_ASSERT_HANDLER c3_assert_fail_handler

namespace CyberCache {

void c3_assert_fail_handler(const char* expr, const char* file, unsigned int line, const char* func)
  __attribute__((noreturn)) __attribute__((cold));

} // CyberCache

#else // !C3_USE_OWN_ASSERT

#define C3_ASSERT_HANDLER __assert_fail

#endif // C3_USE_OWN_ASSERT
#endif // C3_CYGWIN
#endif // _C3_BUILD_ASSERT_H
