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
#include "c3_system.h"
#include "c3_errors.h"

#include <thread>
#include <unistd.h>

namespace CyberCache {

c3_ulong_t c3_get_total_memory() {
  long page_size = sysconf(_SC_PAGESIZE);
  if (page_size > 0) {
    long num_pages = sysconf(_SC_PHYS_PAGES);
    if (num_pages > 0) {
      return (c3_ulong_t) page_size * num_pages;
    }
  }
  c3_set_stdlib_error_message();
  return 0;
}

c3_ulong_t c3_get_available_memory() {
  long page_size = sysconf(_SC_PAGESIZE);
  if (page_size > 0) {
    long available_pages = sysconf(_SC_AVPHYS_PAGES);
    if (available_pages > 0) {
      return (c3_ulong_t) page_size * available_pages;
    }
  }
  c3_set_stdlib_error_message();
  return 0;
}

c3_uint_t c3_get_num_cpus() {
  c3_uint_t result = std::thread::hardware_concurrency();
  if (result > 0) {
    return result;
  }
  long num_processors = sysconf(_SC_NPROCESSORS_ONLN);
  if (num_processors > 0) {
    return (c3_uint_t) num_processors;
  }
  num_processors = sysconf(_SC_NPROCESSORS_CONF);
  if (num_processors > 0) {
    return (c3_uint_t) num_processors;
  }
  c3_set_stdlib_error_message();
  return 0;
}

} // CyberCache
