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
 * Support for performance counters and other profiling features.
 *
 * Helper classes (`perf_xxx_t`) are designed to be as fast as possible at the expense of providing (through their
 * getters) stats that may not be *formally* 100% up to date (e.g. a counter may be incremented in one thread, *then*
 * other thread may immediately request its value, and get previous value). All counters and trackers are, however,
 * designed to never lose any stats; for instance, if two threads simultaneously update the same maximum value
 * tracker, the tracker is guaranteed to end up holding the biggest value; similarly, the increment "missed" because
 * it happened simultaneously with data request (discussed in previous example) will become "visible" during
 * subsequent inspections.
 */
#if C3_INSTRUMENTED
#ifndef _C3_PROFILER_H
#define _C3_PROFILER_H

#include "io_protocol.h"
#include <atomic>
#include <cstdio>

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// BASE COUNTERS
///////////////////////////////////////////////////////////////////////////////

/// Base class for all counters
template <class T, T N> class perf_counter_t {
protected:
  std::atomic<T> pc_value; // value of the counter

public:
  perf_counter_t() { reset(); }

  T get() const { return pc_value.load(std::memory_order_relaxed); }
  void reset() { pc_value.store(N, std::memory_order_relaxed); }
};

template <typename T> const char* print_number(const perf_counter_t<T, 0> &number, char* buffer, size_t size)
  C3_FUNC_COLD;
const char* print_number(const perf_counter_t<c3_uint_t, 0> &number, char* buffer, size_t size) C3_FUNC_COLD;
const char* print_number(const perf_counter_t<c3_ulong_t, 0> &number, char* buffer, size_t size) C3_FUNC_COLD;

/// Simple increment/decrement counter
template <class T> class perf_number_t: public perf_counter_t<T, 0> {
public:
  /*
   * The bodies of the below methods do not compile without `this->`;
   * see https://isocpp.org/wiki/faq/templates#nondependent-name-lookup-members
   */
  void increment() { this->pc_value.fetch_add(1, std::memory_order_relaxed); }
  void decrement() { this->pc_value.fetch_sub(1, std::memory_order_relaxed); }
};

/// Tracker of minimum value of a property
template <class T> class perf_minimum_t: public perf_counter_t<T, ((T)-1ull)> {
public:
  void update(T value) {
    T min = this->pc_value.load(std::memory_order_acquire);
    while (value < min) {
      // the following statement loads new minimum value into `min` if comparison fails
      if (this->pc_value.compare_exchange_weak(min, value, std::memory_order_acq_rel, std::memory_order_acquire)) {
        break;
      }
    }
  }
};

/// Tracker of maximum value of a property
template <class T> class perf_maximum_t: public perf_counter_t<T, 0> {
public:
  void update(T value) {
    T max = this->pc_value.load(std::memory_order_acquire);
    while (value > max) {
      // the following statement loads new maximum value into `max` if comparison fails
      if (this->pc_value.compare_exchange_weak(max, value, std::memory_order_acq_rel, std::memory_order_acquire)) {
        break;
      }
    }
  }
};

/// Tracker of minimum and maximum values of a property
template <class T> class perf_range_t {
  perf_minimum_t<T> pr_min; // minimum encountered value
  perf_maximum_t<T> pr_max; // maximum encountered value

public:
  T get_min() const { return pr_min.get(); }
  T get_max() const { return pr_max.get(); }

  void update(T value) {
    pr_min.update(value);
    pr_max.update(value);
  }
  void reset() {
    pr_min.reset();
    pr_max.reset();
  }
};

template <typename T> const char* print_range(const perf_range_t<T> &range, char* buffer, size_t size);
const char* print_range(const perf_range_t<c3_uint_t> &range, char* buffer, size_t size);
const char* print_range(const perf_range_t<c3_ulong_t> &range, char* buffer, size_t size);

/**
 * Array of N counters; first N-1 register encounters of values N-1 (i.e. one counter per value), last one counts
 * how many times any value greater than or equal to N appeared.
 */
template <class T, T N> class perf_array_t {
  perf_number_t<T> pa_values[N]; // numbers of encounters of each value

public:
  T get(c3_uint_t i) {
    c3_assert(i < N);
    return pa_values[i].get();
  }

  const perf_number_t<T>* get_values() const { return pa_values; }

  void increment(T n) {
    const T combined = N - 1;
    pa_values[n < combined? n: combined].increment();
  }

  void reset() {
    for (c3_uint_t i = 0; i < (c3_uint_t) N; i++) {
      pa_values[i].reset();
    }
  }
};

template <typename T> const char* print_array(const perf_number_t<T>* values, c3_uint_t num, char* buffer, size_t size)
  C3_FUNC_COLD;
const char* print_array(const perf_number_t<c3_uint_t>* values, c3_uint_t num, char* buffer, size_t size) C3_FUNC_COLD;
const char* print_array(const perf_number_t<c3_ulong_t>* values, c3_uint_t num, char* buffer, size_t size) C3_FUNC_COLD;

///////////////////////////////////////////////////////////////////////////////
// NAMED COUNTERS: BASE CLASS
///////////////////////////////////////////////////////////////////////////////

/// Constants that are used to manipulate domain-dependent counters
enum perf_domain_t {
  PD_GLOBAL = 0, // counter belongs to global domain
  PD_SESSION,    // counter belongs to session domain
  PD_FPC,        // counter belongs to FPC domain
  PD_NUMBER_OF_ELEMENTS
};

static_assert(PD_GLOBAL == (DM_GLOBAL - 1), "'perf_domain_t' does not align with 'domain_t'");

class PerfCounter;

/// Type of the performance counters enumeration callback; returns `true` if enumeration should continue
typedef bool (*perf_enum_proc_t)(const PerfCounter* counter, void* context);

/// Base class for all named performance counters
class PerfCounter {
  static PerfCounter* pc_first;   // first performance counter in the list
  PerfCounter* const  pc_next;    // next performance counter in the list
  const char* const   pc_name;    // name of this performance counter
  const c3_byte_t     pc_domains; // a combination of `DM_xxx` constants

protected:
  PerfCounter(c3_byte_t domains, const char* name);
  c3_byte_t get_domain_mask() const { return pc_domains; }
  static const char* get_domain_tag(perf_domain_t domain);

public:
  const char* get_name() const { return pc_name; }
  virtual const char* get_values(c3_byte_t domains, char* buffer, size_t size) const = 0;

  static bool enumerate(c3_byte_t domains, const char* mask, perf_enum_proc_t callback, void* context) C3_FUNC_COLD;
};

///////////////////////////////////////////////////////////////////////////////
// NAMED COUNTERS: SINGLE VALUES
///////////////////////////////////////////////////////////////////////////////

template <class T> class PerfNumberCounter: public PerfCounter {
protected:
  perf_number_t<T> pnc_number;

  const char* get_values(c3_byte_t domains, char* buffer, size_t size) const override C3_FUNC_COLD {
    return print_number(pnc_number, buffer, size);
  }

public:
  PerfNumberCounter(c3_byte_t domains, const char* name): PerfCounter(domains, name) {}

  void increment() { pnc_number.increment(); }
  void decrement() { pnc_number.decrement(); }
};

template <class T> class PerfDomainNumberCounter: public PerfCounter {
  perf_number_t<T> pdnc_numbers[PD_NUMBER_OF_ELEMENTS];

  const char* get_values(c3_byte_t domains, char* buffer, size_t size) const override C3_FUNC_COLD {
    const char* separator = "";
    size_t offset = 0;
    for (c3_uint_t domain = PD_GLOBAL; domain < PD_NUMBER_OF_ELEMENTS; domain++) {
      if (((1 << domain) & domains & get_domain_mask()) != 0) {
        char value[32]; // longest c3_ulong_t is 20 chars
        ssize_t nchars = std::snprintf(buffer + offset, size, "%s%s: %s",
          separator, get_domain_tag((perf_domain_t) domain),
          print_number(pdnc_numbers[domain], value, sizeof value));
        if (nchars > 0 && nchars <= (ssize_t) size) {
          offset += nchars;
          size -= nchars;
          separator = ", ";
        } else {
          break;
        }
      }
    }
    return buffer;
  }

public:
  PerfDomainNumberCounter(c3_byte_t domains, const char* name): PerfCounter(domains, name) {}

  void increment(perf_domain_t domain) { pdnc_numbers[domain].increment(); }
  void increment(domain_t domain) {
    if (domain != DOMAIN_INVALID) {
      increment((perf_domain_t)(domain - 1));
    }
  }
  void decrement(perf_domain_t domain) { pdnc_numbers[domain].decrement(); }
  void decrement(domain_t domain) {
    if (domain != DOMAIN_INVALID) {
      decrement((perf_domain_t)(domain - 1));
    }
  }
};

///////////////////////////////////////////////////////////////////////////////
// NAMED COUNTERS: MAXIMUMS
///////////////////////////////////////////////////////////////////////////////

template <class T> class PerfMaximumCounter: public PerfCounter {
  perf_maximum_t<T> pmc_maximum;

  const char* get_values(c3_byte_t domains, char* buffer, size_t size) const override C3_FUNC_COLD {
    return print_number(pmc_maximum, buffer, size);
  }

public:
  PerfMaximumCounter(c3_byte_t domains, const char* name): PerfCounter(domains, name) {}

  void update(T value) { pmc_maximum.update(value); }
};

template <class T> class PerfDomainMaximumCounter: public PerfCounter {
  perf_maximum_t<T> pdmc_maximums[PD_NUMBER_OF_ELEMENTS];

  const char* get_values(c3_byte_t domains, char* buffer, size_t size) const override C3_FUNC_COLD {
    const char* separator = "";
    size_t offset = 0;
    for (c3_uint_t domain = PD_GLOBAL; domain < PD_NUMBER_OF_ELEMENTS; domain++) {
      if (((1 << domain) & domains & get_domain_mask()) != 0) {
        char value[32]; // longest c3_ulong_t is 20 chars
        ssize_t nchars = std::snprintf(buffer + offset, size, "%s%s: %s",
          separator, get_domain_tag((perf_domain_t) domain),
          print_number(pdmc_maximums[domain], value, sizeof value));
        if (nchars > 0 && nchars <= (ssize_t) size) {
          offset += nchars;
          size -= nchars;
          separator = ", ";
        } else {
          break;
        }
      }
    }
    return buffer;
  }

public:
  PerfDomainMaximumCounter(c3_byte_t domains, const char* name): PerfCounter(domains, name) {}

  void update(perf_domain_t domain, T value) { pdmc_maximums[domain].update(value); }
  void update(domain_t domain, T value) {
    if (domain != DOMAIN_INVALID) {
      update((perf_domain_t)(domain - 1), value);
    }
  }
};

///////////////////////////////////////////////////////////////////////////////
// NAMED COUNTERS: RANGES
///////////////////////////////////////////////////////////////////////////////

template <class T> class PerfRangeCounter: public PerfCounter {
  perf_range_t<T> prc_range;

  const char* get_values(c3_byte_t domains, char* buffer, size_t size) const override C3_FUNC_COLD {
    return print_range(prc_range, buffer, size);
  }

public:
  PerfRangeCounter(c3_byte_t domains, const char* name): PerfCounter(domains, name) {}

  void update(T value) { prc_range.update(value); }
};

template <class T> class PerfDomainRangeCounter: public PerfCounter {
  perf_range_t<T> pdrc_ranges[PD_NUMBER_OF_ELEMENTS];

  const char* get_values(c3_byte_t domains, char* buffer, size_t size) const override C3_FUNC_COLD {
    const char* separator = "";
    size_t offset = 0;
    for (c3_uint_t domain = PD_GLOBAL; domain < PD_NUMBER_OF_ELEMENTS; domain++) {
      if (((1 << domain) & domains & get_domain_mask()) != 0) {
        char values[64];
        ssize_t nchars = std::snprintf(buffer + offset, size, "%s%s: %s",
          separator, get_domain_tag((perf_domain_t) domain),
          print_range(pdrc_ranges[domain], values, sizeof values));
        if (nchars > 0 && nchars <= (ssize_t) size) {
          offset += nchars;
          size -= nchars;
          separator = ", ";
        } else {
          break;
        }
      }
    }
    return buffer;
  }

public:
  PerfDomainRangeCounter(c3_byte_t domains, const char* name): PerfCounter(domains, name) {}

  void update(perf_domain_t domain, T value) { pdrc_ranges[domain].update(value); }
  void update(domain_t domain, T value) {
    if (domain != DOMAIN_INVALID) {
      update((perf_domain_t)(domain - 1), value);
    }
  }
};

///////////////////////////////////////////////////////////////////////////////
// NAMED COUNTERS: ARRAYS
///////////////////////////////////////////////////////////////////////////////

template <class T, T N> class PerfArrayCounter: public PerfCounter {
  perf_array_t<T, N> pac_array;

  const char* get_values(c3_byte_t domains, char* buffer, size_t size) const override C3_FUNC_COLD {
    return print_array(pac_array.get_values(), (c3_uint_t) N, buffer, size);
  }

public:
  PerfArrayCounter(c3_byte_t domains, const char* name): PerfCounter(domains, name) {}

  void increment(T value) { pac_array.increment(value); }
};

} // CyberCache

#endif // _C3_PROFILER_H
#endif // C3_INSTRUMENTED
