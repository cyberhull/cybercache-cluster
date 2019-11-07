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
#include "ls_utils.h"

#include <cstdio>

namespace CyberCache {

rotation_type_t LogUtils::get_log_rotation_type(const char* path) {
  if (path != nullptr && path[0] != '\0' && strlen(path) < MAX_FILE_PATH_LENGTH - TIMER_FORMAT_STRING_LENGTH -
    PRECISION_TIMER_STRING_LENGTH - 6 /* two underscores and ".log" extension*/) {
    rotation_type_t type = RT_INVALID;
    const char* p = path;
    for (;;) {
      switch (*p) {
        case 0:
          return type;
        case '%':
          switch (p[1]) {
            case '%':
              // two consecutive '%' characters in sprintf() format string act like one literal '%'
              p += 2; // skip both
              break;
            case 'd':
              if (type == RT_INVALID) {
                type = RT_TIMESTAMP;
                p += 2;
                break;
              } else {
                // more than one placeholder
                return RT_INVALID;
              }
            case 's':
              if (type == RT_INVALID) {
                type = RT_DATETIME;
                p += 2;
                break;
              } else {
                // more than one placeholder
                return RT_INVALID;
              }
            default:
              // ill-formed or unsupported format specification
              return RT_INVALID;
          }
        default:
          p++;
      }
    }
  } else {
    return RT_INVALID;
  }
}

rotation_result_t LogUtils::rotate_log(const char* current_path, const char* rotation_path, char* rnd_path) {
  if (current_path != nullptr && current_path[1] != 0) {
    char path[MAX_FILE_PATH_LENGTH];
    int length;

    // 1) Prepare path[] for the first renaming attempt
    switch (get_log_rotation_type(rotation_path)) {
      case RT_TIMESTAMP:
        length = std::snprintf(path, MAX_FILE_PATH_LENGTH, rotation_path, Timer::current_timestamp());
        break;
      case RT_DATETIME: {
        char datetime[TIMER_FORMAT_STRING_LENGTH];
        if (Timer::to_ascii(Timer::current_timestamp(), true, datetime) != nullptr) {
          for (char* s = datetime; *s != 0; s++) {
            if (*s == ' ') {
              *s = '_';
            } else if (*s == ':') {
              *s = '-';
            }
          }
          length = std::snprintf(path, MAX_FILE_PATH_LENGTH, rotation_path, datetime);
          break;
        }
        // fall through
      }
      default:
        return RR_ERROR_INVALID_PATH;
    }

    // 2) Try renaming w/o any "randomization"
    c3_assert((size_t) length < MAX_FILE_PATH_LENGTH);
    if (!c3_file_access(path)) {
      // rename() would just overwrite the file w/o any warnings!
      if (c3_rename_file(current_path, path)) {
        if (rnd_path != nullptr) {
          strcpy(rnd_path, path);
        }
        return RR_SUCCESS;
      }
    }

    // 3) Do we have to try to "randomize" destination path?
    if (rnd_path != nullptr) {

      // 4) Find out if rotation final name has extension
      const int MAX_EXTENSION_LENGTH = 8; // somewhat arbitrary parameter for the heuristic...
      char extension[MAX_EXTENSION_LENGTH];
      char* p = path + length - 1;
      bool extension_found = false;
      for (int i = 0; i < MAX_EXTENSION_LENGTH - 1; i++) {
        if (*p == '.') {
          extension_found = true;
          break; // found extension!
        }
        if (*p == '/' || *p == '\\') {
          break; // found path separator; any dots to the left would be in directory names
        }
        if (--p == path) {
          break; // reached the beginning of the buffer; must be a short template
        }
      }

      // 5) Figure out where are we going to insert (or append) "random" portion
      if (extension_found) {
        std::strcpy(extension, p); // stash extension
      } else {
        p = path + length;
      }

      // 6) "Randomize" rotation path using number of nanoseconds since server box boot
      std::sprintf(p, "_%lld", PrecisionTimer::nanoseconds_since_epoch());
      if (extension_found) {
        strcat(path, extension); // restore extension
      }

      // 7) Try renaming to a "randomized" path
      c3_assert(strlen(path) < MAX_FILE_PATH_LENGTH);
      if (!c3_file_access(path)) {
        // rename() just overwrites files w/o a warning
        if (c3_rename_file(current_path, path)) {
          std::strcpy(rnd_path, path);
          return RR_SUCCESS_RND;
        }
      }
    }

    // 8) still having a problem (most probably, file/directory access issue); report it
    return RR_ERROR_RENAME;

  } else {
    return RR_ERROR_INVALID_PATH;
  }
}

}
