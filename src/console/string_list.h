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
 * String vector implementation for the server API.
 */
#ifndef _STRING_LIST_H
#define _STRING_LIST_H

#include "c3lib/c3lib.h"

namespace CyberCache {

/// Helper class to maintain lists of strings (such as tags)
class StringList {
  Vector<String> sl_vector;

  c3_uint_t get_size() const { return (c3_uint_t) sl_vector.get_size(); }
  static int C3_CDECL comparator(const void* e1, const void* e2);
  void sort();

public:
  explicit StringList(c3_uint_t init_capacity) noexcept: sl_vector(init_capacity) {}
  StringList(const StringList&) = delete;
  StringList(StringList&&) = delete;
  ~StringList() { remove_all(); }

  StringList& operator=(const StringList&) = delete;
  StringList& operator=(StringList&&) = delete;

  c3_uint_t get_count() const { return (c3_uint_t) sl_vector.get_count(); }
  const char* get(c3_uint_t i) const { return sl_vector[i].get_chars(); };
  void add(const char* name);
  bool add_unique(const char* name);
  bool remove_unique(const char* name);
  void remove_all();
};

} // CyberCache

#endif // _STRING_LIST_H
