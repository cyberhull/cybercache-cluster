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
 * Logger with fallback functionality (for when main logger had not been started yet).
 */
#ifndef _LS_SYSTEM_LOGGER_H
#define _LS_SYSTEM_LOGGER_H

#include "c3lib/c3lib.h"

namespace CyberCache {

/// Implementation of the abstract logger, to be used in server subsystems.
class SystemLogger: public virtual AbstractLogger {
  /**
   * If global server logger has not been initialized yet, outputs messages to syslog
   * and duplicates them to either `stdout` or `stderr` (depending on severity).
   */
  bool log_message(log_level_t level, const char* message, int length) override C3_FUNC_COLD;
};

} // CyberCache

#endif // _LS_SYSTEM_LOGGER_H
