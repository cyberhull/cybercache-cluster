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

#if !C3_INSTRUMENTED
#error "Profiler definitions should only be included in instrumented builds"
#endif // C3_INSTRUMENTED

/*
 * it's not included from within "c3_profiler_defs.h" if `C3_PERF_GENERATE_DEFINITIONS` is defined (because
 * we'd end up with class definitions in "double" `CyberCache` namespace)
 */
#include "c3_profiler.h"

// generate instances of performance counters
#define C3_PERF_GENERATE_DEFINITIONS 1
#include "c3_profiler_defs.h"
