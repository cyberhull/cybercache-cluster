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
 * Logging services: class implementing concurrent logging.
 *
 * Must *NOT* be instantiated and/or used (directly) within the library.
 */
#ifndef _LS_LOGGER_H
#define _LS_LOGGER_H

#include "c3lib/c3lib.h"
#include "mt_message_queue.h"

namespace CyberCache {

/// Logging service
class Logger: public FileBase {

  /// Commands recognized by the logger
  enum log_command_t: c3_uintptr_t {
    LC_INVALID = 0,      // placeholder; zero value cannot be used
    LC_LEVEL_FIRST_ID,   // marker for the very first level change command
    LC_LEVEL_EXPLICIT = LC_LEVEL_FIRST_ID, // set EXPLICIT logging level
    LC_LEVEL_FATAL,      // set FATAL logging level
    LC_LEVEL_ERROR,      // set ERROR logging level
    LC_LEVEL_WARNING,    // set WARNING logging level
    LC_LEVEL_TERSE,      // set TERSE logging level
    LC_LEVEL_NORMAL,     // set NORMAL logging level
    LC_LEVEL_VERBOSE,    // set VERBOSE logging level
    LC_LEVEL_DEBUG,      // set DEBUG logging level
    LC_LEVEL_LAST_ID = LC_LEVEL_DEBUG, // marker for the very last level change command
    LC_DISABLE_ROTATION, // reset rotation path
    LC_ROTATE,           // force log rotation
    LC_DISABLE,          // disable logging until valid path is sent with a message
    LC_QUIT,             // logger must quit; sent *after* Thread::request_stop() for logger to take notice
    LC_NUMBER_OF_ELEMENTS
  };

  /// Commands that are passed along with file paths (as first chars of the "extended" text)
  enum log_subcommand_t: c3_byte_t {
    LS_INVALID = 0,               // not a valid subcommand
    LS_LOG_PATH_CHANGE,           // messages should now be logged to specified file
    LS_ROTATION_THRESHOLD_CHANGE, // if log grown bigger than passed (as hex string!) size, rotate the log
    LS_ROTATION_PATH_CHANGE,      // path template for next log rotation
    LS_SET_CAPACITY,              // set log message queue capacity
    LS_SET_MAX_CAPACITY,          // set maximum log message queue capacity
    LS_NUMBER_OF_ELEMENTS
  };

  // make sure level-chaning command IDs match logging levels
  static_assert((log_command_t)LL_EXPLICIT == LC_LEVEL_EXPLICIT,
    "Log level do not match level-setting command (EXPLICIT)");
  static_assert((log_command_t)LL_DEBUG == LC_LEVEL_DEBUG,
    "Log level do not match level-setting command (EXPLICIT)");

  /// Internal representation of strings in logger messages
  struct LogString {
    c3_ushort_t ls_length;  // length of the string stored in buffer, *including* terminating '\0'
    log_level_t ls_level;   // severity level of the message
    char        ls_text[1]; // message to be logged

    c3_uint_t get_object_size() const { return ls_length + LOG_STRING_OVERHEAD; }
    Memory& get_memory_object() const { return global_memory; }
    static LogString* create(log_level_t level, const char* msg, size_t length);

  } __attribute__ ((__packed__));

  /**
   * Internal representation of commands in logger messages. We include terminating `\0` into argument
   * length count (and, likewise, into `LogString`'s message length count) so that length would always be
   * positive or negative (never zero) and we would always be able to reliably distinguish regular log
   * messages and log [sub]commands.
   */
  struct LogCommand {
    c3_short_t       lc_length;    // negated length of the string stored in buffer, *including* '\0'
    log_subcommand_t lc_cmd;       // subcommand ID
    union {
      c3_ulong_t     lca_size;     // lof file size threshold
      c3_uint_t      lca_capacity; // queue capacity or maximum capacity
      char           lca_path[1];  // log file path, or rotation path
    } lc_arg;                      // subcommand argument

    bool is_you() const { return lc_length < 0; }
    c3_uint_t get_object_size() const { return -lc_length + LOG_COMMAND_OVERHEAD; }
    Memory& get_memory_object() const { return global_memory; }
    static LogCommand* create(log_subcommand_t cmd, const void* arg, size_t size);
    static LogCommand* create(log_subcommand_t cmd, const char* arg);

  } __attribute__ ((__packed__));

  static constexpr c3_uint_t   LOG_STRING_OVERHEAD  = (c3_uint_t) offsetof(LogString, ls_text); // space taken by size
  static constexpr c3_ushort_t LOG_STRING_MAX_SIZE  = 2048; // max message size w/o size and terminating '\0'
  static constexpr c3_uint_t   LOG_COMMAND_OVERHEAD = (c3_uint_t) offsetof(LogCommand, lc_arg); // size & subcommand
  static constexpr c3_ushort_t LOG_COMMAND_MAX_SIZE = (c3_ushort_t) MAX_FILE_PATH_LENGTH; // max arg size

  static constexpr c3_uint_t  DEFAULT_LOG_QUEUE_SIZE = 8;    // initial size of the message queue
  static constexpr c3_uint_t  MAX_LOG_QUEUE_SIZE     = 1024; // default maximum size of the message queue
  static constexpr c3_ulong_t MIN_LOG_FILE_SIZE      = kilobytes2bytes(64);
  static constexpr c3_ulong_t DEFAULT_LOG_FILE_SIZE  = megabytes2bytes(16);
  static constexpr c3_ulong_t LOG_FILE_SIZE_PADDING  = megabytes2bytes(1); // in case log rotation fails
  static constexpr c3_ulong_t MAX_LOG_FILE_SIZE      = gigabytes2bytes(8);

  /// External representation of logger messages and commands
  typedef CommandMessage<log_command_t, LogCommand, LogString, LC_NUMBER_OF_ELEMENTS> LogMessage;

  /// Internal message queue supporting concurrent access from multiple threads
  typedef MessageQueue<LogMessage> LogMessageQueue;

  static const char* l_level_names[LL_NUMBER_OF_ELEMENTS]; // log level names

  /////////////////////////////////////////////////////////////////////////////
  // INSTANCE FIELDS
  /////////////////////////////////////////////////////////////////////////////

  LogMessageQueue l_queue;     // queue with messages and commands
  LogInterface*   l_host;      // reference to the host implementation
  String          l_path;      // current log path
  String          l_rot_path;  // current rotation path
  log_level_t     l_level;     // current logging level
  bool            l_quitting;  // logger has received "quit" request and is shutting down

  LogInterface& get_host() const {
    c3_assert(l_host);
    return *l_host;
  }
  void set_host(LogInterface* host) {
    c3_assert(l_host == nullptr);
    l_host = host;
  }

  void write_data(log_level_t level, const void* buffer, size_t size);
  void write_string(log_level_t level, const char* format, ...) C3_FUNC_PRINTF(3);
  void write_header_strings() C3_FUNC_COLD;
  void process_log_path_change_command(const char* path) C3_FUNC_COLD;
  void process_rotation_threshold_change_command(c3_ulong_t threshold) C3_FUNC_COLD;
  void process_rotation_path_change_command(const char* path) C3_FUNC_COLD;
  void process_rotate_command(const char* reason) C3_FUNC_COLD;
  void process_capacity_change_command(c3_uint_t capacity) C3_FUNC_COLD;
  void process_max_capacity_change_command(c3_uint_t max_capacity) C3_FUNC_COLD;

  bool send_command(log_command_t cmd) C3_FUNC_COLD;
  bool send_subcommand(log_subcommand_t cmd, const void* data, size_t size) C3_FUNC_COLD;
  bool send_subcommand(log_subcommand_t cmd, const char* data) C3_FUNC_COLD;
  void increment_counts(log_level_t level) const {
    if (level == LL_WARNING) {
      get_host().increment_warning_count();
    }
    if (level == LL_ERROR) {
      get_host().increment_error_count();
    }
  }
  void enter_quit_state() C3_FUNC_COLD;
  void shut_down() C3_FUNC_COLD;

public:
  C3_FUNC_COLD Logger() noexcept: FileBase(DEFAULT_LOG_FILE_SIZE),
    l_queue(DOMAIN_GLOBAL, HO_LOGGER, DEFAULT_LOG_QUEUE_SIZE, MAX_LOG_QUEUE_SIZE, 0) {
    l_host = nullptr;
    l_level = LL_NORMAL;
    l_quitting = false;
  }

  // limits' accessors
  c3_uint_t get_queue_capacity() C3LM_OFF(const) { return l_queue.get_capacity(); }
  c3_uint_t get_max_queue_capacity() C3LM_OFF(const) { return l_queue.get_max_capacity(); }
  static constexpr c3_ulong_t get_min_threshold() noexcept { return MIN_LOG_FILE_SIZE; }
  static constexpr c3_ulong_t get_max_threshold() noexcept { return MAX_LOG_FILE_SIZE; }

  void configure(LogInterface* host) C3_FUNC_COLD { set_host(host); }
  log_level_t get_level() const { return l_level; }
  void set_level(log_level_t level) C3_FUNC_COLD;
  bool log(log_level_t level, const char* format, ...) C3_FUNC_PRINTF(3);
  bool log_string(log_level_t level, const char* str, int length);
  bool send_path_change_command(const char* path) C3_FUNC_COLD;
  bool send_rotation_threshold_change_command(c3_long_t threshold) C3_FUNC_COLD;
  bool send_rotation_path_change_command(const char* path) C3_FUNC_COLD;
  bool send_rotate_command() C3_FUNC_COLD { return send_command(LC_ROTATE); }
  bool send_capacity_change_command(c3_uint_t capacity) C3_FUNC_COLD;
  bool send_max_capacity_change_command(c3_uint_t max_capacity) C3_FUNC_COLD;
  bool send_quit_command() C3_FUNC_COLD { return send_command(LC_QUIT); }

  // this method must *NOT* be called directly: its name should be passed to Thread::start()
  static void thread_proc(c3_uint_t id, ThreadArgument arg);
};

} // CyberCache

#endif // _LS_LOGGER_H
