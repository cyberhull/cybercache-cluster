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
#include "ext_errors.h"
#include "ext_globals.h"

bool get_boolean_option(const zend_string* zstr) {
  int length = (int) ZSTR_LEN(zstr);
  const char* value = ZSTR_VAL(zstr);
  /*
   * We follow Zend semantics here:
   *
   * 1) have to compare string length because the string is binary, and it might as well be some long
   *    string that happens to start with proper character followed by NUL byte,
   *
   * 2) anything that is not "on", "yes", "true", or a non-zero integer is considered `false`.
   */
  if ((length == 2 && strcasecmp(value, "on") == 0) ||
    (length == 3 && strcasecmp(value, "yes") == 0) ||
    (length == 4 && strcasecmp(value, "true") == 0)) {
    return true;
  }
  return zend_atoi(value, length) != 0;
}

c3_ipv4_t get_address_option(const zend_string* zstr) {
  c3_ipv4_t address = c3_resolve_host(ZSTR_VAL(zstr));
  if (address == INVALID_IPV4_ADDRESS) {
    report_error("Could not resolve address: '%.*s'", (int) ZSTR_LEN(zstr), ZSTR_VAL(zstr));
  }
  return address;
}

c3_ushort_t get_port_option(const zend_string* zstr) {
  int port = zend_atoi(ZSTR_VAL(zstr), (int) ZSTR_LEN(zstr));
  if (port < 1000 || port > 65535) {
    report_error("Port number not in [1000..65535] range: '%.*s'", (int) ZSTR_LEN(zstr), ZSTR_VAL(zstr));
    port = 0;
  }
  return (c3_ushort_t) port;
}

c3_uint_t get_threshold_option(const zend_string* zstr) {
  zend_long threshold = zend_atol(ZSTR_VAL(zstr), (int) ZSTR_LEN(zstr));
  if (threshold < 1 || threshold > UINT_MAX_VAL) {
    report_error("Compression threshold not in [1..4294967295] range: '%.*s'", (int) ZSTR_LEN(zstr), ZSTR_VAL(zstr));
    threshold = 0;
  }
  return (c3_ushort_t) threshold;
}

c3_compressor_t get_compressor_option(const zend_string* zstr) {
  static const char* const compressors[] = {
    nullptr, // never matched against input
    "lzf",
    "snappy",
    "lz4",
    "lzss3",
    "brotli",
    "zstd",
    "zlib",
    "lzham"
  };
  static_assert(CT_NUMBER_OF_ELEMENTS == 9, "Number of compressors has changed");
  const char* compressor = ZSTR_VAL(zstr);
  unsigned int i = 1; do {
    if (std::strcmp(compressor, compressors[i]) == 0) {
      auto comp_type = (c3_compressor_t) i;
      #if !C3_ENTERPRISE
      if (comp_type == CT_BROTLI) {
        report_error("The 'brotli' compressor is only supported in Enterprise edition");
        return CT_NONE;
      }
      #endif
      return comp_type;
    }
  } while (++i < CT_NUMBER_OF_ELEMENTS);
  report_error("Unknown compressor: '%.*s'", (int) ZSTR_LEN(zstr), compressor);
  return CT_NONE;
}

c3_hash_t get_password_option(const zend_string* zstr, bool fpc, c3_hash_method_t method) {
  size_t length = ZSTR_LEN(zstr);
  if (length > 0) {
    if (method == HM_INVALID) {
      method = fpc? C3GLOBAL(mg_fpc.do_hasher): C3GLOBAL(mg_session.do_hasher);
    }
    PasswordHasher hasher(method);
    return hasher.hash(ZSTR_VAL(zstr), length);
  }
  return INVALID_HASH_VALUE;
}

c3_hash_method_t get_hasher_option(const zend_string* zstr) {
  static const char* const compressors[] = {
    nullptr, // never matched against input
    "xxhash",
    "farmhash",
    "spookyhash",
    "murmurhash2",
    "murmurhash3"
  };
  static_assert(HM_NUMBER_OF_ELEMENTS == 6, "Number of hash methods has changed");
  const char* hasher = ZSTR_VAL(zstr);
  unsigned int i = 1; do {
    if (std::strcmp(hasher, compressors[i]) == 0) {
      return (c3_hash_method_t) i;
    }
  } while (++i < HM_NUMBER_OF_ELEMENTS);
  report_error("Unknown hash method: '%.*s'", (int) ZSTR_LEN(zstr), hasher);
  return HM_INVALID;
}

info_password_t get_info_pass_option(const zend_string* zstr) {
  static const char* const auth_levels[] = {
    nullptr, // never matched against input
    "none",
    "user",
    "admin"
  };
  const char* info_auth = ZSTR_VAL(zstr);
  unsigned int i = 1; do {
    if (std::strcmp(info_auth, auth_levels[i]) == 0) {
      return (info_password_t) i;
    }
  } while (++i < IPD_NUMBER_OF_ELEMENTS);
  report_error("Unknown info password mode: '%.*s'", (int) ZSTR_LEN(zstr), info_auth);
  return IPD_INVALID;
}
