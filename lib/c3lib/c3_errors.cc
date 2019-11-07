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
#include "c3_build.h"

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <netdb.h>

#include "c3_errors.h"

// use POSIX-compliant version even if feature test macros tell otherwise
extern "C" int __xpg_strerror_r(int, char *, size_t);

namespace CyberCache {

// see http://stackoverflow.com/questions/423248/what-size-should-i-allow-for-strerror-r
const size_t ERRMSG_LEN = 256;
static thread_local char last_error_message[ERRMSG_LEN];

const char*c3_get_error_message() {
  return last_error_message;
}

int c3_set_error_message(const char *format, ...) {
  va_list args;
  va_start(args, format);
  c3_assert_def(int result) std::vsnprintf(last_error_message, ERRMSG_LEN, format, args);
  va_end(args);
  c3_assert(result > 0 && (size_t) result < ERRMSG_LEN);
  return -1;
}

int c3_set_error_message(int code) {
  errno = code;
  return c3_set_stdlib_error_message();
}

int c3_set_einval_error_message() {
  return c3_set_error_message(EINVAL);
}

int c3_set_stdlib_error_message() {
  int code = errno;
  int result = __xpg_strerror_r(code, last_error_message, ERRMSG_LEN);
  if (result != 0) {
    if (result < 0) {
      // __xpg_strerror_r() in glibc < 2.13 returns -1 and sets 'errno'
      result = errno;
      errno = code; // restore, just in case
    }
    switch (result) {
      case EINVAL:
        return c3_set_error_message("__xpg_strerror_r(): invalid 'errno' %d", code);
      case ERANGE:
        return c3_set_error_message("__xpg_strerror_r(): message too long ('errno': %d)", code);
      default:
        return c3_set_error_message("__xpg_strerror_r(): unexpected error %d", result);
    }
  }
  return -1;
}

int c3_set_gai_error_message(int code) {
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wformat-security"
  return c3_set_error_message(gai_strerror(code));
  #pragma GCC diagnostic pop
}

} // CyberCache
