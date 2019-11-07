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
#include "mt_lockable_object.h"
#include "mt_threads.h"
#include "mt_thread_guards.h"

namespace CyberCache {

static_assert(MAX_NUM_THREADS < 64, "Total number of threads must not exceed 63");

bool LockableObject::lock() {
  ThreadSpinLockAcquireGuard guard(this); // guard validates its argument
  if (guard.check_passed()) {
    PERF_DECLARE_LOCAL_INT_COUNT(num_waits)
    const c3_uint_t thread_id = Thread::get_id();
    c3_assert(thread_id < 63);
    const ls_state_t thread_mask = 1ull << thread_id;
    const ls_state_t locking_thread_mask = thread_mask + LO_LOCKED;
    while ((lo_state.fetch_or(locking_thread_mask, std::memory_order_acq_rel) & LO_LOCKED) != 0) {
      PERF_INCREMENT_LOCAL_COUNT(num_waits)
      Thread::wait_for_event();
    }
    /*
     * Remove current thread from the object's "wait list"; here, we use "relaxed" order because, upon unlocking
     * the object, we will modify the mask one more time, so it is only required that we "see" this change ourselves;
     * clearing thread bit here allows us not to bother with current thread's mask in unlocking code (which does help,
     * since thread bit will not be set at all if the object's locked using `try_lock()`)
     */
    lo_state.fetch_and(~thread_mask, std::memory_order_relaxed);
    PERF_UPDATE_ARRAY(Hash_Object_Waits, PERF_LOCAL(num_waits))
    PERF_INCREMENT_COUNTER(Hash_Object_Locks)
    return true;
  }
  return false;
}

bool LockableObject::try_lock() {
  ThreadObjectTryAcquireGuard guard(this); // guard validates its argument
  if (guard.check_passed()) {
    if ((lo_state.fetch_or(LO_LOCKED, std::memory_order_acq_rel) & LO_LOCKED) == 0) {
      guard.set_success();
      PERF_INCREMENT_COUNTER(Hash_Object_Lock_Try_Successes)
      return true;
    }
    PERF_INCREMENT_COUNTER(Hash_Object_Lock_Try_Failures)
  }
  return false;
}

void LockableObject::unlock() {
  ThreadObjectReleaseGuard guard(this); // guard validates its argument
  if (guard.check_passed()) {
    /*
     * We unlock the mutex AND fetch/clear mask of waiting threads using single atomic call. It is possible that
     * immediately afterwards some other thread locks mutex, and it can theoretically even *unlock* the mutex before
     * current thread resumes. That's OK: that other thread wouldn't see the mask we just cleared, so it wouldn't
     * attempt to wake up the same threads (or current thread) -- it can wake up only the threads that went to
     * sleep after the below `exchange()` call.
     */
    ls_state_t state = lo_state.exchange(0, std::memory_order_acq_rel);
    if ((state &= ~LO_LOCKED) != 0) {
      for (c3_uint_t i = 0; i < MAX_NUM_THREADS; i ++) {
        /*
         * Since specialized (i.e. not "worker" threads) are checked first, they take precedence when trying to lock
         * the object; the main thread (#0) may need this lock when dumping store contents to the database file.
         */
        const ls_state_t mask = 1ull << i;
        if ((state & mask) != 0) {
          // it is important to restore mask (of remaining waiting threads) before we wake up the thread
          lo_state.fetch_or(state & ~mask, std::memory_order_acq_rel);
          Thread::trigger_event(i);
          break;
        }
      }
    }
  }
}

} // CyberCache
