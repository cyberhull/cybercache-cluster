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
 * Multithreading support: fastest possible implementation of per-object mutexes.
 */
#ifndef _MT_LOCKABLE_OBJECT_H
#define _MT_LOCKABLE_OBJECT_H

#include "c3lib/c3lib.h"
#include "mt_monitoring.h"

namespace CyberCache {

/**
 * Class that is, essentially, the fastest possible and most compact implementation of per-object mutexes. It relies
 * upon server thread model with its known, finite number of threads:
 *
 * 1) number of threads that can wait on this object is limited by the number of free bits in the mask (63),
 *
 * 2) system part of the mutex implementation is based on Linux futexes (on Linux; on Cygwin, futexes are currently
 *    emulated using pipes), which cannot be associated with objects, because they are created on demand, and we would
 *    end up with absolutely huge number of system objects (which are only cleared by the system upon thread exit,
 *    which in our case [almost] never happens since we're using thread pool); instead, futexes (32-bit ints) are
 *    located in thread objects, and actual number of created futexes will be less than the number of active threads,
 *    as not every thread procedure needs to lock an object.
 */
class LockableObject: public Payload {
  typedef c3_ulong_t ls_state_t;
  static constexpr ls_state_t LO_LOCKED = 0x8000000000000000ull;

  std::atomic_ullong lo_state; // high bit acts like "locked" flag; low bits: mask of waiting threads

public:
  LockableObject() {
    c3_assert(lo_state.is_lock_free());
    lo_state.store(0, std::memory_order_relaxed);
  }

  /**
   * Locks the object; waits on thread's event field if the object is already locked.
   *
   * @return `true` if checks had been passed successfully, and the object was locked; `false` if an error occurred
   *   (e.g. if current thread had already acquired lock on *another* object); the locking itself may never fail.
   */
  bool lock();

  /**
   * Checks if the object is currently locked.
   *
   * @return `true` if the object is locked, `false` otherwise.
   */
  bool is_locked() const { return (lo_state.load(std::memory_order_acquire) & LO_LOCKED) != 0; }

  /**
   * Checks if the object is currently locked. If it's not, locks it and returns `true`; otherwise, does *not* wait
   * and returns `false` immediately.
   *
   * @return `true` if locking attempt succeeded, `false` otherwise.
   */
  bool try_lock();

  /**
   * Unlocks the object; prior to this call, the object must have been locked by the *current* thread.
   */
  void unlock();
};

/// Lock guard for lockable objects
class LockableObjectGuard {
  LockableObject* const qmg_lo;     // object to be locked/unlocked
  bool                  qmg_locked; // whether the object was actually locked

public:
  explicit LockableObjectGuard(LockableObject* lo): qmg_lo(lo) {
    c3_assert(lo);
    #if C3LM_ENABLED
    qmg_locked = lo->lock();
    #else
    lo->lock();
    qmg_locked = true;
    #endif // C3LM_ENABLED
  }
  LockableObjectGuard(LockableObjectGuard&) = delete;
  LockableObjectGuard(LockableObjectGuard&&) = delete;

  ~LockableObjectGuard() {
    if (qmg_locked) {
      qmg_lo->unlock();
    }
  }

  void operator=(LockableObjectGuard&) = delete;
  void operator=(LockableObjectGuard&&) = delete;

  bool is_locked() const { return qmg_locked; }
  void unlock() {
    if (qmg_locked) {
      qmg_lo->unlock();
      qmg_locked = false;
    }
  }
};

} // CyberCache

#endif // _MT_LOCKABLE_OBJECT_H
