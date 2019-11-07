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
 * Implementations of all console commands (command handlers).
 */
#ifndef _CONSOLE_COMMANDS_H
#define _CONSOLE_COMMANDS_H

#include "c3lib/c3lib.h"
#include "server_api.h"

namespace CyberCache {

/// List of log messages
class LogList: private StringList {
  static constexpr c3_uint_t INITIAL_LOG_CAPACITY = 8;

  c3_uint_t ll_num_errors; // number of messages logged at `LL_ERROR` level

public:
  LogList() noexcept: StringList(INITIAL_LOG_CAPACITY) { ll_num_errors = 0; }

  c3_uint_t get_num_errors() const { return ll_num_errors; }
  void increment_error_number() { ll_num_errors++; }
  c3_uint_t get_num_messages() const { return get_count(); }
  void add_message(const char* message) { add(message); }
  const char* get_message(c3_uint_t i) const { return get(i); }
  void print_all();
  void reset();
};

/// Logger that accumulates logged messages in an external string list
class ConsoleLogger: public virtual AbstractLogger {
  static log_level_t cl_log_level; // current logging level

  bool log_message(log_level_t level, const char* message, int length) override;

public:
  static log_level_t get_log_level() { return cl_log_level; }
  static void set_log_level(log_level_t level) { cl_log_level = level; }
};

/// Console command parser.
class ConsoleParser: public ConsoleLogger, public Parser {

  bool cp_exit_on_errors; // whether to quit immediately on encountering an error in batch mode

  void on_error();
  void on_unknown_set(const char* name) override;
  void on_set_error(const char* name) override;
  void on_unknown_get(const char* name) override;
  void on_get_error(const char* name) override;

  static bool enumerator(void* context, const char* command);

public:
  ConsoleParser(c3_uint_t level, bool exit_on_errors);

  /**
   * Executes statement entered by user.
   *
   * If the statement contains space(s), then it is executed by the parser as a `set` command (assuming
   * that spaces separate command arguments). Otherwise, if it contains asterisk(s), it is treated as a
   * request to list all commands matching the mask. If none of that is true, it is treated as a `get`
   * command.
   *
   * Before command execution, list of log messages is reset. Server response object is only set by
   * commands communicating with the server.
   *
   * @param command Command statement to execute
   * @return `true` on success, `false` if there were errors
   */
  bool execute(const char* statement);

  /**
   * Checks parser's error tolerance.
   *
   * @return If `true`, parser will exit upon encountering an error in batch (non-interactive) mode
   */
  bool get_exit_on_errors() const { return cp_exit_on_errors; }

  /**
   * Sets parser's error tolerance.
   *
   * @param mode If `true`, parser will exit upon encountering an error in batch (non-interactive) mode
   */
  void set_exit_on_errors(bool mode) { cp_exit_on_errors = mode; }
};

extern LogList    cc_log;    // messages logged during last console command's execution
extern Result     cc_result; // result of the last executed *server* command
extern CyberCache cc_server; // server proxy

} // CyberCache

#endif // _CONSOLE_COMMANDS_H
