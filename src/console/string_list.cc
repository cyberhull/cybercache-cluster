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
#include "string_list.h"

namespace CyberCache {

int C3_CDECL StringList::comparator(const void* e1, const void* e2) {
  String& s1 = *(String*) e1;
  String& s2 = *(String*) e2;
  if (s1.not_empty() && s2.not_empty()) {
    return std::strcmp(s1.get_chars(), s2.get_chars());
  } else if (s1.not_empty()) {
    return -1; // move non-empty strings to the top
  } else if (s2.not_empty()) {
    return 1; // move empty strings to the bottom
  } else {
    return 0;
  }
}

void StringList::sort() {
  sl_vector.sort(comparator, true);
}

void StringList::add(const char* name) {
  c3_assert(name);
  sl_vector.push(String(DOMAIN_GLOBAL, name));
}

bool StringList::add_unique(const char* name) {
  c3_assert(name);
  for (c3_uint_t i = 0; i < get_count(); i++) {
    if (std::strcmp(get(i), name) == 0) {
      return false; // already exists
    }
  }
  sl_vector.push(String(DOMAIN_GLOBAL, name));
  sort();
  return true; // successfully added
}

bool StringList::remove_unique(const char* name) {
  c3_assert(name);
  for (c3_uint_t i = 0; i < get_size(); i++) {
    const String& str = sl_vector[i];
    if (str.not_empty() && std::strcmp(str.get_chars(), name) == 0) {
      if (sl_vector.clear(i)) {
        sort();
      }
      return true; // successfully removed
    }
  }
  return false; // does not exist
}

void StringList::remove_all() {
  for (c3_uint_t i = 0; i < sl_vector.get_size(); i++) {
    String& str = sl_vector.get(i);
    if (str.not_empty()) {
      str.empty();
    }
  }
  sl_vector.clear();
}

} // CyberCache
