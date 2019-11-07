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
#include "mt_mutexes.h"
#include "mt_thread_guards.h"

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// SharedMutex
///////////////////////////////////////////////////////////////////////////////

SharedMutex::SharedMutex(domain_t domain, host_object_t host, c3_byte_t id):
  Mutex(domain, host, SO_SHARED_MUTEX, id) {
}

bool SharedMutex::lock_shared() {
  ThreadMutexSharedLockGuard guard(this);
  if (guard.check_passed()) {
    sm_mutex.lock_shared();
    guard.set_success();
    assert(!m_exclusive && m_num_readers < MAX_NUM_THREADS);
    m_num_readers++;
    return true;
  }
  return false;
}

bool SharedMutex::lock_exclusive() {
  ThreadMutexExclusiveLockGuard guard(this);
  if (guard.check_passed()) {
    sm_mutex.lock();
    guard.set_success();
    assert(!m_exclusive && m_num_readers == 0);
    m_exclusive = true;
    return true;
  }
  return false;
}

void SharedMutex::unlock_shared() {
  ThreadMutexSharedUnlockGuard guard(this);
  if (guard.check_passed()) {
    assert(!m_exclusive && m_num_readers > 0);
    m_num_readers--;
    sm_mutex.unlock_shared();
  }
}

void SharedMutex::unlock_exclusive() {
  ThreadMutexExclusiveUnlockGuard guard(this);
  if (guard.check_passed()) {
    assert(m_exclusive && m_num_readers == 0);
    m_exclusive = false;
    sm_mutex.unlock();
  }
}

///////////////////////////////////////////////////////////////////////////////
// DynamicMutex
///////////////////////////////////////////////////////////////////////////////

DynamicMutex::DynamicMutex(domain_t domain, host_object_t host, c3_byte_t id):
  Mutex(domain, host, SO_DOWNGRADABLE_MUTEX, id) {
}

bool DynamicMutex::lock_shared() {
  ThreadMutexSharedLockGuard guard(this);
  if (guard.check_passed()) {
    std::unique_lock<std::mutex> lock(dm_mutex);
    auto condition = [this]{ return !m_exclusive; };
    dm_notifier.wait(lock, condition);
    guard.set_success();
    assert(m_num_readers < MAX_NUM_THREADS);
    m_num_readers++;
    return true;
  }
  return false;
}

bool DynamicMutex::lock_exclusive() {
  ThreadMutexExclusiveLockGuard guard(this);
  if (guard.check_passed()) {
    std::unique_lock<std::mutex> lock(dm_mutex);
    auto condition = [this]{ return !m_exclusive && m_num_readers == 0; };
    dm_notifier.wait(lock, condition);
    guard.set_success();
    m_exclusive = true;
    return true;
  }
  return false;
}

bool DynamicMutex::downgrade_lock() {
  bool result = false;
  ThreadMutexDowngradeGuard guard(this);
  if (guard.check_passed()) {
    std::lock_guard<std::mutex> lock(dm_mutex);
    c3_assert(m_exclusive && m_num_readers == 0);
    m_num_readers = 1;
    m_exclusive = false;
    dm_notifier.notify_all();
    result = true;
    guard.set_success();
  }
  return result;
}

bool DynamicMutex::upgrade_lock(c3_uint_t msecs) {
  bool result = false;
  ThreadMutexUpgradeGuard guard(this);
  if (guard.check_passed()) {
    std::unique_lock<std::mutex> lock(dm_mutex);
    c3_assert(!m_exclusive && m_num_readers > 0);
    if (--m_num_readers == 0) {
      // got lucky: there were no other readers
      result = true;
    } else {
      auto condition = [this]{ return !m_exclusive && m_num_readers == 0; };
      if (msecs > 0) {
        result = dm_notifier.wait_for(lock, std::chrono::milliseconds(msecs), condition);
      } else {
        dm_notifier.wait(lock, condition);
        result = true;
      }
    }
    if (result) {
      guard.set_success();
      m_exclusive = true;
    }
  }
  return result;
}

void DynamicMutex::unlock_shared() {
  ThreadMutexSharedUnlockGuard guard(this);
  if (guard.check_passed()) {
    std::lock_guard<std::mutex> lock(dm_mutex);
    c3_assert(!m_exclusive && m_num_readers > 0);
    if (--m_num_readers == 0) {
      // notify waiting writers
      dm_notifier.notify_all();
    }
  }
}

void DynamicMutex::unlock_exclusive() {
  ThreadMutexExclusiveUnlockGuard guard(this);
  if (guard.check_passed()) {
    std::lock_guard<std::mutex> lock(dm_mutex);
    c3_assert(m_exclusive && m_num_readers == 0);
    m_exclusive = false;
    dm_notifier.notify_all();
  }
}

} // CyberCache
