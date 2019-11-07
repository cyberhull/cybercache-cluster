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
#include "regex_matcher.h"
#include "option_utils.h"
#include "ext_errors.h"
#include "ext_globals.h"
#include "ext_resource.h"

const char C3_RESOURCE_NAME[] = "CyberCache";

int le_cybercache_res;

///////////////////////////////////////////////////////////////////////////////
// C3Resource methods
///////////////////////////////////////////////////////////////////////////////

C3Resource* C3Resource::create(HashTable* ht, const char* domain, bool is_fpc) {
  bool result = true;
  auto res = (C3Resource*) emalloc(sizeof(C3Resource));
  res->initialize(is_fpc);
  if (ht != nullptr) {
    HashPosition ht_pos;
    zend_hash_internal_pointer_reset_ex(ht, &ht_pos);
    for (;;) {
      zend_string* key_string;
      zend_ulong key_index;
      int key_type = zend_hash_get_current_key_ex(ht, &key_string, &key_index, &ht_pos);
      if (key_type == HASH_KEY_IS_STRING) {
        const zval* data = zend_hash_get_current_data_ex(ht, &ht_pos);
        if (data != nullptr && Z_TYPE_P(data) == IS_STRING) {
          const char* option = ZSTR_VAL(key_string);
          if (std::strcmp(option, "address") == 0) {
            res->rc_address = get_address_option(Z_STR_P(data));
            if (res->rc_address == INVALID_IPV4_ADDRESS) {
              result = false;
            }
          } else if (std::strcmp(option, "port") == 0) {
            res->rc_port = get_port_option(Z_STR_P(data));
            if (res->rc_port == 0) {
              result = false;
            }
          } else if (std::strcmp(option, "persistent") == 0) {
            res->rc_persistent = get_boolean_option(Z_STR_P(data));
          } else if (std::strcmp(option, "compressor") == 0) {
            res->rc_compressor = get_compressor_option(Z_STR_P(data));
            if (res->rc_compressor == CT_NONE) {
              result = false;
            }
          } else if (std::strcmp(option, "marker") == 0) {
            res->rc_marker = get_boolean_option(Z_STR_P(data));
          } else if (std::strcmp(option, "admin") == 0) {
            res->rc_admin = get_password_option(Z_STR_P(data), is_fpc, res->rc_hasher);
          } else if (std::strcmp(option, "user") == 0) {
            res->rc_user = get_password_option(Z_STR_P(data), is_fpc, res->rc_hasher);
          } else if (std::strcmp(option, "threshold") == 0) {
            res->rc_threshold = get_threshold_option(Z_STR_P(data));
            if (res->rc_threshold == 0) {
              result = false;
            }
          } else if (std::strcmp(option, "hasher") == 0) {
            res->rc_hasher = get_hasher_option(Z_STR_P(data));
            if (res->rc_hasher == HM_INVALID) {
              result = false;
            }
          } // unknown option: Magento must have added a default one
        } // a non-string or empty option: must have been added by Magento
      } else if (key_type == HASH_KEY_NON_EXISTENT) {
        // no more entries
        break;
      } else {
        report_error("c3_%s(): option array must have string keys", domain);
        result = false;
      }
      zend_hash_move_forward_ex(ht, &ht_pos);
    }
  }
  if (result) {
    return res;
  } else {
    efree(res);
    return nullptr;
  }
}

void C3Resource::initialize(bool is_fpc) {
  // set defaults
  domain_options_t* options;
  if (is_fpc) {
    options = &C3GLOBAL(mg_fpc);
    rc_is_fpc = true;
  } else {
    options = &C3GLOBAL(mg_session);
    rc_is_fpc = false;
  }
  rc_address = options->do_address;
  rc_port = options->do_port;
  rc_persistent = options->do_persistent;
  rc_compressor = options->do_compressor;
  rc_marker = options->do_marker;
  rc_admin = options->do_admin;
  rc_user = options->do_user;
  rc_threshold = options->do_threshold;
  rc_hasher = options->do_hasher;
  // figure out user agent type for the request
  rc_user_agent = UA_UNKNOWN;
  if (Z_TYPE(PG(http_globals)[TRACK_VARS_SERVER]) == IS_ARRAY
    || zend_is_auto_global_str(ZEND_STRL((char*)"_SERVER")) != 0) {
    const zval* http_user_agent = zend_hash_str_find(Z_ARRVAL_P(&PG(http_globals)[TRACK_VARS_SERVER]),
      "HTTP_USER_AGENT", sizeof("HTTP_USER_AGENT")-1);
    if (http_user_agent != nullptr) {
      const zend_string* agent_string = Z_STR_P(http_user_agent);
      const char* agent_name = ZSTR_VAL(agent_string);
      #if C3_ENTERPRISE
      // only Enterprise Edition of PHP extension recognizes CyberCache Warmer agent
      if (std::strstr(agent_name, C3_CACHE_WARMER_ID)) {
        rc_user_agent = UA_WARMER;
      } else
      #endif // C3_ENTERPRISE
      if (regex_match(agent_name)) {
        rc_user_agent = UA_BOT;
      } else {
        rc_user_agent = UA_USER;
      }
    }
  }
  rc_last_error = nullptr;
}

void C3Resource::set_error_message(const char* message, size_t length) {
  reset_error_message();
  if (message != nullptr && length > 0) {
    rc_last_error = (char*) emalloc(length + 1);
    std::memcpy(rc_last_error, message, length);
    rc_last_error[length] = 0;
  }
}

void C3Resource::reset_error_message() {
  if (rc_last_error != nullptr) {
    efree(rc_last_error);
    rc_last_error = nullptr;
  }
}

///////////////////////////////////////////////////////////////////////////////
// RESOURCE MANAGEMENT
///////////////////////////////////////////////////////////////////////////////

static ZEND_RSRC_DTOR_FUNC(cybercache_resource_dtor) {
  auto r = (C3Resource*) res->ptr;
  if (r != nullptr) {
    r->reset_error_message();
    efree(r);
  }
}

void register_cybercache_resource(int module_number) {
  le_cybercache_res = zend_register_list_destructors_ex(cybercache_resource_dtor, nullptr,
    C3_RESOURCE_NAME, module_number);
}
