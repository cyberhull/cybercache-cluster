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
 * Base classes and finite-state automaton definitions for all command and response processors.
 */
#ifndef _IO_READER_WRITER_H
#define _IO_READER_WRITER_H

#include "c3_compressor.h"
#include "io_protocol.h"
#include "io_shared_buffers.h"

namespace CyberCache {

/// Return codes for various I/O methods of public interfaces
enum io_result_t {
  IO_RESULT_OK = 0, // success
  IO_RESULT_ERROR,  // an error occurred, use c3_get_error_message()
  IO_RESULT_RETRY,  // async I/O, data/device not ready yet, try again later
  IO_RESULT_EOF     // a connection was dropped by a peer, or end of file reached
};

/// Internal state of a ReaderWriter[-based] object (finite-state automaton states)
enum io_state_t: c3_byte_t {

  /// generic state: placeholder to catch initialization errors
  IO_STATE_INVALID,
  /// generic state: the object had just been created
  IO_STATE_CREATED,
  /// generic state: an unrecoverable I/O error had occured, further processing is not possible
  IO_STATE_ERROR,

  /// reading state: about to read the very first response byte
  IO_STATE_RESPONSE_READ_DESCRIPTOR,
  /// reading state: get_response_header_data_size_length() returned non-zero, reading size bytes
  IO_STATE_RESPONSE_READ_HEADER_SIZE_BYTES,
  /// reading state: reading remaining get_response_header_data_size() bytes of the header
  IO_STATE_RESPONSE_READ_HEADER_BYTES,
  /// reading state: allocated and are reading payload
  IO_STATE_RESPONSE_READ_PAYLOAD_BYTES,
  /// reading state: reading integrity check marker
  IO_STATE_RESPONSE_READ_MARKER_BYTE,
  /// reading state: finished reading response
  IO_STATE_RESPONSE_READ_DONE,

  // response is fully formed and ready for transmission
  IO_STATE_RESPONSE_WRITE_READY,
  // writing state: writing response header
  IO_STATE_RESPONSE_WRITE_HEADER,
  // writing state: writing response payload
  IO_STATE_RESPONSE_WRITE_PAYLOAD,
  // writing state: writing response marker byte
  IO_STATE_RESPONSE_WRITE_MARKER_BYTE,
  // writing state: finished writing response
  IO_STATE_RESPONSE_WRITE_DONE,

  /// reading state: about to read the very first command byte
  IO_STATE_COMMAND_READ_DESCRIPTOR,
  /// reading state: get_command_header_data_size_length() returned non-zero, reading size bytes
  IO_STATE_COMMAND_READ_HEADER_SIZE_BYTES,
  /// reading state: reading remaining bytes of the ("true" or sizeless) header
  IO_STATE_COMMAND_READ_HEADER_BYTES,
  /// reading state: allocated and are reading payload
  IO_STATE_COMMAND_READ_PAYLOAD_BYTES,
  /// reading state: reading integrity check marker
  IO_STATE_COMMAND_READ_MARKER_BYTE,
  /// reading state: finished reading response
  IO_STATE_COMMAND_READ_DONE,

  // command is fully formed and ready for transmission
  IO_STATE_COMMAND_WRITE_READY,
  // writing state: writing command header
  IO_STATE_COMMAND_WRITE_HEADER,
  // writing state: writing command payload
  IO_STATE_COMMAND_WRITE_PAYLOAD,
  // writing state: writing command marker byte
  IO_STATE_COMMAND_WRITE_MARKER_BYTE,
  // writing state: finished writing a command
  IO_STATE_COMMAND_WRITE_DONE
};

/// ReaderWriter object state flags (set in ctors and never changed during object life time)
constexpr c3_byte_t IO_FLAG_IS_READER   = 0x01; // object is a "writer" if this flag is not set
constexpr c3_byte_t IO_FLAG_IS_RESPONSE = 0x02; // object is a "command" if this flag is not set
constexpr c3_byte_t IO_FLAG_NETWORK     = 0x04; // handle is a socket, not file descriptor

/// Container for header data chunks information
struct header_info_t {
  c3_uint_t hi_chunks_offset; // offset of data chunks in the header
  c3_uint_t hi_chunks_size;   // total length of all data chunks in the header

  header_info_t() { invalidate(); }

  bool invalidate() {
    hi_chunks_offset = UINT_MAX_VAL;
    hi_chunks_size = 0;
    return false;
  }
};

/// Container for payload information
struct payload_info_t {
  c3_byte_t*      pi_buffer;     // buffer with payload data
  c3_uint_t       pi_size;       // size of [compressed] data in the buffer
  c3_uint_t       pi_usize;      // size of uncompressed data, >= `pi_size`
  c3_compressor_t pi_compressor; // type of compression; zero if data is uncompressed
  bool            pi_has_errors; // `true` if there were errors while decoding payload information

  payload_info_t() { invalidate(false); }

  /**
   * Invalidates payload information; that is, sets a state that corresponds to "no payload".
   *
   * @param has_errors Reason for invalidation; callers should pass `false` if there is simply no payload, and `true`
   *   if command or response header did specify payload, but its decoding has failed.
   * @return Its argument (`has_errors`).
   */
  bool invalidate(bool has_errors) {
    pi_buffer = nullptr;
    pi_size = 0;
    pi_usize = 0;
    pi_compressor = CT_NONE;
    pi_has_errors = has_errors;
    return has_errors;
  }
};

/// Interface for all device-specific implementations
class DeviceReaderWriter {
public:
  // the following method must be implemented in concrete classes *only*
  virtual c3_uint_t get_object_size() const = 0;
  virtual Memory& get_memory_object() const = 0;

  // these methods should *only* return `OK` result if they successfully read at least one byte
  virtual io_result_t read_bytes(int fd, c3_byte_t* buff, c3_uint_t nbytes, c3_uint_t &nread) const;
  virtual io_result_t write_bytes(int fd, const c3_byte_t* buff, c3_uint_t nbytes, c3_uint_t &nwritten) const;
};

/// Base class for all command/response readers, writers, and processors
class ReaderWriter: virtual public DeviceReaderWriter {
  SharedBuffers* rw_sb;   // pointer to SharedBuffers object
  int            rw_fd;   // socket or file handle (descriptor); always 0 ("inactive"), or positive
  c3_ipv4_t      rw_ipv4; // IP address, or INVALID_IPV4_ADDRESS

  void invalidate() {
    rw_pos = UINT_MAX_VAL;
    rw_remains = 0;
    rw_state = IO_STATE_INVALID;
  }

protected:
  c3_uint_t       rw_pos;     // position within part of the object currently being read/written
  c3_uint_t       rw_remains; // number of bytes to read/write before switching to next state
  const domain_t  rw_domain;  // memory domain within which this object was created
  io_state_t      rw_state;   // current state of the finite state automaton
  const c3_byte_t rw_flags;   // a combination of the IO_FLAG_xxx constants

  ReaderWriter(Memory& memory, c3_byte_t flags, int fd, c3_ipv4_t ipv4, SharedBuffers* sb);
  ReaderWriter(Memory& memory, const ReaderWriter& rw, c3_byte_t flags, int fd, c3_ipv4_t ipv4);
  ReaderWriter(const ReaderWriter& rw, bool full);

  io_result_t set_error_state();

  void configure_descriptor(int fd, c3_ipv4_t ipv4 = INVALID_IPV4_ADDRESS) {
    // if we're in "error" state, that's OK, that's recoverable
    c3_assert(is_valid() && fd > 0);
    rw_fd = fd;
    rw_ipv4 = ipv4;
  }
  void configure_header(c3_uint_t used_size, c3_uint_t full_size) const {
    rw_sb->configure_header(used_size, full_size);
  }

public:
  ReaderWriter(const ReaderWriter&) = delete;
  ReaderWriter(ReaderWriter&&) = delete;
  virtual ~ReaderWriter();

  ReaderWriter& operator=(const ReaderWriter&) = delete;
  ReaderWriter& operator=(ReaderWriter&&) = delete;

  int get_fd() const { return rw_fd; }
  c3_ipv4_t get_ipv4() const { return rw_ipv4; }
  domain_t get_domain() const { return rw_domain; }
  Memory& get_sb_memory_object() const { return rw_sb->get_memory_object(); }

  bool header_is_not_initialized() const { return rw_sb->using_static_header(); }
  void initialize_header(c3_uint_t size) const {
    c3_assert(header_is_not_initialized());
    configure_header(0, size);
  }

  bool is_valid() const { return rw_state != IO_STATE_INVALID && rw_fd >= 0; }
  bool is_active() const { return rw_state != IO_STATE_INVALID && rw_fd > 0; }
  bool is_set(c3_byte_t flags) const { return (rw_flags & flags) != 0; }
  bool is_clear(c3_byte_t flags) const { return (rw_flags & flags) == 0; }
  static void dispose(ReaderWriter* rw);

  // only command readers are allowed to call this method (`domain` == target domain)
  void command_reader_transfer_payload(Payload* payload, domain_t domain, c3_uint_t usize,
    c3_compressor_t compressor) const {
    c3_assert(is_set(IO_FLAG_IS_READER) && is_clear(IO_FLAG_IS_RESPONSE));
    rw_sb->transfer_payload(payload, domain, usize, compressor);
  }
  void command_writer_set_ready_state() {
    c3_assert(is_clear(IO_FLAG_IS_READER) && is_clear(IO_FLAG_IS_RESPONSE) && rw_state == IO_STATE_CREATED);
    rw_state = IO_STATE_COMMAND_WRITE_READY;
  }
  // only response writers are allowed to call these methods
  void response_writer_attach_payload(Payload* payload) const {
    c3_assert(is_clear(IO_FLAG_IS_READER) && is_set(IO_FLAG_IS_RESPONSE));
    rw_sb->attach_payload(payload);
  }
  void response_writer_set_ready_state() {
    c3_assert(is_clear(IO_FLAG_IS_READER) && is_set(IO_FLAG_IS_RESPONSE) && rw_state == IO_STATE_CREATED);
    rw_state = IO_STATE_RESPONSE_WRITE_READY;
  }

  /////////////////////////////////////////////////////////////////////////////
  // ACCESSORS (WRAPPERS): HEADER
  /////////////////////////////////////////////////////////////////////////////

  c3_uint_t get_available_header_size() const { return rw_sb->get_available_header_size(); }

  c3_byte_t* get_header_bytes(c3_uint_t offset, c3_uint_t size) const {
    return rw_sb->get_header_bytes(offset, size);
  }

  const c3_byte_t* get_const_header_bytes(c3_uint_t offset, c3_uint_t size) const {
    return (const c3_byte_t*) rw_sb->get_header_bytes(offset, size);
  }

  /////////////////////////////////////////////////////////////////////////////
  // ACCESSORS: SETTING AND RETRIEVAL OF INTEGERS
  /////////////////////////////////////////////////////////////////////////////

  c3_byte_t get_header_byte_at(c3_uint_t i) const { return *rw_sb->get_header_bytes(i, 1); }

  c3_ushort_t get_header_ushort_at(c3_uint_t i) const {
    const c3_byte_t* p = get_const_header_bytes(i, 2);
    #ifdef C3_UNALIGNED_ACCESS
    return *((c3_ushort_t*) p);
    #else
    return (p[1] << 8) + p[0];
    #endif
  }

  c3_uint_t get_header_uint3_at(c3_uint_t i) const {
    const c3_byte_t* p = get_const_header_bytes(i, 3);
    return (p[2] << 16) + (p[1] << 8) + p[0];
  }

  c3_uint_t get_header_uint_at(c3_uint_t i) const {
    const c3_byte_t* p = get_const_header_bytes(i, 4);
    #ifdef C3_UNALIGNED_ACCESS
    return *((c3_uint_t*) p);
    #else
    return (p[3] << 24) + (p[2] << 16) + (p[1] << 8) + p[0];
    #endif
  }

  c3_ulong_t get_header_ulong_at(c3_uint_t i) const {
    const c3_byte_t* p = get_const_header_bytes(i, 8);
    #ifdef C3_UNALIGNED_ACCESS
    return *((c3_ulong_t*) p);
    #else
    return (p[7] << 56) + (p[6] << 48) + (p[5] << 40) + (p[4] << 32) +
      (p[3] << 24) + (p[2] << 16) + (p[1] << 8) + p[0];
    #endif
  }

  void set_header_byte_at(c3_uint_t i, c3_byte_t n) const { *get_header_bytes(i, 1) = n; }

  void set_header_ushort_at(c3_uint_t i, c3_ushort_t n) const {
    c3_byte_t* p = get_header_bytes(i, 2);
    #ifdef C3_UNALIGNED_ACCESS
    *((c3_ushort_t*) p) = n;
    #else
    p[0] = (c3_byte_t) n;
    p[1] = (c3_byte_t)(n >> 8);
    #endif
  }

  void set_header_uint3_at(c3_uint_t i, c3_uint_t n) const {
    c3_byte_t* p = get_header_bytes(i, 2);
    p[0] = (c3_byte_t) n;
    p[1] = (c3_byte_t)(n >> 8);
    p[2] = (c3_byte_t)(n >> 16);
  }

  void set_header_uint_at(c3_uint_t i, c3_uint_t n) const {
    c3_byte_t* p = get_header_bytes(i, 2);
    #ifdef C3_UNALIGNED_ACCESS
    *((c3_uint_t*) p) = n;
    #else
    p[0] = (c3_byte_t) n;
    p[1] = (c3_byte_t)(n >> 8);
    p[2] = (c3_byte_t)(n >> 16);
    p[3] = (c3_byte_t)(n >> 24);
    #endif
  }

  void set_header_ulong_at(c3_uint_t i, c3_ulong_t n) const {
    c3_byte_t* p = get_header_bytes(i, 2);
    #ifdef C3_UNALIGNED_ACCESS
    *((c3_ulong_t*) p) = n;
    #else
    p[0] = (c3_byte_t) n;
    p[1] = (c3_byte_t)(n >> 8);
    p[2] = (c3_byte_t)(n >> 16);
    p[3] = (c3_byte_t)(n >> 24);
    p[4] = (c3_byte_t)(n >> 32);
    p[5] = (c3_byte_t)(n >> 40);
    p[6] = (c3_byte_t)(n >> 48);
    p[7] = (c3_byte_t)(n >> 56);
    #endif
  }

  /////////////////////////////////////////////////////////////////////////////
  // ACCESSORS (WRAPPERS): PAYLOAD BUFFER
  /////////////////////////////////////////////////////////////////////////////

  bool has_payload_data() const { return (get_header_byte_at(0) & DESC_PAYLOAD_BITS) != 0; }

  c3_uint_t get_payload_size() const { return rw_sb->get_payload_size(); }

  c3_uint_t get_payload_usize() const { return rw_sb->get_payload_usize(); }

  c3_compressor_t get_payload_compressor() const { return rw_sb->get_payload_compressor(); }

  c3_byte_t* get_payload_bytes(c3_uint_t offset, c3_uint_t size) const {
    return rw_sb->get_payload_bytes(offset, size);
  }

  c3_byte_t* set_payload_size(c3_uint_t size) const { return rw_sb->set_payload_size(size); }

  /////////////////////////////////////////////////////////////////////////////
  // I/O METHODS
  /////////////////////////////////////////////////////////////////////////////

  virtual bool get_header_info(header_info_t& hi) const = 0;
  virtual bool get_payload_info(payload_info_t& pi) const = 0;
  Memory& get_memory_object() const override;
  virtual io_result_t read(c3_ulong_t& ntotal);
  virtual io_result_t write(c3_ulong_t& ntotal);
  virtual bool io_completed() const = 0;
  virtual void io_rewind(int fd, c3_ipv4_t ipv4);
};

} // CyberCache

#endif // _IO_READER_WRITER_H
