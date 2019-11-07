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
#include "net_configuration.h"
#include "server_api.h"

#include <cctype>

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// ResultBuilder
///////////////////////////////////////////////////////////////////////////////

ResultBuilder::ResultBuilder(c3_uint_t size) {
  c3_assert(size);
  rb_total_size = size;
  rb_buffer = (char*) allocate_memory(size);
  rb_used_size = 0;
  rb_buffer[0] = 0; // in case nothing is stored
}

ResultBuilder::~ResultBuilder() {
  c3_assert(rb_buffer && rb_used_size < rb_total_size);
  free_memory(rb_buffer, rb_total_size);
}

void ResultBuilder::ensure(c3_uint_t extra_size) {
  const c3_uint_t required_size = rb_used_size + extra_size + 1;
  if (required_size > rb_total_size) {
    c3_uint_t new_size = rb_total_size + max(required_size - rb_total_size, RB_CHUNK_SIZE);
    rb_buffer = (char*) reallocate_memory(rb_buffer, new_size, rb_total_size);
    rb_total_size = new_size;
  }
}

void ResultBuilder::add(const char* format, ...) {
  char buffer[RB_CHUNK_SIZE];
  va_list args;
  va_start(args, format);
  int length = std::vsnprintf(buffer, sizeof buffer, format, args);
  va_end(args);
  c3_assert(length > 0);
  ensure((c3_uint_t) length);
  std::memcpy(rb_buffer + rb_used_size, buffer, (c3_uint_t) length + 1);
  rb_used_size += length;
}

void ResultBuilder::add_char(char c) {
  ensure(1);
  rb_buffer[rb_used_size++] = c;
  rb_buffer[rb_used_size] = 0;
}

void ResultBuilder::add_string(const char* str, c3_uint_t length) {
  ensure(length);
  std::memcpy(rb_buffer + rb_used_size, str, length);
  rb_used_size += length;
  rb_buffer[rb_used_size] = 0;
}

void ResultBuilder::add_number(c3_long_t number) {
  add("%lld", number);
}

///////////////////////////////////////////////////////////////////////////////
// Result
///////////////////////////////////////////////////////////////////////////////

constexpr c3_byte_t  Result::ZERO_LENGTH_BUFFER[];
c3_uint_t            Result::r_bytes_per_line = 16;
c3_uint_t            Result::r_lines_per_screen = 24;
char                 Result::r_np_char = '.';
Result::api_change_t Result::r_change_state = AC_UNCHANGED;

Result::Result(const char* reason) {
  r_change_state = AC_CHANGED;
  init(AR_INTERNAL_ERROR, "%s [%s]", reason, c3_get_error_message());
}

Result::Result(c3_ipv4_t ip, c3_ushort_t port) {
  r_change_state = AC_CHANGED;
  init(AR_CONNECTION_ERROR, "Could not connect to %s:%u [%s]",
    c3_ip2address(ip), port, c3_get_error_message());
}

Result::Result(io_result_t result, bool response) {
  r_change_state = AC_CHANGED;
  const char* what = response? "receive response": "send command";
  switch (result) {
    case IO_RESULT_ERROR:
      init(AR_IO_ERROR, "Could not %s [%s]", what, c3_get_error_message());
      break;
    case IO_RESULT_EOF:
      init(AR_IO_ERROR, "Could not %s: connection dropped", what);
      break;
    default: // this includes "retry", which should not happen when using blocking I/O
      init(AR_INTERNAL_ERROR, "Could not %s [I/O result=%u]", what, result);
  }
}

Result::Result(const SocketResponseReader& reader, bool timestamps) {
  r_change_state = AC_CHANGED;
  response_type_t type = reader.get_type();
  switch (type) {
    case RESPONSE_OK:
      r_type = AR_OK;
      r_num_elements = 0;
      r_memory_size = 0;
      r_buffer = nullptr;
      break;
    case RESPONSE_DATA: {
      bool has_header_data = HeaderChunkIterator::has_header_data(reader);
      bool has_payload_data = PayloadChunkIterator::has_payload_data(reader);
      if (has_header_data && has_payload_data) {
        init(AR_INTERNAL_ERROR, "Invalid DATA response (both header and payload are present)");
      } else {
        if (has_header_data) {
          init_header_response(reader, timestamps);
        } else if (has_payload_data) {
          init_payload_response(reader);
        } else {
          init(AR_INTERNAL_ERROR, "Invalid DATA response (neither header nor payload are present)");
        }
      }
      break;
    }
    case RESPONSE_LIST:
      init_list_response(reader);
      break;
    case RESPONSE_ERROR:
      init_error_response(reader);
      break;
    default: // must be RESPONSE_INVALID
      init(AR_INTERNAL_ERROR, "Bad response object state [%d]", type);
      return;
  }
}

Result::Result(Result&& that) noexcept {
  r_type = that.r_type;
  that.r_type = AR_INVALID;
  r_num_elements = that.r_num_elements;
  that.r_num_elements = 0;
  r_memory_size = that.r_memory_size;
  that.r_memory_size = 0;
  r_buffer = that.r_buffer;
  that.r_buffer = nullptr;
}

Result::~Result() {
  if (r_buffer != nullptr) {
    if (r_buffer != ZERO_LENGTH_BUFFER) {
      c3_assert(r_memory_size && r_num_elements);
      free_memory(r_buffer, r_memory_size);
    }
  } else {
    c3_assert(r_memory_size == 0 && r_num_elements == 0);
  }
}

void Result::init_invalid() {
  r_type = AR_INVALID;
  r_num_elements = 0;
  r_memory_size = 0;
  r_buffer = nullptr;
}

void Result::init(Result::api_result_t type, const char* format, ...) {
  char buffer[MAX_MESSAGE_STRING_SIZE];
  va_list args;
  va_start(args, format);
  const int length = std::vsnprintf(buffer, sizeof buffer, format, args);
  va_end(args);
  c3_assert(length > 0);
  r_type = type;
  r_num_elements = 1;
  r_memory_size = (size_t) length + 1;
  r_buffer = (c3_byte_t*) allocate_memory(r_memory_size);
  std::memcpy(r_buffer, buffer, r_memory_size);
}

void Result::init_header_response(const SocketResponseReader& reader, bool timestamps) {
  ResultBuilder result(1024);
  ResponseHeaderIterator iterator(reader);
  bool separator = false;
  bool list_separator = false;
  do { // the method is only called if we do have some header data
    if (separator) {
      result.add_char(list_separator? '\n': ' ');
      list_separator = false;
    } else {
      separator = true;
    }
    switch (iterator.get_next_chunk_type()) {
      case CHUNK_NUMBER: {
        const NumberChunk number = iterator.get_number();
        if (number.is_valid()) {
          if (timestamps) {
            if (number.is_valid_uint()) {
              char timestamp[TIMER_FORMAT_STRING_LENGTH];
              result.add("[%s]", Timer::to_ascii(number.get_uint(), true, timestamp));
            } else {
              init(AR_INTERNAL_ERROR, "Invalid timestamp received (%lld)", number.get_value());
              return;
            }
          } else {
            result.add_number(number.get_value());
          }
          break;
        } else {
          init(AR_INTERNAL_ERROR, "Invalid number received");
          return;
        }
      }
      case CHUNK_STRING: {
        const StringChunk str = iterator.get_string();
        if (str.is_valid()) {
          result.add_string(str.get_chars(), str.get_length());
          break;
        } else {
          init(AR_INTERNAL_ERROR, "Invalid string received");
          return;
        }
      }
      case CHUNK_LIST: {
        ListChunk list = iterator.get_list();
        if (list.is_valid()) {
          c3_uint_t num_elements = list.get_count();
          result.add("List (%u element%s)", num_elements, plural(num_elements));
          for (c3_uint_t i = 0; i < num_elements; i++) {
            const StringChunk lstr = list.get_string();
            if (lstr.is_valid()) {
              result.add("\n  %u) \'", i + 1);
              result.add_string(lstr.get_chars(), lstr.get_length());
              result.add_char('\'');
            } else {
              init(AR_INTERNAL_ERROR, "Invalid header list string received");
              return;
            }
          }
          list_separator = true;
          break;
        } else {
          init(AR_INTERNAL_ERROR, "Invalid header list received");
          return;
        }
      }
      default:
        c3_assert_failure();
    }
  } while (iterator.has_more_chunks());

  r_type = AR_STRING;
  r_num_elements = 1;
  r_memory_size = result.get_size(); // this returns 1 even if nothing was stored
  r_buffer = (c3_byte_t*) allocate_memory(r_memory_size);
  std::memcpy(r_buffer, result.get_buffer(), r_memory_size);
}

void Result::init_payload_response(const SocketResponseReader& reader) {
  payload_info_t pi;
  c3_assert_def(bool ok) reader.get_payload_info(pi);
  // the method shouldn't have been called
  c3_assert(ok);
  if (pi.pi_has_errors) {
    init(AR_INTERNAL_ERROR, "Invalid payload info in response header received");
    return;
  }
  c3_assert(pi.pi_size <= pi.pi_usize);
  c3_byte_t* buffer;
  c3_uint_t size = pi.pi_usize;
  if (pi.pi_compressor == CT_NONE) {
    c3_assert(pi.pi_size == pi.pi_usize);
    if (size > 0) {
      buffer = (c3_byte_t*) allocate_memory(size);
      std::memcpy(buffer, pi.pi_buffer, size);
    } else {
      buffer = (c3_byte_t*) ZERO_LENGTH_BUFFER;
    }
  } else {
    c3_assert(pi.pi_size < pi.pi_usize);
    buffer = global_compressor.unpack(pi.pi_compressor, pi.pi_buffer, pi.pi_size, size);
  }
  r_type = AR_DATA;
  r_num_elements = size;
  r_memory_size = size;
  r_buffer = buffer;
}

void Result::init_list_response(const SocketResponseReader& reader) {
  ResponseHeaderIterator header(reader);
  const NumberChunk number = header.get_number();
  if (number.is_valid_uint() && !header.has_more_chunks()) {
    c3_uint_t count = number.get_uint();
    ResponsePayloadIterator payload(reader);
    c3_byte_t* buffer = nullptr;
    c3_uint_t size = 0;
    if (count > 0) {
      ListChunk list(payload, count);
      if (list.is_valid()) {
        ResultBuilder result(reader.get_payload_size() + 1);
        for (c3_uint_t i = 0; i < count; ++i) {
          const StringChunk str = list.get_string();
          if (str.is_valid()) {
            result.add_string(str.get_chars(), str.get_length());
            result.add_char(0);
          } else {
            init(AR_INTERNAL_ERROR, "Invalid payload list string received");
            return;
          }
        }
        size = result.get_size(); // yields 1 even if nothing was stored
        buffer = (c3_byte_t*) allocate_memory(size);
        std::memcpy(buffer, result.get_buffer(), size);
      } else {
        init(AR_INTERNAL_ERROR, "Invalid payload list received");
        return;
      }
    }
    r_type = AR_LIST;
    r_num_elements = count;
    r_memory_size = size;
    r_buffer = buffer;
  } else {
    init(AR_INTERNAL_ERROR, "Invalid payload list count received");
  }
}

void Result::init_error_response(const SocketResponseReader& reader) {
  ResponseHeaderIterator iterator(reader);
  if (iterator.get_next_chunk_type() == CHUNK_STRING) {
    const StringChunk str = iterator.get_string();
    if (str.is_valid() && str.get_length() > 0 && !iterator.has_more_chunks()) {
      c3_uint_t size = str.get_length();
      r_type = AR_ERROR;
      r_num_elements = 1;
      r_memory_size = size + 1;
      r_buffer = (c3_byte_t*) allocate_memory(r_memory_size);
      std::memcpy(r_buffer, str.get_chars(), size);
      r_buffer[size] = 0;
      return;
    }
  }
  init(AR_INTERNAL_ERROR, "Invalid error response received");
}

void Result::print_nl(FILE* file) {
  c3_assert(file);
  std::putc('\n', file);
}

void Result::print_nl() {
  std::putc('\n', stdout);
}

c3_uint_t Result::print_data(FILE *file, c3_long_t from, c3_uint_t num) {
  c3_assert(file);
  if (r_num_elements > 0) {
    c3_assert(r_buffer && r_num_elements == r_memory_size);

    // 1) Adjust requested byte range if needed
    // ----------------------------------------

    if (num == 0) {
      num = (r_lines_per_screen - 1) * r_bytes_per_line;
    }
    c3_uint_t offset = from >= 0? (c3_uint_t) from:
      (num <= r_num_elements? r_num_elements - num: 0);
    if ((c3_ulong_t) offset + num > r_num_elements) {
      num = r_num_elements - offset;
    }
    c3_assert(num && num <= r_num_elements);
    std::fprintf(file, "Response: buffer with %u byte%s, printing %u byte%s at offset %u:\n",
      r_num_elements, plural(r_num_elements), num, plural(num), offset);

    // 2) Print out data line by line as XXXXXXXX: XX XX XX XX [...] '.... [...]'
    // --------------------------------------------------------------------------

    c3_assert(r_bytes_per_line);
    const c3_uint_t num_lines = (num + r_bytes_per_line - 1) / r_bytes_per_line;
    c3_uint_t remaining_bytes = num;
    for (c3_uint_t i = 0; i < num_lines; i++) {

      // 2a) Print offset
      // ----------------

      std::fprintf(file, "%08X: ", offset);
      const c3_uint_t line_bytes = remaining_bytes > r_bytes_per_line? r_bytes_per_line: remaining_bytes;
      c3_assert(line_bytes);

      // 2b) Print byte values
      // ---------------------

      for (c3_uint_t j = 0; j < r_bytes_per_line; j ++) {
        if (j < line_bytes) {
          c3_assert(offset + j < r_num_elements);
          std::fprintf(file, "%02X ", r_buffer[offset + j]);
        } else {
          std::fprintf(file, "   ");
        }
      }

      // 2c) Print characters that correspond to byte values
      // ---------------------------------------------------

      std::fprintf(file, "\'");
      for (c3_uint_t k = 0; k < line_bytes; k ++) {
        c3_assert(offset + k < r_num_elements);
        char c = r_buffer[offset + k];
        if (isprint(c) == 0) {
          c = r_np_char;
        }
        std::fprintf(file, "%c", c);
      }
      std::fprintf(file, "\'");

      // 2d) Continue with the next line
      // -------------------------------

      remaining_bytes -= line_bytes;
      offset += line_bytes;
      print_nl();
    }
    c3_assert(remaining_bytes == 0 && offset <= r_num_elements);
    return num;
  } else {
    std::fprintf(file, "Response: empty buffer.\n");
    return 0;
  }
}

c3_uint_t Result::print_list(FILE *file, c3_long_t from, c3_uint_t num) {
  c3_assert(file);
  if (r_num_elements > 0) {
    c3_assert(r_buffer && r_num_elements < r_memory_size);

    // 1) Adjust requested string range if needed
    // ------------------------------------------

    if (num == 0) {
      num = r_lines_per_screen - 1;
    }
    const c3_uint_t offset = from >= 0? (c3_uint_t) from:
      (num <= r_num_elements? r_num_elements - num: 0);
    if ((c3_ulong_t) offset + num > r_num_elements) {
      num = r_num_elements - offset;
    }
    c3_assert(num && num <= r_num_elements);
    std::fprintf(file, "Response: list of %u string%s, printing %u string%s starting at %u:\n",
      r_num_elements, plural(r_num_elements), num, plural(num), offset);

    // 2) Collect strings
    // ------------------

    auto next = (const char*) r_buffer;
    const char* buffer[num];
    for (c3_uint_t i = 0; i < offset + num; i ++) {
      if (i >= offset) {
        buffer[i - offset] = next;
      }
      size_t length = std::strlen(next) + 1;
      c3_assert(((c3_byte_t*) next - r_buffer) + length < r_memory_size);
      next += length;
    }

    // 3) See if strings represent output of `INFO` or a similar command
    // -----------------------------------------------------------------

    bool info_output = true;
    c3_uint_t colon_offsets[num];
    c3_uint_t max_colon_offset = 0;
    for (c3_uint_t j = 0; j < num; j++) {
      const char* const str = buffer[j];
      c3_assert(str);
      const char* colon = std::strstr(str, ": ");
      if (colon != nullptr) {
        const c3_uint_t colon_offset = (c3_uint_t)(colon - str);
        if (colon_offset <= MAX_COLON_OFFSET) {
          colon_offsets[j] = colon_offset;
          if (colon_offset > max_colon_offset) {
            max_colon_offset = colon_offset;
          }
        } else {
          info_output = false;
          break;
        }
      } else {
        info_output = false;
        break;
      }
    }

    // 4) Print out strings line by line as DDDD: 'sss' OR as adjusted sss: sss
    // ------------------------------------------------------------------------

    for (c3_uint_t k = 0; k < num; k++) {
      const char* const str = buffer[k];
      c3_assert(str);
      if (info_output) {
        c3_uint_t colon_offset = colon_offsets[k];
        c3_assert(colon_offset <= max_colon_offset);
        c3_uint_t string_offset = max_colon_offset - colon_offset;
        std::fprintf(file, "%*s%s\n", string_offset, "", str);
      } else {
        std::fprintf(file, "%3d: '%s'\n", offset + k, str);
      }
    }

    return num;
  } else {
    std::fprintf(file, "Response: empty list\n");
    return 0;
  }
}

void Result::print(FILE* file) {
  c3_assert(file);
  r_change_state = AC_PRINTED;
  switch (r_type) {
    case AR_INVALID:
      std::fprintf(file, "Server commands had not been executed yet.");
      break;
    case AR_INTERNAL_ERROR:
      std::fprintf(file, "Internal ERROR: %s\n", r_buffer);
      return;
    case AR_CONNECTION_ERROR:
      std::fprintf(file, "Connection ERROR: %s\n", r_buffer);
      return;
    case AR_IO_ERROR:
      std::fprintf(file, "I/O ERROR: %s\n", r_buffer);
      return;
    case AR_OK:
      std::fprintf(file, "Response: OK\n");
      return;
    case AR_ERROR:
      std::fprintf(file, "ERROR response: %s\n", r_buffer);
      return;
    case AR_STRING:
      std::fprintf(file, "Response: %s\n", r_buffer);
      return;
    case AR_LIST:
      std::fprintf(file, "Response: list of %u string%s.", r_num_elements, plural(r_num_elements));
      break;
    case AR_DATA:
      std::fprintf(file, "Response: buffer with %u byte%s.", r_num_elements, plural(r_num_elements));
      break;
    default:
      c3_assert_failure();
  }
  if (r_num_elements > 0) {
    std::fprintf(file, " Use `RESULT` command to print out.");
  }
  print_nl(file);
}

void Result::print() {
  print(stdout);
}

c3_uint_t Result::print(FILE* file, c3_long_t from, c3_uint_t num) {
  c3_assert(file);
  r_change_state = AC_PRINTED;
  switch (r_type) {
    case AR_INVALID:
    case AR_INTERNAL_ERROR:
    case AR_CONNECTION_ERROR:
    case AR_IO_ERROR:
    case AR_OK:
    case AR_ERROR:
    case AR_STRING:
      print(file);
      break;
    case AR_LIST:
      return print_list(file, from, num > 0? num: r_num_elements);
    case AR_DATA:
      return print_data(file, from, num > 0? num: r_num_elements);
    default:
      c3_assert_failure();
  }
  return 0;
}

c3_uint_t Result::print(c3_long_t from, c3_uint_t num) {
  return print(stdout, from, num);
}

bool Result::contains(const char* str) {
  c3_assert(str);
  switch (r_type) {
    case AR_STRING:
      if (r_memory_size == 0) {
        return false;
      }
      // fall through
    case AR_ERROR:
      c3_assert(r_memory_size && r_num_elements == 1);
      return std::strstr((const char*) r_buffer, str) != nullptr;
    case AR_LIST: {
      if (r_memory_size == 0) {
        return false;
      }
      bool substring = false;
      if (str[0] == '%') {
        str++;
        substring = true;
      }
      auto next = (const char*) r_buffer;
      for (c3_uint_t i = 0; i < r_num_elements; i++) {
        if (substring) {
          if (std::strstr(next, str) != nullptr) {
            return true;
          }
        } else if (std::strcmp(next, str) == 0) {
          return true;
        }
        next += std::strlen(next) + 1;
      }
      return false;
    }
    default:
      c3_assert_failure();
      return false;
  }
}

bool Result::contains(c3_uint_t offset, const void* data, c3_uint_t size) {
  c3_assert(data && r_type == AR_DATA && r_buffer && r_memory_size == r_num_elements);
  if (offset + size <= r_num_elements) {
    if (std::memcmp(r_buffer + offset, data, size) == 0) {
      return true;
    }
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// CommandInfo
///////////////////////////////////////////////////////////////////////////////

CommandInfo::CommandInfo(command_t id): cic_id(id) {
  cic_mode = CEM_AUTO;
  cic_marker = false;
  cic_hash = INVALID_HASH_VALUE;
}

CommandInfo::CommandInfo(c3_byte_t id, bool admin, const char* password, bool check):
  cic_id((command_t) id) {
  if (admin) {
    cic_mode = CEM_ADMIN;
    cic_hash = console_net_config.get_admin_password();
    if (password != nullptr) {
      console_net_config.set_admin_password(password);
    }
  } else {
    cic_mode = CEM_USER;
    cic_hash = console_net_config.get_user_password();
    if (password != nullptr) {
      console_net_config.set_user_password(password);
    }
  }
  cic_marker = console_net_config.get_command_integrity_check();
  console_net_config.set_command_integrity_check(check);
}

CommandInfo::~CommandInfo() {
  if (cic_mode != CEM_AUTO) {
    if (cic_mode == CEM_ADMIN) {
      console_net_config.set_raw_admin_password(cic_hash);
    } else if (cic_mode == CEM_USER) {
      console_net_config.set_raw_user_password(cic_hash);
    }
    console_net_config.set_command_integrity_check(cic_marker);
  }
}

bool CommandInfo::is_admin_command() const {
  switch (cic_mode) {
    case CEM_USER:
      return false;
    case CEM_ADMIN:
      return true;
    default:
      switch (cic_id) {
        case CMD_PING:
        case CMD_CHECK:
        case CMD_INFO:
        case CMD_STATS:
          return console_net_config.get_admin_password() != INVALID_HASH_VALUE;
        case CMD_SHUTDOWN:
        case CMD_LOADCONFIG:
        case CMD_RESTORE:
        case CMD_STORE:
        case CMD_GET:
        case CMD_SET:
        case CMD_LOG:
        case CMD_ROTATE:
          return true;
        case CMD_READ:
        case CMD_WRITE:
        case CMD_DESTROY:
        case CMD_GC:
        case CMD_LOAD:
        case CMD_TEST:
        case CMD_SAVE:
        case CMD_REMOVE:
        case CMD_CLEAN:
        case CMD_GETIDS:
        case CMD_GETTAGS:
        case CMD_GETIDSMATCHINGTAGS:
        case CMD_GETIDSNOTMATCHINGTAGS:
        case CMD_GETIDSMATCHINGANYTAGS:
        case CMD_GETFILLINGPERCENTAGE:
        case CMD_GETMETADATAS:
        case CMD_TOUCH:
          return false;
        default:
          c3_assert_failure();
          return false;
      }
  }
}

bool CommandInfo::sends_timestamps_in_response() const {
  if (cic_mode == CEM_AUTO) {
    switch (cic_id) {
      case CMD_TEST:
      case CMD_GETMETADATAS:
        return true;
      default:
        return false;
    }
  } else {
    return false;
  }
}

///////////////////////////////////////////////////////////////////////////////
// CyberCache
///////////////////////////////////////////////////////////////////////////////

CyberCache::CyberCache() noexcept: cc_socket(true, false), cc_tags(16) {
  c3_address2ip("127.0.0.1", cc_ip);
  cc_port = C3_DEFAULT_PORT;
  cc_auto_result = ARM_LISTS;
  cc_user_agent = UA_USER;
  cc_lifetime = -1; // use default specified in server config file
  cc_offset = 0;
  cc_count = 0; // means "re-calculate"
  cc_persistent = true;
}

const char* CyberCache::get_address() const {
  return c3_ip2address(cc_ip);
}

bool CyberCache::set_address(const char* address) {
  c3_ipv4_t ip = c3_resolve_host(address);
  if (ip != INVALID_IPV4_ADDRESS) {
    cc_ip = ip;
    return true;
  }
  return false;
}

void CyberCache::populate_list(HeaderListChunkBuilder& header_list, const StringList* string_list) {
  // current set of commands can only have one list in the header
  c3_assert(string_list && header_list.get_size() == 0);
  for (c3_uint_t i = 0; i < string_list->get_count(); i++) {
    header_list.estimate(string_list->get(i));
  }
  header_list.configure();
  for (c3_uint_t j = 0; j < string_list->get_count(); j++) {
    header_list.add(string_list->get(j));
  }
  header_list.check();
}

Result CyberCache::run(command_t cmd, const void* buffer, c3_uint_t length,
  const char* format, va_list args) {

  const size_t num_arguments = std::strlen(format);
  CommandArgument arguments[num_arguments];

  const char* type = format;
  c3_uint_t index = 0;
  while (*type != '\0') {
    switch (*type) {
      case 'N': // signed number
        arguments[index] = CommandArgument(va_arg(args, c3_int_t));
        break;
      case 'U': // unsigned number
        arguments[index] = CommandArgument(va_arg(args, c3_uint_t));
        break;
      case 'S': // string
        arguments[index] = CommandArgument(va_arg(args, const char*));
        break;
      case 'L': // string list
        arguments[index] = CommandArgument(va_arg(args, const StringList*));
        break;
      default:
        c3_assert_failure();
    }
    type++;
    index++;
  }
  c3_assert(index == num_arguments);

  const CommandInfo info(cmd);
  return emulate(info, buffer, length, format, arguments);
}

Result CyberCache::emulate(const CommandInfo& info, const void* buffer, c3_uint_t length,
  const char* format, const CommandArgument* arguments) {

  // 1) Establish connection to the server
  // -------------------------------------

  if (!cc_socket.connect(cc_ip, cc_port, cc_persistent)) {
    return Result(cc_ip, cc_port);
  }
  SocketGuard guard(cc_socket);

  // 2) Create necessary objects
  // ---------------------------

  SharedBuffers* cmd_sb = SharedBuffers::create(global_memory);
  SocketCommandWriter command(global_memory, cc_socket.get_fd(), cc_ip, cmd_sb);
  CommandHeaderChunkBuilder header(command, console_net_config, info.get_id(), info.is_admin_command());
  HeaderListChunkBuilder list(command, console_net_config); // just in case...

  // 3) Estimate header size
  // -----------------------

  const char* type = format;
  c3_uint_t index = 0;
  while (*type != '\0') {
    switch (*type) {
      case 'N': { // signed number
        c3_assert_def(c3_uint_t size) header.estimate_number(arguments[index].get_int());
        c3_assert(size);
        break;
      }
      case 'U': { // unsigned number
        c3_assert_def(c3_uint_t size) header.estimate_number(arguments[index].get_uint());
        c3_assert(size);
        break;
      }
      case 'S': { // string
        c3_assert_def(c3_uint_t size) header.estimate_cstring(arguments[index].get_string());
        c3_assert(size);
        break;
      }
      case 'L': { // string list
        populate_list(list, arguments[index].get_list());
        c3_assert_def(c3_uint_t size) header.estimate_list(list);
        c3_assert(size);
        break;
      }
      default:
        c3_assert_failure();
    }
    type++;
    index++;
  }

  // 4) Configure payload
  // --------------------

  if (buffer != nullptr) {
    PayloadChunkBuilder payload(command, console_net_config);
    payload.add((const c3_byte_t*) buffer, length);
    header.configure(&payload);
  } else {
    header.configure(nullptr);
  }

  // 5) Add data chunks to the header
  // --------------------------------

  type = format;
  index = 0;
  while (*type != '\0') {
    switch (*type) {
      case 'N': // signed number
        header.add_number(arguments[index].get_int());
        break;
      case 'U': // unsigned number
        header.add_number(arguments[index].get_uint());
        break;
      case 'S': // string
        header.add_cstring(arguments[index].get_string());
        break;
      case 'L': // string list
        header.add_list(list);
        break;
      default:
        c3_assert_failure();
    }
    type++;
    index++;
  }

  // 6) Complete header configuration
  // --------------------------------

  header.check();

  // 7) Send command to the server and process result
  // ------------------------------------------------

  io_result_t result;
  bool first_time = true;
  for(;;) {
    c3_ulong_t written_bytes;
    result = command.write(written_bytes);
    switch (result) {
      case IO_RESULT_OK:
        break;
      case IO_RESULT_EOF:
      case IO_RESULT_ERROR:
        if (first_time && cc_socket.is_persistent() && cc_socket.reconnect()) {
          /*
           * we get here if console application was put into "persistent connections" mode, while
           * the server works in "per-command connections" mode, so it apparently hung up after last
           * submitted command, and we should retry (but only one time)
           */
          command.io_rewind(cc_socket.get_fd(), cc_socket.get_address());
        } else {
          return Result(result, false);
        }
        // fall through
      case IO_RESULT_RETRY:
        first_time = false;
        continue;
    }
    break;
  }

  // 8) Receive and process response from the server
  // -----------------------------------------------

  SharedBuffers* resp_sb = SharedBuffers::create(global_memory);
  // `reconnect()` could have changed socket handle, so we could not initialize response object earlier
  SocketResponseReader response(global_memory, cc_socket.get_fd(), cc_ip, resp_sb);

  do {
    c3_ulong_t read_bytes;
    result = response.read(read_bytes);
  } while (result == IO_RESULT_RETRY);

  if (result != IO_RESULT_OK) {
    return Result(result, true);
  }
  return Result(response, info.sends_timestamps_in_response());
}

Result CyberCache::execute(command_t cmd, c3_uint_t size, const void* buffer, const char* format, ...) {
  va_list args;
  va_start(args, format);
  Result result = run(cmd, buffer, size, format, args);
  va_end(args);
  return std::move(result);
}

Result CyberCache::execute(command_t cmd, const char* format, ...) {
  va_list args;
  va_start(args, format);
  Result result = run(cmd, nullptr, 0, format, args);
  va_end(args);
  return std::move(result);
}

Result CyberCache::execute(command_t cmd) {
  return execute(cmd, "");
}

} // CyberCache
