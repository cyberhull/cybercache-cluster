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
 * Global variables of the PHP extension.
 */
#ifndef _EXT_GLOBALS_H
#define _EXT_GLOBALS_H

extern zend_module_entry cybercache_module_entry;
#define phpext_cybercache_ptr &cybercache_module_entry

/// Default connection port
#define C3_DEFAULT_PORT_VALUE  8120
#define C3_DEFAULT_PORT_STRING C3_STRINGIFY(C3_DEFAULT_PORT_VALUE)

/// Default compression threshold for records to be stored in cache
#define C3_DEFAULT_THRESHOLD_VALUE  4096
#define C3_DEFAULT_THRESHOLD_STRING C3_STRINGIFY(C3_DEFAULT_THRESHOLD_VALUE)

/// Per-domain (session or FPC) options
struct domain_options_t {
  c3_ipv4_t        do_address;    // IP address to connect to
  c3_ushort_t      do_port;       // connection port number
  c3_compressor_t  do_compressor; // compression algorithm
  bool             do_marker;     // whether to send integrity check marker
  c3_hash_t        do_admin;      // administrative password hash
  c3_hash_t        do_user;       // user-level password hash
  c3_hash_method_t do_hasher;     // hash algorithm for passwords
  bool             do_persistent; // whether server connections are persistent
  c3_uint_t        do_threshold;  // minimum buffer size eligible for compression
};

// module globals: represent default values for respective options
ZEND_BEGIN_MODULE_GLOBALS(cybercache)
  domain_options_t mg_session;       // parameters for the session server
  domain_options_t mg_fpc;           // parameters for the FPC server
  char*            mg_bot_regex;     // regular expression used to detect bots
  info_password_t  mg_info_password; // authentication level for information commands
ZEND_END_MODULE_GLOBALS(cybercache)

ZEND_EXTERN_MODULE_GLOBALS(cybercache)

// accessor for module globals
#define C3GLOBAL(var) ZEND_MODULE_GLOBALS_ACCESSOR(cybercache, var)

void c3_init_globals(zend_cybercache_globals* mg) C3_FUNC_COLD;

ZEND_INI_MH(c3UpdateBool) C3_FUNC_COLD;
ZEND_INI_MH(c3UpdateAddress) C3_FUNC_COLD;
ZEND_INI_MH(c3UpdatePort) C3_FUNC_COLD;
ZEND_INI_MH(c3UpdateThreshold) C3_FUNC_COLD;
ZEND_INI_MH(c3UpdateCompressor) C3_FUNC_COLD;
ZEND_INI_MH(c3UpdateSessionPassword) C3_FUNC_COLD;
ZEND_INI_MH(c3UpdateFPCPassword) C3_FUNC_COLD;
ZEND_INI_MH(c3UpdateHasher) C3_FUNC_COLD;
ZEND_INI_MH(c3UpdateRegex) C3_FUNC_COLD;
ZEND_INI_MH(c3UpdateInfoPassword) C3_FUNC_COLD;

#endif // _EXT_GLOBALS_H
