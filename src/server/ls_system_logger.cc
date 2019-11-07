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
#include "ls_system_logger.h"
#include "cc_subsystems.h"
#include "cc_server.h"

namespace CyberCache {

bool SystemLogger::log_message(log_level_t level, const char* message, int length) {
  if (server.get_state() != SS_RUN) {
    /*
     * Logging during starting up, or shutting down the server.
     *
     * The logic controlling setting log level is in Server::set_log_level(); it takes into account
     * multiple level-setting command line arguments intermixed with configuration files; see comment in
     * that method.
     */
    if (level <= server_logger.get_level()) {
      #pragma GCC diagnostic push
      #pragma GCC diagnostic ignored "-Wformat-security"
      syslog_message(level, message);
      #pragma GCC diagnostic pop

      const char* format;
      FILE* stream = stderr;
      switch (level) {
        case LL_WARNING:
          format = "WARNING: %s\n";
          break;
        case LL_ERROR:
          format = "ERROR: %s\n";
          break;
        case LL_FATAL:
          format = "FATAL ERROR: %s\n";
          break;
        default:
          format = "%s\n";
          stream = stdout;
      }
      return std::fprintf(stream, format, message) > 0;
    }
    return false;
  }
  /*
   * Logging during normal operation (`SS_RUN` execution mode).
   */
  return server_logger.log_string(level, message, length);
}

} // CyberCache
