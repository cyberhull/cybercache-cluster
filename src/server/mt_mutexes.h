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
 * Multithreading support: implementation of mutexes.
 */
#ifndef _MT_MUTEXES_H
#define _MT_MUTEXES_H

#include "c3lib/c3lib.h"
#include "mt_defs.h"

#include <mutex>
#include <shared_mutex>
#include <condition_variable>

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// BASE CLASSES
///////////////////////////////////////////////////////////////////////////////

/**
 * Base class for all mutexes, used as a reference/pointer type and for inspection.
 *
 * If a C++11 mutex had already been locked by current thread, then an attempt to lock it again would
 * result in undefined behavior. CyberCache imposes even stricter constraints: a thread cannot own
 * (lock) more than one `Mutex`-derived class at a time, and all mutexes check this condition in their
 * locking code: hence boolean return values of locking methods, boolean members in mutex guards, and the
 * fact that lock guards are not derived from `std::unique_lock<std::shared_timed_mutex>` and
 * `std::shared_lock<std::shared_timed_mutex>`. Besides, locking attempts can be timed.
 */
class Mutex: public SyncObject {
protected:
  c3_byte_t m_num_readers; // number of currently acquired read locks
  bool      m_exclusive;   // `true` if exclusive (write) lock had been acquired

  C3_FUNC_COLD Mutex(domain_t domain, host_object_t host, sync_object_t type, c3_byte_t id):
    SyncObject(domain, host, type, id) {
    m_num_readers = 0;
    m_exclusive = false;
  }

public:
  /*
   * These methods can *only* be called by lock guards that "know" that they have locked the mutex.
   */
  bool is_locked_exclusively() const { return m_exclusive; }
  c3_byte_t get_num_readers() const { return m_num_readers; }
};

/// Base class for all mutex locks
template <class T> class MutexLockBase {
protected:
  T&   mlb_mutex;  // the mutex this guard operates on
  bool mlb_locked; // `true` if the mutex was successfully locked (MUST be initialized in derived class' dtor)

  explicit MutexLockBase(T& mutex): mlb_mutex(mutex) {}

public:
  bool is_locked() const { return mlb_locked; }
  const T& get_mutex() const { return mlb_mutex; }

  MutexLockBase(const MutexLockBase&) = delete;
  MutexLockBase(MutexLockBase&&) = delete;

  MutexLockBase& operator=(const MutexLockBase&) = delete;
  MutexLockBase& operator=(MutexLockBase&&) = delete;
};

///////////////////////////////////////////////////////////////////////////////
// SHARED MUTEX
///////////////////////////////////////////////////////////////////////////////

/// Mutex supporting shared lock for reading or exclusive lock for writing
class SharedMutex: public Mutex {
  std::shared_timed_mutex sm_mutex; // system object implementing shared locking

public:
  SharedMutex(domain_t domain, host_object_t host, c3_byte_t id) C3_FUNC_COLD;
  SharedMutex(const SharedMutex&) = delete;
  SharedMutex(SharedMutex&&) = delete;

  bool lock_shared();
  bool lock_exclusive();
  void unlock_shared();
  void unlock_exclusive();

  // non-copyable
  SharedMutex& operator=(const SharedMutex&) = delete;
  SharedMutex& operator=(SharedMutex&&) = delete;
};

/// Lock guard providing public API for shared locking/unlocking a shared mutex
class SharedMutexLock: public MutexLockBase<SharedMutex> {
public:
  explicit SharedMutexLock(SharedMutex& mutex): MutexLockBase(mutex) {
    #if C3LM_ENABLED
    mlb_locked = mutex.lock_shared();
    #else
    mutex.lock_shared();
    mlb_locked = true;
    #endif // C3LM_ENABLED
  }
  SharedMutexLock(const SharedMutexLock&) = delete;
  SharedMutexLock(SharedMutexLock&&) = delete;
  ~SharedMutexLock() {
    if (mlb_locked) {
      mlb_mutex.unlock_shared();
    }
  }

  // non-copyable
  SharedMutexLock& operator=(const SharedMutexLock&) = delete;
  SharedMutexLock& operator=(SharedMutexLock&&) = delete;
};

/// Lock guard providing public API for exclusive locking/unlocking a shared mutex
class SharedMutexExclusiveLock: MutexLockBase<SharedMutex> {
public:
  explicit SharedMutexExclusiveLock(SharedMutex& mutex): MutexLockBase(mutex) {
    #if C3LM_ENABLED
    mlb_locked = mutex.lock_exclusive();
    #else
    mutex.lock_exclusive();
    mlb_locked = true;
    #endif // C3LM_ENABLED
  }
  SharedMutexExclusiveLock(const SharedMutexExclusiveLock&) = delete;
  SharedMutexExclusiveLock(SharedMutexExclusiveLock&&) = delete;
  ~SharedMutexExclusiveLock() {
    if (mlb_locked) {
      mlb_mutex.unlock_exclusive();
    }
  }

  // non-copyable
  SharedMutexExclusiveLock& operator=(const SharedMutexExclusiveLock&) = delete;
  SharedMutexExclusiveLock& operator=(SharedMutexExclusiveLock&&) = delete;
};

///////////////////////////////////////////////////////////////////////////////
// SHARED DOWNGRADABLE MUTEX
///////////////////////////////////////////////////////////////////////////////

/// Mutex capable of downgrading its exclusive (write) lock to a shared (read) lock
class DynamicMutex: public Mutex {
  std::mutex              dm_mutex;    // system object implementing simple mutex
  std::condition_variable dm_notifier; // system object implementing notification mechanism

public:
  DynamicMutex(domain_t domain, host_object_t host, c3_byte_t id = 0) C3_FUNC_COLD;
  DynamicMutex(const DynamicMutex&) = delete;
  DynamicMutex(DynamicMutex&&) = delete;

  // non-copyable
  DynamicMutex& operator=(const DynamicMutex&) = delete;
  DynamicMutex& operator=(DynamicMutex&&) = delete;

  bool lock_shared();
  bool lock_exclusive();
  bool downgrade_lock();
  /**
   * There in an *IMPORTANT* difference between downgrading an exclusive lock and upgrading a shared lock.
   *
   * Not only upgrading a lock may take some time, but (most importantly) it is not atomic, in that IF
   * there are other readers at the time `upgrade_lock()` is called, current thread essentially releases
   * its read lock and starts waiting. What can happen next is that an exclusive lock can be acquired by
   * some *other* thread -- current thread that used to own shared lock does not have any preference when
   * scheduler decides who gets [some] lock on the mutex next. Therefore, when current thread finally
   * gets its lock upgraded, it may find the object (protected by the mutex) in a radically different
   * state: for instance, if the thread found an object in a container (protected by the mutex), decided
   * to modify it and requested lock upgrade for that, by the time upgrade finally happens the object it
   * wanted to modify might be gone.
   *
   * On the other hand, upgrading a read lock to exclusive (vs. releasing read lock and trying to
   * acquire write lock "from scratch") does give current thread some advantage in that it does not
   * compete with other threads that might be waiting for an exclusive lock -- again, IF there are no
   * other readers currently.
   */
  bool upgrade_lock(c3_uint_t msecs = 0);
  void unlock_shared();
  void unlock_exclusive();
};

/// Lock guard providing public API for shared locking and unlocking of a shared downgradable mutex
class DynamicMutexLock: public MutexLockBase<DynamicMutex> {
public:
  explicit DynamicMutexLock(DynamicMutex& mutex, bool exclusive = false):
    MutexLockBase(mutex) {
    #if C3LM_ENABLED
    mlb_locked = exclusive? mlb_mutex.lock_exclusive(): mlb_mutex.lock_shared();
    #else
    if (exclusive) {
      mlb_mutex.lock_exclusive();
    } else {
      mlb_mutex.lock_shared();
    }
    mlb_locked = true;
    #endif // C3LM_ENABLED
  }
  DynamicMutexLock(const DynamicMutexLock&) = delete;
  DynamicMutexLock(DynamicMutexLock&&) = delete;
  ~DynamicMutexLock() {
    if (mlb_locked) {
      if (mlb_mutex.is_locked_exclusively()) {
        mlb_mutex.unlock_exclusive();
      } else {
        mlb_mutex.unlock_shared();
      }
    }
  }

  // non-copyable
  DynamicMutexLock& operator=(const DynamicMutexLock&) = delete;
  DynamicMutexLock& operator=(DynamicMutexLock&&) = delete;

  bool downgrade_lock() {
    if (mlb_locked && mlb_mutex.is_locked_exclusively()) {
      return mlb_mutex.downgrade_lock();
    } else {
      c3_assert_failure();
      return false;
    }
  }
  bool upgrade_lock(c3_uint_t msecs) {
    if (mlb_locked && !mlb_mutex.is_locked_exclusively()) {
      return mlb_mutex.upgrade_lock(msecs);
    } else {
      c3_assert_failure();
      return false;
    }
  }
};

} // CyberCache

#endif // _MT_MUTEXES_H
