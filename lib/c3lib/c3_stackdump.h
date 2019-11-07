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
 * Debugging facility: display and saving stack traces.
 */
#ifndef _C3_STACKDUMP_H
#define _C3_STACKDUMP_H

#include "c3_build.h"

#if C3_STACKDUMP_ENABLED

namespace CyberCache {

void c3_show_stackdump(bool include_caller) C3_FUNC_COLD;
bool c3_save_stackdump(const char* path, bool include_caller) C3_FUNC_COLD;

} // CyberCache

#endif // C3_STACKDUMP_ENABLED
#endif // _C3_STACKDUMP_H
