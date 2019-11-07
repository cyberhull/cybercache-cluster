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
 * Multithreading support: spinlock and its guard.
 */
#ifndef _MT_SPINLOCK_H
#define _MT_SPINLOCK_H

#include "c3lib/c3lib.h"

namespace CyberCache {

/**
 * Simple spinlock; meant to be used for really short waits, so it does not employ any other
 * synchronization primitives while waiting.
 */
class SpinLock {
  std::atomic_bool sl_flag; // lock flag; `true` means "locked"

  #if C3_INSTRUMENTED
  #define PERF_SL_DOMAIN sl_domain
  const domain_t sl_domain; // memory domain (used only for stats in instrumented version)
  #else
  #define PERF_SL_DOMAIN
  #endif // C3_INSTRUMENTED

public:
  #if C3_INSTRUMENTED
  SpinLock(domain_t domain): sl_domain(domain) { sl_flag.store(false, std::memory_order_relaxed); }
  #else
  SpinLock() { sl_flag.store(false, std::memory_order_relaxed); }
  #endif

  bool is_unlocked() const { return !sl_flag.load(std::memory_order_acquire); }
  void lock();
  void release() { sl_flag.store(false, std::memory_order_release); }
};

class SpinLockGuard {
  SpinLock& slg_spinlock; // spinlock that we operate on

public:
  explicit SpinLockGuard(SpinLock& lock): slg_spinlock(lock) {
    lock.lock();
  }
  SpinLockGuard(const SpinLockGuard&) = delete;
  SpinLockGuard(SpinLockGuard&&) = delete;
  ~SpinLockGuard() { slg_spinlock.release(); }

  SpinLockGuard& operator=(const SpinLockGuard&) = delete;
  SpinLockGuard& operator=(SpinLockGuard&&) = delete;
};

} // CyberCache

#endif // _MT_SPINLOCK_H
