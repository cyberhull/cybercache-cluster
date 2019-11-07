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
#include "c3_string.h"
#include "c3_memory.h"

#include <cstring>
#include <cctype>

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// UTILITIES
///////////////////////////////////////////////////////////////////////////////

bool c3_matches(const char* str1, const char* str2) {
  if (str1 != nullptr && str2 != nullptr) {
    for (;;) {
      if (std::tolower(*(c3_byte_t*)str1) != std::tolower(*(c3_byte_t*)str2)) {
        return false;
      }
      if (*str1 == '\0') {
        return true;
      }
      str1++;
      str2++;
    }
  } else {
    assert_failure();
    return false;
  }
}

bool c3_starts_with(const char* str, const char* prefix) {
  if (str != nullptr && prefix != nullptr) {
    while (*prefix != '\0') {
      if (std::tolower(*(c3_byte_t*)prefix) != std::tolower(*(c3_byte_t*)str)) {
        return false;
      }
      prefix++;
      str++;
    }
    return true;
  } else {
    assert_failure();
    return false;
  }
}

#if INCLUDE_C3_ENDS_WITH
bool c3_ends_with(const char* str, const char* suffix) {
  if (str != nullptr && suffix != nullptr) {
    size_t str_len = std::strlen(str);
    size_t suffix_len = std::strlen(suffix);
    if (str_len >= suffix_len) {
      return c3_matches(str + (str_len - suffix_len), suffix);
    } else {
      return false;
    }
  } else {
    assert_failure();
    return false;
  }
}
#endif // INCLUDE_C3_ENDS_WITH

///////////////////////////////////////////////////////////////////////////////
// STRING
///////////////////////////////////////////////////////////////////////////////

String& String::init(domain_t domain, const char* buffer, size_t length) {
  // all control paths must set `s_buffer` to some value
  if (buffer != nullptr) {
    if (length <= USHORT_MAX_VAL) {
      s_buffer = alloc<string_t>(domain, length + STRING_T_OVERHEAD);
      s_buffer->s_domain = domain;
      s_buffer->s_length = (c3_ushort_t) length;
      std::memcpy(s_buffer->s_chars, buffer, length);
    } else {
      assert_failure();
      s_buffer = nullptr;
    }
  } else {
    s_buffer = nullptr;
  }
  return *this;
}

String& String::init(domain_t domain, const char* str) {
  if (str != nullptr) {
    return init(domain, str, strlen(str) + 1);
  } else {
    s_buffer = nullptr;
    return *this;
  }
}

String& String::copy(const String& that) {
  empty();
  string_t* str = that.s_buffer;
  if (str != nullptr) {
    return init(str->s_domain, str->s_chars, str->s_length);
  } else {
    return *this;
  }
}

String& String::set(domain_t domain, const char* buffer, size_t length) {
  empty();
  return init(domain, buffer, length);
}

String& String::set(domain_t domain, const char* str) {
  empty();
  return init(domain, str);
}

void String::empty() {
  if (s_buffer != nullptr) {
    c3_assert(s_buffer->s_length &&
      s_buffer->s_domain > DOMAIN_INVALID && s_buffer->s_domain < DOMAIN_NUMBER_OF_ELEMENTS);
    Memory& memory = Memory::get_memory_object(s_buffer->s_domain);
    memory.free(s_buffer, s_buffer->s_length + STRING_T_OVERHEAD);
    s_buffer = nullptr;
  }
}

///////////////////////////////////////////////////////////////////////////////
// STRING MATCHER
///////////////////////////////////////////////////////////////////////////////

c3_uint_t StringMatcher::get_match_length(const char* str, const char* mask) const {
  c3_assert(str && mask && *mask && *mask != '*');
  for (c3_uint_t length = 0; ; length++) {
    char m = *mask;
    switch (m) {
      case 0:
      case '*':
        return length;
      default:
        if (sm_ci? std::tolower((c3_byte_t) *str) != std::tolower((c3_byte_t) m): *str != m) {
          return 0;
        }
        mask++;
        str++;
    }
  }
}

bool StringMatcher::matches(const char* str, const char* mask) const {
  c3_assert(str && mask);
  switch (*mask) {
    case 0:
      return *str == 0;
    case '*':
      do mask++; while (*mask == '*');
      if (*mask == 0) {
        // mask ends with '*', so anything goes
        return true;
      }
      // mask still has non-asterisk characters, so there must be an exact match (somewhere...)
      for (; *str != 0; str++) {
        const char* substr = str;
        c3_uint_t length;
        // find substring that matches mask up to the next '*', or '\0'
        while ((length = get_match_length(substr, mask)) == 0) {
          if (*++substr == 0) {
            return false;
          }
        }
        if (matches(substr + length, mask + length)) {
          return true;
        }
      }
      return false;
    default: {
      c3_uint_t n = get_match_length(str, mask);
      if (n != 0) {
        return matches(str + n, mask + n);
      }
      return false;
    }
  }
}

bool StringMatcher::matches(const char* str) {
  if (matches(str, sm_mask)) {
    sm_num_matches++;
    return true;
  }
  return false;
}

}
