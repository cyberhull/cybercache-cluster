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
 * Logging services: base definitions, interface to the system logging facility.
 */
#ifndef _LS_SYSLOG_H
#define _LS_SYSLOG_H

#include "c3_types.h"

#include <cstddef>

namespace CyberCache {

/// Verbosity levels for logging
enum log_level_t: c3_byte_t {
  LL_INVALID = 0, // logging is disabled permanently, logging service is shutting down
  LL_EXPLICIT,    // the server is explicitly told to log something (e.g. with a `LOG` console command)
  LL_FATAL,       // a message about a fatal error (e.g. memory corruption)
  LL_ERROR,       // a message about an error (e.g. a dropped connection)
  LL_WARNING,     // a warning message (e.g. non-enforceable setting in a config file being loaded)
  LL_TERSE,       // a status change message (e.g. a component had been initialized successfully)
  LL_NORMAL,      // a regular message (e.g. a config option has changed); the default
  LL_VERBOSE,     // a system information message (e.g. a queue or hash table capacity increased)
  LL_DEBUG,       // debugging message (e.g. another connection established)
  LL_NUMBER_OF_ELEMENTS
};

/**
 * Methods that host should provide to the logger if it (the host) needs to monitor numbers of logged
 * warnings and/or errors.
 */
class LogInterface {
public:
  /**
   * Increment number of warning encountered by subsystems; the count is maintained by the host
   * implementation and can be reported during health checks.
   */
  virtual void increment_warning_count() = 0;
  /**
   * Increment number of non-fatal errors encountered by subsystems; the count is maintained by the host
   * implementation and can be reported during health checks.
   */
  virtual void increment_error_count() = 0;
};

/**
 * Generic logging interface.
 *
 * This class is *NOT* used as a base class for `Logger`: its purpose is to serve as a virtual base class
 * for various library classes that need logging services (e.g. socket- and file-related I/O classes).
 * Those classes then can use logging services w/o being tied to a particular implementation.
 *
 * For instance, at the very top (or bottom, depending upon one's perspective) of `SocketInputPipleline`
 * hierarchy is `AbstractLogger` (as a *virtual* base class), so methods of the `SocketInputPipleline`
 * class can use logging services just fine. Then, server will implement `ServerSocketInputPipeline` as
 * `public SocketInputPipleline, public ServerLogger`, where actual logger implementation is
 * `ServerLogger: public virtual AbstractLogger`, which is, in fact, simply forwarding logging requests to
 * server's `Logger` instance.
 */
class AbstractLogger {
protected:
  /**
   * Method that derived classes must implement.
   *
   * @param level Message level (same as for `log()` method)
   * @param message String to be logged
   * @return `true` on success, `false` on error
   */
  virtual bool log_message(log_level_t level, const char* message, int length) = 0;

public:
  /**
   * Log message at specified level.
   *
   * @param level Level of the message; if current logging level is less than specified, the message will
   *   *not* be logged.
   * @param format Format string, same format that is used for printf()/sprintf()/etc.
   * @return `true` if message was successfully logged OR put into logger's queue (depending on
   *   implementation), `false` otherwise
   */
  bool log(log_level_t level, const char* format, ...) C3_FUNC_PRINTF(3);
};

/**
 * Opens connection to the system logging facility. Calling this function is optional; it may only be
 * necessary when the caller needs to set custom app name and/or "daemon" priority for subsequent
 * `syslog_message()` calls (note that priority can also be passed to one of the `syslog_message()`
 * function overloads.
 *
 * @param name Application ID string, each subsequent message will be automatically prefixed with it; if
 *   this argument is `NULL`, application name from `argv[0]` will be used; if this argument is not
 *   `NULL`, it must refer to a string that will remain valid till `syslog_close()` (or another
 *   `syslog_open()`) call
 * @param daemon If `true`, sets "daemon" priority for subsequent mesages; if `false` (the default),
 *   "application" priority will be used instead
 * @param host Host interface to be used to report non-fatal errors
 */
void syslog_open(const char* name = nullptr, bool daemon = false, LogInterface* host = nullptr) C3_FUNC_COLD;

/**
 * Sends message to system logging facility using default priority set with `syslog_open()`.
 *
 * @param level Message level, an `LL_xxx` constant
 * @param format Message format string, similar to that of `fprintf()` family of functions
 */
void syslog_message(log_level_t level, const char* format, ...) C3_FUNC_COLD C3_FUNC_PRINTF(2);

/**
 * Closes connection to system logging facility; calling this function is optional: only necessary for
 * code in shared libraries that can be unloaded.
 */
void syslog_close() C3_FUNC_COLD;

}

#endif // _LS_SYSLOG_H
