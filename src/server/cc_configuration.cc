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
#include "cc_configuration.h"
#include "cc_subsystems.h"
#include "cc_worker_threads.h"
#include "cc_server.h"
#include "ls_utils.h"
#include "pl_net_configuration.h"

namespace CyberCache {

static const char* config_password_types[PT_NUMBER_OF_ELEMENTS];
static const char* config_log_levels[LL_NUMBER_OF_ELEMENTS];
static const char* config_eviction_modes[EM_NUMBER_OF_ELEMENTS];
static const char* config_compressors[CT_NUMBER_OF_ELEMENTS];
static const char* config_hashers[HM_NUMBER_OF_ELEMENTS];
static const char* config_sync_modes[SM_NUMBER_OF_ELEMENTS];
static const char* config_user_agents[UA_NUMBER_OF_ELEMENTS];

///////////////////////////////////////////////////////////////////////////////
// HELPERS TO BE USED BY OPTIONS' GETTERS/SETTERS
///////////////////////////////////////////////////////////////////////////////

bool Configuration::require_arguments(Parser& parser, c3_uint_t num, const char* type, c3_uint_t min_num,
  c3_uint_t max_num) {
  if (num >= min_num && num <= max_num) {
    return true;
  }
  if (min_num == max_num) {
    parser.log_command_error("expected single <%s> argument", type);
  } else {
    parser.log_command_error("expected %u to %u <%s> arguments", min_num, max_num, type);
  }
  return false;
}

bool Configuration::require_single_argument(Parser& parser, c3_uint_t num, const char* type) {
  return require_arguments(parser, num, type, 1, 1);
}

bool Configuration::require_single_number_argument(Parser& parser, c3_uint_t num) {
  return require_single_argument(parser, num, "number");
}

void Configuration::check_order(Parser& parser, parser_token_t* args, c3_uint_t num,
  const c3_uint_t* values, const char* type, bool ascending) {
  for (c3_uint_t i = 0; i < num - 1; i++) {
    c3_uint_t prev = values[i];
    c3_uint_t next = values[i + 1];
    const char* order;
    char sign;
    bool violated;
    if (ascending) {
      order = "ascending";
      sign = '>';
      violated = prev > next;
    } else {
      order = "descending";
      sign = '<';
      violated = prev < next;
    }
    if (violated) {
      parser.log_command_status(LL_WARNING, "%s should be monotonous or in %s order, but '%s' %c '%s'",
        type, order, args[i].get_string(), sign, args[i + 1].get_string());
      // warn only once
      break;
    }
  }
}

bool Configuration::check_password(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (require_single_argument(parser, num, "string")) {
    const char* password = args[0].get_string();
    size_t length = std::strlen(password);
    if (length > 0) {
      if (length < 6) {
        parser.log_command_status(LL_WARNING, "Password should be empty or at least 6 characters long");
      }
      bool has_uc_letters = false;
      bool has_lc_letters = false;
      bool has_digits = false;
      bool has_special_chars = false;
      size_t i = 0;
      do {
        char c = password[i];
        if (std::isupper(c) != 0) {
          has_uc_letters = true;
        }
        if (std::islower(c) != 0) {
          has_lc_letters = true;
        }
        if (std::isdigit(c) != 0) {
          has_digits = true;
        }
        if (isprint(c) == 0 || c == ' ') {
          has_special_chars = true;
        }
      } while (++i < length);
      if (!has_uc_letters) {
        parser.log_command_status(LL_WARNING, "Password should have at least one uppercase letter");
      }
      if (!has_lc_letters) {
        parser.log_command_status(LL_WARNING, "Password should have at least one lowercase letter");
      }
      if (!has_digits) {
        parser.log_command_status(LL_WARNING, "Password should have at least one digit");
      }
      if (has_special_chars) {
        parser.log_command_status(LL_WARNING,
          "Password contains characters that cannot be entered from console");
      }
    }
    return true;
  }
  return false;
}

bool Configuration::check_path(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (num == 1) {
    const char* path = args[0].get_string();
    size_t length = std::strlen(path);
    if (length < MAX_FILE_PATH_LENGTH) { // zero length is OK
      for (size_t i = 0; i < length; i++) {
        char c = path[i];
        if (c == ' ' || c == '*' || c == '?') {
          parser.log_command_error("<path> must not contain spaces, '?' or '*' characters");
          return false;
        }
      }
      return true;
    } else {
      parser.log_command_error("<path> argument longer than %u characters",
        (c3_uint_t) MAX_FILE_PATH_LENGTH);
    }
  } else {
    parser.log_command_error("single <path> argument expected (use '' for empty paths)");
  }
  return false;
}

bool Configuration::check_rotation_path(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (check_path(parser, args, num)) {
    const char* path = args[0].get_string();
    if (path[0] == 0 || LogUtils::get_log_rotation_type(path) != RT_INVALID) {
      return true;
    }
    parser.log_command_error("ill-formed rotation path (no or multiple %%d and/or %%s placeholders)");
  }
  return false;
}

ssize_t Configuration::print_boolean(char* buff, size_t length, bool value) {
  return std::snprintf(buff, length, "%s", value? "true": "false");
}

bool Configuration::get_boolean(Parser& parser, parser_token_t* args, c3_uint_t num, bool& value) {
  if (require_single_argument(parser, num, "boolean")) {
    return args[0].get_boolean(value);
  }
  return false;
}

ssize_t Configuration::print_number(char* buff, size_t length, c3_uint_t value) {
  return snprintf(buff, length, "%u", value);
}

ssize_t Configuration::print_numbers(char* buff, size_t length, const c3_uint_t* values, c3_uint_t num) {
  size_t pos = 0;
  for (c3_uint_t i = 0; i < num && pos < length; i++) {
    if (i > 0) {
      buff[pos++] = ' '; // worst case, buffer size passed to `snprintf()` will be zero
    }
    ssize_t len = snprintf(buff + pos, length - pos, "%u", values[i]);
    if (len > 0) {
      pos += len;
    } else {
      return -1;
    }
  }
  return pos;
}

bool Configuration::get_number(Parser& parser, parser_token_t* args, c3_uint_t num, c3_uint_t& value) {
  if (require_single_number_argument(parser, num)) {
    parser_token_t& arg = args[0];
    c3_uint_t number;
    if (arg.get_uint(number)) {
      value = number;
      return true;
    }
    parser.log_command_error("ill-formed unsigned integer value: '%s'", arg.get_string());
  }
  return false;
}

bool Configuration::get_number(Parser& parser, parser_token_t* args, c3_uint_t num, c3_uint_t& value,
  c3_uint_t min_value, c3_uint_t max_value) {
  if (require_single_number_argument(parser, num)) {
    parser_token_t& arg = args[0];
    c3_uint_t number;
    if (arg.get_uint(number) && number >= min_value && number <= max_value) {
      value = number;
      return true;
    }
    parser.log_command_error("value not in [%u..%u] range: '%s'", min_value, max_value, arg.get_string());
  }
  return false;
}

bool Configuration::get_numbers(Parser& parser, parser_token_t* args, c3_uint_t num, c3_uint_t* values,
  c3_uint_t max_num, bool ascending) {
  if (require_arguments(parser, num, "number", 1, max_num)) {
    c3_uint_t i = 0;
    do {
      c3_uint_t value;
      if (args[i].get_uint(value)) {
        values[i] = value;
      } else {
        parser.log_command_error("ill-formed number: '%s'", args[i].get_string());
        return false;
      }
    } while (++i < num);
    // values that are not set explicitly default to last specified value
    while (i < max_num) {
      values[i] = values[i - 1];
      i++;
    }
    check_order(parser, args, num, values, "counts", ascending);
    return true;
  }
  return false;
}

ssize_t Configuration::print_durations(char* buff, size_t length, const c3_uint_t* durations) {
  size_t pos = 0;
  for (c3_uint_t i = 0; i < UA_NUMBER_OF_ELEMENTS && pos < length; i++) {
    if (i != 0) {
      buff[pos++] = ' '; // worst case, buffer size passed to `print_duration()` will be zero
    }
    ssize_t len = Parser::print_duration(buff + pos, length - pos, durations[i]);
    if (len > 0) {
      pos += len;
    } else {
      return -1;
    }
  }
  return pos;
}

bool Configuration::get_duration_value(Parser& parser, parser_token_t* args, c3_uint_t i,
  c3_uint_t& value) {
  parser_token_t& token = args[i];
  c3_uint_t seconds;
  if (token.get_duration(seconds)) {
    if (seconds > 0) {
      /*
       * It is possible to enter some very big value with a suffix, cause overflow, and end up with
       * some formally legal value that is nonetheless not what the user thought it would be... Currently,
       * we do not check for that.
       */
      if (seconds <= MAX_DURATION) {
        value = seconds;
        return true;
      } else {
        parser.log_command_error("duration too long (more than a year): %u seconds", seconds);
      }
    } else {
      parser.log_command_error("duration cannot be zero");
    }
  } else {
    parser.log_command_error("ill-formed duration: '%s'", token.get_string());
  }
  return false;
}

bool Configuration::get_duration(Parser& parser, parser_token_t* args, c3_uint_t num, c3_uint_t& value) {
  if (require_single_argument(parser, num, "duration")) {
    return get_duration_value(parser, args, 0, value);
  }
  return false;
}

bool Configuration::get_durations(Parser& parser, parser_token_t* args, c3_uint_t num, c3_uint_t* values) {
  if (require_arguments(parser, num, "duration", 1, UA_NUMBER_OF_ELEMENTS)) {
    c3_uint_t i = 0;
    do {
      if (!get_duration_value(parser, args, i, values[i])) {
        return false;
      }
    } while (++i < num);
    // durations that are not set explicitly default to last duration
    while (i < UA_NUMBER_OF_ELEMENTS) {
      values[i] = values[i - 1];
      i++;
    }
    check_order(parser, args, num, values, "durations", true);
    return true;
  }
  return false;
}

bool Configuration::get_size(Parser& parser, parser_token_t* args, c3_uint_t num, c3_ulong_t& size) {
  if (require_single_argument(parser, num, "size")) {
    c3_ulong_t nbytes;
    if (args[0].get_size(nbytes)) {
      size = nbytes;
      return true;
    }
    parser.log_command_error("ill-formed <size> argument: '%s'", args[0].get_string());
  }
  return false;
}

bool Configuration::get_size(Parser& parser, parser_token_t* args, c3_uint_t num, c3_ulong_t& size,
  c3_ulong_t min_value, c3_ulong_t max_value) {
  c3_ulong_t value;
  if (get_size(parser, args, num, value)) {
    if (value >= min_value && value <= max_value) {
      size = value;
      return true;
    }
    parser.log_command_error("<size> '%s' not in [%llu..%llu] range",
      args[0].get_string(), min_value, max_value);
  }
  return false;
}

bool Configuration::get_recompression_threshold(Parser &parser, parser_token_t* args, c3_uint_t num,
  Optimizer &optimizer) {
  c3_ulong_t threshold;
  if (get_size(parser, args, num, threshold, 1, UINT_MAX_VAL)) {
    return optimizer.post_config_recompression_threshold_message((c3_uint_t) threshold);
  }
  return false;
}

void Configuration::log_keyword_error(Parser& parser, const char** options, c3_uint_t num_options) {
  const size_t BUFFER_SIZE = 1024;
  char buffer[BUFFER_SIZE];
  const char* separator = "";
  buffer[0] = 0;
  size_t pos = 0;
  c3_uint_t i = 0;
  do {
    const char* option = options[i];
    if (option != nullptr) {
      const char* format = i == num_options - 1? "%sor '%s'": "%s'%s'";
      int length = std::snprintf(buffer + pos, BUFFER_SIZE - pos, format, separator, option);
      if (length > 0) {
        pos += length;
        separator = ", ";
      } else {
        break;
      }
    }
  } while (++i < num_options && pos < BUFFER_SIZE);
  parser.log_command_error("expected %s as argument", buffer);
}

ssize_t Configuration::print_keyword(char* buff, size_t length, c3_uint_t index,
  const char** options, c3_uint_t num_options) {
  c3_assert(index < num_options);
  const char* option = options[index];
  c3_assert(option);
  return std::snprintf(buff, length, "%s", option);
  return -1;
}

int Configuration::get_keyword_index(Parser& parser, parser_token_t& arg, const char** options,
  c3_uint_t num_options) {
  c3_assert(options && num_options);
  c3_uint_t i = 0;
  do {
    const char* option = options[i];
    // case-insensitive comparison
    if (option != nullptr && c3_matches(arg.get_string(), option)) {
      return i;
    }
  } while (++i < num_options);
  log_keyword_error(parser, options, num_options);
  return -1;
}

int Configuration::get_single_keyword_index(Parser& parser, parser_token_t* args, c3_uint_t num,
  const char** options, c3_uint_t num_options) {
  if (num == 1) {
    return get_keyword_index(parser, args[0], options, num_options);
  }
  log_keyword_error(parser, options, num_options);
  return -1;
}

ssize_t Configuration::print_compressor(char* buff, size_t length, c3_uint_t index) {
  return print_keyword(buff, length, index, config_compressors, CT_NUMBER_OF_ELEMENTS);
}

ssize_t Configuration::print_compressors(char* buff, size_t length, Optimizer& optimizer) {
  const c3_uint_t num_compressors = Optimizer::get_num_compressors();
  const c3_compressor_t* compressors = optimizer.get_compressors();
  size_t pos = 0;
  for (c3_uint_t i = 0; i < num_compressors && pos < length; ++i) {
    c3_compressor_t comp = compressors[i];
    if (comp != CT_NONE) {
      if (pos > 0) {
        buff[pos++] = ' '; // worst case, buffer length passed to `print_compressor()` will be zero
      }
      ssize_t len = print_compressor(buff + pos, length - pos, comp);
      if (len > 0) {
        pos += len;
      } else {
        break;
      }
    } else {
      break;
    }
  }
  return pos;
}

int Configuration::get_compressor_index(Parser& parser, parser_token_t& arg) {
  int compressor = get_keyword_index(parser, arg, config_compressors, CT_NUMBER_OF_ELEMENTS);
  #if !C3_ENTERPRISE
  if (compressor == CT_BROTLI) {
    parser.log(LL_ERROR, "The 'brotli' compressor is available in Enterprise Edition only");
    compressor = -1;
  }
  #endif
  return compressor;
}

int Configuration::get_single_compressor_index(Parser& parser, parser_token_t* args, c3_uint_t num) {
  int compressor = get_single_keyword_index(parser, args, num, config_compressors, CT_NUMBER_OF_ELEMENTS);
  #if !C3_ENTERPRISE
  if (compressor == CT_BROTLI) {
    parser.log(LL_ERROR, "The 'brotli' compressor is available in Enterprise Edition only");
    compressor = -1;
  }
  #endif
  return compressor;
}

bool Configuration::get_compressors(Parser& parser, parser_token_t* args, c3_uint_t num,
  Optimizer& optimizer) {
  const c3_uint_t num_compressors = Optimizer::get_num_compressors();
  if (require_arguments(parser, num, "compressor", 1, num_compressors)) {
    c3_compressor_t compressors[num_compressors];
    bool used[CT_NUMBER_OF_ELEMENTS];
    std::memset(used, 0, sizeof used);
    c3_uint_t i = 0;
    do {
      int index = get_compressor_index(parser, args[i]);
      if (index > 0) {
        if (used[index]) {
          parser.log_command_error("compressor '%s' specified more than once", config_compressors[index]);
          return false;
        }
        used[index] = true;
        compressors[i] = (c3_compressor_t) index;
      } else {
        return false;
      }
    } while (++i < num);
    // clean up remaining types
    while (i < num_compressors) {
      compressors[i++] = CT_NONE;
    }
    return optimizer.post_config_compressors_message(compressors);
  }
  return false;
}

ssize_t Configuration::print_max_memory(Parser& parser, char* buff, size_t length, Memory& memory) {
  return Parser::print_size(buff, length, memory.get_quota());
}

bool Configuration::set_max_memory(Parser &parser, parser_token_t* args, c3_uint_t num, Memory &memory) {
  c3_ulong_t quota;
  if (get_size(parser, args, num, quota)) {
    if (quota == 0 || (quota >= Memory::get_min_quota() && quota <= Memory::get_max_quota())) {
      memory.set_quota(quota);
      return true;
    }
    parser.log_command_error("Memory quota not zero or in [%llu..%llu] range",
      Memory::get_min_quota(), Memory::get_max_quota());
  }
  return false;
}

ssize_t Configuration::get_max_file_size(Parser& parser, char* buff, size_t length, FileBase& file) {
  return Parser::print_size(buff, length, file.get_max_size());
}

bool Configuration::set_ips(Parser &parser, parser_token_t* args, c3_uint_t num, SocketPipeline &pipeline) {
  if (require_arguments(parser, num, "address", 1, MAX_IPS_PER_SERVICE)) {
    c3_ipv4_t ips[MAX_IPS_PER_SERVICE];
    c3_uint_t i = 0;
    do {
      const char* address = args[i].get_string();
      c3_ipv4_t ip = c3_resolve_host(address);
      if (ip == INVALID_IPV4_ADDRESS) {
        parser.log_command_error("could not resolve address: '%s'", address);
        return false;
      }
      ips[i] = ip;
    } while (++i < num);
    return pipeline.send_ip_set_change_command(ips, num);
  }
  return false;
}

bool Configuration::set_port(Parser &parser, parser_token_t* args, c3_uint_t num, SocketPipeline &pipeline) {
  c3_uint_t port;
  if (get_number(parser, args, num, port, 1024, 65535)) {
    return pipeline.send_port_change_command((c3_ushort_t) port);
  }
  return false;
}

bool Configuration::set_persistence(Parser &parser, parser_token_t* args, c3_uint_t num, SocketPipeline &pipeline) {
  bool enabled;
  if (Configuration::get_boolean(parser, args, num, enabled)) {
    return pipeline.send_set_persistent_connections_command(enabled);
  }
  return false;
}

ssize_t Configuration::print_fill_factor(char* buff, size_t length, Store& store) {
  return std::snprintf(buff, length, "%f", store.get_fill_factor());
}

bool Configuration::set_fill_factor(Parser &parser, parser_token_t* args, c3_uint_t num, Store &store) {
  if (require_single_argument(parser, num, "float")) {
    float factor;
    if (args[0].get_float(factor) && factor >= Store::get_min_fill_factor() &&
      factor <= Store::get_max_fill_factor()) {
      store.set_fill_factor(factor);
      return true;
    }
    parser.log_command_error("Fill factor not in [%f..%f] range: '%s'",
      Store::get_min_fill_factor(), Store::get_max_fill_factor(), args[0].get_string());
  }
  return false;
}

bool Configuration::set_num_tables(Parser &parser, parser_token_t* args, c3_uint_t num, ObjectStore &store) {
  c3_uint_t num_tables;
  if (get_number(parser, args, num, num_tables, 1, MAX_NUM_TABLES_PER_STORE)) {
    if (is_power_of_2(num_tables)) {
      return store.set_num_tables(num_tables);
    }
    parser.log_command_error("number of tables per store not a power of 2: %u", num_tables);
  }
  return false;
}

bool Configuration::set_init_capacity(Parser &parser, parser_token_t* args, c3_uint_t num, ObjectStore &store) {
  c3_uint_t capacity;
  if (get_number(parser, args, num, capacity)) {
    // if server is not in CONFIG state, we fail silently
    if (server.get_state() <= SS_CONFIG) {
      store.set_table_capacity(capacity);
    }
    return true;
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// OPTION HANDLERS
///////////////////////////////////////////////////////////////////////////////

/*
 * GCC only allows assigning attributes to function/method declarations, not definitions... So we have
 * to do a bit of magic here: instead of using `PARSER_GET_PROC()` and `PARSER_SET_PROC()` macros that
 * just mangle names, we use new `CONFIG_xxx()` macros that do what their `PARSER_xxx()` counterparts do,
 * plus they create function prototypes, and apply "cold" attribute to them.
 */
#define CONFIG_GET_PROC(name) \
  PARSER_GET_PROC(name)(Parser& parser, char* buff, size_t length) C3_FUNC_COLD; \
  static ssize_t PARSER_GET_PROC(name)

#define CONFIG_SET_PROC(name) \
  PARSER_SET_PROC(name)(Parser& parser, parser_token_t* args, c3_uint_t num) C3_FUNC_COLD; \
  static bool PARSER_SET_PROC(name)

static ssize_t CONFIG_GET_PROC(max_memory)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_max_memory(parser, buff, length, global_memory);
}

static bool CONFIG_SET_PROC(max_memory)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::set_max_memory(parser, args, num, global_memory);
}

static ssize_t CONFIG_GET_PROC(max_session_memory)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_max_memory(parser, buff, length, session_memory);
}

static bool CONFIG_SET_PROC(max_session_memory)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::set_max_memory(parser, args, num, session_memory);
}

static ssize_t CONFIG_GET_PROC(max_fpc_memory)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_max_memory(parser, buff, length, fpc_memory);
}

static bool CONFIG_SET_PROC(max_fpc_memory)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::set_max_memory(parser, args, num, fpc_memory);
}

static bool CONFIG_SET_PROC(listener_addresses)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::set_ips(parser, args, num, server_listener);
}

static bool CONFIG_SET_PROC(listener_port)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::set_port(parser, args, num, server_listener);
}

static bool CONFIG_SET_PROC(session_replicator_addresses)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::set_ips(parser, args, num, session_replicator);
}

static bool CONFIG_SET_PROC(session_replicator_port)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::set_port(parser, args, num, session_replicator);
}

static bool CONFIG_SET_PROC(fpc_replicator_addresses)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::set_ips(parser, args, num, fpc_replicator);
}

static bool CONFIG_SET_PROC(fpc_replicator_port)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::set_port(parser, args, num, fpc_replicator);
}

static ssize_t CONFIG_GET_PROC(listener_persistent)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_boolean(buff, length, server_listener.is_using_persistent_connections());
}

static bool CONFIG_SET_PROC(listener_persistent)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::set_persistence(parser, args, num, server_listener);
}

static ssize_t CONFIG_GET_PROC(session_replicator_persistent)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_boolean(buff, length, session_replicator.is_using_persistent_connections());
}

static bool CONFIG_SET_PROC(session_replicator_persistent)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::set_persistence(parser, args, num, session_replicator);
}

static ssize_t CONFIG_GET_PROC(fpc_replicator_persistent)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_boolean(buff, length, fpc_replicator.is_using_persistent_connections());
}

static bool CONFIG_SET_PROC(fpc_replicator_persistent)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::set_persistence(parser, args, num, fpc_replicator);
}

static bool CONFIG_SET_PROC(user_password)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (Configuration::check_password(parser, args, num)) {
    if (!parser.is_interactive()) {
      // if server is not in CONFIG state, the following call will fail silently
      server.set_user_password(args[0].get_string());
      return true;
    }
  }
  return false;
}

static bool CONFIG_SET_PROC(admin_password)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (Configuration::check_password(parser, args, num)) {
    if (!parser.is_interactive()) {
      // if server is not in CONFIG state, the following call will fail silently
      server.set_admin_password(args[0].get_string());
      return true;
    }
  }
  return false;
}

static bool CONFIG_SET_PROC(bulk_password)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (Configuration::check_password(parser, args, num)) {
    if (!parser.is_interactive()) {
      // if server is not in CONFIG state, the following call will fail silently
      server.set_bulk_password(args[0].get_string());
      return true;
    }
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(info_password_type)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_keyword(buff, length, ConnectionThread::get_info_password_type(),
    config_password_types, PT_NUMBER_OF_ELEMENTS);
}

static bool CONFIG_SET_PROC(info_password_type)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  int option = Configuration::get_single_keyword_index(parser, args, num,
    config_password_types, PT_NUMBER_OF_ELEMENTS);
  if (option >= 0) {
    ConnectionThread::set_info_password_type((password_type_t) option);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(log_level)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_keyword(buff, length, server_logger.get_level(),
    config_log_levels, LL_NUMBER_OF_ELEMENTS);
}

static bool CONFIG_SET_PROC(log_level)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  int option = Configuration::get_single_keyword_index(parser, args, num,
    config_log_levels, LL_NUMBER_OF_ELEMENTS);
  if (option >= 0) {
    server.set_log_level((log_level_t) option);
    return true;
  }
  return false;
}

static bool CONFIG_SET_PROC(log_file)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (Configuration::check_path(parser, args, num)) {
    return server.set_log_file_path(args[0].get_string());
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(log_rotation_threshold)(Parser& parser, char* buff, size_t length) {
  return Configuration::get_max_file_size(parser, buff, length, server_logger);
}

static bool CONFIG_SET_PROC(log_rotation_threshold)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_ulong_t threshold;
  if (Configuration::get_size(parser, args, num, threshold,
    Logger::get_min_threshold(), Logger::get_max_threshold())) {
    return server_logger.send_rotation_threshold_change_command(threshold);
  }
  return false;
}

static bool CONFIG_SET_PROC(log_rotation_path)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (Configuration::check_rotation_path(parser, args, num)) {
    return server_logger.send_rotation_path_change_command(args[0].get_string());
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(num_connection_threads)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, Thread::get_num_connection_threads());
}

static bool CONFIG_SET_PROC(num_connection_threads)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t num_threads;
  if (Configuration::get_number(parser, args, num, num_threads, 1, MAX_NUM_CONNECTION_THREADS)) {
    return server.set_num_connection_threads(num_threads);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(session_lock_wait_time)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, SessionObject::get_lock_wait_time());
}

static bool CONFIG_SET_PROC(session_lock_wait_time)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t milliseconds;
  if (Configuration::get_number(parser, args, num, milliseconds, 0, 60 * 1000)) {
    SessionObject::set_lock_wait_time(milliseconds);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(session_first_write_lifetimes)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_durations(buff, length, session_optimizer.get_first_write_lifetimes());
}

static bool CONFIG_SET_PROC(session_first_write_lifetimes)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  c3_uint_t durations[UA_NUMBER_OF_ELEMENTS];
  if (Configuration::get_durations(parser, args, num, durations)) {
    return session_optimizer.post_session_first_write_lifetimes_message(durations);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(session_first_write_nums)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_numbers(buff, length, session_optimizer.get_first_write_nums(),
    UA_NUMBER_OF_ELEMENTS);
}

static bool CONFIG_SET_PROC(session_first_write_nums)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t numbers[UA_NUMBER_OF_ELEMENTS];
  if (Configuration::get_numbers(parser, args, num, numbers, UA_NUMBER_OF_ELEMENTS, false)) {
    return session_optimizer.post_session_first_write_nums_message(numbers);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(session_default_lifetimes)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_durations(buff, length, session_optimizer.get_default_lifetimes());
}

static bool CONFIG_SET_PROC(session_default_lifetimes)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  c3_uint_t durations[UA_NUMBER_OF_ELEMENTS];
  if (Configuration::get_durations(parser, args, num, durations)) {
    return session_optimizer.post_session_default_lifetimes_message(durations);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(session_read_extra_lifetimes)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_durations(buff, length, session_optimizer.get_read_extra_lifetimes());
}

static bool CONFIG_SET_PROC(session_read_extra_lifetimes)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  c3_uint_t durations[UA_NUMBER_OF_ELEMENTS];
  if (Configuration::get_durations(parser, args, num, durations)) {
    return session_optimizer.post_session_read_extra_lifetimes_message(durations);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(fpc_default_lifetimes)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_durations(buff, length, fpc_optimizer.get_default_lifetimes());
}

static bool CONFIG_SET_PROC(fpc_default_lifetimes)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t durations[UA_NUMBER_OF_ELEMENTS];
  if (Configuration::get_durations(parser, args, num, durations)) {
    return fpc_optimizer.post_fpc_default_lifetimes_message(durations);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(fpc_read_extra_lifetimes)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_durations(buff, length, fpc_optimizer.get_read_extra_lifetimes());
}

static bool CONFIG_SET_PROC(fpc_read_extra_lifetimes)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t durations[UA_NUMBER_OF_ELEMENTS];
  if (Configuration::get_durations(parser, args, num, durations)) {
    return fpc_optimizer.post_fpc_read_extra_lifetimes_message(durations);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(fpc_max_lifetimes)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_durations(buff, length, fpc_optimizer.get_max_lifetimes());
}

static bool CONFIG_SET_PROC(fpc_max_lifetimes)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t durations[UA_NUMBER_OF_ELEMENTS];
  if (Configuration::get_durations(parser, args, num, durations)) {
    return fpc_optimizer.post_fpc_max_lifetimes_message(durations);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(session_eviction_mode)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_keyword(buff, length, session_optimizer.get_eviction_mode(),
    config_eviction_modes, EM_NUMBER_OF_ELEMENTS);
}

static bool CONFIG_SET_PROC(session_eviction_mode)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  int option = Configuration::get_single_keyword_index(parser, args, num,
    config_eviction_modes, EM_NUMBER_OF_ELEMENTS);
  if (option >= 0) {
    session_optimizer.post_config_eviction_mode_message((eviction_mode_t) option);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(fpc_eviction_mode)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_keyword(buff, length, fpc_optimizer.get_eviction_mode(),
    config_eviction_modes, EM_NUMBER_OF_ELEMENTS);
}

static bool CONFIG_SET_PROC(fpc_eviction_mode)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  int option = Configuration::get_single_keyword_index(parser, args, num,
    config_eviction_modes, EM_NUMBER_OF_ELEMENTS);
  if (option >= 0) {
    fpc_optimizer.post_config_eviction_mode_message((eviction_mode_t) option);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(session_optimization_interval)(Parser& parser, char* buff, size_t length) {
  return Parser::print_duration(buff, length, session_optimizer.get_optimization_interval());
}

static bool CONFIG_SET_PROC(session_optimization_interval)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  c3_uint_t duration;
  if (Configuration::get_duration(parser, args, num, duration)) {
    return session_optimizer.post_config_wait_time_message(duration);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(fpc_optimization_interval)(Parser& parser, char* buff, size_t length) {
  return Parser::print_duration(buff, length, fpc_optimizer.get_optimization_interval());
}

static bool CONFIG_SET_PROC(fpc_optimization_interval)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  c3_uint_t duration;
  if (Configuration::get_duration(parser, args, num, duration)) {
    return fpc_optimizer.post_config_wait_time_message(duration);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(session_optimization_compressors)(Parser& parser, char* buff,
  size_t length) {
  return Configuration::print_compressors(buff, length, session_optimizer);
}

static bool CONFIG_SET_PROC(session_optimization_compressors)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  return Configuration::get_compressors(parser, args, num, session_optimizer);
}

static ssize_t CONFIG_GET_PROC(fpc_optimization_compressors)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_compressors(buff, length, fpc_optimizer);
}

static bool CONFIG_SET_PROC(fpc_optimization_compressors)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  return Configuration::get_compressors(parser, args, num, fpc_optimizer);
}

static ssize_t CONFIG_GET_PROC(session_recompression_threshold)(Parser& parser, char* buff,
  size_t length) {
  return Configuration::print_number(buff, length, session_optimizer.get_recompression_threshold());
}

static bool CONFIG_SET_PROC(session_recompression_threshold)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::get_recompression_threshold(parser, args, num, session_optimizer);
}

static ssize_t CONFIG_GET_PROC(fpc_recompression_threshold)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, fpc_optimizer.get_recompression_threshold());
}

static bool CONFIG_SET_PROC(fpc_recompression_threshold)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::get_recompression_threshold(parser, args, num, fpc_optimizer);
}

static ssize_t CONFIG_GET_PROC(response_compression_threshold)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, server_net_config.get_compression_threshold());
}

static bool CONFIG_SET_PROC(response_compression_threshold)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_ulong_t threshold;
  if (Configuration::get_size(parser, args, num, threshold, 1, UINT_MAX_VAL)) {
    server_net_config.set_compression_threshold((c3_uint_t) threshold);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(session_tables_per_store)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, session_store.get_num_tables());
}

static bool CONFIG_SET_PROC(session_tables_per_store)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::set_num_tables(parser, args, num, session_store);
}

static ssize_t CONFIG_GET_PROC(fpc_tables_per_store)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, fpc_store.get_num_tables());
}

static bool CONFIG_SET_PROC(fpc_tables_per_store)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::set_num_tables(parser, args, num, fpc_store);
}

static ssize_t CONFIG_GET_PROC(tags_tables_per_store)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, tag_manager.get_num_tables());
}

static bool CONFIG_SET_PROC(tags_tables_per_store)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::set_num_tables(parser, args, num, tag_manager);
}

static ssize_t CONFIG_GET_PROC(health_check_interval)(Parser& parser, char* buff, size_t length) {
  return Parser::print_duration(buff, length, server.get_health_check_interval());
}

static bool CONFIG_SET_PROC(health_check_interval)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t interval;
  if (Configuration::get_duration(parser, args, num, interval)) {
    server.set_health_check_interval(interval);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(free_disk_space_warning_threshold)(Parser& parser, char* buff, size_t length) {
  c3_long_t size = server.get_free_disk_space_threshold();
  return Parser::print_size(buff, length, (c3_ulong_t) size);
}

static bool CONFIG_SET_PROC(free_disk_space_warning_threshold)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_ulong_t size;
  if (Configuration::get_size(parser, args, num, size)) {
    server.set_free_disk_space_threshold(size);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(thread_activity_time_warning_threshold)(Parser& parser, char* buff, size_t length) {
  // stored value is in microseconds, reported value is in milliseconds
  c3_uint_t msecs = (c3_uint_t)(server.get_thread_activity_threshold() / 1000);
  return Configuration::print_number(buff, length, msecs);
}

static bool CONFIG_SET_PROC(thread_activity_time_warning_threshold)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  c3_uint_t msecs;
  if (Configuration::get_number(parser, args, num, msecs, 0, 60 * 60 * 1000)) {
    // entered value is in milliseconds, while stored value is in microseconds
    server.set_thread_activity_threshold((c3_long_t) msecs * 1000);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(global_response_compressor)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_compressor(buff, length,
    server_net_config.get_compressor(DOMAIN_GLOBAL));
}

static bool CONFIG_SET_PROC(global_response_compressor)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  int index = Configuration::get_single_compressor_index(parser, args, num);
  if (index > 0) {
    server_net_config.set_compressor(DOMAIN_GLOBAL, (c3_compressor_t) index);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(session_response_compressor)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_compressor(buff, length,
    server_net_config.get_compressor(DOMAIN_SESSION));
}

static bool CONFIG_SET_PROC(session_response_compressor)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  int index = Configuration::get_single_compressor_index(parser, args, num);
  if (index > 0) {
    server_net_config.set_compressor(DOMAIN_SESSION, (c3_compressor_t) index);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(fpc_response_compressor)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_compressor(buff, length,
    server_net_config.get_compressor(DOMAIN_FPC));
}

static bool CONFIG_SET_PROC(fpc_response_compressor)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  int index = Configuration::get_single_compressor_index(parser, args, num);
  if (index > 0) {
    server_net_config.set_compressor(DOMAIN_FPC, (c3_compressor_t) index);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(command_integrity_check)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_boolean(buff, length, server_net_config.get_command_integrity_check());
}

static bool CONFIG_SET_PROC(command_integrity_check)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  bool value;
  if (Configuration::get_boolean(parser, args, num, value)) {
    server_net_config.set_command_integrity_check(value);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(response_integrity_check)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_boolean(buff, length, server_net_config.get_response_integrity_check());
}

static bool CONFIG_SET_PROC(response_integrity_check)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  bool value;
  if (Configuration::get_boolean(parser, args, num, value)) {
    server_net_config.set_response_integrity_check(value);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(binlog_integrity_check)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_boolean(buff, length, server_net_config.get_file_integrity_check());
}

static bool CONFIG_SET_PROC(binlog_integrity_check)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  bool value;
  if (Configuration::get_boolean(parser, args, num, value)) {
    server_net_config.set_file_integrity_check(value);
    return true;
  }
  return false;
}

static bool CONFIG_SET_PROC(session_binlog_file)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (Configuration::check_path(parser, args, num)) {
    return session_binlog.send_open_binlog_command(args[0].get_string());
  }
  return false;
}

static bool CONFIG_SET_PROC(fpc_binlog_file)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (Configuration::check_path(parser, args, num)) {
    return fpc_binlog.send_open_binlog_command(args[0].get_string());
  }
  return false;
}

static bool CONFIG_SET_PROC(session_binlog_rotation_path)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (Configuration::check_rotation_path(parser, args, num)) {
    return session_binlog.send_set_rotation_path_command(args[0].get_string());
  }
  return false;
}

static bool CONFIG_SET_PROC(fpc_binlog_rotation_path)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (Configuration::check_rotation_path(parser, args, num)) {
    return fpc_binlog.send_set_rotation_path_command(args[0].get_string());
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(session_binlog_rotation_threshold)(Parser& parser, char* buff, size_t length) {
  return Configuration::get_max_file_size(parser, buff, length, session_binlog);
}

static bool CONFIG_SET_PROC(session_binlog_rotation_threshold)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_ulong_t threshold;
  if (Configuration::get_size(parser, args, num, threshold,
    SessionBinlog::get_min_rotation_threshold(), SessionBinlog::get_max_rotation_threshold())) {
    return session_binlog.send_set_rotation_threshold(threshold);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(fpc_binlog_rotation_threshold)(Parser& parser, char* buff, size_t length) {
  return Configuration::get_max_file_size(parser, buff, length, fpc_binlog);
}

static bool CONFIG_SET_PROC(fpc_binlog_rotation_threshold)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_ulong_t threshold;
  if (Configuration::get_size(parser, args, num, threshold,
    PageBinlog::get_min_rotation_threshold(), PageBinlog::get_max_rotation_threshold())) {
    return fpc_binlog.send_set_rotation_threshold(threshold);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(session_binlog_sync)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_keyword(buff, length, session_binlog.get_sync_mode(),
    config_sync_modes, SM_NUMBER_OF_ELEMENTS);
}

static bool CONFIG_SET_PROC(session_binlog_sync)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  int option = Configuration::get_single_keyword_index(parser, args, num,
    config_sync_modes, SM_NUMBER_OF_ELEMENTS);
  if (option >= 0) {
    return session_binlog.send_set_sync_mode_command((sync_mode_t) option);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(fpc_binlog_sync)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_keyword(buff, length, fpc_binlog.get_sync_mode(),
    config_sync_modes, SM_NUMBER_OF_ELEMENTS);
}

static bool CONFIG_SET_PROC(fpc_binlog_sync)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  int option = Configuration::get_single_keyword_index(parser, args, num,
    config_sync_modes, SM_NUMBER_OF_ELEMENTS);
  if (option >= 0) {
    return fpc_binlog.send_set_sync_mode_command((sync_mode_t) option);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(session_db_sync)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_keyword(buff, length, server.get_session_db_sync_mode(),
    config_sync_modes, SM_NUMBER_OF_ELEMENTS);
}

static bool CONFIG_SET_PROC(session_db_sync)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  int option = Configuration::get_single_keyword_index(parser, args, num,
    config_sync_modes, SM_NUMBER_OF_ELEMENTS);
  if (option >= 0) {
    server.set_session_db_sync_mode((sync_mode_t) option);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(fpc_db_sync)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_keyword(buff, length, server.get_fpc_db_sync_mode(),
    config_sync_modes, SM_NUMBER_OF_ELEMENTS);
}

static bool CONFIG_SET_PROC(fpc_db_sync)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  int option = Configuration::get_single_keyword_index(parser, args, num,
    config_sync_modes, SM_NUMBER_OF_ELEMENTS);
  if (option >= 0) {
    server.set_fpc_db_sync_mode((sync_mode_t) option);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(session_db_include)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_keyword(buff, length, server.get_session_db_included_agents(),
    config_user_agents, UA_NUMBER_OF_ELEMENTS);
}

static bool CONFIG_SET_PROC(session_db_include)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  int option = Configuration::get_single_keyword_index(parser, args, num,
    config_user_agents, UA_NUMBER_OF_ELEMENTS);
  if (option >= 0) {
    server.set_session_db_included_agents((user_agent_t) option);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(fpc_db_include)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_keyword(buff, length, server.get_fpc_db_included_agents(),
    config_user_agents, UA_NUMBER_OF_ELEMENTS);
}

static bool CONFIG_SET_PROC(fpc_db_include)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  int option = Configuration::get_single_keyword_index(parser, args, num,
    config_user_agents, UA_NUMBER_OF_ELEMENTS);
  if (option >= 0) {
    server.set_fpc_db_included_agents((user_agent_t) option);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(session_db_file)(Parser& parser, char* buff, size_t length) {
  return std::snprintf(buff, length, "%s", server.get_session_db_file_name());
}

static bool CONFIG_SET_PROC(session_db_file)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (Configuration::check_path(parser, args, num)) {
    server.set_session_db_file_name(args[0].get_string());
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(fpc_db_file)(Parser& parser, char* buff, size_t length) {
  return std::snprintf(buff, length, "%s", server.get_fpc_db_file_name());
}

static bool CONFIG_SET_PROC(fpc_db_file)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (Configuration::check_path(parser, args, num)) {
    server.set_fpc_db_file_name(args[0].get_string());
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(session_auto_save_interval)(Parser& parser, char* buff, size_t length) {
  return Parser::print_duration(buff, length, server.get_session_autosave_interval());
}

static bool CONFIG_SET_PROC(session_auto_save_interval)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t duration;
  if (Configuration::get_duration(parser, args, num, duration)) {
    server.set_session_autosave_interval(duration);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(fpc_auto_save_interval)(Parser& parser, char* buff, size_t length) {
  return Parser::print_duration(buff, length, server.get_fpc_autosave_interval());
}

static bool CONFIG_SET_PROC(fpc_auto_save_interval)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t duration;
  if (Configuration::get_duration(parser, args, num, duration)) {
    server.set_fpc_autosave_interval(duration);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(table_hash_method)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_keyword(buff, length, table_hasher.get_method(),
    config_hashers, HM_NUMBER_OF_ELEMENTS);
}

static bool CONFIG_SET_PROC(table_hash_method)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  int option = Configuration::get_single_keyword_index(parser, args, num,
    config_hashers, HM_NUMBER_OF_ELEMENTS);
  if (option >= 0) {
    if (!parser.is_interactive()) {
      // if server is not in CONFIG state, we fail silently
      if (server.get_state() <= SS_CONFIG) {
        table_hasher.set_method((c3_hash_method_t) option);
      }
      return true;
    }
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(password_hash_method)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_keyword(buff, length, password_hasher.get_method(),
    config_hashers, HM_NUMBER_OF_ELEMENTS);
}

static bool CONFIG_SET_PROC(password_hash_method)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  int option = Configuration::get_single_keyword_index(parser, args, num,
    config_hashers, HM_NUMBER_OF_ELEMENTS);
  if (option >= 0) {
    if (!parser.is_interactive()) {
      // if server is not in CONFIG state, we fail silently
      if (server.get_state() <= SS_CONFIG) {
        password_hasher.set_method((c3_hash_method_t) option);
      }
      return true;
    }
  }
  return false;
}

static bool CONFIG_SET_PROC(include)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (Configuration::check_path(parser, args, num)) {
    c3_uint_t nested_level = parser.get_nesting_level() + 1;
    if (nested_level < MAX_CONFIG_INCLUDE_LEVEL) {
      char path[MAX_FILE_PATH_LENGTH];
      std::strcpy(path, args[0].get_string());
      if (path[0] != '.' && path[0] != '/') {
        const char* base_path = parser.get_file_path();
        if (base_path != nullptr) {
          const char* dir_char = std::strrchr(base_path, '/');
          if (dir_char != nullptr) {
            size_t dir_len = dir_char - base_path;
            std::memcpy(path, base_path, dir_len);
            std::strcpy(path + dir_len, args[0].get_string());
          }
        }
      }
      ConfigParser nested_parser(nested_level);
      return nested_parser.parse(path);
    }
    parser.log(LL_ERROR, "include '%s' from '%s': not more than %u recursive inclusions allowed",
      args[0].get_string(), parser.get_file_path(), MAX_CONFIG_INCLUDE_LEVEL - 1);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(version)(Parser& parser, char* buff, size_t length) {
  return std::snprintf(buff, length, "%s", c3lib_full_version_string);
}

static ssize_t CONFIG_GET_PROC(perf_dealloc_chunk_size)(Parser& parser, char* buff, size_t length) {
  return Parser::print_size(buff, length, server.get_dealloc_chunk_size());
}

static bool CONFIG_SET_PROC(perf_dealloc_chunk_size)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_ulong_t chunk_size;
  if (Configuration::get_size(parser, args, num, chunk_size)) {
    server.set_dealloc_chunk_size(chunk_size);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_dealloc_max_wait_time)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, server.get_dealloc_max_wait_time());
}

static bool CONFIG_SET_PROC(perf_dealloc_max_wait_time)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t max_wait_time;
  if (Configuration::get_number(parser, args, num, max_wait_time)) {
    server.set_dealloc_max_wait_time(max_wait_time);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_store_wait_time)(Parser& parser, char* buff, size_t length) {
  return Parser::print_duration(buff, length, server.get_store_wait_time());
}

static bool CONFIG_SET_PROC(perf_store_wait_time)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t duration;
  if (Configuration::get_duration(parser, args, num, duration)) {
    server.set_store_wait_time(duration);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_store_max_wait_time)(Parser& parser, char* buff, size_t length) {
  return Parser::print_duration(buff, length, server.get_store_max_wait_time());
}

static bool CONFIG_SET_PROC(perf_store_max_wait_time)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t duration;
  if (Configuration::get_duration(parser, args, num, duration)) {
    server.set_store_max_wait_time(duration);
    return true;
  }
  return false;
}

static bool CONFIG_SET_PROC(perf_session_opt_num_checks)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  const c3_uint_t num_slots = Optimizer::get_num_cpu_load_levels();
  c3_uint_t slots[num_slots];
  if (Configuration::get_numbers(parser, args, num, slots, num_slots, false)) {
    return session_optimizer.post_config_num_checks_message(slots);
  }
  return false;
}

static bool CONFIG_SET_PROC(perf_fpc_opt_num_checks)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  const c3_uint_t num_slots = Optimizer::get_num_cpu_load_levels();
  c3_uint_t slots[num_slots];
  if (Configuration::get_numbers(parser, args, num, slots, num_slots, false)) {
    return fpc_optimizer.post_config_num_checks_message(slots);
  }
  return false;
}

static bool CONFIG_SET_PROC(perf_session_opt_num_comp_attempts)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  const c3_uint_t num_slots = Optimizer::get_num_cpu_load_levels();
  c3_uint_t slots[num_slots];
  if (Configuration::get_numbers(parser, args, num, slots, num_slots, false)) {
    return session_optimizer.post_config_num_comp_attempts_message(slots);
  }
  return false;
}

static bool CONFIG_SET_PROC(perf_fpc_opt_num_comp_attempts)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  const c3_uint_t num_slots = Optimizer::get_num_cpu_load_levels();
  c3_uint_t slots[num_slots];
  if (Configuration::get_numbers(parser, args, num, slots, num_slots, false)) {
    return fpc_optimizer.post_config_num_comp_attempts_message(slots);
  }
  return false;
}

static bool CONFIG_SET_PROC(perf_session_opt_retain_counts)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t slots[UA_NUMBER_OF_ELEMENTS];
  if (Configuration::get_numbers(parser, args, num, slots, UA_NUMBER_OF_ELEMENTS, true)) {
    return session_optimizer.post_config_retain_counts_message(slots);
  }
  return false;
}

static bool CONFIG_SET_PROC(perf_fpc_opt_retain_counts)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t slots[UA_NUMBER_OF_ELEMENTS];
  if (Configuration::get_numbers(parser, args, num, slots, UA_NUMBER_OF_ELEMENTS, true)) {
    return fpc_optimizer.post_config_retain_counts_message(slots);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_session_unlinking_quotas)(Parser& parser, char* buff, size_t length) {
  c3_uint_t slots[2];
  session_store.get_unlinking_quotas(slots[0], slots[1]);
  return Configuration::print_numbers(buff, length, slots, 2);
}

static bool CONFIG_SET_PROC(perf_session_unlinking_quotas)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t slots[2]; // while rebuilding, while not rebuilding the table
  if (Configuration::get_numbers(parser, args, num, slots, 2, true)) {
    session_store.set_unlinking_quotas(slots[0], slots[1]);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_fpc_unlinking_quotas)(Parser& parser, char* buff, size_t length) {
  c3_uint_t slots[2];
  fpc_store.get_unlinking_quotas(slots[0], slots[1]);
  return Configuration::print_numbers(buff, length, slots, 2);
}

static bool CONFIG_SET_PROC(perf_fpc_unlinking_quotas)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t slots[2]; // while rebuilding, while not rebuilding the table
  if (Configuration::get_numbers(parser, args, num, slots, 2, true)) {
    fpc_store.set_unlinking_quotas(slots[0], slots[1]);
    return true;
  }
  return false;
}

#if C3_ENTERPRISE
static ssize_t CONFIG_GET_PROC(perf_num_internal_tag_refs)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, PageObject::get_num_internal_tag_refs());
}

static bool CONFIG_SET_PROC(perf_num_internal_tag_refs)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t num_refs;
  if (Configuration::get_number(parser, args, num, num_refs, 1, MAX_NUM_INTERNAL_TAG_REFS)) {
    if (!parser.is_interactive()) {
      // if server is not in CONFIG state, we fail silently
      if (server.get_state() <= SS_CONFIG) {
        PageObject::set_num_internal_tag_refs(num_refs);
      }
      return true;
    }
  }
  return false;
}
#endif // C3_ENTERPRISE

static ssize_t CONFIG_GET_PROC(perf_thread_wait_quit_time)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, server.get_thread_quit_time());
}

static bool CONFIG_SET_PROC(perf_thread_wait_quit_time)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t quit_time;
  if (Configuration::get_number(parser, args, num, quit_time)) {
    server.set_thread_quit_time(quit_time);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_session_table_fill_factor)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_fill_factor(buff, length, session_store);
}

static bool CONFIG_SET_PROC(perf_session_table_fill_factor)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::set_fill_factor(parser, args, num, session_store);
}

static ssize_t CONFIG_GET_PROC(perf_fpc_table_fill_factor)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_fill_factor(buff, length, fpc_store);
}

static bool CONFIG_SET_PROC(perf_fpc_table_fill_factor)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::set_fill_factor(parser, args, num, fpc_store);
}

static ssize_t CONFIG_GET_PROC(perf_tags_table_fill_factor)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_fill_factor(buff, length, tag_manager);
}

static bool CONFIG_SET_PROC(perf_tags_table_fill_factor)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::set_fill_factor(parser, args, num, tag_manager);
}

static bool CONFIG_SET_PROC(perf_session_init_table_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::set_init_capacity(parser, args, num, session_store);
}

static bool CONFIG_SET_PROC(perf_fpc_init_table_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::set_init_capacity(parser, args, num, fpc_store);
}

static bool CONFIG_SET_PROC(perf_tags_init_table_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return Configuration::set_init_capacity(parser, args, num, tag_manager);
}

static ssize_t CONFIG_GET_PROC(perf_session_opt_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, session_optimizer.get_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_session_opt_queue_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t capacity;
  if (Configuration::get_number(parser, args, num, capacity)) {
    return session_optimizer.post_queue_capacity_message(capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_fpc_opt_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, fpc_optimizer.get_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_fpc_opt_queue_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t capacity;
  if (Configuration::get_number(parser, args, num, capacity)) {
    return fpc_optimizer.post_queue_capacity_message(capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_session_opt_max_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, session_optimizer.get_max_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_session_opt_max_queue_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t max_capacity;
  if (Configuration::get_number(parser, args, num, max_capacity)) {
    return session_optimizer.post_queue_max_capacity_message(max_capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_fpc_opt_max_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, fpc_optimizer.get_max_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_fpc_opt_max_queue_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t max_capacity;
  if (Configuration::get_number(parser, args, num, max_capacity)) {
    return fpc_optimizer.post_queue_max_capacity_message(max_capacity);
  }
  return false;
}

static bool CONFIG_SET_PROC(perf_session_store_queue_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t capacity;
  if (Configuration::get_number(parser, args, num, capacity)) {
    session_store.set_queue_capacity(capacity);
    return true;
  }
  return false;
}

static bool CONFIG_SET_PROC(perf_fpc_store_queue_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t capacity;
  if (Configuration::get_number(parser, args, num, capacity)) {
    fpc_store.set_queue_capacity(capacity);
    return true;
  }
  return false;
}

static bool CONFIG_SET_PROC(perf_session_store_max_queue_capacity)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  c3_uint_t max_capacity;
  if (Configuration::get_number(parser, args, num, max_capacity)) {
    session_store.set_max_queue_capacity(max_capacity);
    return true;
  }
  return false;
}

static bool CONFIG_SET_PROC(perf_fpc_store_max_queue_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t max_capacity;
  if (Configuration::get_number(parser, args, num, max_capacity)) {
    fpc_store.set_max_queue_capacity(max_capacity);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_tag_manager_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, tag_manager.get_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_tag_manager_queue_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t capacity;
  if (Configuration::get_number(parser, args, num, capacity)) {
    return tag_manager.post_capacity_change_message(capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_tag_manager_max_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, tag_manager.get_max_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_tag_manager_max_queue_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t max_capacity;
  if (Configuration::get_number(parser, args, num, max_capacity)) {
    return tag_manager.post_max_capacity_change_message(max_capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_log_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, server_logger.get_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_log_queue_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t capacity;
  if (Configuration::get_number(parser, args, num, capacity)) {
    return server_logger.send_capacity_change_command(capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_log_max_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, server_logger.get_max_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_log_max_queue_capacity)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  c3_uint_t capacity;
  if (Configuration::get_number(parser, args, num, capacity)) {
    return server_logger.send_max_capacity_change_command(capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_session_binlog_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, session_binlog.get_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_session_binlog_queue_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t capacity;
  if (Configuration::get_number(parser, args, num, capacity)) {
    return session_binlog.send_set_capacity_command(capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_session_binlog_max_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, session_binlog.get_max_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_session_binlog_max_queue_capacity)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  c3_uint_t max_capacity;
  if (Configuration::get_number(parser, args, num, max_capacity)) {
    return session_binlog.send_set_max_capacity_command(max_capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_fpc_binlog_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, fpc_binlog.get_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_fpc_binlog_queue_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t capacity;
  if (Configuration::get_number(parser, args, num, capacity)) {
    return fpc_binlog.send_set_capacity_command(capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_fpc_binlog_max_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, fpc_binlog.get_max_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_fpc_binlog_max_queue_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t max_capacity;
  if (Configuration::get_number(parser, args, num, max_capacity)) {
    return fpc_binlog.send_set_max_capacity_command(max_capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_binlog_loader_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, binlog_loader.get_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_binlog_loader_queue_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t capacity;
  if (Configuration::get_number(parser, args, num, capacity)) {
    return binlog_loader.send_set_capacity_command(capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_binlog_loader_max_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, binlog_loader.get_max_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_binlog_loader_max_queue_capacity)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  c3_uint_t max_capacity;
  if (Configuration::get_number(parser, args, num, max_capacity)) {
    return binlog_loader.send_set_max_capacity_command(max_capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_listener_input_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, server_listener.get_input_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_listener_input_queue_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t capacity;
  if (Configuration::get_number(parser, args, num, capacity)) {
    return server_listener.send_input_queue_capacity_change_command(capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_listener_input_queue_max_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, server_listener.get_max_input_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_listener_input_queue_max_capacity)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  c3_uint_t max_capacity;
  if (Configuration::get_number(parser, args, num, max_capacity)) {
    return server_listener.send_max_input_queue_capacity_change_command(max_capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_listener_output_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, server_listener.get_output_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_listener_output_queue_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t capacity;
  if (Configuration::get_number(parser, args, num, capacity)) {
    return server_listener.send_output_queue_capacity_change_command(capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_listener_output_queue_max_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, server_listener.get_max_output_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_listener_output_queue_max_capacity)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  c3_uint_t max_capacity;
  if (Configuration::get_number(parser, args, num, max_capacity)) {
    return server_listener.send_max_output_queue_capacity_change_command(max_capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_session_replicator_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, session_replicator.get_input_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_session_replicator_queue_capacity)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  c3_uint_t capacity;
  if (Configuration::get_number(parser, args, num, capacity)) {
    return session_replicator.send_input_queue_capacity_change_command(capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_session_replicator_max_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, session_replicator.get_max_input_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_session_replicator_max_queue_capacity)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  c3_uint_t max_capacity;
  if (Configuration::get_number(parser, args, num, max_capacity)) {
    return session_replicator.send_max_input_queue_capacity_change_command(max_capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_fpc_replicator_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, fpc_replicator.get_input_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_fpc_replicator_queue_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t capacity;
  if (Configuration::get_number(parser, args, num, capacity)) {
    return fpc_replicator.send_input_queue_capacity_change_command(capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_fpc_replicator_max_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, fpc_replicator.get_max_input_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_fpc_replicator_max_queue_capacity)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  c3_uint_t max_capacity;
  if (Configuration::get_number(parser, args, num, max_capacity)) {
    return fpc_replicator.send_max_input_queue_capacity_change_command(max_capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_session_replicator_local_queue_capacity)(Parser& parser, char* buff,
  size_t length) {
  return Configuration::print_number(buff, length, session_replicator.get_local_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_session_replicator_local_queue_capacity)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  c3_uint_t capacity;
  if (Configuration::get_number(parser, args, num, capacity)) {
    return session_replicator.send_local_queue_capacity_change_command(capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_session_replicator_local_max_queue_capacity)(Parser& parser, char* buff,
  size_t length) {
  return Configuration::print_number(buff, length, session_replicator.get_local_queue_max_capacity());
}

static bool CONFIG_SET_PROC(perf_session_replicator_local_max_queue_capacity)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  c3_uint_t capacity;
  if (Configuration::get_number(parser, args, num, capacity)) {
    return session_replicator.send_max_local_queue_capacity_change_command(capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_fpc_replicator_local_queue_capacity)(Parser& parser, char* buff,
  size_t length) {
  return Configuration::print_number(buff, length, fpc_replicator.get_local_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_fpc_replicator_local_queue_capacity)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  c3_uint_t capacity;
  if (Configuration::get_number(parser, args, num, capacity)) {
    return fpc_replicator.send_local_queue_capacity_change_command(capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_fpc_replicator_local_max_queue_capacity)(Parser& parser, char* buff,
  size_t length) {
  return Configuration::print_number(buff, length, fpc_replicator.get_local_queue_max_capacity());
}

static bool CONFIG_SET_PROC(perf_fpc_replicator_local_max_queue_capacity)(Parser& parser, parser_token_t* args,
  c3_uint_t num) {
  c3_uint_t capacity;
  if (Configuration::get_number(parser, args, num, capacity)) {
    return fpc_replicator.send_max_local_queue_capacity_change_command(capacity);
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_config_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, server.get_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_config_queue_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t capacity;
  if (Configuration::get_number(parser, args, num, capacity)) {
    server.set_queue_capacity(capacity);
    return true;
  }
  return false;
}

static ssize_t CONFIG_GET_PROC(perf_config_max_queue_capacity)(Parser& parser, char* buff, size_t length) {
  return Configuration::print_number(buff, length, server.get_max_queue_capacity());
}

static bool CONFIG_SET_PROC(perf_config_max_queue_capacity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t max_capacity;
  if (Configuration::get_number(parser, args, num, max_capacity)) {
    server.set_max_queue_capacity(max_capacity);
    return true;
  }
  return false;
}

static parser_command_t server_options[] = {
  PARSER_ENTRY(max_memory),
  PARSER_ENTRY(max_session_memory),
  PARSER_ENTRY(max_fpc_memory),
  PARSER_SET_ENTRY(listener_addresses),
  PARSER_SET_ENTRY(listener_port),
  PARSER_SET_ENTRY(session_replicator_addresses),
  PARSER_SET_ENTRY(session_replicator_port),
  PARSER_SET_ENTRY(fpc_replicator_addresses),
  PARSER_SET_ENTRY(fpc_replicator_port),
  PARSER_ENTRY(listener_persistent),
  PARSER_ENTRY(session_replicator_persistent),
  PARSER_ENTRY(fpc_replicator_persistent),
  PARSER_SET_ENTRY(user_password),
  PARSER_SET_ENTRY(admin_password),
  PARSER_SET_ENTRY(bulk_password),
  PARSER_ENTRY(info_password_type),
  PARSER_ENTRY(log_level),
  PARSER_SET_ENTRY(log_file),
  PARSER_ENTRY(log_rotation_threshold),
  PARSER_SET_ENTRY(log_rotation_path),
  PARSER_ENTRY(num_connection_threads),
  PARSER_ENTRY(session_lock_wait_time),
  PARSER_ENTRY(session_first_write_lifetimes),
  PARSER_ENTRY(session_first_write_nums),
  PARSER_ENTRY(session_default_lifetimes),
  PARSER_ENTRY(session_read_extra_lifetimes),
  PARSER_ENTRY(fpc_default_lifetimes),
  PARSER_ENTRY(fpc_read_extra_lifetimes),
  PARSER_ENTRY(fpc_max_lifetimes),
  PARSER_ENTRY(session_eviction_mode),
  PARSER_ENTRY(fpc_eviction_mode),
  PARSER_ENTRY(session_optimization_interval),
  PARSER_ENTRY(fpc_optimization_interval),
  PARSER_ENTRY(session_optimization_compressors),
  PARSER_ENTRY(fpc_optimization_compressors),
  PARSER_ENTRY(session_recompression_threshold),
  PARSER_ENTRY(fpc_recompression_threshold),
  PARSER_ENTRY(response_compression_threshold),
  PARSER_ENTRY(session_tables_per_store),
  PARSER_ENTRY(fpc_tables_per_store),
  PARSER_ENTRY(tags_tables_per_store),
  PARSER_ENTRY(health_check_interval),
  PARSER_ENTRY(free_disk_space_warning_threshold),
  PARSER_ENTRY(thread_activity_time_warning_threshold),
  PARSER_ENTRY(global_response_compressor),
  PARSER_ENTRY(session_response_compressor),
  PARSER_ENTRY(fpc_response_compressor),
  PARSER_ENTRY(command_integrity_check),
  PARSER_ENTRY(response_integrity_check),
  PARSER_ENTRY(binlog_integrity_check),
  PARSER_SET_ENTRY(session_binlog_file),
  PARSER_SET_ENTRY(fpc_binlog_file),
  PARSER_SET_ENTRY(session_binlog_rotation_path),
  PARSER_SET_ENTRY(fpc_binlog_rotation_path),
  PARSER_ENTRY(session_binlog_rotation_threshold),
  PARSER_ENTRY(fpc_binlog_rotation_threshold),
  PARSER_ENTRY(session_binlog_sync),
  PARSER_ENTRY(fpc_binlog_sync),
  PARSER_ENTRY(session_db_sync),
  PARSER_ENTRY(fpc_db_sync),
  PARSER_ENTRY(session_db_include),
  PARSER_ENTRY(fpc_db_include),
  PARSER_ENTRY(session_db_file),
  PARSER_ENTRY(fpc_db_file),
  PARSER_ENTRY(session_auto_save_interval),
  PARSER_ENTRY(fpc_auto_save_interval),
  PARSER_ENTRY(table_hash_method),
  PARSER_ENTRY(password_hash_method),
  PARSER_GET_ENTRY(version),
  PARSER_SET_ENTRY(include),
  PARSER_ENTRY(perf_dealloc_chunk_size),
  PARSER_ENTRY(perf_dealloc_max_wait_time),
  PARSER_ENTRY(perf_store_wait_time),
  PARSER_ENTRY(perf_store_max_wait_time),
  PARSER_SET_ENTRY(perf_session_opt_num_checks),
  PARSER_SET_ENTRY(perf_fpc_opt_num_checks),
  PARSER_SET_ENTRY(perf_session_opt_num_comp_attempts),
  PARSER_SET_ENTRY(perf_fpc_opt_num_comp_attempts),
  PARSER_SET_ENTRY(perf_session_opt_retain_counts),
  PARSER_SET_ENTRY(perf_fpc_opt_retain_counts),
  PARSER_ENTRY(perf_session_unlinking_quotas),
  PARSER_ENTRY(perf_fpc_unlinking_quotas),
#if C3_ENTERPRISE
  PARSER_ENTRY(perf_num_internal_tag_refs),
#endif // C3_ENTERPRISE
  PARSER_ENTRY(perf_thread_wait_quit_time),
  PARSER_ENTRY(perf_session_table_fill_factor),
  PARSER_ENTRY(perf_fpc_table_fill_factor),
  PARSER_ENTRY(perf_tags_table_fill_factor),
  PARSER_SET_ENTRY(perf_session_init_table_capacity),
  PARSER_SET_ENTRY(perf_fpc_init_table_capacity),
  PARSER_SET_ENTRY(perf_tags_init_table_capacity),
  PARSER_ENTRY(perf_session_opt_queue_capacity),
  PARSER_ENTRY(perf_fpc_opt_queue_capacity),
  PARSER_ENTRY(perf_session_opt_max_queue_capacity),
  PARSER_ENTRY(perf_fpc_opt_max_queue_capacity),
  PARSER_SET_ENTRY(perf_session_store_queue_capacity),
  PARSER_SET_ENTRY(perf_fpc_store_queue_capacity),
  PARSER_SET_ENTRY(perf_session_store_max_queue_capacity),
  PARSER_SET_ENTRY(perf_fpc_store_max_queue_capacity),
  PARSER_ENTRY(perf_tag_manager_queue_capacity),
  PARSER_ENTRY(perf_tag_manager_max_queue_capacity),
  PARSER_ENTRY(perf_log_queue_capacity),
  PARSER_ENTRY(perf_log_max_queue_capacity),
  PARSER_ENTRY(perf_session_binlog_queue_capacity),
  PARSER_ENTRY(perf_session_binlog_max_queue_capacity),
  PARSER_ENTRY(perf_fpc_binlog_queue_capacity),
  PARSER_ENTRY(perf_fpc_binlog_max_queue_capacity),
  PARSER_ENTRY(perf_binlog_loader_queue_capacity),
  PARSER_ENTRY(perf_binlog_loader_max_queue_capacity),
  PARSER_ENTRY(perf_listener_input_queue_capacity),
  PARSER_ENTRY(perf_listener_input_queue_max_capacity),
  PARSER_ENTRY(perf_listener_output_queue_capacity),
  PARSER_ENTRY(perf_listener_output_queue_max_capacity),
  PARSER_ENTRY(perf_session_replicator_queue_capacity),
  PARSER_ENTRY(perf_session_replicator_max_queue_capacity),
  PARSER_ENTRY(perf_fpc_replicator_queue_capacity),
  PARSER_ENTRY(perf_fpc_replicator_max_queue_capacity),
  PARSER_ENTRY(perf_session_replicator_local_queue_capacity),
  PARSER_ENTRY(perf_session_replicator_local_max_queue_capacity),
  PARSER_ENTRY(perf_fpc_replicator_local_queue_capacity),
  PARSER_ENTRY(perf_fpc_replicator_local_max_queue_capacity),
  PARSER_ENTRY(perf_config_queue_capacity),
  PARSER_ENTRY(perf_config_max_queue_capacity)
};

constexpr size_t NUM_SERVER_OPTIONS = sizeof(server_options) / sizeof(parser_command_t);

///////////////////////////////////////////////////////////////////////////////
// ConfigParser
///////////////////////////////////////////////////////////////////////////////

ConfigParser::ConfigParser(c3_uint_t level):
  Parser(level, server_options, NUM_SERVER_OPTIONS) {
}

///////////////////////////////////////////////////////////////////////////////
// Configuration
///////////////////////////////////////////////////////////////////////////////

Configuration configuration;

Configuration::Configuration() noexcept: c_parser(0) {
  static_assert(PT_NUMBER_OF_ELEMENTS == 3, "Number of password types has changed");
  config_password_types[PT_NO_PASSWORD] = "none";
  config_password_types[PT_USER_PASSWORD] = "user";
  config_password_types[PT_ADMIN_PASSWORD] = "admin";

  static_assert(LL_NUMBER_OF_ELEMENTS == 9, "Number of log levels has changed");
  config_log_levels[LL_INVALID] = nullptr;
  config_log_levels[LL_EXPLICIT] = "explicit";
  config_log_levels[LL_FATAL] = "fatal";
  config_log_levels[LL_ERROR] = "error";
  config_log_levels[LL_WARNING] = "warning";
  config_log_levels[LL_TERSE] = "terse";
  config_log_levels[LL_NORMAL] = "normal";
  config_log_levels[LL_VERBOSE] = "verbose";
  config_log_levels[LL_DEBUG] = "debug";

  static_assert(EM_NUMBER_OF_ELEMENTS == 5, "Number of eviction modes has changed");
  config_eviction_modes[EM_INVALID] = nullptr;
  config_eviction_modes[EM_STRICT_EXPIRATION_LRU] = "strict-expiration-lru";
  config_eviction_modes[EM_EXPIRATION_LRU] = "expiration-lru";
  config_eviction_modes[EM_LRU] = "lru";
  config_eviction_modes[EM_STRICT_LRU] = "strict-lru";

  static_assert(CT_NUMBER_OF_ELEMENTS == 9, "Number of compression types has changed");
  config_compressors[CT_NONE] = nullptr;
  config_compressors[CT_LZF] = "lzf";
  config_compressors[CT_SNAPPY] = "snappy";
  config_compressors[CT_LZ4] = "lz4";
  config_compressors[CT_LZSS3] = "lzss3";
  config_compressors[CT_BROTLI] = "brotli";
  config_compressors[CT_ZSTD] = "zstd";
  config_compressors[CT_ZLIB] = "zlib";
  config_compressors[CT_LZHAM] = "lzham";

  static_assert(HM_NUMBER_OF_ELEMENTS == 6, "Number of hash methods has changed");
  config_hashers[HM_INVALID] = nullptr;
  config_hashers[HM_XXHASH] = "xxhash";
  config_hashers[HM_FARMHASH] = "farmhash";
  config_hashers[HM_SPOOKYHASH] = "spookyhash";
  config_hashers[HM_MURMURHASH2] = "murmurhash2";
  config_hashers[HM_MURMURHASH3] = "murmurhash3";

  static_assert(SM_NUMBER_OF_ELEMENTS == 3, "Number of synchronization modes has changed");
  config_sync_modes[SM_NONE] = "none";
  config_sync_modes[SM_DATA_ONLY] = "data-only";
  config_sync_modes[SM_FULL] = "full";

  static_assert(UA_NUMBER_OF_ELEMENTS == 4, "Number of user agent types has changed");
  config_user_agents[UA_UNKNOWN] = "unknown";
  config_user_agents[UA_BOT] = "bot";
  config_user_agents[UA_WARMER] = "warmer";
  config_user_agents[UA_USER] = "user";
  /*
   * We initialize option array only once; if we later create a new instance of the server configuration
   * parser using the same set of options, sorting option array won't be necessary.
   */
  Parser::initialize_commands(server_options, NUM_SERVER_OPTIONS);
}

bool Configuration::set_option(const char* option, c3_uint_t length, bool interactive) {
  c3_assert(option && length);
  return c_parser.parse("option", option, length, interactive);
}

bool Configuration::set_option(char short_option, const char* value, c3_uint_t length) {
  /*
   * In addition to the below "short" options, server also recognizes "-h" (for "--help",) and "-v" (for
   * "--version"), but those are command-line-only, and are processed by server upon startup.
   */
  const char* option;
  switch (short_option) {
    case 'i':
      option = "include";
      break;
    case 'l':
      option = "log_level";
      break;
    case 'n':
      option = "num_connection_threads";
      break;
    case 'm':
      option = "max_memory";
      break;
    case 's':
      option = "max_session_memory";
      break;
    case 'f':
      option = "max_fpc_memory";
      break;
    case 'a':
      option = "listener_addresses";
      break;
    case 'p':
      option = "listener_port";
      break;
    default:
      option = nullptr;
  }
  if (option != nullptr) {
    size_t option_length = std::strlen(option);
    size_t full_length = option_length + (length > 0? 1: 0) + length;
    c3_assert(full_length <= MAX_FILE_PATH_LENGTH + 2);
    char full_option[full_length];
    std::memcpy(full_option, option, option_length);
    if (length > 0) {
      full_option[option_length] = ' '; // separator
      std::memcpy(full_option + option_length + 1, value, length);
    }
    return set_option(full_option, (c3_uint_t) full_length, false);
  } else {
    c_parser.log(LL_ERROR, "Unknown short option: '-%c'", short_option);
    return false;
  }
}

} // CyberCache
