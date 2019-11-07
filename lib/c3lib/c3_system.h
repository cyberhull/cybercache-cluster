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
 * Retrievan of various system settings.
 */
#ifndef _C3_SYSTEM_H
#define _C3_SYSTEM_H

#include "c3_types.h"

namespace CyberCache {

/*
 * Unlike other system functions (e.g. those working with files), these functions return zero if there is
 * an error (in which case they do set "last error" that can be retrieved using `c3_error` functions).
 */
c3_ulong_t c3_get_total_memory() C3_FUNC_COLD;
c3_ulong_t c3_get_available_memory() C3_FUNC_COLD;
c3_uint_t c3_get_num_cpus() C3_FUNC_COLD;

} // CyberCache

#endif // _C3_SYSTEM_H
