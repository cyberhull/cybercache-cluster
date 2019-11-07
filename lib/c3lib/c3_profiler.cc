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
#include "c3_profiler.h"
#include "c3_string.h"

#if !C3_INSTRUMENTED
#error "Profiler should only be included in instrumented builds"
#endif // C3_INSTRUMENTED

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// HELPER PRINT METHODS: SCALARS
///////////////////////////////////////////////////////////////////////////////

const char* print_number(const perf_counter_t<c3_uint_t, 0> &number, char* buffer, size_t size) {
  std::snprintf(buffer, size, "%u", number.get());
  return buffer;
}

const char* print_number(const perf_counter_t<c3_ulong_t, 0> &number, char* buffer, size_t size) {
  std::snprintf(buffer, size, "%llu", number.get());
  return buffer;
}

///////////////////////////////////////////////////////////////////////////////
// HELPER PRINT METHODS: RANGES
///////////////////////////////////////////////////////////////////////////////

const char* print_range(const perf_range_t<c3_uint_t> &range, char* buffer, size_t size) {
  c3_uint_t minimum = range.get_min();
  c3_uint_t maximum = range.get_max();
  if (minimum == UINT_MAX_VAL && maximum == 0) {
    std::snprintf(buffer, size, "(none)");
  } else {
    std::snprintf(buffer, size, "%u..%u", minimum, maximum);
  }
  return buffer;
}

const char* print_range(const perf_range_t<c3_ulong_t> &range, char* buffer, size_t size) {
  c3_ulong_t minimum = range.get_min();
  c3_ulong_t maximum = range.get_max();
  if (minimum == ULONG_MAX_VAL && maximum == 0) {
    std::snprintf(buffer, size, "(none)");
  } else {
    std::snprintf(buffer, size, "%llu..%llu", minimum, maximum);
  }
  return buffer;
}

///////////////////////////////////////////////////////////////////////////////
// HELPER PRINT METHODS: ARRAYS
///////////////////////////////////////////////////////////////////////////////

const char* print_array(const perf_number_t<c3_uint_t>* values, c3_uint_t num, char* buffer, size_t size) {
  c3_uint_t combined = num - 1;
  size_t offset = 0;
  for (c3_uint_t i = 0; i < num; i++) {
    const char* format = i > 0? (i == combined? ", %u (rest)": ", %u"): "%u";
    ssize_t nchars = std::snprintf(buffer + offset, size, format, values[i].get());
    if (nchars > 0 && nchars <= (ssize_t) size) {
      offset += nchars;
      size -= nchars;
    } else {
      break;
    }
  }
  return buffer;
}

const char* print_array(const perf_number_t<c3_ulong_t>* values, c3_uint_t num, char* buffer, size_t size) {
  c3_uint_t combined = num - 1;
  size_t offset = 0;
  for (c3_uint_t i = 0; i < num; i++) {
    const char* format = i > 0? (i == combined? ", %llu (rest)": ", %llu"): "%llu";
    ssize_t nchars = std::snprintf(buffer + offset, size, format, values[i].get());
    if (nchars > 0 && nchars <= (ssize_t) size) {
      offset += nchars;
      size -= nchars;
    } else {
      break;
    }
  }
  return buffer;
}

///////////////////////////////////////////////////////////////////////////////
// PerfCounter
///////////////////////////////////////////////////////////////////////////////

PerfCounter* PerfCounter::pc_first = nullptr;

PerfCounter::PerfCounter(c3_byte_t domains, const char* name):
  pc_next(pc_first), pc_name(name), pc_domains(domains) {
  c3_assert(domains <= DM_ALL && name);
  pc_first = this;
}

const char* PerfCounter::get_domain_tag(perf_domain_t domain) {
  switch (domain) {
    case PD_GLOBAL:
      return "global";
    case PD_SESSION:
      return "session";
    case PD_FPC:
      return "fpc";
    default:
      assert_failure();
      return "<INVALID>";
  }
}

bool PerfCounter::enumerate(c3_byte_t domains, const char* mask, perf_enum_proc_t callback, void* context) {
  StringMatcher matcher(mask, true);
  PerfCounter* counter = pc_first;
  while (counter != nullptr) {
    if ((counter->pc_domains & domains) != 0 && matcher.matches(counter->pc_name)) {
      if (!callback(counter, context)) {
        return false;
      }
    }
    counter = counter->pc_next;
  }
  return true;
}

}
