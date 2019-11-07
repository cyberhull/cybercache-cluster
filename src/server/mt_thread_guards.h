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
 * Multithreading support: thread guards for synchronization objects. These classes were introduced so
 * that not to pollute synchronization objects' code with tons of thread state-related defines.
 */
#ifndef _MT_THREAD_GUARDS_H
#define _MT_THREAD_GUARDS_H

#include "c3lib/c3lib.h"
#include "mt_threads.h"

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// THREAD-LEVEL MUTEX GUARDS
///////////////////////////////////////////////////////////////////////////////

/// Base class for all mutex guards
class ThreadMutexGuardBase {
protected:
  C3LM_DEF(const Mutex* const         tmg_mutex)         // mutex the guard is operating on
  C3LM_DEF(const thread_mutex_state_t tmg_success_state) // what state to set in case of successful operation
  C3LM_DEF(bool                       tmg_check_passed)  // whether setting initial state succeeded

  ThreadMutexGuardBase(const Mutex* mutex, thread_mutex_state_t entry_state,
      thread_mutex_state_t success_state, bool skip_spinlock_check)
  #if C3LM_ENABLED
    : tmg_mutex(mutex), tmg_success_state(success_state) {
    tmg_check_passed = Thread::set_mutex_state(mutex, entry_state, skip_spinlock_check);
  }
  #else
  {}
  #endif // C3LM_ENABLED

public:
  bool check_passed() const { return C3LM_IF(tmg_check_passed, true); }
};

/// Base class: guard for mutex operations that cannot fail because of mutex itself but can still fail
/// because of thread-level checks (a two-state guard)
class ThreadMutexGuard2: public ThreadMutexGuardBase {
protected:
  ThreadMutexGuard2(const Mutex* mutex,
    thread_mutex_state_t entry_state, thread_mutex_state_t success_state):
    ThreadMutexGuardBase(mutex, entry_state, success_state, false) {}

public:
  ThreadMutexGuard2(const ThreadMutexGuard2&) = delete;
  ThreadMutexGuard2(ThreadMutexGuard2&&) = delete;

  #if C3LM_ENABLED
  ~ThreadMutexGuard2() {
    if (tmg_check_passed) {
      Thread::set_mutex_state(tmg_mutex, tmg_success_state, false);
    }
  }
  #endif // C3LM_ENABLED

  ThreadMutexGuard2& operator=(const ThreadMutexGuard2&) = delete;
  ThreadMutexGuard2& operator=(ThreadMutexGuard2&&) = delete;
};

/// Base class: guard for mutex operations that can fail (a three-state guard)
class ThreadMutexGuard3: public ThreadMutexGuardBase {
  C3LM_DEF(const thread_mutex_state_t tmg_failure_state)       // what state to set in case of failure
  C3LM_DEF(bool                       tmg_success)             // current status of the operation itself
  C3LM_DEF(const bool                 tmg_skip_spinlock_check) // `true` if spinlock can be active during [un]lock

protected:
  ThreadMutexGuard3(const Mutex* mutex, thread_mutex_state_t entry_state,
      thread_mutex_state_t success_state, thread_mutex_state_t failure_state, bool skip_spinlock_check):
    ThreadMutexGuardBase(mutex, entry_state, success_state, skip_spinlock_check)
  #if C3LM_ENABLED
    , tmg_failure_state(failure_state), tmg_skip_spinlock_check(skip_spinlock_check) {
    tmg_success = false;
  }
  #else
  {}
  #endif // C3LM_ENABLED

public:
  ThreadMutexGuard3(const ThreadMutexGuard3&) = delete;
  ThreadMutexGuard3(ThreadMutexGuard3&&) = delete;

  #if C3LM_ENABLED
  ~ThreadMutexGuard3() {
    if (tmg_check_passed) {
      Thread::set_mutex_state(tmg_mutex, tmg_success? tmg_success_state: tmg_failure_state,
        tmg_skip_spinlock_check);
    }
  }
  #endif // C3LM_ENABLED

  ThreadMutexGuard3& operator=(const ThreadMutexGuard3&) = delete;
  ThreadMutexGuard3& operator=(ThreadMutexGuard3&&) = delete;

  void set_success() { C3LM_ON(tmg_success = true); }
};

/// Guard for locking mutexes in read mode
class ThreadMutexSharedLockGuard: public ThreadMutexGuard3 {
public:
  explicit ThreadMutexSharedLockGuard(const Mutex* mutex):
    ThreadMutexGuard3(mutex, MXS_BEGIN_SHARED_LOCK, MXS_ACQUIRED_SHARED_LOCK, MXS_SHARED_LOCK_FAILED,
      false) {}
};

/// Guard for locking mutexes in write mode
class ThreadMutexExclusiveLockGuard: public ThreadMutexGuard3 {
public:
  explicit ThreadMutexExclusiveLockGuard(const Mutex* mutex):
    ThreadMutexGuard3(mutex, MXS_BEGIN_EXCLUSIVE_LOCK, MXS_ACQUIRED_EXCLUSIVE_LOCK,
      MXS_EXCLUSIVE_LOCK_FAILED,
      false) {}
};

/// Guard for releasing read locks
class ThreadMutexSharedUnlockGuard: public ThreadMutexGuard2 {
public:
  explicit ThreadMutexSharedUnlockGuard(const Mutex* mutex):
    ThreadMutexGuard2(mutex, MXS_BEGIN_SHARED_UNLOCK, MXS_UNLOCKED) {}
};

/// Guard for releasing write locks
class ThreadMutexExclusiveUnlockGuard: public ThreadMutexGuard2 {
public:
  explicit ThreadMutexExclusiveUnlockGuard(const Mutex* mutex):
    ThreadMutexGuard2(mutex, MXS_BEGIN_EXCLUSIVE_UNLOCK, MXS_UNLOCKED) {}
};

/// Guard for downgrading write locks to read locks
class ThreadMutexDowngradeGuard: public ThreadMutexGuard3 {
public:
  explicit ThreadMutexDowngradeGuard(const Mutex* mutex):
    ThreadMutexGuard3(mutex, MXS_BEGIN_DOWNGRADE, MXS_ACQUIRED_SHARED_LOCK, MXS_DOWNGRADE_FAILED,
      true) {}
};

/// Guard for upgrading read locks to write locks
class ThreadMutexUpgradeGuard: public ThreadMutexGuard3 {
public:
  explicit ThreadMutexUpgradeGuard(const Mutex* mutex):
    ThreadMutexGuard3(mutex, MXS_BEGIN_UPGRADE, MXS_ACQUIRED_EXCLUSIVE_LOCK, MXS_UPGRADE_FAILED,
      true) {}
};

///////////////////////////////////////////////////////////////////////////////
// THREAD-LEVEL OBJECT (QUICK MUTEX) GUARDS
///////////////////////////////////////////////////////////////////////////////

/// Base class for all lockable object (quick mutex) guards
class ThreadObjectGuardBase {
protected:
  C3LM_DEF(const LockableObject* const tog_object)        // lockable object the guard is operating on
  C3LM_DEF(const thread_object_state_t tog_success_state) // state to set in case of successful operation
  C3LM_DEF(bool                        tog_check_passed)  // whether setting initial state succeeded

  ThreadObjectGuardBase(const LockableObject* lo,
    thread_object_state_t entry_state, thread_object_state_t success_state)
  #if C3LM_ENABLED
    : tog_object(lo), tog_success_state(success_state) {
    tog_check_passed = Thread::set_object_state(lo, entry_state);
  }
  #else
  {}
  #endif // C3LM_ENABLED

public:
  bool check_passed() const { return C3LM_IF(tog_check_passed, true); }
};

/// Base class: guard for lockable object (quick mutex) operations that can fail (a three-state guard)
class ThreadObjectGuard3: public ThreadObjectGuardBase {
  C3LM_DEF(const thread_object_state_t tog_failure_state) // what state to set in case of failure
  C3LM_DEF(bool                        tog_success)       // current status of the operation itself

protected:
  ThreadObjectGuard3(const LockableObject* lo, thread_object_state_t entry_state,
    thread_object_state_t success_state, thread_object_state_t failure_state):
    ThreadObjectGuardBase(lo, entry_state, success_state)
  #if C3LM_ENABLED
    , tog_failure_state(failure_state) {
    tog_success = false;
  }
  #else
  {}
  #endif // C3LM_ENABLED

public:
  ThreadObjectGuard3(const ThreadObjectGuard3&) = delete;
  ThreadObjectGuard3(ThreadObjectGuard3&&) = delete;

  #if C3LM_ENABLED
  ~ThreadObjectGuard3() {
    if (tog_check_passed) {
      Thread::set_object_state(tog_object, tog_success ? tog_success_state : tog_failure_state);
    }
  }
  #endif // C3LM_ENABLED

  ThreadObjectGuard3& operator=(const ThreadObjectGuard3&) = delete;
  ThreadObjectGuard3& operator=(ThreadObjectGuard3&&) = delete;

  void set_success() { C3LM_ON(tog_success = true); }
};

/// Guard for locking objects (quick mutexes) without waiting
class ThreadObjectTryAcquireGuard: public ThreadObjectGuard3 {
public:
  explicit ThreadObjectTryAcquireGuard(const LockableObject* lo):
    ThreadObjectGuard3(lo, LOS_BEGIN_TRY_LOCK, LOS_ACQUIRED_LOCK, LOS_LOCK_FAILED) {}
};

/// Guard for acquiring spin locks
class ThreadSpinLockAcquireGuard: public ThreadObjectGuardBase {
public:
  explicit ThreadSpinLockAcquireGuard(const LockableObject* lo):
    ThreadObjectGuardBase(lo, LOS_BEGIN_LOCK, LOS_ACQUIRED_LOCK /* set member won't be used */) {}

  ThreadSpinLockAcquireGuard(const ThreadSpinLockAcquireGuard&) = delete;
  ThreadSpinLockAcquireGuard(ThreadSpinLockAcquireGuard&&) = delete;

  #if C3LM_ENABLED
  ~ThreadSpinLockAcquireGuard() {
    if (tog_check_passed) {
      Thread::set_object_state(tog_object, LOS_ACQUIRED_LOCK);
    }
  }
  #endif // C3LM_ENABLED

  ThreadSpinLockAcquireGuard& operator=(const ThreadSpinLockAcquireGuard&) = delete;
  ThreadSpinLockAcquireGuard& operator=(ThreadSpinLockAcquireGuard&&) = delete;
};

/// Guard for releasing object locks (quick mutexes)
class ThreadObjectReleaseGuard: public ThreadObjectGuardBase {
public:
  explicit ThreadObjectReleaseGuard(const LockableObject* ho):
    ThreadObjectGuardBase(ho, LOS_BEGIN_UNLOCK, LOS_UNLOCKED /* set member won't be used */) {}

  ThreadObjectReleaseGuard(const ThreadObjectReleaseGuard&) = delete;
  ThreadObjectReleaseGuard(ThreadObjectReleaseGuard&&) = delete;

  #if C3LM_ENABLED
  ~ThreadObjectReleaseGuard() {
    if (tog_check_passed) {
      Thread::set_object_state(tog_object, LOS_UNLOCKED);
    }
  }
  #endif // C3LM_ENABLED

  ThreadObjectReleaseGuard& operator=(const ThreadObjectReleaseGuard&) = delete;
  ThreadObjectReleaseGuard& operator=(ThreadObjectReleaseGuard&&) = delete;
};

///////////////////////////////////////////////////////////////////////////////
// THREAD-LEVEL MESSAGE QUEUE GUARDS
///////////////////////////////////////////////////////////////////////////////

/// Base class for all message queue guards
class ThreadMessageQueueGuardBase {
protected:
  C3LM_DEF(const SyncObject* const tmqg_queue)        // message queue the guard is operating on
  C3LM_DEF(bool                    tmqg_check_passed) // whether setting initial state succeeded

  ThreadMessageQueueGuardBase(const SyncObject* queue, thread_queue_state_t entry_state)
  #if C3LM_ENABLED
    : tmqg_queue(queue) {
    tmqg_check_passed = Thread::set_queue_state(queue, entry_state);
  }
  #else
  {}
  #endif // C3LM_ENABLED

public:
  ThreadMessageQueueGuardBase(const ThreadMessageQueueGuardBase&) = delete;
  ThreadMessageQueueGuardBase(ThreadMessageQueueGuardBase&&) = delete;

  #if C3LM_ENABLED
  ~ThreadMessageQueueGuardBase() {
    if (tmqg_check_passed) {
      Thread::set_queue_state(tmqg_queue, MQS_UNUSED);
    }
  }
  #endif // C3LM_ENABLED

  ThreadMessageQueueGuardBase& operator=(const ThreadMessageQueueGuardBase&) = delete;
  ThreadMessageQueueGuardBase& operator=(ThreadMessageQueueGuardBase&&) = delete;

  bool check_passed() const { return C3LM_IF(tmqg_check_passed, true); }
};

/// Guard for adding new elements to a message queue
class ThreadMessageQueuePutGuard: public ThreadMessageQueueGuardBase {
public:
  explicit ThreadMessageQueuePutGuard(const SyncObject* queue):
    ThreadMessageQueueGuardBase(queue, MQS_IN_PUT) {}
};

/// Guard for retrieving existing elements from a message queue without waiting
class ThreadMessageQueueTryGetGuard: public ThreadMessageQueueGuardBase {
public:
  explicit ThreadMessageQueueTryGetGuard(const SyncObject* queue):
    ThreadMessageQueueGuardBase(queue, MQS_IN_TRY_GET) {}
};

/// Guard for retrieving existing elements from a message queue
class ThreadMessageQueueGetGuard: public ThreadMessageQueueGuardBase {
public:
  explicit ThreadMessageQueueGetGuard(const SyncObject* queue):
    ThreadMessageQueueGuardBase(queue, MQS_IN_GET) {}
};

/// Guard for retrieving current queue capacity
class ThreadMessageQueueGetCapacityGuard: public ThreadMessageQueueGuardBase {
public:
  explicit ThreadMessageQueueGetCapacityGuard(const SyncObject* queue):
    ThreadMessageQueueGuardBase(queue, MQS_IN_GET_CAPACITY) {}
};

/// Guard for retrieving current maximum queue capacity
class ThreadMessageQueueGetMaxCapacityGuard: public ThreadMessageQueueGuardBase {
public:
  explicit ThreadMessageQueueGetMaxCapacityGuard(const SyncObject* queue):
    ThreadMessageQueueGuardBase(queue, MQS_IN_GET_MAX_CAPACITY) {}
};

/// Guard for setting new queue capacity
class ThreadMessageQueueSetCapacityGuard: public ThreadMessageQueueGuardBase {
public:
  explicit ThreadMessageQueueSetCapacityGuard(const SyncObject* queue):
    ThreadMessageQueueGuardBase(queue, MQS_IN_SET_CAPACITY) {}
};

/// Guard for setting new maximum queue capacity
class ThreadMessageQueueSetMaxCapacityGuard: public ThreadMessageQueueGuardBase {
public:
  explicit ThreadMessageQueueSetMaxCapacityGuard(const SyncObject* queue):
    ThreadMessageQueueGuardBase(queue, MQS_IN_SET_MAX_CAPACITY) {}
};

}

#endif // _MT_THREAD_GUARDS_H
