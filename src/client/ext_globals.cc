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
#include "regex_matcher.h"
#include "ext_globals.h"

ZEND_DECLARE_MODULE_GLOBALS(cybercache)

static void init_domain_globals(domain_options_t* opt) {
  opt->do_address = INVALID_IPV4_ADDRESS;
  opt->do_port = C3_DEFAULT_PORT_VALUE;
  opt->do_compressor = CT_NONE;
  opt->do_marker = false;
  opt->do_admin = INVALID_HASH_VALUE;
  opt->do_user = INVALID_HASH_VALUE;
  opt->do_hasher = HM_INVALID;
  opt->do_persistent = true;
  opt->do_threshold = C3_DEFAULT_THRESHOLD_VALUE;
}

void c3_init_globals(zend_cybercache_globals* mg) {
  init_domain_globals(&mg->mg_session);
  init_domain_globals(&mg->mg_fpc);
  mg->mg_bot_regex = nullptr;
}

/*
 * INI file handlers for the entries defined by CyberCache extension.
 *
 * We have separate entries for session and FPC passwords because we need to know what hash algorithm to
 * use for the entry.
 */
ZEND_INI_MH(c3UpdateBool) {
  #ifndef ZTS
  auto base = (char*) mh_arg2;
  #else
  auto base = (char*) ts_resource(*((int*) mh_arg2));
  #endif

  auto field = (bool*)(base + (size_t) mh_arg1);
  *field = get_boolean_option(new_value);
  return SUCCESS;
}

ZEND_INI_MH(c3UpdateAddress) {
  c3_ipv4_t ip = get_address_option(new_value);
  if (ip != INVALID_IPV4_ADDRESS) {
    #ifndef ZTS
    auto base = (char*) mh_arg2;
    #else
    auto base = (char*) ts_resource(*((int*) mh_arg2));
    #endif

    auto field = (c3_ipv4_t*)(base + (size_t) mh_arg1);
    *field = ip;
    return SUCCESS;
  }
  return FAILURE;
}

ZEND_INI_MH(c3UpdatePort) {
  c3_ushort_t port = get_port_option(new_value);
  if (port != 0) {
    #ifndef ZTS
    auto base = (char*) mh_arg2;
    #else
    auto base = (char*) ts_resource(*((int*) mh_arg2));
    #endif

    auto field = (c3_ushort_t*)(base + (size_t) mh_arg1);
    *field = port;
    return SUCCESS;
  }
  return FAILURE;
}

ZEND_INI_MH(c3UpdateThreshold) {
  c3_uint_t threshold = get_threshold_option(new_value);
  if (threshold != 0) {
    #ifndef ZTS
    auto base = (char*) mh_arg2;
    #else
    auto base = (char*) ts_resource(*((int*) mh_arg2));
    #endif

    auto field = (c3_uint_t*)(base + (size_t) mh_arg1);
    *field = threshold;
    return SUCCESS;
  }
  return FAILURE;
}

ZEND_INI_MH(c3UpdateCompressor) {
  c3_compressor_t compressor = get_compressor_option(new_value);
  if (compressor != CT_NONE) {
    #ifndef ZTS
    auto base = (char*) mh_arg2;
    #else
    auto base = (char*) ts_resource(*((int*) mh_arg2));
    #endif

    auto field = (c3_compressor_t*)(base + (size_t) mh_arg1);
    *field = compressor;
    return SUCCESS;
  }
  return FAILURE;
}

ZEND_INI_MH(c3UpdateSessionPassword) {
  #ifndef ZTS
  auto base = (char*) mh_arg2;
  #else
  auto base = (char*) ts_resource(*((int*) mh_arg2));
  #endif

  c3_hash_t hash = get_password_option(new_value, false, HM_INVALID);
  auto field = (c3_hash_t*)(base + (size_t) mh_arg1);
  *field = hash;
  return SUCCESS;
}

ZEND_INI_MH(c3UpdateFPCPassword) {
  #ifndef ZTS
  auto base = (char*) mh_arg2;
  #else
  auto base = (char*) ts_resource(*((int*) mh_arg2));
  #endif

  c3_hash_t hash = get_password_option(new_value, true, HM_INVALID);
  auto field = (c3_hash_t*)(base + (size_t) mh_arg1);
  *field = hash;
  return SUCCESS;
}

ZEND_INI_MH(c3UpdateHasher) {
  c3_hash_method_t hasher = get_hasher_option(new_value);
  if (hasher != HM_INVALID) {
    #ifndef ZTS
    auto base = (char*) mh_arg2;
    #else
    auto base = (char*) ts_resource(*((int*) mh_arg2));
    #endif

    auto field = (c3_hash_method_t*)(base + (size_t) mh_arg1);
    *field = hasher;
    return SUCCESS;
  }
  return FAILURE;
}

ZEND_INI_MH(c3UpdateRegex) {
  char* pattern = new_value != nullptr? ZSTR_VAL(new_value): nullptr;
  if (regex_compile(pattern)) {
    #ifndef ZTS
    auto base = (char*) mh_arg2;
    #else
    auto base = (char*) ts_resource(*((int*) mh_arg2));
    #endif

    auto field = (char**)(base + (size_t) mh_arg1);
    *field = pattern;
    return SUCCESS;
  }
  return FAILURE;
}

ZEND_INI_MH(c3UpdateInfoPassword) {
  info_password_t info_pass = get_info_pass_option(new_value);
  if (info_pass != IPD_INVALID) {
    #ifndef ZTS
    auto base = (char*) mh_arg2;
    #else
    auto base = (char*) ts_resource(*((int*) mh_arg2));
    #endif

    auto field = (info_password_t*)(base + (size_t) mh_arg1);
    *field = info_pass;
    return SUCCESS;
  }
  return FAILURE;
}
