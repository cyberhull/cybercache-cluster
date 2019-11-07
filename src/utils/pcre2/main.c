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
#define HAVE_CONFIG_H 1
#define PCRE2_CODE_UNIT_WIDTH 8
#include "regex/pcre2/pcre2.h"

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static void fail(const char* format, ...) {
  char buffer[2048];
  va_list args;
  va_start(args, format);
  int length = vsnprintf(buffer, sizeof buffer, format, args);
  va_end(args);
  assert(length > 0);
  (void) length;
  fprintf(stderr, "[ERROR] %s.\n", buffer);
  exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {

  // 1) Check program arguments
  // --------------------------

  if (argc != 3) {
    printf("\nUsage: %s <pattern> <search-string>\n", argv[0]);
    return EXIT_FAILURE;
  }

  // 2) Compile regular expression
  // -----------------------------

  PCRE2_SPTR pattern = (PCRE2_SPTR) argv[1];
  int error_number;
  PCRE2_SIZE error_offset;
  pcre2_code* re = pcre2_compile(
    pattern,               // regular expression pattern
    PCRE2_ZERO_TERMINATED, // indicate that pattern is NUL-terminated
    0,                     // use default options
    &error_number,         // return value: error number (if any)
    &error_offset,         // return value: where the error occurred
    NULL);                 // use default memory context

  // 3) Handle pattern compilation errors
  // ------------------------------------

  if (re == NULL) {
    PCRE2_UCHAR buffer[256];
    pcre2_get_error_message(error_number, buffer, sizeof buffer);
    fail("PCRE2 compilation failed at offset %d: %s", (int) error_offset, buffer);
  }

  // 4) Allocate buffer for match results
  // ------------------------------------

  pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(
    re,    // regular expression pattern (allocated buffer will have enough space for all groups)
    NULL); // use default memory context

  // 5) Match supplied text against compiled regular expression
  // ----------------------------------------------------------

  PCRE2_SPTR text = (PCRE2_SPTR) argv[2];
  size_t text_length = strlen((char*) text);
  int result = pcre2_match(
    re,          // compiled regular expression pattern
    text,        // search text
    text_length, // length of the search text
    0,           // start offset
    0,           // use default options
    match_data,  // storage for the result
    NULL);       // use default memory context

  // 6) Process matching result
  // --------------------------

  switch(result) {
    // errors -1 and -2 are "expected" errors, other are "true" (implementation/usage) errors
    case PCRE2_ERROR_NOMATCH:
      printf("No match\n");
      break;
    case PCRE2_ERROR_PARTIAL:
      printf("Only partial match\n");
      break;
    default:
      if (result < 0) {
        // there are 60 other errors, about half of them are related to Unicode handling
        printf("Matching error [%d]\n", result);
      } else {
        /*
         * Do sanity check (result would only be zero if `match_data` wasn't big enough to hold all groups,
         * which "couldn't happen" as we allocated it based on the regular expression pattern), then
         * print out match position and matched substrings.
         */
        assert(result > 0);
        PCRE2_SIZE* offset_vector = pcre2_get_ovector_pointer(match_data);
        printf("Match succeeded at offset %d:\n", (int) offset_vector[0]);
        int i = 0; do {
          int index = i * 2;
          PCRE2_SPTR substring_start = text + offset_vector[index];
          size_t substring_length = offset_vector[index + 1] - offset_vector[index];
          printf("%2d) '%.*s'\n", i, (int) substring_length, (char*) substring_start);
        } while (++i < result);
      }
  }

  // 7) Free up resources
  // --------------------

  pcre2_match_data_free(match_data); // release match result
  pcre2_code_free(re);               // release compiled regular expression
  return result? EXIT_SUCCESS: EXIT_FAILURE;
}
