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
 * Definition of the PHP extension interface.
 */
#ifndef _EXT_FUNCTIONS_H
#define _EXT_FUNCTIONS_H

// ID of the request being processed by the extension
extern thread_local c3_uint_t c3_request_id;

// object handling persistent connections
extern thread_local Socket c3_request_socket;

extern const zend_function_entry cybercache_functions[];

#endif // _EXT_FUNCTIONS_H
