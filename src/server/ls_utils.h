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
 * Logging services: generic logging-related types and utilities.
 */
#ifndef _LS_UTILS_H
#define _LS_UTILS_H

#include "c3lib/c3lib.h"

namespace CyberCache {

/// Log rotation type deduced from the rotation path
enum rotation_type_t {
  RT_INVALID = 0, // NULL path, too long path, no or ill-formed placeholder(s)
  RT_TIMESTAMP,   // rotation path contains "%d", so numeric timestamp will be used
  RT_DATETIME     // rotation path contains "%s", so textual timestamp will be used
};

/// Result codes for a log rotation attempt
enum rotation_result_t {
  RR_SUCCESS,            // the log file had been rotated according to the request
  RR_SUCCESS_RND,        // the log had been rotated, but we had to "randomize" log file name
  RR_ERROR_INVALID_PATH, // invalid or ill-formed path(s) had been submitted to the function
  RR_ERROR_RENAME        // could not rename even to "randomized" path; file/directory access issue?
};

/// Wrapper for generic log rotation functions; can be used for regular logs, or binlogs.
class LogUtils {
public:
  /**
   * Checks what kind of rotation is specified by provided argument.
   *
   * @param path Rotation path; must contain exactly one "%d" or "%s" placeholder
   * @return Check result (an element of the `rotation_type_t` enum)
   */
  static rotation_type_t get_log_rotation_type(const char* path) C3_FUNC_COLD;

  /**
   * Performs log rotation.
   *
   * @param current_path Current path to the log file; any file handles to this file must be closed prior to
   *  calling this function
   * @param rotation_path Path (file name) to which existing log file will be renamed; the path must contain
   *   either "%d" (placeholder for current timestamp) or "%s" (placeholder for current date and time in
   *   textual format) character sequences, but not both; the placeholder will be replaced with current
   *   timestamp (or date/time) before renaming
   * @param rnd_path If not `NULL`, then, if first attempt at renaming fails, the function will "randomize"
   *   rotation path to make it unique; note though that even in this case rotation can still fail because
   *   of many reasons (e.g. if running process do not have write permissions to the directory in
   *   which "rotated" log is supposed to reside); if rotation attempt succeeds after "randomization",
   *   final path will be compied to `rnd_path`, which must have at least MAX_FILE_PATH_LENGTH bytes. If
   *   very first rotation attempt suceeds and `rnd_path` is not zero, then path to which the log was
   *   "rotated" (renamed) gets copied to `rnd_path` as well.
   * @return Rotation result, an element of the `rotation_result_t` enum.
   */
  static rotation_result_t rotate_log(const char* current_path, const char* rotation_path,
    char* rnd_path = nullptr) C3_FUNC_COLD;
};

}

#endif // _LS_UTILS_H
