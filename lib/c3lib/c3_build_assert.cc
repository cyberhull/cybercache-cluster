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
#include "c3_types.h"

#if C3_USE_OWN_ASSERT

#include "c3_files.h"
#include "c3_stackdump.h"
#include "c3_errors.h"
#include "c3_logger.h"
#include "c3_version.h"

#include <cstdio>

namespace CyberCache {

void c3_assert_fail_handler(const char* expr, const char* file, unsigned int line, const char* func) {
  syslog_message(LL_FATAL, "[%s] Assertion '%s' failed in %s:%u",
    c3lib_version_build_string, expr, file, line);
  #if C3_USE_STACKDUMP_FILE
  const char* dumpfile_name = "c3lib-assert.stacktrace";
#if C3_STACKDUMP_FILE_IN_HOME
  char path[MAX_FILE_PATH_LENGTH];
  dumpfile_name = c3_get_home_path(path, sizeof path, dumpfile_name);
#endif // C3_STACKDUMP_FILE_IN_HOME
  const char facility[] = "[C3 Assert Handler]";
  if (c3_save_stackdump(dumpfile_name, false)) {
    syslog_message(LL_FATAL, "Stack trace saved to '%s'", dumpfile_name);
    fprintf(stderr, "%s\nStack trace saved to '%s'\n", facility, dumpfile_name);
  } else {
    syslog_message(LL_FATAL, "Could not save stack trace to '%s' (%s)", dumpfile_name, c3_get_error_message());
    fprintf(stderr, "%s\nCould not save stack trace to '%s' (%s)\n", facility, dumpfile_name, c3_get_error_message());
  }
#else // !C3_USE_STACKDUMP_FILE
  c3_show_stackdump();
#endif // C3_USE_STACKDUMP_FILE
  __assert_fail(expr, file, line, func);
}

} // CyberCache

#endif // C3_USE_OWN_ASSERT
