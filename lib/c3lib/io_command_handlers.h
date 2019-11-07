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
 * Classes implementing high-level command handling.
 */
#ifndef _IO_COMMAND_HANDLERS_H
#define _IO_COMMAND_HANDLERS_H

#include "io_reader_writer.h"
#include "io_protocol.h"
#include "io_chunk_iterators.h"

// whether to compile `CommandPayloadIterator` class
#define INCLUDE_COMMANDPAYLOADITERATOR 0
// whether to compile `CommandWriter::clone()`
#define INCLUDE_COMMANDWRITER_CLONE 0
// whether to compile `CommandWriter::get_command_id()`
#define INCLUDE_COMMANDWRITER_GET_COMMAND_ID 0

namespace CyberCache {

/// Command password types
enum command_password_type_t: c3_byte_t {
  CPT_NO_PASSWORD    = DESC_NO_AUTH,    // command does not have a password in its header
  CPT_USER_PASSWORD  = DESC_USER_AUTH,  // command contains "user" password
  CPT_ADMIN_PASSWORD = DESC_ADMIN_AUTH, // command contains "admin" password
  CPT_BULK_PASSWORD  = DESC_BULK_AUTH   // command contains "bulk" password
};

static_assert(CPT_NO_PASSWORD == 0, "Password type constants must start at 0");

/**
 * Base class for all command-related classes; provides various accessors, but does not implement either
 * logical or physical read() methods.
 *
 * It is callers' responsibility to make sure respective data are already available.
 */
class CommandAccessor : public ReaderWriter {
protected:
  CommandAccessor(Memory& memory, c3_byte_t flags, int fd, c3_ipv4_t ipv4, SharedBuffers* sb):
    ReaderWriter(memory, flags, fd, ipv4, sb) {
  }
  CommandAccessor(Memory& memory, const ReaderWriter& rw, c3_byte_t flags, int fd, c3_ipv4_t ipv4):
    ReaderWriter(memory, rw, flags, fd, ipv4) {
  }
  CommandAccessor(const ReaderWriter& rw, bool full): ReaderWriter(rw, full) {}

  c3_byte_t get_command_descriptor() const { return get_header_byte_at(0); }

  c3_byte_t get_command_id_byte() const {
    return get_header_byte_at(get_command_header_data_size_length() + 1);
  }

  c3_byte_t get_auth_type() const { return get_command_descriptor() & DESC_AUTH_BITS; }

  c3_uint_t get_command_header_data_size_length() const {
    switch (get_command_descriptor() & DESC_HEADER_BITS) {
      case DESC_NO_HEADER:
        return 0;
      case DESC_BYTE_HEADER:
        return 1;
      case DESC_WORD_HEADER:
        return 2;
      default:
        return 4;
    }
  }

  c3_uint_t get_command_sizeless_header_size() const {
    c3_assert(get_command_header_data_size_length() == 0);
    // descriptor + command ID + optional password hash
    return get_auth_type() != DESC_NO_AUTH? sizeof(c3_hash_t) + 2: 2;
  }

  c3_uint_t get_command_header_size() const {
    switch (get_command_descriptor() & DESC_HEADER_BITS) {
      case DESC_NO_HEADER:
        return get_command_sizeless_header_size();
      case DESC_BYTE_HEADER:
        return get_header_byte_at(1) + 2u; // descriptor, size byte, size itself
      case DESC_WORD_HEADER:
        return get_header_ushort_at(1) + 3u; // descriptor, size word, size itself
      default:
        return get_header_uint_at(1) + 5u; // descriptor, size dword, size itself
    }
  }

  bool command_marker_is_present() const {
    return (get_command_descriptor() & DESC_MARKER_IS_PRESENT) != 0;
  }

public:
  command_password_type_t get_command_pwd_hash(c3_hash_t& hash) const;
  bool set_command_pwd_hash(command_password_type_t type, c3_hash_t hash);

  bool get_header_info(header_info_t &hi) const override;
  bool get_payload_info(payload_info_t &pi) const override;
};

/**
 * Base class for all command readers.
 *
 * Implements logical read() and supports data retrieval; physical read() method must be implemented
 * by a derived class.
 */
class CommandReader: public CommandAccessor {
protected:
  CommandReader(Memory& memory, c3_byte_t flags, int fd, c3_ipv4_t ipv4, SharedBuffers* sb):
    CommandAccessor(memory, flags, fd, ipv4, sb) {
  }
  CommandReader(Memory& memory, const ReaderWriter& rw, c3_byte_t flags, int fd, c3_ipv4_t ipv4):
    CommandAccessor(memory, rw, flags, fd, ipv4) {
  }
  CommandReader(const ReaderWriter& rw, bool full): CommandAccessor(rw, full) {}

public:
  CommandReader* clone(bool full) const;

  io_result_t read(c3_ulong_t& ntotal) override;
  bool io_completed() const override;
  command_t get_command_id() const;
};

/// Iterator used to retrieve data from response headers
class CommandHeaderIterator: public HeaderChunkIterator {
public:
  explicit CommandHeaderIterator(const CommandReader& cr): HeaderChunkIterator(cr) {
    if (!cr.io_completed()) {
      invalidate();
    }
  }
};

#if INCLUDE_COMMANDPAYLOADITERATOR
/// Iterator used to retrieve strings from `LIST` type responses
class CommandPayloadIterator: public PayloadChunkIterator {
public:
  explicit CommandPayloadIterator(const CommandReader& cr): PayloadChunkIterator(cr) {
    if (!cr.io_completed()) {
      invalidate();
    }
  }
};
#endif // INCLUDE_COMMANDPAYLOADITERATOR

/**
 * Base class for all command writers.
 *
 * Implements logical write() and supports data retrieval; physical write() method must be implemented
 * by a derived class.
 */
class CommandWriter: public CommandAccessor {
protected:
  CommandWriter(Memory& memory, c3_byte_t flags, int fd, c3_ipv4_t ipv4, SharedBuffers* sb):
    CommandAccessor(memory, flags, fd, ipv4, sb) {
  }
  CommandWriter(Memory& memory, const ReaderWriter& rw, c3_byte_t flags, int fd, c3_ipv4_t ipv4):
    CommandAccessor(memory, rw, flags, fd, ipv4) {
  }
  CommandWriter(const ReaderWriter& rw, bool full): CommandAccessor(rw, full) {}

public:
  #if INCLUDE_COMMANDWRITER_CLONE
  CommandWriter* clone(bool full);
  #endif // INCLUDE_COMMANDWRITER_CLONE

  io_result_t write(c3_ulong_t& ntotal) override;
  bool io_completed() const override;
  void io_rewind(int fd, c3_ipv4_t ipv4) override;

  #if INCLUDE_COMMANDWRITER_GET_COMMAND_ID
  command_t get_command_id() const;
  #endif // INCLUDE_COMMANDWRITER_GET_COMMAND_ID
};

} // CyberCache

#endif // _IO_COMMAND_HANDLERS_H
