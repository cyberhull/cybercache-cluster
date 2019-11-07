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
 * This file should be the very first included from within any extension module.
 */
#ifndef _OPTION_UTILS_H
#define _OPTION_UTILS_H

/// Authentication levels required by information commands (`INFO`, `STATS`, etc.)
enum info_password_t {
  IPD_INVALID = 0, // an invalid level
  IPD_NONE,        // info commands do not require authentication (the default)
  IPD_USER,        // info commands require user-level authentication
  IPD_ADMIN,       // info commands require administrative authentication
  IPD_NUMBER_OF_ELEMENTS
};

bool get_boolean_option(const zend_string* zstr);
c3_ipv4_t get_address_option(const zend_string* zstr);
c3_ushort_t get_port_option(const zend_string* zstr);
c3_uint_t get_threshold_option(const zend_string* zstr);
c3_compressor_t get_compressor_option(const zend_string* zstr);
c3_hash_t get_password_option(const zend_string* zstr, bool fpc, c3_hash_method_t method);
c3_hash_method_t get_hasher_option(const zend_string* zstr);
info_password_t get_info_pass_option(const zend_string* zstr);

#endif // _OPTION_UTILS_H
