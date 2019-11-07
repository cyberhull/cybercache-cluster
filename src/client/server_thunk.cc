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
#include "ext_errors.h"
#include "ext_globals.h"
#include "ext_functions.h"
#include "ext_resource.h"
#include "server_thunk.h"

#include <cstdio>

///////////////////////////////////////////////////////////////////////////////
// ERROR HANDLING
///////////////////////////////////////////////////////////////////////////////

// how many characters to print out in diagnostic messages
static constexpr int STRING_PRINTABLE_PREFIX_LENGTH = 16;

// GCC does not allow attributes on definitions, so we have to provide declaration here
static void set_error(c3_error_return_t error_return, zval* return_value) C3_FUNC_COLD;
static void set_error(c3_error_return_t error_return, zval* return_value,
  const char* format, ...) C3_FUNC_PRINTF(3) C3_FUNC_COLD;
static void set_internal_error(c3_error_return_t error_return, zval* return_value,
  const char* format, ...) C3_FUNC_PRINTF(3) C3_FUNC_COLD;

static void set_error(c3_error_return_t error_return, zval* return_value) {
  c3_assert(return_value);
  switch (error_return) {
    case ESR_FALSE_FROM_OK:
    case ESR_FALSE_FROM_ERROR:
      RETVAL_FALSE;
      return;
    case ESR_ZERO_FROM_ERROR:
      RETVAL_LONG(0);
      return;
    case ESR_EMPTY_STRING_FROM_OK:
      RETVAL_EMPTY_STRING();
      return;
    case ESR_EMPTY_ARRAY_FROM_ERROR:
      array_init(return_value);
      return;
    default:
      assert_failure();
  }
}

static void set_error(c3_error_return_t error_return, zval* return_value, const char* format, ...) {
  c3_assert(format);
  char buffer[512];
  va_list args;
  va_start(args, format);
  std::vsnprintf(buffer, sizeof buffer, format, args);
  va_end(args);
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wformat-security"
  report_error(buffer);
  #pragma GCC diagnostic pop
  set_error(error_return, return_value);
}

static void set_internal_error(c3_error_return_t error_return, zval* return_value,
  const char* format, ...) {
  c3_assert(format);
  char buffer[512];
  va_list args;
  va_start(args, format);
  std::vsnprintf(buffer, sizeof buffer, format, args);
  va_end(args);
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wformat-security"
  report_internal_error(buffer);
  #pragma GCC diagnostic pop
  set_error(error_return, return_value);
}

///////////////////////////////////////////////////////////////////////////////
// StringAllocator
///////////////////////////////////////////////////////////////////////////////

/**
 * Allocator to be used for unpacking data buffers received from the server.
 *
 * It doesn't do actual allocation or freeing: it takes existing `zend_string` structure and returns its
 * string data buffer upon `alloc()` call.
 */
class StringAllocator: public Allocator {
  const zend_string* const sa_zstring; // string that will hold uncompressed data

  c3_byte_t* alloc(c3_uint_t size) override;
  void free(void* buff, c3_uint_t size) override;

public:
  explicit StringAllocator(const zend_string* zstring): sa_zstring(zstring) {
    c3_assert(zstring);
  }
};

c3_byte_t* StringAllocator::alloc(c3_uint_t size) {
  c3_assert(ZSTR_LEN(sa_zstring) == size);
  return (c3_byte_t*) ZSTR_VAL(sa_zstring);
}

void StringAllocator::free(void* buff, c3_uint_t size) {
  c3_assert(ZSTR_LEN(sa_zstring) == size);
}

///////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION
///////////////////////////////////////////////////////////////////////////////

static bool populate_list(HeaderListChunkBuilder& header_list, HashTable* string_list,
  c3_error_return_t error_return, zval* return_value) {

  // current set of commands can only have one list in the header
  c3_assert(header_list.get_size() == 0);

  // 1) Estimate header list size
  // ----------------------------

  HashPosition ht_pos;
  if (string_list != nullptr) {
    c3_ulong_t total_size = 5; // maximum possible list overhead
    zend_hash_internal_pointer_reset_ex(string_list, &ht_pos);
    for (;;) {
      const zval* data = zend_hash_get_current_data_ex(string_list, &ht_pos);
      if (data != nullptr) {
        if (Z_TYPE_P(data) == IS_STRING) {
          size_t length = Z_STRLEN_P(data);
          if (length <= UINT_MAX_VAL) {
            c3_uint_t chunk_size = header_list.estimate((c3_uint_t) length);
            c3_assert(chunk_size);
            total_size += chunk_size;
            if (total_size >= UINT_MAX_VAL) {
              set_error(error_return, return_value, "String list is too big: %llu bytes", total_size);
              return false;
            }
          } else {
            const char* long_string = Z_STRVAL_P(data);
            set_error(error_return, return_value, "List string is too long (%lu bytes): '%.*s'",
              length, STRING_PRINTABLE_PREFIX_LENGTH, long_string);
            return false;
          }
        } else {
          set_error(error_return, return_value, "Array value is not a string");
          return false;
        }
        zend_hash_move_forward_ex(string_list, &ht_pos);
      } else {
        break;
      }
    }
  }

  // 2) Configure header list
  // ------------------------

  header_list.configure();

  // 3) Add strings to the list
  // --------------------------

  if (string_list != nullptr) {
    zend_hash_internal_pointer_reset_ex(string_list, &ht_pos);
    for (;;) {
      const zval* data = zend_hash_get_current_data_ex(string_list, &ht_pos);
      if (data != nullptr) {
        c3_assert(Z_TYPE_P(data) == IS_STRING);
        size_t length = Z_STRLEN_P(data);
        c3_assert(length <= UINT_MAX_VAL);
        header_list.add((c3_uint_t) length, Z_STRVAL_P(data));
        zend_hash_move_forward_ex(string_list, &ht_pos);
      } else {
        break;
      }
    }
  }

  // 4) Validate the list
  // --------------------

  header_list.check();
  return true;
}

static void fetch_error_message(const SocketResponseReader& reader, C3Resource* res,
  c3_error_return_t error_return, zval* return_value) C3_FUNC_COLD;
static void fetch_error_message(const SocketResponseReader& reader, C3Resource* res,
  c3_error_return_t error_return, zval* return_value) {
  ResponseHeaderIterator iterator(reader);
  if (iterator.get_next_chunk_type() == CHUNK_STRING) {
    StringChunk str = iterator.get_string();
    if (str.is_valid() && str.get_length() > 0 && !iterator.has_more_chunks()) {
      res->set_error_message(str.get_chars(), str.get_length());
      set_error(error_return, return_value);
      return;
    }
  }
  set_internal_error(error_return, return_value, "received malformed ERROR response");
}

static void fetch_number_from_data_header(const SocketResponseReader& reader,
  c3_error_return_t error_return, zval* return_value) {
  if (!PayloadChunkIterator::has_payload_data(reader)) {
    ResponseHeaderIterator iterator(reader);
    if (iterator.get_next_chunk_type() == CHUNK_NUMBER) {
      NumberChunk number = iterator.get_number();
      if (number.is_valid() && !iterator.has_more_chunks()) {
        RETVAL_LONG(number.get_value());
        return;
      }
    }
  }
  set_internal_error(error_return, return_value, "received malformed NUMBER response");
}

static void fetch_num3_array_from_data_header(const SocketResponseReader& reader,
  c3_error_return_t error_return, zval* return_value) {
  if (!PayloadChunkIterator::has_payload_data(reader)) {
    ResponseHeaderIterator iterator(reader);
    if (iterator.get_next_chunk_type() == CHUNK_NUMBER) {
      NumberChunk num1 = iterator.get_number();
      if (num1.is_valid() && iterator.get_next_chunk_type() == CHUNK_NUMBER) {
        NumberChunk num2 = iterator.get_number();
        if (num2.is_valid() && iterator.get_next_chunk_type() == CHUNK_NUMBER) {
          NumberChunk num3 = iterator.get_number();
          if (num3.is_valid() && !iterator.has_more_chunks()) {
            // only initialize return value when we're sure the data are OK
            array_init(return_value);
            add_next_index_long(return_value, num1.get_value());
            add_next_index_long(return_value, num2.get_value());
            add_next_index_long(return_value, num3.get_value());
            return;
          }
        }
      }
    }
  }
  set_internal_error(error_return, return_value, "received malformed NUMERIC ARRAY response");
}

static void fetch_metadata_from_data_header(const SocketResponseReader& reader,
  c3_error_return_t error_return, zval* return_value) {
  if (!PayloadChunkIterator::has_payload_data(reader)) {
    ResponseHeaderIterator iterator(reader);
    if (iterator.get_next_chunk_type() == CHUNK_NUMBER) {
      NumberChunk expiration = iterator.get_number();
      if (expiration.is_valid() && iterator.get_next_chunk_type() == CHUNK_NUMBER) {
        NumberChunk modification = iterator.get_number();
        if (modification.is_valid() && iterator.get_next_chunk_type() == CHUNK_LIST) {
          ListChunk list = iterator.get_list();
          if (list.is_valid()) {
            c3_uint_t num_elements = list.get_count();
            /*
             * We have pulled as much data (and did as many checks) as we could before we must commit and
             * start forming a valid response.
             *
             * Any errors beyond this point will be reported, but ignored (in that malformed data will be
             * simply excluded from the result).
             */
            array_init(return_value);
            add_assoc_long(return_value, "expire", expiration.get_value());
            add_assoc_long(return_value, "mtime", modification.get_value());
            zval tags;
            array_init(&tags);
            bool errors = false;
            for (c3_uint_t i = 0; i < num_elements; i++) {
              StringChunk lstr = list.get_string();
              if (lstr.is_valid()) {
                add_next_index_stringl(&tags, lstr.get_chars(), lstr.get_length());
              } else {
                errors = true;
              }
            }
            add_assoc_zval(return_value, "tags", &tags);
            if (errors || iterator.has_more_chunks()) {
              report_internal_error("received METADATA response with malformed chunk(s)");
            }
            return;
          }
        }
      }
    }
  }
  set_internal_error(error_return, return_value, "received malformed METADATA response");
}

static void fetch_string_from_data_payload(const SocketResponseReader& reader,
  c3_error_return_t error_return, zval* return_value) {
  if (!HeaderChunkIterator::has_header_data(reader)) {
    payload_info_t pi;
    if (reader.get_payload_info(pi) && !pi.pi_has_errors) {
      if (pi.pi_size > 0) {
        c3_assert(pi.pi_size && pi.pi_usize && pi.pi_buffer);
        zend_string* zstring = zend_string_alloc(pi.pi_usize, 0);
        if (pi.pi_compressor == CT_NONE) {
          c3_assert(pi.pi_size == pi.pi_usize);
          std::memcpy(ZSTR_VAL(zstring), pi.pi_buffer, pi.pi_usize);
        } else {
          c3_assert(pi.pi_size < pi.pi_usize);
          StringAllocator allocator(zstring);
          if (global_compressor.unpack(pi.pi_compressor, pi.pi_buffer, pi.pi_size,
            pi.pi_usize, allocator) == nullptr) {
            zend_string_free(zstring);
            set_internal_error(error_return, return_value, "received corrupt DATA response");
            return;
          }
        }
        ZSTR_VAL(zstring)[pi.pi_usize] = 0; // terminating '\0'
        RETVAL_NEW_STR(zstring);
        return;
      } else {
        RETVAL_EMPTY_STRING();
        return;
      }
    }
  }
  set_internal_error(error_return, return_value, "received malformed DATA response");
}

static void fetch_array_from_list_payload(const SocketResponseReader& reader,
  c3_error_return_t error_return, zval* return_value) {
  ResponseHeaderIterator header(reader);
  NumberChunk number = header.get_number();
  if (number.is_valid_uint() && !header.has_more_chunks()) {
    c3_uint_t count = number.get_uint();
    /*
     * We've done as many checks as we could before committing to a valid response. Any malformed
     * data beyond this point will be reported, but otherwise ignored.
     */
    array_init(return_value);
    bool errors = false;
    if (count > 0) {
      ResponsePayloadIterator payload(reader);
      ListChunk list(payload, count);
      if (list.is_valid()) {
        for (c3_uint_t i = 0; i < count; ++i) {
          StringChunk str = list.get_string();
          if (str.is_valid()) {
            add_next_index_stringl(return_value, str.get_chars(), str.get_length());
          } else {
            errors = true;
          }
        }
      } else {
        errors = true;
      }
    }
    if (errors) {
      report_internal_error("received LIST response with malformed string(s)");
    }
    return;
  }
  set_internal_error(error_return, return_value, "received malformed LIST response");
}

///////////////////////////////////////////////////////////////////////////////
// INTERFACE
///////////////////////////////////////////////////////////////////////////////

void call_c3(const zval* rc, zval* return_value, c3_ok_return_t ok_return, c3_error_return_t error_return,
  command_t cmd, c3_auth_type_t auth, const char* format, c3_arg_t* args) {

  c3_assert(rc && return_value && cmd && format && (format[0] == 0 || args));

  // 1) Get and validate resource handle
  // -----------------------------------

  auto res = (C3Resource*) zend_fetch_resource(Z_RES_P(rc), C3_RESOURCE_NAME, le_cybercache_res);
  if (res == nullptr) {
    set_error(error_return, return_value, "Invalid or incompatible resource");
    return;
  }
  res->reset_error_message();

  // 2) Figure out whether the command should be submitted with admin- or user-level authentication
  // ----------------------------------------------------------------------------------------------

  bool execute_as_admin;
  switch (auth) {
    default:
      c3_assert_failure();
      // either does not return, or does nothing
    case AUT_USER:
      execute_as_admin = false;
      break;
    case AUT_ADMIN:
      execute_as_admin = true;
      break;
    case AUT_INFO:
      execute_as_admin = C3GLOBAL(mg_info_password) == IPD_ADMIN;
      break;
  }

  // 3) Create network configuration object
  // --------------------------------------

  NetworkConfiguration net_config(res->get_user_password(), res->get_admin_password(),
    res->get_compressor(), res->get_threshold(), res->get_marker());

  // 4) Establish connection to the server
  // -------------------------------------

  char ip_address[C3_SOCK_MIN_ADDR_LENGTH];
  if (!c3_request_socket.connect(res->get_address(), res->get_port(), res->is_persistent())) {
    set_error(error_return, return_value, "Could not connect to '%s:%hu'",
      c3_ip2address(res->get_address(), ip_address), res->get_port());
    return;
  }
  SocketGuard guard(c3_request_socket);

  // 5) Create I/O objects
  // ---------------------

  SharedBuffers* cmd_sb = SharedBuffers::create(global_memory);
  SocketCommandWriter command(global_memory, c3_request_socket.get_fd(), res->get_address(), cmd_sb);
  CommandHeaderChunkBuilder header(command, net_config, cmd, execute_as_admin);
  HeaderListChunkBuilder list(command, net_config); // just in case...

  // 3) Estimate header size and fetch payload (if any)
  // --------------------------------------------------

  const c3_arg_t* payload = nullptr;
  c3_ulong_t header_size = 0;
  c3_uint_t index = 0;
  const char* specifier = format;
  while (*specifier != '\0') {
    switch (*specifier++) {
      case 'N': { // signed number
        c3_long_t num = args[index].a_number;
        /*
         * Here, we test the number against the full range [currently] supported by the protocol, even
         * though its actual use in the PHP extension only requires [-1..INT_MAX_VAL], to transfer domain
         * index masks (1..7), numbers of seconds for UNIX timestamps (0..INT_MAX_VAL), and default
         * lifetimes (-1).
         */
        if (num < INT_MIN_VAL || num > UINT_MAX_VAL) {
          set_error(error_return, return_value, "Number not in [%d..%u] range: %lld",
            INT_MIN_VAL, UINT_MAX_VAL, num);
          return;
        }
        c3_uint_t chunk_size = header.estimate_number(num);
        header_size += chunk_size;
        c3_assert(chunk_size);
        index++;
        break;
      }
      case 'S': { // string
        c3_assert(args[index].a_string);
        size_t length = args[index].a_size;
        if (length > UINT_MAX_VAL) {
          set_error(error_return, return_value, "String longer than %u bytes (%lu bytes): '%.*s ...'",
            UINT_MAX_VAL, length, STRING_PRINTABLE_PREFIX_LENGTH, args[index].a_string);
          return;
        }
        c3_uint_t chunk_size = header.estimate_string((c3_uint_t) length);
        header_size += chunk_size;
        c3_assert(chunk_size);
        index++;
        break;
      }
      case 'L':
        if (populate_list(list, args[index].a_list, error_return, return_value)) {
          c3_uint_t chunk_size = header.estimate_list(list);
          header_size += chunk_size;
          c3_assert(chunk_size);
          index++;
          break;
        } else {
          // error message had already been printed, and return value set
          return;
        }
      case 'A': {
        c3_assert(res->get_user_agent() < UA_NUMBER_OF_ELEMENTS);
        c3_uint_t chunk_size = header.estimate_number(res->get_user_agent());
        header_size += chunk_size;
        c3_assert(chunk_size);
        break;
      }
      case 'P':
        c3_assert(payload == nullptr && args[index].a_buffer);
        payload = args + index;
        if (payload->a_size > UINT_MAX_VAL) {
          set_error(error_return, return_value, "Data buffer bigger than %u bytes: %lu bytes",
            UINT_MAX_VAL, payload->a_size);
          return;
        }
        index++;
        break;
      default:
        c3_assert_failure();
    }
    if (header_size > UINT_MAX_VAL) {
      set_error(error_return, return_value, "Command header bigger than %u bytes: %llu bytes",
        UINT_MAX_VAL, header_size);
      return;
    }
  }

  // 4) Configure payload
  // --------------------

  if (payload != nullptr) {
    PayloadChunkBuilder payload_builder(command, net_config);
    payload_builder.add(payload->a_buffer, (c3_uint_t) payload->a_size);
    header.configure(&payload_builder);
  } else {
    header.configure(nullptr);
  }

  // 5) Add data chunks to the header
  // --------------------------------

  index = 0;
  specifier = format;
  while (*specifier != '\0') {
    switch (*specifier++) {
      case 'N': // signed number
        header.add_number(args[index].a_number);
        index++;
        break;
      case 'S': // string
        header.add_string(args[index].a_string, (c3_uint_t) args[index].a_size);
        index++;
        break;
      case 'L': // string list
        header.add_list(list);
        index++;
        break;
      case 'A': // "virtual" user agent
        header.add_number(res->get_user_agent());
        break;
      case 'P':
        // nothing else to do
        index++;
        break;
      default:
        c3_assert_failure();
    }
  }

  // 6) Validate header
  // ------------------

  header.check();

  // 7) Send command to the server
  // -----------------------------

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
        if (first_time && res->is_persistent() && c3_request_socket.reconnect()) {
          /*
           * we get here if PHP extension was put into "persistent connections" mode, while
           * the server works in "per-command connections" mode, so it apparently hung up after last
           * submitted command, and we should retry (but only once)
           */
          command.io_rewind(c3_request_socket.get_fd(), res->get_address());
        } else {
          set_error(error_return, return_value, "Could not send command to %s:%hu (result=%u)",
            c3_ip2address(res->get_address(), ip_address), res->get_port(), result);
          return;
        }
        // fall through
      case IO_RESULT_RETRY:
        first_time = false;
        continue;
    }
    break;
  }

  // 8) Receive response from the server
  // -----------------------------------

  SharedBuffers* resp_sb = SharedBuffers::create(global_memory);
  // `reconnect()` could have changed socket handle, so we could not initialize response earlier
  SocketResponseReader response(global_memory, c3_request_socket.get_fd(), res->get_address(), resp_sb);

  do {
    c3_ulong_t read_bytes;
    result = response.read(read_bytes);
  } while (result == IO_RESULT_RETRY);

  if (result != IO_RESULT_OK) {
    set_error(error_return, return_value, "Could not receive response from %s:%hu (result=%u)",
      c3_ip2address(res->get_address(), ip_address), res->get_port(), result);
    return;
  }

  // 9) Process server response
  // --------------------------

  response_type_t type = response.get_type();
  switch (type) {
    case RESPONSE_OK:
      if (ok_return == OSR_TRUE_FROM_OK) {
        RETVAL_TRUE;
        return;
      }
      switch (error_return) {
        case ESR_FALSE_FROM_OK:
          RETVAL_FALSE;
          return;
        case ESR_EMPTY_STRING_FROM_OK:
          RETVAL_EMPTY_STRING();
          return;
        default:
          break;
      }
      break;
    case RESPONSE_DATA:
      switch (ok_return) {
        case OSR_NUMBER_FROM_DATA_HEADER:
          fetch_number_from_data_header(response, error_return, return_value);
          return;
        case OSR_NUM3_ARRAY_FROM_DATA_HEADER:
          fetch_num3_array_from_data_header(response, error_return, return_value);
          return;
        case OSR_METADATA_FROM_DATA_HEADER:
          fetch_metadata_from_data_header(response, error_return, return_value);
          return;
        case OSR_STRING_FROM_DATA_PAYLOAD:
          fetch_string_from_data_payload(response, error_return, return_value);
          return;
        default:
          break;
      }
      break;
    case RESPONSE_LIST:
      if (ok_return == OSR_ARRAY_FROM_LIST_PAYLOAD) {
        fetch_array_from_list_payload(response, error_return, return_value);
        return;
      }
      break;
    case RESPONSE_ERROR:
      fetch_error_message(response, res, error_return, return_value);
      return;
    default:
      c3_assert_failure();
  }

  // 10) Process unexpected responses
  // --------------------------------

  set_internal_error(error_return, return_value, "unexpected server response [C%02X:R%u:E%u]",
    cmd, type, ok_return);
}
