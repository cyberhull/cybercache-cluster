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
 * Simple dynamic string container.
 *
 * Why does the world needs another `string`? We need it to be domain-aware, and we do not want to store
 * domain separately, let alone store domain in the first character of `string` and then do acrobatics to
 * retrieve string characters etc. We know our strings only need 16 bits to store their lengths. Besides,
 * we do not need anything fancy like concatenations or iterators, and we do not need uber-performance
 * while *manipulating* `String`s: they are used sparingly to store very seldom-changing values; small
 * memory footprint is a priority.
 */
#ifndef _C3_STRING_H
#define _C3_STRING_H

#include "c3_types.h"

#include <utility>
#include <cstddef>

namespace CyberCache {

// whether to compile `c3_ends_with()` function
#define INCLUDE_C3_ENDS_WITH 0

// utilities for case-INsensitive comparisons
bool c3_matches(const char* str1, const char* str2);
bool c3_starts_with(const char* str, const char* prefix);
#if INCLUDE_C3_ENDS_WITH
bool c3_ends_with(const char* str, const char* suffix);
#endif // INCLUDE_C3_ENDS_WITH

class String {
  struct string_t {
    c3_ushort_t s_length;   // string length, characters; includes terminating '\0'
    domain_t    s_domain;   // memory domain in which this structure is allocated
    char        s_chars[1]; // string characters ending with `\0`
  } __attribute__ ((__packed__));;

  static constexpr c3_ushort_t STRING_T_OVERHEAD = (c3_ushort_t) offsetof(string_t, s_chars);

  string_t* s_buffer; // string data: length, domain, and characters OR `NULL`

  String& init(domain_t domain, const char* buffer, size_t length);
  String& init(domain_t domain, const char* str);
  String& copy(const String& that);

public:
  String() { s_buffer = nullptr; }
  String(domain_t domain, const char* buffer, size_t length) { init(domain, buffer, length); }
  String(domain_t domain, const char* str) { init(domain, str); }
  String(String& that): s_buffer(nullptr) { copy(that); }
  String(String&& that) noexcept: s_buffer(nullptr) {
    s_buffer = that.s_buffer;
    that.s_buffer = nullptr;
  }
  String& operator=(const String& that) { return copy(that); }
  String& operator=(String&& that) noexcept { std::swap(s_buffer, that.s_buffer); return *this; }
  operator const char*() { return s_buffer != nullptr? s_buffer->s_chars: nullptr; }
  operator bool() { return s_buffer != nullptr; }
  ~String() { empty(); }

  c3_ushort_t get_length() const { return s_buffer != nullptr? s_buffer->s_length: (c3_ushort_t) 0; }
  const char* get_chars() const { return s_buffer != nullptr? s_buffer->s_chars: nullptr; }
  String& set(domain_t domain, const char* buffer, size_t length);
  String& set(domain_t domain, const char* str);
  void empty();
  bool is_empty() const { return s_buffer == nullptr; }
  bool not_empty() const { return s_buffer != nullptr; }
};

/**
 * String matcher: matches strings against mask that may contain arbitrary number of wildcard
 * characters (asterisks, '*'). Supports case-sensitive and case-insensitive matching.
 */
class StringMatcher {
  const char* const sm_mask;        // mask against which strings are matched
  c3_uint_t         sm_num_matches; // number of matches found since object creation
  const bool        sm_ci;          // `true` if matching is case-insensitive

  // checks that string matches mask up to the first '*' or '\0' in the latter; if so, returns length of the match
  c3_uint_t get_match_length(const char* str, const char* mask) const;

  // checks that string matches specified mask
  bool matches(const char* str, const char* mask) const;

public:
  /**
   * Constructor.
   *
   * @param mask Mask against which all future matches are made; may contain wildcards ('*' characters)
   * @param ci Whether matching should be case-INsensitive
   */
  explicit StringMatcher(const char* mask, bool ci = false): sm_mask(mask), sm_ci(ci) {
    sm_num_matches = 0;
  }

  /**
   * Fetches number of successful matches since creation of the object
   *
   * @return An unsigned integer
   */
  c3_uint_t get_num_matches() const { return sm_num_matches; }

  /**
   * Matches specified string against mask passed to the constructor.
   *
   * @param str String to match against mask.
   * @return `true` is the string matches mask, `false` otherwise.
   */
  bool matches(const char* str);
};

}

#endif // _C3_STRING_H
