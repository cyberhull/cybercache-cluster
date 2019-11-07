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
 * General-purpose parser.
 */
#ifndef _C3_PARSER_H
#define _C3_PARSER_H

#include "c3_memory.h"
#include "c3_logger.h"

#include <cstring>

namespace CyberCache {

#define PARSER_GET_PROC(proc_name) pGet_ ## proc_name
#define PARSER_SET_PROC(proc_name) pSet_ ## proc_name
#define PARSER_ENTRY(proc_name) { #proc_name, PARSER_GET_PROC(proc_name), PARSER_SET_PROC(proc_name) }
#define PARSER_GET_ENTRY(proc_name) { \
  #proc_name, PARSER_GET_PROC(proc_name), PARSER_SET_PROC(default_proc) }
#define PARSER_SET_ENTRY(proc_name) { \
  #proc_name, PARSER_GET_PROC(default_proc), PARSER_SET_PROC(proc_name) }

const c3_uint_t PARSER_MAX_ARGS        = 16;   // max number of arguments that can be passed to a handler
const c3_uint_t PARSER_TOTAL_ARGS_SIZE = 4096; // max total length of all arguments passed to a handler

/**
 * Argument passed to a command handler as array element; this representation (pointer + length) had been
 * chosen over C-strings to let the same handlers process individual configuration commands submitted to
 * them as server commands, which always store strings in <vlq-length><data> format.
 */
class parser_token_t {
  friend class Parser;
  const char* pt_data;   // string representation of the argument
  c3_uint_t   pt_length; // length of the string representation, *not* counting terminating '\0'

public:
  const char* get_string() const { return pt_data; }
  bool is(const char* str);
  bool get_long(c3_long_t& num);
  bool get_ulong(c3_ulong_t& num, char* suffux = nullptr);
  bool get_int(c3_int_t& num);
  bool get_uint(c3_uint_t& num, char* suffux = nullptr);
  bool get_float(float& num);
  bool get_double(double& num);
  bool get_size(c3_ulong_t& bytes);
  bool get_duration(c3_uint_t& seconds);
  bool get_boolean(bool& value);
};

class Parser;

/// Type of the "get" parser command handler; returns length of stored data, -1 on errors
typedef long (*parser_get_proc_t)(Parser& parser, char* buff, size_t size);
/// Type of the "set" parser command handler; returns `true` on success
typedef bool (*parser_set_proc_t)(Parser& parser, parser_token_t* args, c3_uint_t num);
/// Type of the "enumeration" command callback; returns `true` if enumeration should continue
typedef bool (*parser_enum_proc_t)(void* context, const char* command);

// handlers for actions that are not implemented for a command
ssize_t PARSER_GET_PROC(default_proc)(Parser& parser, char* buff, size_t size);
bool PARSER_SET_PROC(default_proc)(Parser& parser, parser_token_t* args, c3_uint_t num);

/// Parser command handlers and their names
struct parser_command_t {
  const char*       pc_name;     // name of the command or option
  parser_get_proc_t pc_get_proc; // pointer to the function that handles "get" action
  parser_set_proc_t pc_set_proc; // pointer to the function that handles "set" action
};

/**
 * General-purpose parser, can be used to process configuration files, configuration commands, and the
 * likes. Its purpose is to break a string into individual tokens, and deal with comments, quoted
 * strings, line continuations, etc. in the process.
 */
class Parser: public virtual AbstractLogger {
  constexpr static const char* LAST_EXECUTED_COMMAND_PLACEHOLDER = "<INVALID>";

  char              pr_tokens[PARSER_TOTAL_ARGS_SIZE]; // internal buffer for tokens
  parser_token_t    pr_args[PARSER_MAX_ARGS];          // command arguments
  const char*       pr_what;                           // what is being processed (info for the logger)
  const char*       pr_file_path;                      // path to the file being processed, or NULL
  const char*       pr_buffer;                         // input buffer being processed
  size_t            pr_size;                           // input buffer size, characters
  size_t            pr_pos;                            // current position in the input buffer
  const parser_command_t* const pr_commands;           // commands understood/supported by parser
  const c3_uint_t   pr_ncommands;                      // number of commands registered with the parser
  c3_uint_t         pr_line;                           // current line number
  c3_uint_t         pr_tokpos;                         // current position within the buffer with tokens
  c3_uint_t         pr_nargs;                          // current number of elements in the argument array
  const char*       pr_last_command;                   // name of the last executed command
  const c3_uint_t   pr_nesting_level;                  // how deep is current "include" recursion
  bool              pr_interactive;                    // `true` if processing `SET` command

  // callbacks invoked upon various errors
  virtual void on_unknown_set(const char* name) C3_FUNC_COLD;
  virtual void on_set_error(const char* name) C3_FUNC_COLD;
  virtual void on_unknown_get(const char* name) C3_FUNC_COLD;
  virtual void on_get_error(const char* name) C3_FUNC_COLD;

  // helper methods for escape sequence processor
  static bool is_valid_xdigit(char c) {
    return (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f') || (c >= '0' && c <= '9');
  }
  static int xdigit_value(char c) {
    if (c >= 'A' && c <= 'F') {
      return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
      return c - 'a' + 10;
    }
    return c - '0';
  }

  // skip all white space (including new lines) up to the next command
  bool skip_ws();

  // skip all white space up to the next command argument
  bool skip_line_ws();

  // get next token (a command name, or a command argument)
  const char* get_token(c3_uint_t& length);

  // add token to argument array (first goes command name, then argument strings)
  bool add_argument(const char* token, c3_uint_t length);

  // find command handler with given name
  parser_command_t* find_command(const char* name);

protected:
  /**
   * Constructor.
   *
   * @param level Nesting level for the parser; levels start at 0 for the top-level one
   * @param commands Array of commands/options supported by the parser
   * @param ncommands Number of supported commands and/or options
   */
  Parser(c3_uint_t level, const parser_command_t* commands, c3_uint_t ncommands);

public:
  /**
   * Utility method: prints duration with a suffix (e.g. duration of 120 would be printed as "2m").
   *
   * @param buff Destination buffer
   * @param length Size of the destination buffer, characters
   * @param duration Value of the duration, seconds
   * @return Number of characters printed into the buffer, or -1 in case of error
   */
  static ssize_t print_duration(char* buff, size_t length, c3_uint_t duration);
  /**
   * Utility method: prints size with a suffix (e.g. size of 2048 would be printed as "2k").
   *
   * @param buff Destination buffer
   * @param length Size of the destination buffer, characters
   * @param duration Value of the size, bytes
   * @return Number of characters printed into the buffer, or -1 in case of error
   */
  static ssize_t print_size(char* buff, size_t length, c3_ulong_t size);
  /**
   * Check if we're processing a command in interactive mode; namely, a `SET` command received over the
   * network. Command handlers may need to report certain errors that they would silently ignore otherwise;
   * for instance, some options can only be set in initially loaded configuration file, or via command line
   * options, but cannot be changed at run time.
   *
   * @return `true` if executing a command in interactive mode.
   */
  bool is_interactive() const { return pr_interactive; }
  /**
   * Check how deep is current recursion level: how many files were recursively loaded using "include" or
   * similar statements.
   *
   * @return Current recursion level; zero means root file or a stand-alone option is being processed.
   */
  c3_uint_t get_nesting_level() const { return pr_nesting_level; }

  /**
   * If a file is being parsed, then this method will return path to that file. This method is to be used
   * for the implementation of `include` and similar commands that may need to build paths relative to
   * that of the currently processed file.
   *
   * @return Path to the file being parsed, or NULL
   */
  const char* get_file_path() const { return pr_file_path; }

  /**
   * Get number of the line being parsed.
   *
   * @return Current line number.
   */
  const c3_uint_t get_line_number() const { return pr_line; }

  /**
   * Sorts command array for binary search on that array to work correctly; this method has to be called
   * on each command array only once during application life time, before that array is passed to the
   * parsing method.
   *
   * @param commands Array of commands
   * @param num Number of commands in the array
   */
  static void initialize_commands(parser_command_t* commands, size_t num) C3_FUNC_COLD;

  /**
   * Get name of the last command for which getter or setter had been called.
   *
   * @return Pointer to the static string with command name, or to LAST_EXECUTED_COMMAND_PLACEHOLDER.
   */
  const char* get_command_name() const { return pr_last_command; }
  /**
   * Log general parsing error (records `what` and line number).
   *
   * @param level Message level, an `LL_xxx` constant
   * @param format Format string, similar to that of the `fprintf()` family of functions.
   * @param ... Optional arguments; number and types must match respective specifiers in the format string.
   */
  void log_status(log_level_t level, const char* format, ...) C3_FUNC_COLD C3_FUNC_PRINTF(3);

  /**
   * Log parsing message that is specific to the command being processed. Logs current cummand's name
   * (along with all the information logged by the generic error logging method).
   *
   * @param level Message level, an `LL_xxx` constant
   * @param format Format string, similar to that of the `fprintf()` family of functions.
   * @param ... Optional arguments; number and types must match respective specifiers in the format string.
   */
  void log_command_status(log_level_t level, const char* format, ...) C3_FUNC_COLD C3_FUNC_PRINTF(3);

  /**
   * Log parsing error that is specific to the command being processed. Logs current cummand's name
   * (along with all the information logged by the generic error logging method).
   *
   * @param format Format string, similar to that of the `fprintf()` family of functions.
   * @param ... Optional arguments; number and types must match respective specifiers in the format string.
   */
  void log_command_error(const char* format, ...) C3_FUNC_COLD C3_FUNC_PRINTF(2);

  /**
   * Log parsing error in a way that depends on context. If parser is in "interactive" mode, this method
   * acts like `log()` in that it does not output any additional information. If, however, parser in in
   * "batch" mode, processing a file (i.e. is in non-interactive mode), then this method acts like
   * `log_command_error()`, and outputs file name, line number, and command name along with the error
   * message.
   *
   * @param format Format string, similar to that of the `fprintf()` family of functions.
   * @param ... Optional arguments; number and types must match respective specifiers in the format string.
   */
  void log_error(const char* format, ...) C3_FUNC_COLD C3_FUNC_PRINTF(2);

  /**
   * Parses buffer into command names and arguments, and calls command handler for each found command.
   *
   * @param what What is being parsed: this will be used as prefix for logging messages
   * @param buffer Data to be parsed
   * @param size Size of data to be parsed
   * @param interactive `true` if processing `SET` command received over network
   * @return `true` if all encountered commands had been identified, and all invoked command handlers
   *   returned `true`; `false` otherwise (actual errors and warnings will be logged)
   */
  bool parse(const char* what, const char* buffer, size_t size, bool interactive);

  /**
   * Loads specified file into a buffer, then parses that buffer into command names and their arguments,
   * and calls command handler for each command.
   *
   * @param path Configuration/command file to load.
   * @return `true` if file was successfully loaded, all encountered commands hadbeen identified, and all
   *   invoked command handlers returned `true`; `false` otherwise (errors and warnings would be logged)
   */
  bool parse(const char* path, Memory& memory = global_memory);

  /**
   * Get current value of an option.
   *
   * @param command Option/command name, a '\0'-terminated string
   * @param buff Buffer where option value will be stored
   * @param size Size of the value buffer
   * @return Number of stored characters on success, -1 on error
   */
  ssize_t query(const char* command, char* buff, size_t size);

  /**
   * Calls specified callback with pointer to commands that match specified mask. The mask may contain
   * arbitrary number of two asterisks ('*', denoting "zero or more arbitrary characters") to form command
   * search criteria like the following:
   *
   * - *                   (will match all commands),
   * - perf_*              (matches commands starting with "perf_"),
   * - *read*              (matches commands that contain "read" *anywhere* in their names),
   * - perf_*replicator*   (matches commands that start with "perf_" and contain "replicator" in the rest of the name),
   * - *_capacity          (matches commands that end with "_capacity"),
   * - *session*attempts   (matches commands with names that contain "capacity" and end with "attempts"),
   *
   * and so on.
   *
   * @param mask Search template (see description above)
   * @param callback Pointer to function to call on each found command
   * @return Number of commands that matched search criteria, or `-1` if mask was found to be malformed.
   */
  c3_uint_t enumerate(const char* mask, parser_enum_proc_t callback, void* context) C3_FUNC_COLD;
};

}

#endif // _C3_PARSER_H
