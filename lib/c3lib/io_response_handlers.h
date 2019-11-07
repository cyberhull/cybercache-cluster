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
 * Classes implementing high-level response handling.
 */
#ifndef _IO_RESPONSE_HANDLERS_H
#define _IO_RESPONSE_HANDLERS_H

#include "io_reader_writer.h"
#include "io_protocol.h"
#include "io_chunk_iterators.h"

// whether to compile `ResponseReader::clone()` method
#define INCLUDE_RESPONSEREADER_CLONE 0
// whether to compile `ResponseWriter::clone()` method
#define INCLUDE_RESPONSEWRITER_CLONE 0

namespace CyberCache {

enum response_type_t {
  RESPONSE_INVALID = 0, // object is in invalid state
  RESPONSE_OK,          // "success", no extra data
  RESPONSE_DATA,        // "success", structured data received
  RESPONSE_LIST,        // "success", list of strings received
  RESPONSE_ERROR        // "failure", error code and message received
};

/**
 * Base class for all response-related classes; provides various accessors, but does not implement either
 * logical or physical read() methods.
 *
 * It is callers' responsibility to make sure respective data are already available.
 */
class ResponseAccessor: public ReaderWriter {
protected:
  ResponseAccessor(Memory& memory, c3_byte_t flags, int fd, c3_ipv4_t ipv4, SharedBuffers* sb):
    ReaderWriter(memory, flags, fd, ipv4, sb) {
  }
  ResponseAccessor(Memory& memory, const ReaderWriter& rw, c3_byte_t flags, int fd, c3_ipv4_t ipv4):
    ReaderWriter(memory, rw, flags, fd, ipv4) {
  }
  ResponseAccessor(const ReaderWriter& rw, bool full): ReaderWriter(rw, full) {}

  c3_byte_t get_response_descriptor() const { return get_header_byte_at(0); }

  c3_byte_t get_response_type() const { return get_response_descriptor() & RESP_TYPE_BITS; }

  response_type_t get_translated_response_type() const {
    switch (get_response_type()) {
      case RESP_TYPE_OK:
        return RESPONSE_OK;
      case RESP_TYPE_DATA:
        return RESPONSE_DATA;
      case RESP_TYPE_LIST:
        return RESPONSE_LIST;
      default:
        return RESPONSE_ERROR;
    }
  }

  c3_uint_t get_response_header_data_size_length() const {
    switch (get_response_descriptor() & RESP_HEADER_BITS) {
      case RESP_NO_HEADER:
        return 0;
      case RESP_BYTE_HEADER:
        return 1;
      case RESP_WORD_HEADER:
        return 2;
      default:
        return 4;
    }
  }

  c3_uint_t get_response_header_size() const {
    switch (get_response_descriptor() & RESP_HEADER_BITS) {
      case RESP_NO_HEADER:
        return 1; // just descriptor
      case RESP_BYTE_HEADER:
        return get_header_byte_at(1) + 2u; // descriptor, size byte, size itself
      case RESP_WORD_HEADER:
        return get_header_ushort_at(1) + 3u; // descriptor, size word, size itself
      default:
        return get_header_uint_at(1) + 5u; // descriptor, size dword, size itself
    }
  }

  bool response_marker_is_present() const {
    return (get_response_descriptor() & RESP_MARKER_IS_PRESENT) != 0;
  }

public:
  #if C3_DEBUG_ON
  c3_byte_t get_raw_response_type() const { return get_response_type(); }
  #endif // C3_DEBUG_ON

  bool get_header_info(header_info_t& hi) const override;
  bool get_payload_info(payload_info_t& pi) const override;
};

/**
 * Base class for all response readers.
 *
 * Implements logical read() and supports data retrieval; physical read() method must be implemented
 * by a derived class.
 */
class ResponseReader: public ResponseAccessor {
protected:
  ResponseReader(Memory& memory, c3_byte_t flags, int fd, c3_ipv4_t ipv4, SharedBuffers* sb):
    // response readers have nothing to do with existing lockable hash objects
    ResponseAccessor(memory, flags, fd, ipv4, sb) {
  }
  ResponseReader(Memory& memory, const ReaderWriter& rw, c3_byte_t flags, int fd, c3_ipv4_t ipv4):
    ResponseAccessor(memory, rw, flags, fd, ipv4) {
  }
  ResponseReader(const ReaderWriter& rw, bool full): ResponseAccessor(rw, full) {}

public:
  #if INCLUDE_RESPONSEREADER_CLONE
  ResponseReader* clone(bool full);
  #endif // INCLUDE_RESPONSEREADER_CLONE

  io_result_t read(c3_ulong_t& ntotal) override;
  bool io_completed() const override;
  response_type_t get_type() const;
};

/// Iterator used to retrieve data from response headers
class ResponseHeaderIterator: public HeaderChunkIterator {
public:
  explicit ResponseHeaderIterator(const ResponseReader& rr): HeaderChunkIterator(rr) {
    response_type_t type = rr.get_type(); // this checks that response is fully read
    if (type == RESPONSE_INVALID || type == RESPONSE_OK) {
      invalidate();
    }
  }
};

/// Iterator used to retrieve strings from `LIST` type responses
class ResponsePayloadIterator: public PayloadChunkIterator {
public:
  explicit ResponsePayloadIterator(const ResponseReader& rr): PayloadChunkIterator(rr) {
    if (rr.get_type() != RESPONSE_LIST) {
      invalidate();
    }
  }

  // number of list elements is always stored in the header
  ListChunk get_list(c3_uint_t num_elements) {
    return ListChunk(*this, num_elements);
  }
};

/**
 * Base class for all response writers.
 *
 * Implements logical write() and supports data retrieval; physical write() method must be implemented
 * by a derived class.
 */
class ResponseWriter: public ResponseAccessor {
protected:
  ResponseWriter(Memory& memory, c3_byte_t flags, int fd, c3_ipv4_t ipv4, SharedBuffers* sb):
    ResponseAccessor(memory, flags, fd, ipv4, sb) {
  }
  ResponseWriter(Memory& memory, const ReaderWriter& rw, c3_byte_t flags, int fd, c3_ipv4_t ipv4):
    ResponseAccessor(memory, rw, flags, fd, ipv4) {
  }
  ResponseWriter(const ReaderWriter& rw, bool full): ResponseAccessor(rw, full) {}

public:
  #if INCLUDE_RESPONSEWRITER_CLONE
  ResponseWriter* clone(bool full);
  #endif // INCLUDE_RESPONSEWRITER_CLONE

  io_result_t write(c3_ulong_t& ntotal) override;
  bool io_completed() const override;
  response_type_t get_type() const;
};

} // CyberCache

#endif // _IO_RESPONSE_HANDLERS_H
