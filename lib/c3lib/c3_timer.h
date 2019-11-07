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
 * Implementation of the Snappy compression engine.
 */
#ifndef _C3_TIMER_H
#define _C3_TIMER_H

#include "c3_build.h"
#include "c3_types.h"

#include <ctime>
#include <chrono>
#include <atomic>

using namespace std::chrono;

namespace CyberCache {

// utilities
constexpr c3_uint_t minutes2seconds(c3_uint_t minutes) noexcept { return 60 * minutes; }
constexpr c3_uint_t hours2seconds(c3_uint_t hours) noexcept { return minutes2seconds(60) * hours; }
constexpr c3_uint_t days2seconds(c3_uint_t days) noexcept { return hours2seconds(24) * days; }
constexpr c3_uint_t weeks2seconds(c3_uint_t weeks) noexcept { return days2seconds(7) * weeks; }

/**
 * Class for measuring smallest time intervals; to be used for profiling and similar tasks: a portable
 * wrapper around C++11 high resolution clock. See http://en.cppreference.com/w/cpp/chrono/duration
 */
class PrecisionTimer {
  high_resolution_clock::time_point pt_time; // time stamp

public:
  explicit PrecisionTimer(bool set = true) {
    if (set) {
      pt_time = high_resolution_clock::now();
    }
  }
  PrecisionTimer(const PrecisionTimer& timer) = default;
  PrecisionTimer(PrecisionTimer&& timer) = default;
  PrecisionTimer& operator=(const PrecisionTimer& timer) = default;
  PrecisionTimer& operator=(PrecisionTimer&& timer) = default;
  ~PrecisionTimer() = default;

  void register_time() {
    pt_time = high_resolution_clock::now();
  }
  // nanoseconds
  static c3_long_t nanoseconds_since_epoch() {
    return duration_cast<std::chrono::nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
  }
  static c3_long_t nanoseconds_since(c3_long_t nsecs) {
    return nanoseconds_since_epoch() - nsecs;
  }
  c3_long_t nanoseconds_since(const PrecisionTimer& timer) const {
    return duration_cast<std::chrono::nanoseconds>(pt_time - timer.pt_time).count();
  }
  // microseconds
  static c3_long_t microseconds_since_epoch() {
    return duration_cast<std::chrono::microseconds>(high_resolution_clock::now().time_since_epoch()).count();
  }
  static c3_long_t microseconds_since(c3_long_t usecs) {
    return microseconds_since_epoch() - usecs;
  }
  c3_long_t microseconds_since(const PrecisionTimer& timer) const {
    return duration_cast<std::chrono::microseconds>(pt_time - timer.pt_time).count();
  }
  // milliseconds
  static c3_long_t milliseconds_since_epoch() {
    return duration_cast<std::chrono::milliseconds>(high_resolution_clock::now().time_since_epoch()).count();
  }
  static c3_long_t milliseconds_since(c3_long_t msecs) {
    return milliseconds_since_epoch() - msecs;
  }
  c3_long_t milliseconds_since(const PrecisionTimer& timer) const {
    return duration_cast<std::chrono::milliseconds>(pt_time - timer.pt_time).count();
  }
  // seconds
  static c3_long_t seconds_since_epoch() {
    return duration_cast<std::chrono::seconds>(high_resolution_clock::now().time_since_epoch()).count();
  }
  static c3_long_t seconds_since(c3_long_t secs) {
    return seconds_since_epoch() - secs;
  }
  c3_long_t seconds_since(const PrecisionTimer& timer) const {
    return duration_cast<std::chrono::seconds>(pt_time - timer.pt_time).count();
  }
  // return all "components" of elapsed time
  c3_uint_t time_since(const PrecisionTimer &timer, c3_uint_t *milli,
    c3_uint_t *micro = nullptr, c3_uint_t *nano = nullptr) const;
};

constexpr size_t PRECISION_TIMER_STRING_LENGTH = 20; // "9223372036854775807"
constexpr size_t TIMER_FORMAT_STRING_LENGTH    = 20; // "YYYY.mm.dd HH:MM:SS"

/// Timestamp: seconds since UNIX epoch; this might be changed to `c3_ulong_t` at some point.
typedef c3_uint_t c3_timestamp_t;

/// Type for inter-thread timestamp exchanges
typedef std::atomic<c3_timestamp_t> atomic_timestamp_t;

/**
 * Portable wrapper around POSIX time() function.
 */
class Timer {
  std::time_t tr_time; // seconds since UNIX epoch (January 1st 1970)

public:
  static const c3_timestamp_t MAX_TIMESTAMP = INT_MAX_VAL;

  explicit Timer(bool set = true) {
    tr_time = set? std::time(nullptr): 0;
  }
  Timer(const Timer& timer) = default;
  Timer(Timer&& timer) = default;
  Timer& operator=(const Timer& timer) = default;
  Timer& operator=(Timer&& timer) = default;
  ~Timer() = default;

  void register_time() {
    tr_time = std::time(nullptr);
  }
  c3_timestamp_t seconds_since(const Timer& timer) const {
    return timer.tr_time > tr_time? c3_timestamp_t(timer.tr_time - tr_time): 0;
  }
  c3_timestamp_t seconds_since(c3_timestamp_t time) const {
    return time > tr_time? c3_uint_t(tr_time - tr_time): 0;
  }

  c3_timestamp_t timestamp() const { return (c3_timestamp_t) tr_time; }
  static c3_timestamp_t current_timestamp() {
    return (c3_timestamp_t) std::time(nullptr);
  }

  static const char *to_ascii(c3_timestamp_t time, bool local = true, char* buff = nullptr);
  const char* to_ascii(bool local = true, char* buff = nullptr) const {
    return to_ascii(timestamp(), local, buff);
  }
};

} // CyberCache

#endif // _C3_TIMER_H
