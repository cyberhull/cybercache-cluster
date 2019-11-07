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
 * Structures and data implementing server connection parameters.
 */
#ifndef _EXT_RESOURCE_H
#define _EXT_RESOURCE_H

/// Name of the resource for PHP/Zend functions
extern const char C3_RESOURCE_NAME[];

/// Resource ID / handle
extern int le_cybercache_res;

/**
 * Container for connection parameters and result.
 *
 * Both session and FPC connection use absolutely identical structures except for the boolean flag that
 * is used for sanity checks.
 *
 * Since this structure is allocated using Zend memory management functions, we `initialize()` and
 * `cleanup()` methods are used instead of ctors/dtor.
 */
class C3Resource {
  c3_ipv4_t        rc_address;    // IP address to connect to
  c3_ushort_t      rc_port;       // connection port number (session server)
  c3_compressor_t  rc_compressor; // compression algorithm (session server)
  bool             rc_marker;     // whether to send integrity check marker (session server)
  c3_hash_t        rc_admin;      // administrative password hash (session server)
  c3_hash_t        rc_user;       // user-level password hash (session server)
  c3_uint_t        rc_threshold;  // do not compress buffers smaller than this
  c3_hash_method_t rc_hasher;     // hash algorithm for passwords (session server)
  user_agent_t     rc_user_agent; // deduced user agent type for current request
  bool             rc_persistent; // `true` if uses persistent server connections
  bool             rc_is_fpc;     // `true` if created by `c3_fpc()`, `false` if by `c3_session()`
  char*            rc_last_error; // last error message returned by the *server*, or NULL

  void initialize(bool is_fpc);

public:
  static C3Resource* create(HashTable* ht, const char* domain, bool is_fpc);

  c3_ipv4_t get_address() const { return rc_address; }
  c3_ushort_t get_port() const { return rc_port; }
  bool is_persistent() const { return rc_persistent; }
  c3_compressor_t get_compressor() const { return rc_compressor; }
  c3_uint_t get_threshold() const { return rc_threshold; }
  bool get_marker() const { return rc_marker; }
  c3_hash_t get_admin_password() const { return rc_admin; }
  c3_hash_t get_user_password() const { return rc_user; }
  c3_hash_method_t get_hasher() const { return rc_hasher; }
  user_agent_t get_user_agent() const { return rc_user_agent; }

  const char* get_error_message() const { return rc_last_error; }
  void set_error_message(const char* message, size_t length);
  void reset_error_message();
  void cleanup() { reset_error_message(); }
};

void register_cybercache_resource(int module_number) C3_FUNC_COLD;

#endif // _EXT_RESOURCE_H
