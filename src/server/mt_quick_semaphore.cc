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
#include "mt_quick_semaphore.h"
#include "mt_threads.h"

namespace CyberCache {

void QuickSemaphore::unregister_reader() {
  const qs_state_t state = fs_state.fetch_sub(1, std::memory_order_acq_rel);
  const qs_state_t num_readers = state & READERS_COUNT_MASK;
  assert(num_readers != 0 && num_readers <= READERS_COUNT_MASK);
  /*
   * Because of the second test (for a waiting thread), we have to keep thread index PLUS one, so that it would work
   * even for the main thread (having index 0).
   */
  if (num_readers == 1 && (state & WRITER_THREAD_INDEX_MASK) != 0) {
    const c3_uint_t waiting_thread_index = (state >> 24) - 1;
    c3_assert(waiting_thread_index < MAX_NUM_THREADS);
    Thread::trigger_event(waiting_thread_index);
  }
}

void QuickSemaphore::wait_until_no_readers() {
  const c3_uint_t thread_id = Thread::get_id();
  const qs_state_t waiting_thread_mask = (thread_id + 1) << 24;
  const qs_state_t state = fs_state.fetch_or(waiting_thread_mask, std::memory_order_acq_rel);
  assert((state & WRITER_THREAD_INDEX_MASK) == 0);
  if ((state & READERS_COUNT_MASK) != 0) {
    PERF_UPDATE_ARRAY(Waits_Until_No_Readers, thread_id)
    Thread::wait_for_event();
  }
  // reset "waiting thread" mask
  fs_state.store(0, std::memory_order_release);
}

} // CyberCache
