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
 * Data types and iterators that are to be used to access commands' and responses' data.
 */
#ifndef _IO_CHUNK_ITERATORS_H
#define _IO_CHUNK_ITERATORS_H

#include "c3_build.h"
#include "c3_memory.h"
#include "io_reader_writer.h"

namespace CyberCache {

enum chunk_t {
  CHUNK_NONE = 0, // invalid / end-of-stream
  CHUNK_NUMBER,   // a signed integer 64-bit number
  CHUNK_STRING,   // a binary string
  CHUNK_LIST      // a list
};

/// Base class for all iterators
class ChunkIterator {
  friend class StringChunk;
  friend class ListChunk;

  // accessors for strings and list iterators
  virtual c3_byte_t get_byte(c3_uint_t offset) const = 0;
  virtual const c3_byte_t* get_bytes(c3_uint_t offset, c3_uint_t size) const = 0;

protected:
  const ReaderWriter& ci_rw;      // object being read
  c3_uint_t           ci_offset;  // current offset into the header data buffer
  c3_uint_t           ci_remains; // how many chunk bytes remains in the header

  explicit ChunkIterator(const ReaderWriter& rw): ci_rw(rw) {
    ci_offset = 0;
    ci_remains = 0;
  }

  void skip(c3_uint_t n) {
    c3_assert(n <= ci_remains);
    ci_offset += n;
    ci_remains -= n;
  }

  // this method is to be used *before* reading starts
  bool is_valid() const { return ci_offset != UINT_MAX_VAL; }
  void invalidate() {
    ci_offset = UINT_MAX_VAL;
    ci_remains = 0;
  }

public:
  // application-level API for class users
  bool has_more_chunks() const { return ci_remains != 0; }
  static bool has_any_data(const ReaderWriter& rw);
};

/// Representation of a number passed in the header
class NumberChunk {
  const c3_long_t nc_value;

public:
  NumberChunk(): nc_value(LONG_MAX_VAL) {} // "invalid number" state
  explicit NumberChunk(c3_long_t value): nc_value(value) {}

  bool is_valid() const { return nc_value != LONG_MAX_VAL; }
  bool is_valid_int() const { return nc_value >= INT_MIN_VAL && nc_value <= INT_MAX_VAL; }
  bool is_valid_uint() const { return nc_value >= 0 && nc_value <= UINT_MAX_VAL; }
  bool is_in_range(c3_long_t from, c3_long_t to) const { return nc_value >= from && nc_value <= to; }
  bool is_negative() const { return nc_value < 0; }
  c3_int_t get_int() const {
    c3_assert(nc_value >= INT_MIN_VAL && nc_value <= INT_MAX_VAL);
    return (c3_int_t) nc_value;
  }
  c3_uint_t get_uint() const {
    c3_assert(nc_value >= 0 && nc_value <= UINT_MAX_VAL);
    return (c3_uint_t) nc_value;
  }
  c3_long_t get_value() const { return nc_value; }
  explicit operator c3_int_t() const { return get_int(); }
  explicit operator c3_uint_t() const { return get_uint(); }
};

/// Representation of a string passed in the header, header list, or payload list
class StringChunk {
  const ChunkIterator& sc_iterator; // iterator from which the string was retrieved
  const c3_uint_t      sc_offset;   // offset into memory buffer of the iterator
  const c3_uint_t      sc_length;   // length of the string (there is no terminating `0` byte)

public:
  explicit StringChunk(const ChunkIterator& iterator): // "invalid iterator" state
    sc_iterator(iterator), sc_offset(UINT_MAX_VAL), sc_length(0) {
  }
  StringChunk(const ChunkIterator& iterator, c3_uint_t offset, c3_uint_t length):
    sc_iterator(iterator), sc_offset(offset), sc_length(length) {
  }

  bool is_valid() const { return sc_iterator.is_valid() && sc_offset != UINT_MAX_VAL; }
  bool is_valid_name() const { return is_valid() && sc_length > 0 && sc_length <= USHORT_MAX_VAL; }
  c3_uint_t get_length() const { return sc_length; }
  c3_ushort_t get_short_length() const {
    c3_assert(sc_length <= USHORT_MAX_VAL);
    return (c3_ushort_t) sc_length;
  }
  const char* get_chars() const;
  char* to_cstring(char* buffer, size_t size);
};

/**
 * Representation of string list passed in the header or in the payload. It differs from other chunks in
 * that fetching data from it (with `get_string()`) advances iterator with which it was initialized.
 */
class ListChunk {
  ChunkIterator& sl_iterator; // iterator from which strings are to be retrieved
  c3_uint_t      sl_count;    // total number of strings in the list
  c3_uint_t      sl_num;      // number of strings retrieved so far

public:
  explicit ListChunk(ChunkIterator& iterator): sl_iterator(iterator) {
    sl_count = 0;
    sl_num = UINT_MAX_VAL; // "invalid list" state
  }
  ListChunk(ChunkIterator& iterator, c3_uint_t count): sl_iterator(iterator) {
    sl_count = count;
    sl_num = 0;
  }

  bool is_valid() const { return sl_iterator.is_valid() && sl_num != UINT_MAX_VAL; }
  c3_uint_t get_count() const { return sl_count; }
  StringChunk get_string();
};

/// Iterator for fetching data chunks from a command or response header
class HeaderChunkIterator: public ChunkIterator {
  struct string_t {
    c3_uint_t s_offset; // offset within header or payload
    c3_uint_t s_length; // length of the string (which does *not* have terminating `0` byte)
  };
  struct list_t {
    c3_uint_t l_count;  // number of elements (strings) in the list
  };
  union chunk_data_t {
    c3_long_t dc_number; // a signed integer 64-bit number
    string_t  dc_string; // a binary string
    list_t    dc_list;   // a list of binary strings
  };

  c3_uint_t get_count(c3_byte_t c);
  chunk_t get_next_chunk(chunk_data_t& dc);

  // implementation of accessors for strings and lists
  c3_byte_t get_byte(c3_uint_t offset) const override;
  const c3_byte_t* get_bytes(c3_uint_t offset, c3_uint_t size) const override;

protected:
  /*
   * Derived classes must make sure ReaderWriter-derived class is in the correct state and call
   * invalidate() if there is a problem.
   */
  explicit HeaderChunkIterator(const ReaderWriter& rw): ChunkIterator(rw) {
    header_info_t hi;
    // this calls virtual function that has different implementations for commands and responses
    if (rw.get_header_info(hi)) {
      ci_offset = hi.hi_chunks_offset;
      ci_remains = hi.hi_chunks_size;
    }
  }

public:
  // application-level API for class users
  static bool has_header_data(const ReaderWriter& rw);
  chunk_t get_next_chunk_type() const;
  NumberChunk get_number();
  StringChunk get_string();
  ListChunk get_list();
};

/// Iterator for fetching strings from response payload
class PayloadChunkIterator: public ChunkIterator {
  c3_byte_t* pci_buffer; // optional buffer with uncompressed data
  c3_uint_t  pci_usize;  // size of uncompressed data in the buffer

  // implementation of accessors for strings and lists
  c3_byte_t get_byte(c3_uint_t offset) const override;
  const c3_byte_t* get_bytes(c3_uint_t offset, c3_uint_t size) const override;

protected:
  /*
   * Derived classes must make sure ReaderWriter-derived class is in the correct state and call
   * invalidate() if there is a problem.
   */
  explicit PayloadChunkIterator(const ReaderWriter& rw);
  ~PayloadChunkIterator();

public:
  PayloadChunkIterator(const PayloadChunkIterator&) = delete;
  PayloadChunkIterator(PayloadChunkIterator&&) = delete;

  PayloadChunkIterator& operator=(const PayloadChunkIterator&) = delete;
  PayloadChunkIterator& operator=(PayloadChunkIterator&&) = delete;

  // application-level API for class users
  static bool has_payload_data(const ReaderWriter &rw) { return rw.has_payload_data(); }
};

} // CyberCache

#endif // _IO_CHUNK_ITERATORS_H
