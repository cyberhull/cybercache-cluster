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
 * Classes that are to be used to compose commands' and responses' data.
 */
#ifndef _IO_CHUNK_BUILDERS_H
#define _IO_CHUNK_BUILDERS_H

#include "c3_build.h"
#include "c3_memory.h"
#include "io_protocol.h"
#include "io_reader_writer.h"
#include "io_net_config.h"

#include <cstring>

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// GENERIC CHUNK BUILDERS
///////////////////////////////////////////////////////////////////////////////

/// Base class for all chunk builders
class ChunkBuilder {
protected:
  ReaderWriter&               cb_container;  // object that will receive built chunks
  const NetworkConfiguration& cb_net_config; // where to get I/O options from

  ChunkBuilder(ReaderWriter& container, const NetworkConfiguration& net_config):
    cb_container(container), cb_net_config(net_config) {
    assert(container.is_clear(IO_FLAG_IS_READER));
  }

  static c3_uint_t measure_entity(c3_uint_t n) {
    if (n < CHNK_LARGE_BIAS) {
      return 1;
    }
    n -= CHNK_LARGE_BIAS;
    if ((n & 0xFFFFFF00) == 0) {
      return 2;
    }
    if ((n & 0xFFFF0000) == 0) {
      return 3;
    }
    if ((n & 0xFF000000) == 0) {
      return 4;
    }
    return 5;
  }

  static c3_uint_t put_entity(c3_byte_t* p, c3_uint_t n,
    c3_byte_t small_mask, c3_byte_t medium_mask, c3_byte_t large_mask);
};

/**
 * Base class for list chunk builders.
 *
 * Its fields are `protected` and not `private` because they are meant to be handled differently by
 * derived classes. Header lists estimate all their strings first, and only then add them; so buffer is
 * only allocated after estimation is done, for which, in turn, it is necessary to increment string count
 * during estimation. Payload lists do not "know" their strings in advance, so buffer is allocated right
 * away (and later resized as needed), and string count is incremented as strings are added to the list:
 * there is no estimation phase.
 */
class ListChunkBuilder: public ChunkBuilder {
protected:
  c3_byte_t* lcb_buffer;         // buffer where strings are to be stored
  c3_uint_t  lcb_allocated_size; // current size of the buffer
  c3_uint_t  lcb_used_size;      // used portion of the buffer (also, current position in the buffer)
  c3_uint_t  lcb_count;          // number of strings in the list
  domain_t   lcb_domain;         // memory domain ID

  ListChunkBuilder(ReaderWriter& container, const NetworkConfiguration& net_config);
  ~ListChunkBuilder();

  Memory& get_memory_object() const { return Memory::get_memory_object(lcb_domain); }

  // these methods check nothing; it is callers' responsibility to do all relevant checks
  static c3_uint_t measure_string(c3_uint_t size) { return size / 255 + 1 + size; }
  static c3_uint_t measure_cstring(const char* str) { return measure_string((c3_uint_t) std::strlen(str)); }
  void put_string(c3_uint_t size, const char* str);

public:
  ListChunkBuilder(const ListChunkBuilder&) = delete;
  ListChunkBuilder(ListChunkBuilder&&) = delete;

  ListChunkBuilder& operator=(const ListChunkBuilder&) = delete;
  ListChunkBuilder& operator=(ListChunkBuilder&&) = delete;

  c3_uint_t get_count() const { return lcb_count; }
  const c3_byte_t* get_buffer() const { return lcb_buffer; }
  c3_uint_t get_size() const { return lcb_used_size; }
};

/**
 * Builder of lists that are to be stored in a command or response header. Usage pattern:
 *
 * - create an object,
 * - call estimate() zero or more times,
 * - call configure(): this allocates internal buffer,
 * - call add() exactly as many times as estimate() was called,
 * - call check().
 *
 * Callers should check results of `estimate()` functions and stop processing if *any* of them returns zero.
 */
class HeaderListChunkBuilder: public ListChunkBuilder {
  c3_uint_t flcb_estimated_size; // calculated full size of the list

public:
  HeaderListChunkBuilder(ReaderWriter& container, const NetworkConfiguration& net_config);

  c3_uint_t estimate(c3_uint_t size);
  c3_uint_t estimate(const char* str);
  void configure();
  void add(c3_uint_t size, const char* str);
  void add(const char* str);
  void check() const {
    c3_assert(lcb_buffer != nullptr && lcb_allocated_size > 0 && lcb_allocated_size == lcb_used_size);
  }
};

/**
 * Builder of lists that are to be stored in payload chunk. Usage pattern:
 *
 * - create an object,
 * - call is_valid(),
 * - call add() for each string.
 *
 * Callers should check results of all calls and stop processing if any of them returns `false`.
 */
class PayloadListChunkBuilder: public ListChunkBuilder {
  static const size_t MAX_FORMATED_STRING_LENGTH = 1024;

public:
  PayloadListChunkBuilder(ReaderWriter& container, const NetworkConfiguration& net_config,
      c3_uint_t min_guess, c3_uint_t max_guess, c3_uint_t average_length);

  bool is_valid() const { return lcb_allocated_size > 0; }
  bool add(c3_uint_t size, const char* str);
  bool add(const char* str);
  bool addf(const char* format, ...) C3_FUNC_PRINTF(2);
};

/// Allocates payload buffer in specified `ReaderWriter` container
class PayloadAllocator: public Allocator {
  ReaderWriter& pa_container;

public:
  explicit PayloadAllocator(ReaderWriter& container): pa_container(container) {}
  c3_byte_t* alloc(c3_uint_t size) override;
  void free(void* buff, c3_uint_t size) override;
};

/**
 * Builder of payload buffer; payload buffer must be built *before* builting header. Usage pattern:
 *
 * - create an object using appropriate ctor,
 * - call *one* of the `add()` methods
 */
class PayloadChunkBuilder: public ChunkBuilder {
  c3_uint_t       pcb_usize;          // size of uncompressed data
  c3_uint_t       pcb_comp_threshold; // compression threshold
  c3_compressor_t pcb_compressor;     // compressor type

public:
  PayloadChunkBuilder(ReaderWriter& container, const NetworkConfiguration& net_config);

  c3_compressor_t get_compressor() const { return pcb_compressor; }
  c3_uint_t get_usize() const { return pcb_usize; }

  void add(const c3_byte_t* buffer, c3_uint_t usize, comp_data_t hint = CD_GENERIC);
  void add(const PayloadListChunkBuilder &list);
  void add(Payload* payload);
  void add();
};

/// Base class for header chunk builders
class HeaderChunkBuilder: public ChunkBuilder {

  c3_uint_t put(c3_uint_t n, c3_byte_t small_mask, c3_byte_t medium_mask, c3_byte_t large_mask) {
    c3_uint_t size = measure_entity(n);
    c3_byte_t* buffer = cb_container.get_header_bytes(hcb_used_size, size);
    c3_assert(buffer != nullptr);
    put_entity(buffer, n, small_mask, medium_mask, large_mask);
    hcb_used_size += size;
    return size;
  }

  // configuration and config support methods
  virtual c3_byte_t get_descriptor_initializer() const = 0;
  virtual command_t get_command() const = 0;
  virtual bool get_password_hash(c3_hash_t& hash) const = 0;
  virtual bool verify() const;

protected:
  c3_uint_t hcb_estimated_size; // calculated full size of the header
  c3_uint_t hcb_used_size;      // used portion of the buffer (also, current position in the buffer)

  HeaderChunkBuilder(ReaderWriter& container, const NetworkConfiguration& net_config):
    ChunkBuilder(container, net_config) {
    hcb_estimated_size = 0;
    hcb_used_size = 0;
  }

  static c3_uint_t estimate_size_bytes(c3_uint_t size) {
    return size <= 0xFF? 1: (size <= 0xFFFF? 2: 4);
  }

  void put_size_bytes(c3_uint_t size, c3_uint_t reference_size, c3_byte_t &desc,
    c3_byte_t small_mask, c3_byte_t medium_mask, c3_byte_t large_mask) {
    assert(size <= reference_size);
    if (reference_size <= 0xFF) {
      cb_container.set_header_byte_at(hcb_used_size, (c3_byte_t) size);
      hcb_used_size++;
      desc |= small_mask;
    } else if (reference_size <= 0xFFFF) {
      cb_container.set_header_ushort_at(hcb_used_size, (c3_ushort_t) size);
      hcb_used_size += 2;
      desc |= medium_mask;
    } else {
      cb_container.set_header_uint_at(hcb_used_size, size);
      hcb_used_size += 4;
      desc |= large_mask;
    }
  }

  void put_size_bytes(c3_uint_t size, c3_uint_t reference_size) {
    assert(size <= reference_size);
    if (reference_size <= 0xFF) {
      cb_container.set_header_byte_at(hcb_used_size, (c3_byte_t) size);
      hcb_used_size++;
    } else if (reference_size <= 0xFFFF) {
      cb_container.set_header_ushort_at(hcb_used_size, (c3_ushort_t) size);
      hcb_used_size += 2;
    } else {
      cb_container.set_header_uint_at(hcb_used_size, size);
      hcb_used_size += 4;
    }
  }

public:
  c3_uint_t estimate_number(c3_long_t num);
  c3_uint_t estimate_string(c3_uint_t size);
  c3_uint_t estimate_cstring(const char* str);
  c3_uint_t estimate_list(const HeaderListChunkBuilder& list);

  void configure(const PayloadChunkBuilder* payload = nullptr);

  void add_number(c3_long_t num);
  void add_string(const char* str, c3_uint_t size);
  void add_cstring(const char* str);
  void add_list(const HeaderListChunkBuilder &list);

  void check() const {
    c3_assert(hcb_used_size == hcb_estimated_size);
    // it is *REQUIRED* that call to verify() is the very last check done in this method
    verify();
  }
};

///////////////////////////////////////////////////////////////////////////////
// COMMAND-SPECIFIC CHUNK BUILDERS
///////////////////////////////////////////////////////////////////////////////

/**
 * Header chunk builder for all types of commands. Usage pattern:
 *
 * - create an object,
 * - optionally call estimate_xxx() methods,
 * - call configure() method,
 * - call add_xxx() methods exactly as many times as their estimate_xxx() counterparts were called,
 * - call check().
 *
 * Callers should check all results and stop processing if any call returns `false` or zero.
 */
class CommandHeaderChunkBuilder: public HeaderChunkBuilder {

  c3_byte_t get_descriptor_initializer() const override;
  command_t get_command() const override;
  bool get_password_hash(c3_hash_t& hash) const override;
  bool verify() const override;

protected:
  c3_hash_t       chcb_hash;  // password hash code
  const command_t chcb_cmd;   // command ID
  bool            chcb_auth;  // command has to use authentication
  const bool      chcb_admin; // admin-level authentication

public:
  CommandHeaderChunkBuilder(ReaderWriter& container, const NetworkConfiguration& net_config,
    command_t cmd, bool admin);
};

///////////////////////////////////////////////////////////////////////////////
// RESPONSE-SPECIFIC CHUNK BUILDERS
///////////////////////////////////////////////////////////////////////////////

// forward declaration
class ResponseWriter;

/// Base class for all response header chunk builders
class ResponseHeaderChunkBuilder: public HeaderChunkBuilder {
  const c3_byte_t rhcb_type; // response type, a RESP_TYPE_xxx constant

  c3_byte_t get_descriptor_initializer() const override;
  command_t get_command() const override;
  bool get_password_hash(c3_hash_t& hash) const override;

protected:
  ResponseHeaderChunkBuilder(ResponseWriter& container, const NetworkConfiguration& net_config,
    c3_byte_t type);
};

/**
 * Header chunk builder for `OK` responses. Usage pattern:
 *
 * - create an object,
 * - call configure() method with `NULL` payload pointer,
 * - call check().
 *
 * Callers should check configure() and check() results and stop processing if any call returns `false`.
 */
class OkResponseHeaderChunkBuilder: public ResponseHeaderChunkBuilder {
  bool verify() const override;

public:
  OkResponseHeaderChunkBuilder(ResponseWriter& container, const NetworkConfiguration& net_config);
};

/**
 * Header chunk builder for `ERROR` responses. Usage pattern:
 *
 * - create an object,
 * - call estimate_xxx() methods,
 * - call configure() method with `NULL` payload pointer,
 * - call add_xxx() methods exactly as many times as their estimate_xxx() counterparts were called,
 * - call check().
 *
 * Callers should check all results and stop processing if any call returns `false` or zero.
 */
class ErrorResponseHeaderChunkBuilder: public ResponseHeaderChunkBuilder {
  bool verify() const override;

public:
  ErrorResponseHeaderChunkBuilder(ResponseWriter& container, const NetworkConfiguration& net_config);
};

/**
 * Header chunk builder for `DATA` responses. Usage pattern:
 *
 * - create an object,
 * - call estimate_xxx() methods,
 * - call configure() method with a valid payload pointer OR `NULL`,
 * - call add_xxx() methods exactly as many times as their estimate_xxx() counterparts were called,
 * - call check().
 *
 * Callers should check all results and stop processing if any call returns `false` or zero.
 */
class DataResponseHeaderChunkBuilder: public ResponseHeaderChunkBuilder {
  bool verify() const override;

public:
  DataResponseHeaderChunkBuilder(ResponseWriter& container, const NetworkConfiguration& net_config);
};

/**
 * Header chunk builder for `LIST` responses. Usage pattern:
 *
 * - create an object,
 * - call estimate_xxx() methods,
 * - call configure() method with a valid payload pointer (must be list payload chunk builder),
 * - call add_xxx() methods exactly as many times as their estimate_xxx() counterparts were called,
 * - call check().
 *
 * Callers should check all results and stop processing if any call returns `false` or zero.
 */
class ListResponseHeaderChunkBuilder: public ResponseHeaderChunkBuilder {
  bool verify() const override;

public:
  ListResponseHeaderChunkBuilder(ResponseWriter& container, const NetworkConfiguration& net_config);
};

} // CyberCache

#endif // _IO_CHUNK_BUILDERS_H
