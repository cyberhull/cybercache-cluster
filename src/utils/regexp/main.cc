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
#include <regex>

int main(int argc, char** argv) {
  if (argc != 3) {
    std::printf("Usage: %s <pattern> <search-text>", argv[0]);
    exit(EXIT_FAILURE);
  }
  const char* pattern = argv[1];
  const char* text = argv[2];
  try {
    std::regex rx(pattern, std::regex::optimize /* | std::regex::nosubs */);
    std::cmatch match;
    if (std::regex_search(text, match, rx, std::regex_constants::match_not_null)) {
      std::printf("Matched:\n");
      for (unsigned int i = 0; i < match.size(); i++) {
        std::csub_match sub = match[i];
        const char* str = sub.str().data();
        std::printf("%2u) '%.*s'\n", i, (int) sub.length(), str);
      }
    } else {
      std::printf("Did not match.");
    }
  } catch (const std::regex_error& e) {
    std::printf("Pattern error: %s", e.what());
  }
}
