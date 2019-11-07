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
 * Server configuration object (this module and its classes are logically part of the server object).
 */
#ifndef _CC_CONFIGURATION_H
#define _CC_CONFIGURATION_H

#include "c3lib/c3lib.h"
#include "ls_system_logger.h"
#include "ht_optimizer.h"
#include "pl_socket_pipelines.h"

namespace CyberCache {

/// Parser that will be populated with commands representing configuration options
class ConfigParser: public Parser, public SystemLogger {
public:
  explicit ConfigParser(c3_uint_t level) C3_FUNC_COLD;
};

/// Server configuration
class Configuration {
  static constexpr c3_uint_t MAX_DURATION = days2seconds(365);
  /**
   * Root configuration parser object.
   *
   * Getters and setters still require their own `Parser` arguments, since the arguments passed to them are
   * "current" parsers, which could have been instantiated upon encountering an `include` statement;
   * hence, they are implemented as `static` methods.
   */
  ConfigParser c_parser;

public:
  Configuration() noexcept C3_FUNC_COLD;

  /////////////////////////////////////////////////////////////////////////////
  // HELPERS USED BY OPTIONS' GETTERS/SETTERS
  /////////////////////////////////////////////////////////////////////////////

  // check number of arguments specified to an option
  static bool require_arguments(Parser& parser, c3_uint_t num, const char* type,
    c3_uint_t min_num, c3_uint_t max_num) C3_FUNC_COLD;
  static bool require_single_argument(Parser& parser, c3_uint_t num, const char* type) C3_FUNC_COLD;
  static bool require_single_number_argument(Parser& parser, c3_uint_t num) C3_FUNC_COLD;

  // verification helpers
  static void check_order(Parser& parser, parser_token_t* args, c3_uint_t num, const c3_uint_t* values,
    const char* type, bool ascending) C3_FUNC_COLD;
  static bool check_password(Parser& parser, parser_token_t* args, c3_uint_t num) C3_FUNC_COLD;
  static bool check_path(Parser& parser, parser_token_t* args, c3_uint_t num) C3_FUNC_COLD;
  static bool check_rotation_path(Parser& parser, parser_token_t* args, c3_uint_t num) C3_FUNC_COLD;

  // output/fetch a boolean
  static ssize_t print_boolean(char* buff, size_t length, bool value) C3_FUNC_COLD;
  static bool get_boolean(Parser& parser, parser_token_t* args, c3_uint_t num, bool& value) C3_FUNC_COLD;

  // output/fetch one or several 32-bit unsigned integers
  static ssize_t print_number(char* buff, size_t length, c3_uint_t value) C3_FUNC_COLD;
  static ssize_t print_numbers(char* buff, size_t length, const c3_uint_t* values, c3_uint_t num) C3_FUNC_COLD;
  static bool get_number(Parser& parser, parser_token_t* args, c3_uint_t num, c3_uint_t& value) C3_FUNC_COLD;
  static bool get_number(Parser& parser, parser_token_t* args, c3_uint_t num, c3_uint_t& value,
    c3_uint_t min_value, c3_uint_t max_value) C3_FUNC_COLD;
  static bool get_numbers(Parser& parser, parser_token_t* args, c3_uint_t num, c3_uint_t* values,
    c3_uint_t max_num, bool ascending) C3_FUNC_COLD;

  static ssize_t print_durations(char* buff, size_t length, const c3_uint_t* durations) C3_FUNC_COLD;
  static bool get_duration_value(Parser& parser, parser_token_t* args, c3_uint_t i, c3_uint_t& value) C3_FUNC_COLD;
  static bool get_duration(Parser& parser, parser_token_t* args, c3_uint_t num, c3_uint_t& value) C3_FUNC_COLD;
  static bool get_durations(Parser& parser, parser_token_t* args, c3_uint_t num, c3_uint_t* values) C3_FUNC_COLD;

  static bool get_size(Parser& parser, parser_token_t* args, c3_uint_t num, c3_ulong_t& size) C3_FUNC_COLD;
  static bool get_size(Parser& parser, parser_token_t* args, c3_uint_t num, c3_ulong_t& size,
    c3_ulong_t min_value, c3_ulong_t max_value) C3_FUNC_COLD;
  static bool get_recompression_threshold(Parser &parser, parser_token_t* args, c3_uint_t num,
    Optimizer &optimizer) C3_FUNC_COLD;

  // output/fetch keyword arguments
  static void log_keyword_error(Parser& parser, const char** options, c3_uint_t num_options) C3_FUNC_COLD;
  static ssize_t print_keyword(char* buff, size_t length, c3_uint_t index, const char** options,
    c3_uint_t num_options) C3_FUNC_COLD;
  static int get_keyword_index(Parser& parser, parser_token_t& arg,
    const char** options, c3_uint_t num_options) C3_FUNC_COLD;
  static int get_single_keyword_index(Parser& parser, parser_token_t* args, c3_uint_t num,
    const char** options, c3_uint_t num_options) C3_FUNC_COLD;

  // output/fetch keyword arguments that are compressor IDs
  static ssize_t print_compressor(char* buff, size_t length, c3_uint_t index) C3_FUNC_COLD;
  static ssize_t print_compressors(char* buff, size_t length, Optimizer& optimizer) C3_FUNC_COLD;
  static int get_compressor_index(Parser& parser, parser_token_t& arg) C3_FUNC_COLD;
  static int get_single_compressor_index(Parser& parser, parser_token_t* args, c3_uint_t num) C3_FUNC_COLD;
  static bool get_compressors(Parser& parser, parser_token_t* args, c3_uint_t num, Optimizer& optimizer) C3_FUNC_COLD;

  // helpers for configuring memory objects
  static ssize_t print_max_memory(Parser& parser, char* buff, size_t length, Memory& memory) C3_FUNC_COLD;
  static bool set_max_memory(Parser &parser, parser_token_t* args, c3_uint_t num, Memory &memory) C3_FUNC_COLD;

  // helpers for configuring file rotation thresholds
  static ssize_t get_max_file_size(Parser& parser, char* buff, size_t length, FileBase& file) C3_FUNC_COLD;

  // helpers for socket pipelines
  static bool set_ips(Parser &parser, parser_token_t* args, c3_uint_t num, SocketPipeline &pipeline) C3_FUNC_COLD;
  static bool set_port(Parser &parser, parser_token_t* args, c3_uint_t num, SocketPipeline &pipeline) C3_FUNC_COLD;
  static bool set_persistence(Parser &parser, parser_token_t* args, c3_uint_t num, SocketPipeline &pipeline)
    C3_FUNC_COLD;

  // helpers for output/fetching fill factors of object stores
  static ssize_t print_fill_factor(char* buff, size_t length, Store& store) C3_FUNC_COLD;
  static bool set_fill_factor(Parser &parser, parser_token_t* args, c3_uint_t num, Store &store) C3_FUNC_COLD;

  // helpers for configuring hash tables in object stores
  static bool set_num_tables(Parser &parser, parser_token_t* args, c3_uint_t num, ObjectStore &store) C3_FUNC_COLD;
  static bool set_init_capacity(Parser &parser, parser_token_t* args, c3_uint_t num, ObjectStore &store) C3_FUNC_COLD;

  /////////////////////////////////////////////////////////////////////////////
  // CONFIGURATION API
  /////////////////////////////////////////////////////////////////////////////

  bool set_option(const char* option, c3_uint_t length, bool interactive) C3_FUNC_COLD;
  bool set_option(char short_option, const char* value, c3_uint_t length) C3_FUNC_COLD;
  bool load_file(const char* path) C3_FUNC_COLD { return c_parser.parse(path); }
  ssize_t get_option(const char* option, char* buff, size_t size) C3_FUNC_COLD {
    return c_parser.query(option, buff, size);
  }
  c3_uint_t enumerate_options(const char* mask, parser_enum_proc_t callback, void* context) C3_FUNC_COLD {
    return c_parser.enumerate(mask, callback, context);
  }
};

extern Configuration configuration;

} // CyberCache

#endif // _CC_CONFIGURATION_H
