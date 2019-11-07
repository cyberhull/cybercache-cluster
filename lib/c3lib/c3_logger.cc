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
#include "c3_logger.h"

/*
 * If the following constant is defined to be non-zero, logger will write down, with each message, number of
 * milliseconds elapsed since last log call. To be used during profiling sessions.
 */
#define LOG_MILLISECONDS_SINCE_LAST_CALL 0
#if LOG_MILLISECONDS_SINCE_LAST_CALL
#include "c3_timer.h"
#endif

#include <cstdarg>
#include <cstdio>
#include <syslog.h>

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// BASE CLASS FOR CUSTOM LOGGERS
///////////////////////////////////////////////////////////////////////////////

bool AbstractLogger::log(log_level_t level, const char* format, ...) {
  char buffer[2048
#if LOG_MILLISECONDS_SINCE_LAST_CALL
  + 16
#endif
  ];
  va_list args;
  va_start(args, format);

  int prefix_len = 0; // will be optimized out if not in profiling mode
#if LOG_MILLISECONDS_SINCE_LAST_CALL
  static PrecisionTimer last;
  PrecisionTimer now;
  c3_long_t elapsed = now.milliseconds_since(last);
  last = now;
  prefix_len = std::snprintf(buffer, 16, "%4lld: ", elapsed);
#endif

  int length = std::vsnprintf(buffer + prefix_len, sizeof buffer - prefix_len, format, args);
  va_end(args);
  if (length > 0) {
    return log_message(level, buffer, prefix_len + length);
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// SYSTEM LOGGER
///////////////////////////////////////////////////////////////////////////////

// translation of CyberCache log levels to those of the system
static c3_byte_t sm_levels[LL_NUMBER_OF_ELEMENTS] = {
  LOG_EMERG,   // LL_INVALID (this is *not* what LL_INVALID normally means, but it is still useful)
  LOG_ALERT,   // LL_EXPLICIT
  LOG_CRIT,    // LL_FATAL
  LOG_ERR,     // LL_ERROR
  LOG_WARNING, // LL_WARNING
  LOG_NOTICE,  // LL_TERSE
  LOG_INFO,    // LL_NORMAL
  LOG_INFO,    // LL_VERBOSE
  LOG_DEBUG    // LL_DEBUG
};

static LogInterface* syslog_host = nullptr;

void syslog_open(const char* name, bool daemon, LogInterface* host) {
  syslog_host = host;
  openlog(name, LOG_NDELAY | LOG_PID, daemon? LOG_DAEMON: LOG_USER);
}

void syslog_message(log_level_t level, const char* format, ...) {
  assert(level < LL_NUMBER_OF_ELEMENTS && format != nullptr);
  if (syslog_host != nullptr) {
    if (level == LL_WARNING) {
      syslog_host->increment_warning_count();
    } else if (level == LL_ERROR) {
      syslog_host->increment_error_count();
    }
  }
  va_list args;
  va_start(args, format);
  vsyslog(sm_levels[level], format, args);
  va_end(args);
}

void syslog_close() {
  closelog();
}

}
