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
#include "c3_timer.h"

namespace CyberCache {

c3_uint_t PrecisionTimer::time_since(const PrecisionTimer &timer, c3_uint_t *milli,
  c3_uint_t *micro, c3_uint_t *nano) const {
  c3_long_t time = nanoseconds_since(timer);
  if (nano != nullptr) {
    *nano = c3_uint_t(time % 1000);
  }
  time /= 1000;
  if (micro != nullptr) {
    *micro = c3_uint_t(time % 1000);
  }
  time /= 1000;
  if (milli != nullptr) {
    *milli = c3_uint_t(time % 1000);
  }
  return c3_uint_t(time / 1000);
}

const char* Timer::to_ascii(c3_timestamp_t time, bool local, char* buff) {
  static thread_local char time_buff[TIMER_FORMAT_STRING_LENGTH];
  if (buff == nullptr) {
    buff = time_buff;
  }
  struct tm time_info;
  time_t time_value = (time_t) time;
  struct tm* p = local? localtime_r(&time_value, &time_info): gmtime_r(&time_value, &time_info);
  if (p != nullptr) {
    c3_assert_def(size_t nchars) std::strftime(buff, TIMER_FORMAT_STRING_LENGTH, "%Y.%m.%d %H:%M:%S", p);
    c3_assert(nchars == TIMER_FORMAT_STRING_LENGTH-1);
    return buff;
  } else {
    c3_assert_failure();
  }
  return nullptr;
}

} // CyberCache
