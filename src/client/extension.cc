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
#include "external_apis.h"
#include "option_utils.h"
#include "extension.h"
#include "ext_globals.h"
#include "ext_functions.h"
#include "ext_resource.h"
#include "regex_matcher.h"

#include "php_ini.h"
// needed for module information printing (PHP_MINFO_FUNCTION(cybercache))
#include "ext/standard/info.h"

// configuration file entries
PHP_INI_BEGIN()
  STD_PHP_INI_ENTRY("c3.session_address", "127.0.0.1", PHP_INI_ALL, c3UpdateAddress,
    mg_session.do_address, zend_cybercache_globals, cybercache_globals)
  STD_PHP_INI_ENTRY("c3.session_port", "8120", PHP_INI_ALL, c3UpdatePort,
    mg_session.do_port, zend_cybercache_globals, cybercache_globals)
  STD_PHP_INI_ENTRY("c3.session_persistent", "on", PHP_INI_ALL, c3UpdateBool,
    mg_session.do_persistent, zend_cybercache_globals, cybercache_globals)
  STD_PHP_INI_ENTRY("c3.session_compressor", "snappy", PHP_INI_ALL, c3UpdateCompressor,
    mg_session.do_compressor, zend_cybercache_globals, cybercache_globals)
  STD_PHP_INI_ENTRY("c3.session_marker", "on", PHP_INI_ALL, c3UpdateBool,
    mg_session.do_marker, zend_cybercache_globals, cybercache_globals)
  STD_PHP_INI_ENTRY("c3.session_admin", "", PHP_INI_ALL, c3UpdateSessionPassword,
    mg_session.do_admin, zend_cybercache_globals, cybercache_globals)
  STD_PHP_INI_ENTRY("c3.session_user", "", PHP_INI_ALL, c3UpdateSessionPassword,
    mg_session.do_user, zend_cybercache_globals, cybercache_globals)
  STD_PHP_INI_ENTRY("c3.session_hasher", "murmurhash2", PHP_INI_ALL, c3UpdateHasher,
    mg_session.do_hasher, zend_cybercache_globals, cybercache_globals)
  STD_PHP_INI_ENTRY("c3.session_threshold", "256", PHP_INI_ALL, c3UpdateThreshold,
    mg_session.do_threshold, zend_cybercache_globals, cybercache_globals)

  STD_PHP_INI_ENTRY("c3.fpc_address", "127.0.0.1", PHP_INI_ALL, c3UpdateAddress,
    mg_fpc.do_address, zend_cybercache_globals, cybercache_globals)
  STD_PHP_INI_ENTRY("c3.fpc_port", C3_DEFAULT_PORT_STRING, PHP_INI_ALL, c3UpdatePort,
    mg_fpc.do_port, zend_cybercache_globals, cybercache_globals)
  STD_PHP_INI_ENTRY("c3.fpc_persistent", "on", PHP_INI_ALL, c3UpdateBool,
    mg_fpc.do_persistent, zend_cybercache_globals, cybercache_globals)
  STD_PHP_INI_ENTRY("c3.fpc_compressor", "snappy", PHP_INI_ALL, c3UpdateCompressor,
    mg_fpc.do_compressor, zend_cybercache_globals, cybercache_globals)
  STD_PHP_INI_ENTRY("c3.fpc_marker", "on", PHP_INI_ALL, c3UpdateBool,
    mg_fpc.do_marker, zend_cybercache_globals, cybercache_globals)
  STD_PHP_INI_ENTRY("c3.fpc_admin", "", PHP_INI_ALL, c3UpdateFPCPassword,
    mg_fpc.do_admin, zend_cybercache_globals, cybercache_globals)
  STD_PHP_INI_ENTRY("c3.fpc_user", "", PHP_INI_ALL, c3UpdateFPCPassword,
    mg_fpc.do_user, zend_cybercache_globals, cybercache_globals)
  STD_PHP_INI_ENTRY("c3.fpc_hasher", "murmurhash2", PHP_INI_ALL, c3UpdateHasher,
    mg_fpc.do_hasher, zend_cybercache_globals, cybercache_globals)
	STD_PHP_INI_ENTRY("c3.fpc_threshold", C3_DEFAULT_THRESHOLD_STRING, PHP_INI_ALL, c3UpdateThreshold,
		mg_fpc.do_threshold, zend_cybercache_globals, cybercache_globals)

  STD_PHP_INI_ENTRY("c3.bot_regex", C3_DEFAULT_BOT_REGEX, PHP_INI_SYSTEM, c3UpdateRegex,
    mg_bot_regex, zend_cybercache_globals, cybercache_globals)
  STD_PHP_INI_ENTRY("c3.info_password_type", "none", PHP_INI_ALL, c3UpdateInfoPassword,
		mg_info_password, zend_cybercache_globals, cybercache_globals)
PHP_INI_END()

// module initialization callback
PHP_MINIT_FUNCTION(cybercache) C3_FUNC_COLD;
PHP_MINIT_FUNCTION(cybercache) {
  ZEND_INIT_MODULE_GLOBALS(cybercache, c3_init_globals, nullptr);
  NetworkConfiguration::set_sync_io(true); // use blocking I/O
  regex_init();
  REGISTER_INI_ENTRIES();
  REGISTER_LONG_CONSTANT("C3_DOMAIN_GLOBAL",  DM_GLOBAL,    CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("C3_DOMAIN_SESSION", DM_SESSION,   CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("C3_DOMAIN_FPC",     DM_FPC,       CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("C3_DOMAIN_ALL",     DM_ALL,       CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("C3_UA_UNKNOWN",     UA_UNKNOWN,   CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("C3_UA_BOT",         UA_BOT,       CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("C3_UA_WARMER",      UA_WARMER,    CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("C3_UA_USER",        UA_USER,      CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("C3_SYNC_NONE",      SM_NONE,      CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("C3_SYNC_DATA_ONLY", SM_DATA_ONLY, CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("C3_SYNC_FULL",      SM_FULL,      CONST_CS | CONST_PERSISTENT);
  register_cybercache_resource(module_number);
	return SUCCESS;
}

// module cleanup callback
PHP_MSHUTDOWN_FUNCTION(cybercache) C3_FUNC_COLD;
PHP_MSHUTDOWN_FUNCTION(cybercache) {
	UNREGISTER_INI_ENTRIES();
  regex_cleanup();
	return SUCCESS;
}

// request initialization callback
PHP_RINIT_FUNCTION(cybercache) {
#ifdef ZTS
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	// generate non-zero request ID for session reads and writes
	static c3_uint_t request_id;
	do request_id++; while (request_id == 0);
	c3_request_id = request_id;

	return SUCCESS;
}

// request cleanup callback
PHP_RSHUTDOWN_FUNCTION(cybercache) {
	c3_request_socket.disconnect(true);
	return SUCCESS;
}

// handler of the module information request (e.g. used by `php -i`)
PHP_MINFO_FUNCTION(cybercache) C3_FUNC_COLD;
PHP_MINFO_FUNCTION(cybercache) {
	php_info_print_table_start();
	php_info_print_table_header(2, "CyberCache support", "enabled");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}

// module information
zend_module_entry cybercache_module_entry = {
	STANDARD_MODULE_HEADER,
	"cybercache",
	cybercache_functions,
	PHP_MINIT(cybercache),
	PHP_MSHUTDOWN(cybercache),
	PHP_RINIT(cybercache),
	PHP_RSHUTDOWN(cybercache),
	PHP_MINFO(cybercache),
	C3_VERSION_STRING,
	STANDARD_MODULE_PROPERTIES
};

/*
 * The following macro defines `extern "C"` function `get_module()` that returns address of an instance
 * of structure of type `zend_module_entry`, which is assumed to be named `<macro-argument>_module_entry`.
 */
ZEND_GET_MODULE(cybercache)
