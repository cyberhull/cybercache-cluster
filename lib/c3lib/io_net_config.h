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
 * Configuration parameters for network I/O.
 */
#ifndef _IO_NET_CONFIG_H
#define _IO_NET_CONFIG_H

#include "c3_types.h"
#include "c3_compressor.h"

namespace CyberCache {

/// Default port number used by the server
constexpr c3_ushort_t C3_DEFAULT_PORT = 8120;

/**
 * Configuration options for network I/O.
 *
 * Most of these options are not implemented as explicitly atomic, as that's not needed: password hash codes
 * are set only once, when threads that may access them are not even started, and most remaining options are
 * byte-sized (and the exact timing of changes taking effect is not important). The only exception is compression
 * threshold, which is dword-sized.
 */
class NetworkConfiguration {
  static constexpr c3_uint_t DEFAULT_COMPRESSION_THRESHOLD = 2048;

  c3_hash_t        nc_user_password;            // hash code of user password
  c3_hash_t        nc_admin_password;           // hash code of administrative password
  c3_hash_t        nc_bulk_password;            // hash code of bulk (replication and binlog) password
  std::atomic_uint nc_compression_threshold;    // smallest buffer size eligible for compression (attempt)
  c3_compressor_t  nc_global_compressor;        // compressor for payload chunks in global domain
  c3_compressor_t  nc_session_compressor;       // compressor for payload chunks in session domain
  c3_compressor_t  nc_fpc_compressor;           // compressor for payload chunks in FPC domain
  bool             nc_command_integrity_check;  // whether to use integrity check marker in commands
  bool             nc_response_integrity_check; // whether to use integrity check marker in responses
  bool             nc_file_integrity_check;     // whether to store IC marker in binlog files
  static bool      nc_sync_io;                  // whether I/O is synchronous (blocking) or not

  static void set_password(c3_hash_t& hash, const char* password, c3_uint_t length);
  static void set_password(c3_hash_t& hash, const char* password);

public:
  NetworkConfiguration() noexcept;
  NetworkConfiguration(const NetworkConfiguration&) = delete;
  NetworkConfiguration(NetworkConfiguration&&) = delete;
  NetworkConfiguration(c3_hash_t user_password, c3_hash_t admin_password,
    c3_compressor_t global_compressor, c3_uint_t compression_threshold, bool command_integrity_check);
  ~NetworkConfiguration() = default;

  void operator=(NetworkConfiguration&) = delete;
  void operator=(NetworkConfiguration&&) = delete;

  c3_hash_t get_user_password() const { return nc_user_password; }
  void set_raw_user_password(c3_hash_t hash) { nc_user_password = hash; }
  void set_user_password(const char* password) { set_password(nc_user_password, password); };

  c3_hash_t get_admin_password() const { return nc_admin_password; }
  void set_raw_admin_password(c3_hash_t hash) { nc_admin_password = hash; }
  void set_admin_password(const char* password) { set_password(nc_admin_password, password); }

  c3_hash_t get_bulk_password() const { return nc_bulk_password; }
  void set_raw_bulk_password(c3_hash_t hash) { nc_bulk_password = hash; }
  void set_bulk_password(const char* password) { set_password(nc_bulk_password, password); }

  c3_uint_t get_compression_threshold() const {
    return nc_compression_threshold.load(std::memory_order_relaxed);
  }
  void set_compression_threshold(c3_uint_t threshold) {
    nc_compression_threshold.store(threshold, std::memory_order_relaxed);
  }

  c3_compressor_t get_compressor(domain_t domain) const;
  void set_compressor(domain_t domain, c3_compressor_t compressor);

  bool get_command_integrity_check() const { return nc_command_integrity_check; }
  void set_command_integrity_check(bool use) { nc_command_integrity_check = use; }

  bool get_response_integrity_check() const { return nc_response_integrity_check; }
  void set_response_integrity_check(bool use) { nc_response_integrity_check = use; }

  bool get_file_integrity_check() const { return nc_file_integrity_check; }
  void set_file_integrity_check(bool use) { nc_file_integrity_check = use; }

  static bool get_sync_io() { return nc_sync_io; }
  static void set_sync_io(bool blocking) { nc_sync_io = blocking; }
};

} // CyberCache

#endif // _IO_NET_CONFIG_H
