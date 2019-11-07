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
#include "mt_spinlock.h"

namespace CyberCache {

void CyberCache::SpinLock::lock() {
  PERF_DECLARE_LOCAL_INT_COUNT(num_waits)
  PERF_INCREMENT_VAR_DOMAIN_COUNTER(PERF_SL_DOMAIN, SpinLock_Acquisitions)
  while (sl_flag.exchange(true, std::memory_order_acq_rel)) {
    PERF_INCREMENT_VAR_DOMAIN_COUNTER(PERF_SL_DOMAIN, SpinLock_Total_Waits)
    PERF_INCREMENT_LOCAL_COUNT(num_waits)
  }
  PERF_UPDATE_VAR_DOMAIN_MAXIMUM(PERF_SL_DOMAIN, SpinLock_Max_Waits, PERF_LOCAL(num_waits))
}

} // CyberCache
