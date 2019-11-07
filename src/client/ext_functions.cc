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
#include "ext_errors.h"
#include "ext_exceptions.h"
#include "ext_resource.h"
#include "ext_functions.h"
#include "server_thunk.h"

#include <cstdio>

///////////////////////////////////////////////////////////////////////////////
// GLOBAL DATA UNIQUE FOR EACH THREAD
///////////////////////////////////////////////////////////////////////////////

thread_local c3_uint_t c3_request_id;

thread_local Socket c3_request_socket(true, false);

///////////////////////////////////////////////////////////////////////////////
// ARGUMENT DESCRIPTORS
///////////////////////////////////////////////////////////////////////////////

ZEND_BEGIN_ARG_INFO_EX(arginfo_option_array, 0, 0, 0)
  ZEND_ARG_ARRAY_INFO(0, options, 1) // can be NULL
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_rc_id, 0)
    ZEND_ARG_INFO(0, resource)
    ZEND_ARG_INFO(0, id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_rc_id_request, 0, 0, 2)
    ZEND_ARG_INFO(0, resource)
    ZEND_ARG_INFO(0, id)
    ZEND_ARG_INFO(0, request_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_rc_id_lifetime_data_request, 0, 0, 4)
    ZEND_ARG_INFO(0, resource)
    ZEND_ARG_INFO(0, id)
    ZEND_ARG_INFO(0, lifetime)
    ZEND_ARG_INFO(0, data)
    ZEND_ARG_INFO(0, request_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_rc_seconds, 0)
    ZEND_ARG_INFO(0, resource)
    ZEND_ARG_INFO(0, seconds)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_rc_id_lifetime_tags_data, 0)
    ZEND_ARG_INFO(0, resource)
    ZEND_ARG_INFO(0, id)
    ZEND_ARG_INFO(0, lifetime)
    ZEND_ARG_INFO(0, tags) // can be an array, a string, or NULL
    ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_rc_mode_tags, 0, 0, 2)
    ZEND_ARG_INFO(0, resource)
    ZEND_ARG_INFO(0, mode)
    ZEND_ARG_INFO(0, tags) // can be an array, a string, or NULL
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_rc, 0)
    ZEND_ARG_INFO(0, resource)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_rc_tags, 0)
    ZEND_ARG_INFO(0, resource)
    ZEND_ARG_ARRAY_INFO(0, tags, 0) // can *NOT* be NULL
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_rc_id_xlifetime, 0)
    ZEND_ARG_INFO(0, resource)
    ZEND_ARG_INFO(0, id)
    ZEND_ARG_INFO(0, extra_lifetime)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_void, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_rc_domain, 0, 0, 1)
    ZEND_ARG_INFO(0, resource)
    ZEND_ARG_INFO(0, domain)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_rc_domain_mask, 0, 0, 1)
    ZEND_ARG_INFO(0, resource)
    ZEND_ARG_INFO(0, domain)
    ZEND_ARG_INFO(0, name_mask)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_rc_path, 0)
    ZEND_ARG_INFO(0, resource)
    ZEND_ARG_INFO(0, path)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_rc_domain_path_ua_sync, 0, 0, 3)
    ZEND_ARG_INFO(0, resource)
    ZEND_ARG_INFO(0, domain)
    ZEND_ARG_INFO(0, path)
    ZEND_ARG_INFO(0, user_agent)
    ZEND_ARG_INFO(0, sync_mode)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_rc_option_names, 0, 0, 2)
    ZEND_ARG_INFO(0, resource)
    ZEND_ARG_VARIADIC_INFO(0, option_names)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_rc_option_name_value, 0)
    ZEND_ARG_INFO(0, resource)
    ZEND_ARG_INFO(0, option_name)
    ZEND_ARG_INFO(0, option_value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_rc_message, 0)
    ZEND_ARG_INFO(0, resource)
    ZEND_ARG_INFO(0, message)
ZEND_END_ARG_INFO()

///////////////////////////////////////////////////////////////////////////////
// MODULE API: SESSION COMMANDS
///////////////////////////////////////////////////////////////////////////////

static PHP_FUNCTION(c3_session) {
  HashTable* options = nullptr;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|h!", &options) == FAILURE) {
    return;
  }
  C3Resource* res = C3Resource::create(options, "session", false);
  if (res != nullptr) {
    zend_resource* zres = zend_register_resource(res, le_cybercache_res);
    RETURN_RES(zres);
  } else {
    throw_php_exception("Could not initialize CyberCache SESSION resource, see error log");
    RETURN_NULL();
  }
}

static PHP_FUNCTION(c3_read) {
  const zval* rc;
  enum { ID = 0, REQUEST_ID, NUM_OF_ARGUMENTS };
  c3_arg_t args[NUM_OF_ARGUMENTS];
  args[REQUEST_ID].a_number = -1;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs|l", &rc, &args[ID].a_string, &args[ID].a_size,
    &args[REQUEST_ID].a_number) == FAILURE) {
    return;
  }
  if (args[REQUEST_ID].a_number < 0) {
    args[REQUEST_ID].a_number = c3_request_id;
  }
  call_c3(rc, return_value, OSR_STRING_FROM_DATA_PAYLOAD, ESR_EMPTY_STRING_FROM_OK,
    CMD_READ, AUT_USER, "SAN", args);
}

static PHP_FUNCTION(c3_write) {
  const zval* rc;
  enum { ID = 0, LIFETIME, DATA, REQUEST_ID, NUM_OF_ARGUMENTS };
  c3_arg_t args[NUM_OF_ARGUMENTS];
  args[REQUEST_ID].a_number = -1;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "rsls|l", &rc,
    &args[ID].a_string, &args[ID].a_size, &args[LIFETIME].a_number,
    &args[DATA].a_buffer, &args[DATA].a_size, &args[REQUEST_ID].a_number) == FAILURE) {
    return;
  }
  if (args[REQUEST_ID].a_number < 0) {
    args[REQUEST_ID].a_number = c3_request_id;
  }
  call_c3(rc, return_value, OSR_TRUE_FROM_OK, ESR_FALSE_FROM_ERROR,
    CMD_WRITE, AUT_USER, "SANPN", args);
}

static PHP_FUNCTION(c3_destroy) {
  const zval* rc;
  c3_arg_t id;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs", &rc, &id.a_string, &id.a_size) == FAILURE) {
    return;
  }
  call_c3(rc, return_value, OSR_TRUE_FROM_OK, ESR_FALSE_FROM_ERROR,
    CMD_DESTROY, AUT_USER, "S", &id);
}

static PHP_FUNCTION(c3_gc) {
  const zval* rc;
  c3_arg_t seconds;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "rl", &rc, &seconds.a_number) == FAILURE) {
    return;
  }
  call_c3(rc, return_value, OSR_TRUE_FROM_OK, ESR_FALSE_FROM_ERROR,
    CMD_GC, AUT_USER, "N", &seconds);
}

///////////////////////////////////////////////////////////////////////////////
// MODULE API: FPC COMMANDS
///////////////////////////////////////////////////////////////////////////////

static PHP_FUNCTION(c3_fpc) {
  HashTable* options = nullptr;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|h!", &options) == FAILURE) {
    return;
  }
  C3Resource* res = C3Resource::create(options, "fpc", true);
  if (res != nullptr) {
    zend_resource* zres = zend_register_resource(res, le_cybercache_res);
    RETURN_RES(zres);
  } else {
    throw_php_exception("Could not initialize CyberCache FPC resource, see error log");
    RETURN_NULL();
  }
}

static PHP_FUNCTION(c3_load) {
  const zval* rc;
  c3_arg_t id;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs", &rc, &id.a_string, &id.a_size) == FAILURE) {
    return;
  }
  call_c3(rc, return_value, OSR_STRING_FROM_DATA_PAYLOAD, ESR_FALSE_FROM_OK,
    CMD_LOAD, AUT_USER, "SA", &id);
}

static PHP_FUNCTION(c3_test) {
  const zval* rc;
  c3_arg_t id;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs", &rc, &id.a_string, &id.a_size) == FAILURE) {
    return;
  }
  call_c3(rc, return_value, OSR_NUMBER_FROM_DATA_HEADER, ESR_FALSE_FROM_OK,
    CMD_TEST, AUT_USER, "SA", &id);
}

static PHP_FUNCTION(c3_save) {
  const zval* rc;
  enum { ID = 0, LIFETIME, TAGS, DATA, NUM_OF_ARGUMENTS };
  c3_arg_t args[NUM_OF_ARGUMENTS];
  zval temp_array;
  zval* lifetime;
  bool array_initialized = false; // usually not needed
  if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS(), "rszh!s", &rc,
    &args[ID].a_string, &args[ID].a_size, &lifetime,
    &args[TAGS].a_list, &args[DATA].a_buffer, &args[DATA].a_size) == FAILURE) {
    // see if tag is passed as a single string
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rszss", &rc,
      &args[ID].a_string, &args[ID].a_size, &lifetime,
      &args[TAGS].a_string, &args[TAGS].a_size, &args[DATA].a_buffer, &args[DATA].a_size) == FAILURE) {
      return;
    }
    array_init(&temp_array);
    add_next_index_stringl(&temp_array, args[TAGS].a_string, args[TAGS].a_size);
    args[TAGS].a_list = Z_ARRVAL(temp_array);
    array_initialized = true;
  }
  switch (Z_TYPE_P(lifetime)) {
    case IS_LONG:
      args[LIFETIME].a_number = Z_LVAL_P(lifetime);
      break;
    case IS_FALSE: // "do not set specific lifetime" ==> default lifetime
      args[LIFETIME].a_number = -1;
      break;
    default: // must be `IS_NULL` ==> infinite lifetime
      args[LIFETIME].a_number = 0;
  }
  call_c3(rc, return_value, OSR_TRUE_FROM_OK, ESR_FALSE_FROM_ERROR,
    CMD_SAVE, AUT_USER, "SANLP", args);
  if (array_initialized) {
    zval_dtor(&temp_array);
  }
}

static PHP_FUNCTION(c3_remove) {
  const zval* rc;
  c3_arg_t id;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs", &rc, &id.a_string, &id.a_size) == FAILURE) {
    return;
  }
  call_c3(rc, return_value, OSR_TRUE_FROM_OK, ESR_FALSE_FROM_ERROR,
    CMD_REMOVE, AUT_USER, "S", &id);
}

static PHP_FUNCTION(c3_clean) {
  const zval* rc;
  enum { MODE = 0, TAGS, NUM_OF_ARGUMENTS };
  c3_arg_t args[NUM_OF_ARGUMENTS];
  args[TAGS].a_list = nullptr; // optional
  zval temp_array;
  ZVAL_NULL(&temp_array); // usually not needed
  if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS(), "rs|h!", &rc,
    &args[MODE].a_string, &args[MODE].a_size, &args[TAGS].a_list) == FAILURE) {
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rss", &rc,
      &args[MODE].a_string, &args[MODE].a_size, &args[TAGS].a_string, &args[TAGS].a_size) == FAILURE) {
      return;
    }
    array_init(&temp_array);
    add_next_index_stringl(&temp_array, args[TAGS].a_string, args[TAGS].a_size);
    args[TAGS].a_list = Z_ARRVAL(temp_array);
  }
  clean_mode_t mode;
  const char* mode_name = args[MODE].a_string;
  if (std::strcmp(mode_name, "all") == 0) {
    mode = CM_ALL;
  } else if (std::strcmp(mode_name, "old") == 0) {
    mode = CM_OLD;
  } else if (std::strcmp(mode_name, "matchingTag") == 0) {
    mode = CM_MATCHING_ALL_TAGS;
  } else if (std::strcmp(mode_name, "notMatchingTag") == 0) {
    mode = CM_NOT_MATCHING_ANY_TAG;
  } else if (std::strcmp(mode_name, "matchingAnyTag") == 0) {
    mode = CM_MATCHING_ANY_TAG;
  } else {
    report_error("Invalid cleaning mode: '%s'", mode_name);
    zval_dtor(&temp_array);
    RETURN_FALSE;
  }
  const char* format;
  args[MODE].a_number = mode;
  switch (mode) {
    case CM_ALL:
    case CM_OLD:
      // ignore tags even if specified
      args[TAGS].a_list = nullptr;
      format = "N";
      break;
    default:
      if (args[TAGS].a_list == nullptr) { // NULL passed instead of an array?
        array_init(&temp_array);
        args[TAGS].a_list = Z_ARRVAL(temp_array);
      }
      if (mode != CM_NOT_MATCHING_ANY_TAG && zend_hash_num_elements(args[TAGS].a_list) == 0) {
        RETVAL_TRUE;
        zval_dtor(&temp_array);
        return;
      }
      // even NULL hash table would do (we'll pass empty list then)
      format = "NL";
  }
  call_c3(rc, return_value, OSR_TRUE_FROM_OK, ESR_FALSE_FROM_ERROR,
    CMD_CLEAN, AUT_USER, format, args);
  zval_dtor(&temp_array);
}

static PHP_FUNCTION(c3_get_ids) {
  const zval* rc;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &rc) == FAILURE) {
    return;
  }
  call_c3(rc, return_value, OSR_ARRAY_FROM_LIST_PAYLOAD, ESR_EMPTY_ARRAY_FROM_ERROR,
    CMD_GETIDS, AUT_USER, "");
}

static PHP_FUNCTION(c3_get_tags) {
  const zval* rc;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &rc) == FAILURE) {
    return;
  }
  call_c3(rc, return_value, OSR_ARRAY_FROM_LIST_PAYLOAD, ESR_EMPTY_ARRAY_FROM_ERROR,
    CMD_GETTAGS, AUT_USER, "");
}

static PHP_FUNCTION(c3_get_ids_matching_tags) {
  const zval* rc;
  c3_arg_t tags;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "rh", &rc, &tags.a_list) == FAILURE) {
    return;
  }
  if (zend_hash_num_elements(tags.a_list)) {
    call_c3(rc, return_value, OSR_ARRAY_FROM_LIST_PAYLOAD, ESR_EMPTY_ARRAY_FROM_ERROR,
      CMD_GETIDSMATCHINGTAGS, AUT_USER, "L", &tags);
  } else {
    array_init(return_value);
  }
}

static PHP_FUNCTION(c3_get_ids_not_matching_tags) {
  const zval* rc;
  c3_arg_t tags;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "rh", &rc, &tags.a_list) == FAILURE) {
    return;
  }
  call_c3(rc, return_value, OSR_ARRAY_FROM_LIST_PAYLOAD, ESR_EMPTY_ARRAY_FROM_ERROR,
    CMD_GETIDSNOTMATCHINGTAGS, AUT_USER, "L", &tags);
}

static PHP_FUNCTION(c3_get_ids_matching_any_tags) {
  const zval* rc;
  c3_arg_t tags;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "rh", &rc, &tags.a_list) == FAILURE) {
    return;
  }
  if (zend_hash_num_elements(tags.a_list)) {
    call_c3(rc, return_value, OSR_ARRAY_FROM_LIST_PAYLOAD, ESR_EMPTY_ARRAY_FROM_ERROR,
      CMD_GETIDSMATCHINGANYTAGS, AUT_USER, "L", &tags);
  } else {
    array_init(return_value);
  }
}

static PHP_FUNCTION(c3_get_filling_percentage) {
  const zval* rc;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &rc) == FAILURE) {
    return;
  }
  call_c3(rc, return_value, OSR_NUMBER_FROM_DATA_HEADER, ESR_ZERO_FROM_ERROR,
    CMD_GETFILLINGPERCENTAGE, AUT_USER, "");
}

static PHP_FUNCTION(c3_get_metadatas) {
  const zval* rc;
  c3_arg_t id;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs", &rc, &id.a_string, &id.a_size) == FAILURE) {
    return;
  }
  // returns array ['expire' => <timestamp>, 'mtime' => <timestamp>, 'tags' => <array>]
  call_c3(rc, return_value, OSR_METADATA_FROM_DATA_HEADER, ESR_FALSE_FROM_OK,
    CMD_GETMETADATAS, AUT_USER, "S", &id);
}

static PHP_FUNCTION(c3_touch) {
  const zval* rc;
  enum { ID = 0, EXTRA_LIFETIME, NUM_OF_ARGUMENTS };
  c3_arg_t args[NUM_OF_ARGUMENTS];
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "rsl", &rc,
    &args[ID].a_string, &args[ID].a_size, &args[EXTRA_LIFETIME].a_number) == FAILURE) {
    return;
  }
  call_c3(rc, return_value, OSR_TRUE_FROM_OK, ESR_FALSE_FROM_ERROR,
    CMD_TOUCH, AUT_USER, "SN", args);
}

static PHP_FUNCTION(c3_get_capabilities) {
  if (zend_parse_parameters_none() == FAILURE) {
    return;
  }
  array_init(return_value);
  // return associative array of backend capabilities
  add_assoc_bool(return_value, "automatic_cleaning", 0);
  add_assoc_bool(return_value, "tags", 1);
  add_assoc_bool(return_value, "expired_read", 1);
  add_assoc_bool(return_value, "priority", 0);
  add_assoc_bool(return_value, "infinite_lifetime", 1);
  add_assoc_bool(return_value, "get_list", 1);
}

///////////////////////////////////////////////////////////////////////////////
// MODULE API: INFORMATION COMMANDS
///////////////////////////////////////////////////////////////////////////////

static PHP_FUNCTION(c3_ping) {
  const zval* rc;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &rc) == FAILURE) {
    return;
  }
  call_c3(rc, return_value, OSR_TRUE_FROM_OK, ESR_FALSE_FROM_ERROR,
    CMD_PING, AUT_INFO, "");
}

static PHP_FUNCTION(c3_check) {
  const zval* rc;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &rc) == FAILURE) {
    return;
  }
  call_c3(rc, return_value, OSR_NUM3_ARRAY_FROM_DATA_HEADER, ESR_EMPTY_ARRAY_FROM_ERROR,
    CMD_CHECK, AUT_INFO, "");
}

static PHP_FUNCTION(c3_info) {
  const zval* rc;
  c3_arg_t domains;
  domains.a_number = DM_ALL;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "r|l", &rc, &domains.a_number) == FAILURE) {
    return;
  }
  call_c3(rc, return_value, OSR_ARRAY_FROM_LIST_PAYLOAD, ESR_EMPTY_ARRAY_FROM_ERROR,
    CMD_INFO, AUT_INFO, "N", &domains);
}

static PHP_FUNCTION(c3_stats) {
  const zval* rc;
  enum { DOMAINS = 0, MASK, NUM_OF_ARGUMENTS };
  c3_arg_t args[NUM_OF_ARGUMENTS];
  args[DOMAINS].a_number = DM_ALL;
  args[MASK].a_string = "*";
  args[MASK].a_size = 1;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "r|ls", &rc,
    &args[DOMAINS].a_number, &args[MASK].a_string, &args[MASK].a_size) == FAILURE) {
    return;
  }
  call_c3(rc, return_value, OSR_ARRAY_FROM_LIST_PAYLOAD, ESR_EMPTY_ARRAY_FROM_ERROR,
    CMD_STATS, AUT_INFO, "NS", args);
}

///////////////////////////////////////////////////////////////////////////////
// MODULE API: ADMINISTRATIVE COMMANDS
///////////////////////////////////////////////////////////////////////////////

static PHP_FUNCTION(c3_shutdown) {
  const zval* rc;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &rc) == FAILURE) {
    return;
  }
  call_c3(rc, return_value, OSR_TRUE_FROM_OK, ESR_FALSE_FROM_ERROR,
    CMD_SHUTDOWN, AUT_ADMIN, "");
}

static PHP_FUNCTION(c3_local_config) {
  const zval* rc;
  c3_arg_t path;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "rp", &rc, &path.a_string, &path.a_size) == FAILURE) {
    return;
  }
  c3_arg_t contents;
  contents.a_string = (const char*) c3_load_file(path.a_string, contents.a_size, global_memory);
  if (contents.a_string != nullptr) {
    call_c3(rc, return_value, OSR_TRUE_FROM_OK, ESR_FALSE_FROM_ERROR,
      CMD_SET, AUT_ADMIN, "S", &contents);
    global_memory.free((char*) contents.a_string, contents.a_size);
  } else {
    report_error("Could not load local configuration file '%s' (%s)",
      path.a_string, c3_get_error_message());
    RETURN_FALSE;
  }
}

static PHP_FUNCTION(c3_remote_config) {
  const zval* rc;
  c3_arg_t path;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "rp", &rc, &path.a_string, &path.a_size) == FAILURE) {
    return;
  }
  call_c3(rc, return_value, OSR_TRUE_FROM_OK, ESR_FALSE_FROM_ERROR,
    CMD_LOADCONFIG, AUT_ADMIN, "S", &path);
}

static PHP_FUNCTION(c3_restore) {
  const zval* rc;
  c3_arg_t path;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "rp", &rc, &path.a_string, &path.a_size) == FAILURE) {
    return;
  }
  call_c3(rc, return_value, OSR_TRUE_FROM_OK, ESR_FALSE_FROM_ERROR,
    CMD_RESTORE, AUT_ADMIN, "S", &path);
}

static PHP_FUNCTION(c3_store) {
  enum { DOMAINS = 0, PATH, UA, SYNC, NUM_OF_ARGUMENTS };
  c3_arg_t args[NUM_OF_ARGUMENTS];
  args[UA].a_number = UA_UNKNOWN;
  args[SYNC].a_number = SM_NONE;
  const zval* rc;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "rlp|ll", &rc,
    &args[DOMAINS].a_number, &args[PATH].a_string, &args[PATH].a_size,
    &args[UA].a_number, &args[SYNC].a_number) == FAILURE) {
    return;
  }
  call_c3(rc, return_value, OSR_TRUE_FROM_OK, ESR_FALSE_FROM_ERROR,
    CMD_STORE, AUT_ADMIN, "NSNN", args);
}

static PHP_FUNCTION(c3_get) {
  const zval* rc;
  const zval* args = nullptr;
  int argc;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "r+", &rc, &args, &argc) == FAILURE) {
    return;
  }
  zval znames;
  array_init(&znames);
  bool ok = true;
  for (int i = 0; i < argc; i++) {
    const zval* name = args + i;
    if (Z_TYPE_P(name) == IS_STRING) {
      add_next_index_stringl(&znames, Z_STRVAL_P(name), Z_STRLEN_P(name));
    } else {
      ok = false;
      break;
    }
  }
  if (ok) {
    c3_arg_t names;
    names.a_list = Z_ARRVAL(znames);
    call_c3(rc, return_value, OSR_ARRAY_FROM_LIST_PAYLOAD, ESR_EMPTY_ARRAY_FROM_ERROR,
      CMD_GET, AUT_ADMIN, "L", &names);
  } else {
    report_error("Option names must be strings");
    array_init(return_value);
  }
  zval_dtor(&znames);
}

static PHP_FUNCTION(c3_set) {
  const zval* rc;
  const char* opt_name;
  size_t opt_name_len;
  const char* opt_value;
  size_t opt_value_len;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "rss", &rc,
    &opt_name, &opt_name_len, &opt_value, &opt_value_len) == FAILURE) {
    return;
  }
  const size_t command_len = opt_name_len + opt_value_len + 4;
  char command_string[command_len];
  int actual_size;
  if (opt_value_len == 0 || opt_value[0] == ' ') {
    actual_size = std::snprintf(command_string, command_len, "%s '%s'", opt_name, opt_value);
  } else {
    actual_size = std::snprintf(command_string, command_len, "%s %s", opt_name, opt_value);
  }
  c3_arg_t command;
  /*
   * Here, we pass pointer to the string (character array) allocated on stack, which is safe: the
   * array will only be used during `call_c3()` function execution, and will not be passed further.
   */
  command.a_string = command_string;
  command.a_size = (size_t) actual_size;
  call_c3(rc, return_value, OSR_TRUE_FROM_OK, ESR_FALSE_FROM_ERROR,
    CMD_SET, AUT_ADMIN, "S", &command);
}

static PHP_FUNCTION(c3_log) {
  const zval* rc;
  c3_arg_t message;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs", &rc, &message.a_string, &message.a_size) == FAILURE) {
    return;
  }
  call_c3(rc, return_value, OSR_TRUE_FROM_OK, ESR_FALSE_FROM_ERROR,
    CMD_LOG, AUT_ADMIN, "S", &message);
}

static PHP_FUNCTION(c3_rotate) {
  const zval* rc;
  c3_arg_t domain;
  domain.a_number = DM_GLOBAL;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "r|l", &rc, &domain.a_number) == FAILURE) {
    return;
  }
  call_c3(rc, return_value, OSR_TRUE_FROM_OK, ESR_FALSE_FROM_ERROR,
    CMD_ROTATE, AUT_ADMIN, "N", &domain);
}

static PHP_FUNCTION(c3_get_last_error) {
  const zval* rc;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &rc) == FAILURE) {
    return;
  }
  auto res = (C3Resource*) zend_fetch_resource(Z_RES_P(rc), C3_RESOURCE_NAME, le_cybercache_res);
  if (res != nullptr) {
    const char* message = res->get_error_message();
    if (message == nullptr) {
      message = "";
    }
    RETURN_STRING(message);
  } else {
    RETURN_EMPTY_STRING();
  }
}

///////////////////////////////////////////////////////////////////////////////
// MODULE API: TABLE OF METHODS
///////////////////////////////////////////////////////////////////////////////

// table of functions exported by the module
const zend_function_entry cybercache_functions[] = {
  // session methods
  PHP_FE(c3_session, arginfo_option_array)
  PHP_FE(c3_read, arginfo_rc_id_request)
  PHP_FE(c3_write, arginfo_rc_id_lifetime_data_request)
  PHP_FE(c3_destroy, arginfo_rc_id)
  PHP_FE(c3_gc, arginfo_rc_seconds)
  // FPC methods
  PHP_FE(c3_fpc, arginfo_option_array)
  PHP_FE(c3_load, arginfo_rc_id)
  PHP_FE(c3_test, arginfo_rc_id)
  PHP_FE(c3_save, arginfo_rc_id_lifetime_tags_data)
  PHP_FE(c3_remove, arginfo_rc_id)
  PHP_FE(c3_clean, arginfo_rc_mode_tags)
  PHP_FE(c3_get_ids, arginfo_rc)
  PHP_FE(c3_get_tags, arginfo_rc)
  PHP_FE(c3_get_ids_matching_tags, arginfo_rc_tags)
  PHP_FE(c3_get_ids_not_matching_tags, arginfo_rc_tags)
  PHP_FE(c3_get_ids_matching_any_tags, arginfo_rc_tags)
  PHP_FE(c3_get_filling_percentage, arginfo_rc)
  PHP_FE(c3_get_metadatas, arginfo_rc_id)
  PHP_FE(c3_touch, arginfo_rc_id_xlifetime)
  PHP_FE(c3_get_capabilities, arginfo_void)
  // auxiliary methods
  PHP_FE(c3_ping, arginfo_rc)
  PHP_FE(c3_check, arginfo_rc)
  PHP_FE(c3_info, arginfo_rc_domain)
  PHP_FE(c3_stats, arginfo_rc_domain_mask)
  PHP_FE(c3_shutdown, arginfo_rc)
  PHP_FE(c3_local_config, arginfo_rc_path)
  PHP_FE(c3_remote_config, arginfo_rc_path)
  PHP_FE(c3_restore, arginfo_rc_path)
  PHP_FE(c3_store, arginfo_rc_domain_path_ua_sync)
  PHP_FE(c3_get, arginfo_rc_option_names)
  PHP_FE(c3_set, arginfo_rc_option_name_value)
  PHP_FE(c3_log, arginfo_rc_message)
  PHP_FE(c3_rotate, arginfo_rc_domain)
  PHP_FE(c3_get_last_error, arginfo_rc)
  // end-of-table marker
  PHP_FE_END
};
