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
 * Multithreading support: fastest possible implementation of semaphores on Linux.
 */
#ifndef _MT_QUICK_SEMAPHORE_H
#define _MT_QUICK_SEMAPHORE_H

#include "c3lib/c3lib.h"

namespace CyberCache {

/**
 * This class implements specialized semaphore: if a thread needs to access object buffer for reading, it should call
 * `register_reader()`; when done, it should call `unregister_reader()` method. When a thread needs to modify object
 * data buffer in any way, it should call `wait_until_no_readers()`, which will wait on thread's event object if there
 * are indeed any readers currently working with the buffer. When last reader is done, it will check if some other
 * thread is waiting to write and, if any, will wake it up.
 *
 * The implementation uses the following assumptions (satisfying which is the responsibility of the *callers*):
 *
 * 1) Both `register_reader()` and `wait_until_no_readers()` should only be called by threads having a lock on the
 *    object; therefore, a) a new reader cannot be registered when some thread is already waiting to write (and,
 *    consequently, the thread wouldn't be waiting "forever"), and b) there cannot be more than one thread waiting
 *    for write access (so we can store thread index byte instead of a bit mask covering all threads),
 *
 * 2) Since the thread calling `wait_until_no_readers()` is already supposed to have a lock on the object, it is known
 *    *not* to be waiting on its event (which is a field in the `Thread` class) for "regular" object lock; therefore,
 *    we can re-use that event to wait for the last reader to finish its job.
 *
 * 3) Both `has_readers()` and `unregistered_reader()` methods can be called at any time, without acquiring any
 *    locks whatsoever.
 */
class QuickSemaphore {
  typedef c3_uint_t qs_state_t;

  static constexpr qs_state_t READERS_COUNT_MASK       = 0x00FFFFFF; // enough for 16+ million readers
  static constexpr qs_state_t WRITER_THREAD_INDEX_MASK = 0xFF000000; // bits where thread index PLUS one is kept

  std::atomic_uint fs_state; // low 24 bits: number of readers; high 8 bits: index of the thread waiting to write

public:
  QuickSemaphore() {
    fs_state.store(0, std::memory_order::memory_order_relaxed);
  }

  bool has_readers() const {
    return (fs_state.load(std::memory_order_acquire) & READERS_COUNT_MASK) != 0;
  }

  void register_reader() {
    c3_assert_def(qs_state_t state) fs_state.fetch_add(1, std::memory_order_acq_rel);
    c3_assert((state & READERS_COUNT_MASK) != READERS_COUNT_MASK && (state & WRITER_THREAD_INDEX_MASK) == 0);
  }

  void unregister_reader();

  void wait_until_no_readers();
};

}

#endif // _MT_QUICK_SEMAPHORE_H
