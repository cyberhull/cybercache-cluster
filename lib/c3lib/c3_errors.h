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
 * Utilities for reporting errors.
 */
#ifndef _C3_ERRORS_H
#define _C3_ERRORS_H

namespace CyberCache {

const char* c3_get_error_message() C3_FUNC_COLD;
int c3_set_error_message(const char *format, ...) C3_FUNC_COLD C3_FUNC_PRINTF(1);
int c3_set_error_message(int code) C3_FUNC_COLD;
int c3_set_einval_error_message() C3_FUNC_COLD;
int c3_set_stdlib_error_message() C3_FUNC_COLD;
int c3_set_gai_error_message(int code) C3_FUNC_COLD;

}

#endif // _C3_ERRORS_H
