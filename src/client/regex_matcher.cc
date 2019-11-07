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
#include "regex_matcher.h"

#include <regex>

static std::regex bot_matcher;

void regex_init() {
  /*
   * Intentionally left empty.
   *
   * PCRE2-based implementation would require some initialization code here, but C++11 regex-based
   * implementation does not.
   */
}

bool regex_compile(const char* pattern) {
  try {
    if (pattern == nullptr) {
      pattern = ""; // this won't match anything
    }
    bot_matcher.assign(pattern, std::regex::optimize | std::regex::nosubs | std::regex::icase);
    return true;
  } catch (const std::regex_error& e) {
    return false;
  }
}

bool regex_match(const char* text) {
  if (text != nullptr) {
    try {
      return std::regex_search(text, bot_matcher, std::regex_constants::match_not_null);
    } catch (const std::regex_error& e) {
      return false;
    }
  } else {
    return false;
  }
}

void regex_cleanup() {
  /*
   * Intentionally left empty.
   *
   * PCRE2-based implementation would require some cleanup code here, but C++11 regex-based
   * implementation does not.
   */
}
