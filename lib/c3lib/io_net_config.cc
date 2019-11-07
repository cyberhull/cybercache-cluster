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
#include "io_net_config.h"
#include "c3_hasher.h"

#include <cstring>

namespace CyberCache {

bool NetworkConfiguration::nc_sync_io = false;

NetworkConfiguration::NetworkConfiguration() noexcept {
  nc_user_password = INVALID_HASH_VALUE;
  nc_admin_password = INVALID_HASH_VALUE;
  nc_bulk_password = INVALID_HASH_VALUE;
  set_compression_threshold(DEFAULT_COMPRESSION_THRESHOLD);
  nc_global_compressor = CT_DEFAULT;
  nc_session_compressor = CT_DEFAULT;
  nc_fpc_compressor = CT_DEFAULT;
  nc_command_integrity_check = true;
  nc_response_integrity_check = false;
  nc_file_integrity_check = true;
}

NetworkConfiguration::NetworkConfiguration(c3_hash_t user_password, c3_hash_t admin_password,
  c3_compressor_t global_compressor, c3_uint_t compression_threshold, bool command_integrity_check) {
  nc_user_password = user_password;
  nc_admin_password = admin_password;
  nc_bulk_password = INVALID_HASH_VALUE;
  set_compression_threshold(compression_threshold);
  nc_global_compressor = global_compressor;
  nc_session_compressor = CT_DEFAULT;
  nc_fpc_compressor = CT_DEFAULT;
  nc_command_integrity_check = command_integrity_check;
  nc_response_integrity_check = false;
  nc_file_integrity_check = true;
}

void NetworkConfiguration::set_password(c3_hash_t& hash, const char* password, c3_uint_t length) {
  c3_assert(password); // password may be *not* '\0'-terminated
  if (length > 0) {
    hash = password_hasher.hash(password, length);
  } else {
    hash = INVALID_HASH_VALUE;
  }
}

void NetworkConfiguration::set_password(c3_hash_t& hash, const char* password) {
  if (password != nullptr) {
    size_t length = std::strlen(password);
    c3_assert(length < UINT_MAX_VAL);
    set_password(hash, password, (c3_uint_t) length);
  } else {
    hash = INVALID_HASH_VALUE;
  }
}

c3_compressor_t NetworkConfiguration::get_compressor(domain_t domain) const {
  switch (domain) {
    case DOMAIN_GLOBAL:
      return nc_global_compressor;
    case DOMAIN_SESSION:
      return nc_session_compressor;
    case DOMAIN_FPC:
      return nc_fpc_compressor;
    default:
      c3_assert_failure();
      return CT_NONE;
  }
}

void NetworkConfiguration::set_compressor(domain_t domain, c3_compressor_t compressor) {
  c3_assert(compressor > CT_NONE && compressor < CT_NUMBER_OF_ELEMENTS);
  switch (domain) {
    case DOMAIN_GLOBAL:
      nc_global_compressor = compressor;
      break;
    case DOMAIN_SESSION:
      nc_session_compressor = compressor;
      break;
    case DOMAIN_FPC:
      nc_fpc_compressor = compressor;
      break;
    default:
      c3_assert_failure();
  }
}

}
