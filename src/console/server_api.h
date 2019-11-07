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
 * Server command API implementation, and convenience container for responses.
 */
#ifndef _SERVER_API_H
#define _SERVER_API_H

#include "c3lib/c3lib.h"
#include "string_list.h"

#include <cstdarg>
#include <cstdio>

namespace CyberCache {

/// Container for holding arguments passed to various command execution methods
union CommandArgument {
private:
  c3_long_t         ca_number; // integer number
  const char*       ca_string; // nul-terminated "C" string
  const StringList* ca_list;   // pointer to a list of strings

public:
  CommandArgument() { ca_number = 0; }
  explicit CommandArgument(c3_long_t number) { ca_number = number; }
  explicit CommandArgument(const char* str) { ca_string = str; }
  explicit CommandArgument(const StringList* list) { ca_list = list; }

  c3_int_t get_int() const {
    c3_assert(ca_number >= INT_MIN_VAL && ca_number <= INT_MAX_VAL);
    return (c3_int_t) ca_number;
  }
  c3_uint_t get_uint() const {
    c3_assert(ca_number >= 0 && ca_number <= UINT_MAX_VAL);
    return (c3_uint_t) ca_number;
  }
  const char* get_string() const { return ca_string; }
  const StringList* get_list() const { return ca_list; }
};

/// Helper class for building result message strings
class ResultBuilder {
  static const c3_uint_t RB_CHUNK_SIZE = 4096;

  char*     rb_buffer;     // buffer with data collected so far
  c3_uint_t rb_total_size; // total size of the buffer
  c3_uint_t rb_used_size;  // used portion of the buffer

  void ensure(c3_uint_t extra_size);

public:
  explicit ResultBuilder(c3_uint_t size = RB_CHUNK_SIZE);
  ResultBuilder(const ResultBuilder&) = delete;
  ResultBuilder(ResultBuilder&&) = delete;
  ~ResultBuilder();

  ResultBuilder& operator=(const ResultBuilder&) = delete;
  ResultBuilder& operator=(ResultBuilder&&) = delete;

  void add(const char* format, ...) C3_FUNC_PRINTF(2);
  void add_char(char c);
  void add_string(const char* str, c3_uint_t length);
  void add_number(c3_long_t number);

  const char* get_buffer() const { return rb_buffer; }
  c3_uint_t get_size() const { return rb_used_size + 1; }
};

/// Container for server responses that is immediately usable by console.
class Result {
  constexpr static c3_byte_t ZERO_LENGTH_BUFFER[] = "RES_ZeroLengthBuffer";

  /// Internal result types (how the class should interpret its contents)
  enum api_result_t: c3_uint_t {
    AR_INVALID,          // an invalid type (placeholder)
    AR_INTERNAL_ERROR,   // an internal error occurred (number of elements == 1)
    AR_CONNECTION_ERROR, // socket creation or connecting to IP failed (number of elements == 1)
    AR_IO_ERROR,         // data transmission error (number of elements == 1)
    AR_OK,               // `OK` response that has no data associated with it (number of elements == 0)
    AR_ERROR,            // Error message from the server (number of elements == 1)
    AR_STRING,           // Response header converted to a string (number of elements == 1)
    AR_LIST,             // List of strings (number of elements == number of strings in the list)
    AR_DATA              // Binary data (number of elements == number of payload bytes)
  };
  /// Result usage states
  enum api_change_t: c3_byte_t {
    AC_UNCHANGED, // result was not accessed
    AC_CHANGED,   // result was changed after command execution
    AC_PRINTED    // result was printed out
  };
  static constexpr c3_uint_t MAX_COLON_OFFSET = 80;
  static constexpr c3_uint_t MAX_MESSAGE_STRING_SIZE = 4096;
  static c3_uint_t    r_bytes_per_line;   // number of bytes to display per data buffer line
  static c3_uint_t    r_lines_per_screen; // default number of lines for the [first] `print()` calls
  static char         r_np_char;          // character to display instead of non-printable characters
  static api_change_t r_change_state;     // `true` if Result object has been changed since last reset

  api_result_t r_type;         // type of the data contained in the object
  c3_uint_t    r_num_elements; // number of strings in the list, or bytes in the buffer, or 0, or 1
  size_t       r_memory_size;  // total size of allocated memory
  c3_byte_t*   r_buffer;       // result data, or `NULL`

  void init_invalid();
  void init(api_result_t type, const char* format, ...) C3_FUNC_PRINTF(3);
  void init_header_response(const SocketResponseReader& reader, bool timestamps);
  void init_payload_response(const SocketResponseReader& reader);
  void init_list_response(const SocketResponseReader& reader);
  void init_error_response(const SocketResponseReader& reader);

  static void print_nl(FILE* file);
  static void print_nl();
  c3_uint_t print_data(FILE *file, c3_long_t from, c3_uint_t num);
  c3_uint_t print_list(FILE *file, c3_long_t from, c3_uint_t num);

public:
  Result() noexcept { init_invalid(); };
  /**
   * Ctor to be used in case of general internal errors.
   *
   * @param reason What went wrong
   */
  explicit Result(const char* reason);
  /**
   * Ctor that is to be used if console could not connect to CyberCache server.
   *
   * @param ip Address to which connection attempt had been made
   * @param port Port number used for failed connection
   */
  Result(c3_ipv4_t ip, c3_ushort_t port);
  /**
   * Ctor that has to be used if connection was successful, but subsequent attempt to read or write data
   * has failed (ended with a result other than IO_RESULT_OK).
   *
   * @param result Result of failed read() or write() call
   * @param response `true` if could not receive response, `false` if could not send command
   */
  Result(io_result_t result, bool response);
  /**
   * Ctor that has to be used if complete response had been received from CyberCache server.
   *
   * @param reader Object that contains server response data
   * @param timestamps `true` if all numbers in response headers should be treated as timestamps
   */
  explicit Result(const SocketResponseReader& reader, bool timestamps = false);
  Result(const Result&) = delete;
  Result(Result&& that) noexcept;
  ~Result();

  Result& operator=(const Result&) = delete;
  Result& operator=(Result&& that) noexcept {
    std::swap(r_type, that.r_type);
    std::swap(r_num_elements, that.r_num_elements);
    std::swap(r_memory_size, that.r_memory_size);
    std::swap(r_buffer, that.r_buffer);
    return *this;
  }

  // object state inspection
  bool is_valid() const { return r_type != AR_INVALID; }
  bool is_ok() const { return r_type == AR_OK; }
  bool is_error() const { return r_type == AR_ERROR; }
  bool is_string() const { return r_type == AR_STRING; }
  bool is_array() const { return r_type == AR_DATA; }
  bool is_list() const { return r_type == AR_LIST; }
  c3_uint_t get_num_elements() const { return r_num_elements; }
  bool contains(const char* str);
  bool contains(c3_uint_t offset, const void* data, c3_uint_t size);

  // global state inspection and manipulation
  bool has_changed() const { return r_change_state == AC_CHANGED; }
  bool was_printed() const { return r_change_state == AC_PRINTED; }
  void reset_changed_state() { r_change_state = AC_UNCHANGED; }

  // display settings
  static c3_uint_t get_bytes_per_line() { return r_bytes_per_line; }
  static void set_bytes_per_line(c3_uint_t num) { r_bytes_per_line = num; }
  static c3_uint_t get_lines_per_screen() { return r_lines_per_screen; }
  static void set_lines_per_screen(c3_uint_t num) { r_lines_per_screen = num; }
  static char get_substitution_char() { return r_np_char; }
  static void set_substitution_char(char c) { r_np_char = c; }

  /**
   * Implementation of `print()` that outputs data to an arbitrary file/stream.
   */
  void print(FILE* file);

  /**
   * Method called right after a command is executed.
   */
  void print();

  /**
   * Implementation of `print(from, num)` that outputs data to an arbitrary file/stream.
   *
   * @param from First byte of data buffer, or first string of a list to display.
   * @param num Number of bytes of data, or number of strings of a list to display.
   *
   * @return Number of elements (buffer bytes or list strings) actually printed; zero if the object does
   *   not contain a data buffer or a string list
   */
  c3_uint_t print(FILE* file, c3_long_t from, c3_uint_t num);

  /**
   * Method called upon `RESULT` command.
   *
   * @param from First byte of data buffer, or first string of a list to display.
   * @param num Number of bytes of data, or number of strings of a list to display.
   *
   * @return Number of elements (buffer bytes or list strings) actually printed; zero if the object does
   *   not contain a data buffer or a string list
   */
  c3_uint_t print(c3_long_t from, c3_uint_t num);
};

/// Command information container
class CommandInfo {

  /// Command emulation mode
  enum command_emulation_mode_t: c3_byte_t {
    CEM_USER = 0, // command is handled as user-level irrespective of its ID
    CEM_ADMIN,    // command is handled as administrative irrespective of its ID
    CEM_AUTO      // command level is defined by its ID
  };

  const c3_byte_t          cic_id;     // command ID
  command_emulation_mode_t cic_mode;   // how the command should be handled (authentication level)
  bool                     cic_marker; // integrity check marker to be *restored* after emulation
  c3_hash_t                cic_hash;   // password hash code that has to be *restored* after emulation

public:
  explicit CommandInfo(command_t id);
  CommandInfo(c3_byte_t id, bool admin, const char* password, bool check);
  CommandInfo(const CommandInfo& that) = delete;
  CommandInfo(CommandInfo&& that) noexcept: cic_id(that.cic_id) {
    cic_mode = that.cic_mode;
    cic_marker = that.cic_marker;
    cic_hash = that.cic_hash;
    that.cic_mode = CEM_AUTO;
  }
  ~CommandInfo();

  void operator=(const CommandInfo&) = delete;
  void operator=(CommandInfo&&) = delete;

  command_t get_id() const { return (command_t) cic_id; }
  bool is_auto() const { return cic_mode == CEM_AUTO; }
  bool is_admin_command() const;
  bool sends_timestamps_in_response() const;
};

/// Result printing modes: what to print out upon command execution
enum auto_result_mode_t: c3_byte_t {
  ARM_SIMPLE = 0, // all results except lists and data buffers
  ARM_LISTS,      // all results except data buffers (the default)
  ARM_ALL         // all results
};

/**
 * Server API proxy.
 *
 * Its getters/setters provide access to configuration parameters, such as lifetime of FPC cache entries,
 * that are not kept anywhere else in the library.
 *
 * The `execute()` methods are used to carry out server commands. The format string passed to most of
 * these methods can contain the following characters denoting types of values passed as remaining
 * arguments; specifically:
 *
 * - `N` : respective argument is a signed 32-bit integer,
 * - 'U' : unsigned 32-bit integer,
 * - 'S' : C-style `\0`-terminated string,
 * - 'L' : pointer to an instance of the `StringList` class (an array of strings).
 *
 * NOTE: some commands (e.g. `WRITE`, `SAVE`) load data from file and then pass it as a C string. If the
 * file happens to have zero bytes, it will get truncated at first zero byte at the point of sending
 * through `execute()` family of functions. The solution would be to implement two flavor of the string,
 * "C" (a nul-terminated C string), and "S" (a Pascal-style string with specified size) as is done
 * internally in the server for similar methods.
 */
class CyberCache {
  Socket             cc_socket;      // server connection socket
  c3_ipv4_t          cc_ip;          // IP address to connect to
  c3_ushort_t        cc_port;        // port number to connect to
  auto_result_mode_t cc_auto_result; // which results to print out after command execution
  user_agent_t       cc_user_agent;  // user agent ID to pass along with certain server commands
  StringList         cc_tags;        // list of tags to be passed along with `SAVE` command
  c3_int_t           cc_lifetime;    // lifetime to be passed along with `SAVE` command
  c3_uint_t          cc_offset;      // from where to continue printing server result by default
  c3_uint_t          cc_count;       // how many server result elements to print by default
  bool               cc_persistent;  // `true` if server connection is persistent

  static void populate_list(HeaderListChunkBuilder& header_list, const StringList* string_list);
  Result run(command_t cmd, const void* buffer, c3_uint_t length, const char* format, va_list args);

public:
  CyberCache() noexcept;

  const char* get_address() const;
  bool set_address(const char* address);

  c3_ushort_t get_port() const { return cc_port; }
  void set_port(c3_ushort_t port) { cc_port = port; };

  auto_result_mode_t get_auto_result_mode() const { return cc_auto_result; }
  void set_auto_result_mode(auto_result_mode_t mode) { cc_auto_result = mode; }

  user_agent_t get_user_agent() const { return cc_user_agent; }
  void set_user_agent(user_agent_t ua) { cc_user_agent = ua; }

  c3_uint_t get_num_tags() const { return cc_tags.get_count(); }
  const char* get_tag(c3_uint_t i) const { return cc_tags.get(i); }
  const StringList* get_tags() const { return &cc_tags; }
  bool add_tag(const char* name) { return cc_tags.add_unique(name); }
  bool remove_tag(const char* name) { return cc_tags.remove_unique(name); }
  void remove_all_tags() { cc_tags.remove_all(); }

  c3_int_t get_lifetime() const { return cc_lifetime; }
  void set_lifetime(c3_int_t lifetime) { cc_lifetime = lifetime; }

  c3_uint_t get_offset() const { return cc_offset; }
  void set_offset(c3_uint_t offset) { cc_offset = offset; }
  c3_uint_t get_count() const { return cc_count; }
  void set_count(c3_uint_t count) { cc_count = count; }

  bool is_persistent() const { return cc_persistent; }
  void set_persistent(bool persistent) { cc_persistent = persistent; }

  Result emulate(const CommandInfo& info, const void* buffer, c3_uint_t length, const char* format,
    const CommandArgument* arguments);
  Result execute(command_t cmd, c3_uint_t size, const void* buffer, const char* format, ...);
  Result execute(command_t cmd, const char* format, ...);
  Result execute(command_t cmd);
};

} // CyberCache

#endif // _SERVER_API_H
