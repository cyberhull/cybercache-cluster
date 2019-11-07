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
#include "c3_compressor.h"
#include "io_chunk_iterators.h"

#include <cstring>

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// ChunkIterator implementation
///////////////////////////////////////////////////////////////////////////////

bool ChunkIterator::has_any_data(const ReaderWriter& rw) {
  return HeaderChunkIterator::has_header_data(rw) || PayloadChunkIterator::has_payload_data(rw);
}

///////////////////////////////////////////////////////////////////////////////
// StringChunk implementation
///////////////////////////////////////////////////////////////////////////////

const char* StringChunk::get_chars() const {
  c3_assert(is_valid());
  return (const char*) sc_iterator.get_bytes(sc_offset, sc_length);
}

char* StringChunk::to_cstring(char* buffer, size_t size) {
  assert(buffer);
  c3_assert(is_valid() && sc_length < size);
  if (sc_length > 0) {
    std::memcpy(buffer, get_chars(), sc_length);
  }
  buffer[sc_length] = 0;
  return buffer;
}

///////////////////////////////////////////////////////////////////////////////
// ListChunk implementation
///////////////////////////////////////////////////////////////////////////////

StringChunk ListChunk::get_string() {
  c3_assert(is_valid() && sl_num < sl_count);
  c3_uint_t length = 0;
  c3_byte_t c;
  do {
    c = sl_iterator.get_byte(sl_iterator.ci_offset);
    sl_iterator.skip(1);
    length += c;
  } while (c == 255);
  c3_uint_t offset = sl_iterator.ci_offset;
  sl_iterator.skip(length);
  sl_num++;
  return StringChunk(sl_iterator, offset, length);
}

///////////////////////////////////////////////////////////////////////////////
// HeaderChunkIterator implementation
///////////////////////////////////////////////////////////////////////////////

c3_byte_t HeaderChunkIterator::get_byte(c3_uint_t offset) const {
  return ci_rw.get_header_byte_at(offset);
}

const c3_byte_t* HeaderChunkIterator::get_bytes(c3_uint_t offset, c3_uint_t size) const {
  return ci_rw.get_const_header_bytes(offset, size);
}

c3_uint_t HeaderChunkIterator::get_count(c3_byte_t c) {
  c3_uint_t result;
  switch (c & CHNK_SHORT_MASK) {
    case 0:
      result = ci_rw.get_header_byte_at(ci_offset + 1);
      skip(2);
      return result;
    case 1:
      result = ci_rw.get_header_ushort_at(ci_offset + 1);
      skip(3);
      return result;
    case 2:
      result = ci_rw.get_header_uint3_at(ci_offset + 1);
      skip(4);
      return result;
    case 3:
      result = ci_rw.get_header_uint_at(ci_offset + 1);
      skip(5);
      return result;
    default:
      c3_assert_failure();
      return 0;
  }
}

chunk_t HeaderChunkIterator::get_next_chunk(chunk_data_t& dc) {
  if (ci_remains > 0) {
    c3_byte_t c = ci_rw.get_header_byte_at(ci_offset);
    switch (c & CHNK_TYPE_BITS) {
      case CHNK_INTEGER:
        dc.dc_number = (c & CHNK_LONG_MASK) + CHNK_INTEGER_BIAS;
        skip(1);
        return CHUNK_NUMBER;
      case CHNK_STRING:
        dc.dc_string.s_length = (c & CHNK_LONG_MASK) + CHNK_STRING_BIAS;
        dc.dc_string.s_offset = ci_offset + 1;
        skip(dc.dc_string.s_length + 1);
        return CHUNK_STRING;
      case CHNK_LIST:
        dc.dc_list.l_count = (c & CHNK_LONG_MASK) + CHNK_LIST_BIAS;
        skip(1);
        return CHUNK_LIST;
      default: // CHNK_SUBTYPE
        switch (c & CHNK_SUBTYPE_BITS) {
          case CHNK_SMALL_NEGATIVE:
            dc.dc_number = -(c3_long_t)(c & CHNK_SHORT_MASK) + CHNK_SMALL_NEGATIVE_BIAS;
            skip(1);
            return CHUNK_NUMBER;
          case CHNK_BIG_NEGATIVE:
            dc.dc_number = -(c3_long_t)get_count(c) + CHNK_BIG_NEGATIVE_BIAS;
            return CHUNK_NUMBER;
          case CHNK_SMALL_INTEGER:
            dc.dc_number = c & CHNK_SHORT_MASK;
            skip(1);
            return CHUNK_NUMBER;
          case CHNK_SHORT_STRING:
            dc.dc_string.s_length = c & CHNK_SHORT_MASK;
            dc.dc_string.s_offset = ci_offset + 1;
            skip(dc.dc_string.s_length + 1);
            return CHUNK_STRING;
          case CHNK_SHORT_LIST:
            dc.dc_list.l_count = c & CHNK_SHORT_MASK;
            skip(1);
            return CHUNK_LIST;
          case CHNK_BIG_INTEGER:
            dc.dc_number = get_count(c) + CHNK_BIG_INTEGER_BIAS;
            return CHUNK_NUMBER;
          case CHNK_LONG_STRING:
            dc.dc_string.s_length = get_count(c) + CHNK_LONG_STRING_BIAS;
            dc.dc_string.s_offset = ci_offset;
            skip(dc.dc_string.s_length);
            return CHUNK_STRING;
          default: // CHNK_LONG_LIST:
            dc.dc_list.l_count = get_count(c) + CHNK_LONG_LIST_BIAS;
            return CHUNK_LIST;
        }
    }
  }
  return CHUNK_NONE;
}

bool HeaderChunkIterator::has_header_data(const ReaderWriter& rw) {
  header_info_t hi;
  return rw.get_header_info(hi) && hi.hi_chunks_size > 0;
}

chunk_t HeaderChunkIterator::get_next_chunk_type() const {
  if (ci_remains > 0) {
    c3_byte_t c = ci_rw.get_header_byte_at(ci_offset);
    switch (c & CHNK_TYPE_BITS) {
      case CHNK_INTEGER:
        return CHUNK_NUMBER;
      case CHNK_STRING:
        return CHUNK_STRING;
      case CHNK_LIST:
        return CHUNK_LIST;
      default: // CHNK_SUBTYPE
        switch (c & CHNK_SUBTYPE_BITS) {
          case CHNK_SHORT_STRING:
          case CHNK_LONG_STRING:
            return CHUNK_STRING;
          case CHNK_SHORT_LIST:
          case CHNK_LONG_LIST:
            return CHUNK_LIST;
          default:
            return CHUNK_NUMBER;
        }
    }
  }
  return CHUNK_NONE;
}

NumberChunk HeaderChunkIterator::get_number() {
  chunk_data_t dc;
  c3_assert_def(chunk_t type) get_next_chunk(dc);
  c3_assert(type == CHUNK_NUMBER && dc.dc_number >= INT_MIN_VAL && dc.dc_number <= UINT_MAX_VAL);
  return NumberChunk(dc.dc_number);
}

StringChunk HeaderChunkIterator::get_string() {
  chunk_data_t dc;
  c3_assert_def(chunk_t type) get_next_chunk(dc);
  c3_assert(type == CHUNK_STRING);
  return StringChunk(*this, dc.dc_string.s_offset, dc.dc_string.s_length);
}

ListChunk HeaderChunkIterator::get_list() {
  chunk_data_t dc;
  c3_assert_def(chunk_t type) get_next_chunk(dc);
  // for each string, there should be at least a count byte
  c3_assert(type == CHUNK_LIST && dc.dc_list.l_count <= ci_remains);
  return ListChunk(*this, dc.dc_list.l_count);
}

///////////////////////////////////////////////////////////////////////////////
// PayloadChunkIterator implementation
///////////////////////////////////////////////////////////////////////////////

c3_byte_t PayloadChunkIterator::get_byte(c3_uint_t offset) const {
  if (pci_buffer != nullptr) {
    assert(offset < pci_usize);
    return pci_buffer[offset];
  } else {
    return *ci_rw.get_payload_bytes(offset, 1);
  }
}

const c3_byte_t* PayloadChunkIterator::get_bytes(c3_uint_t offset, c3_uint_t size) const {
  if (pci_buffer != nullptr) {
    assert(offset + size <= pci_usize);
    return pci_buffer + offset;
  } else {
    return ci_rw.get_payload_bytes(offset, size);
  }
}

PayloadChunkIterator::PayloadChunkIterator(const ReaderWriter& rw):
  ChunkIterator(rw) {
  // assume buffer is uncompressed; offset and remaining length are reset in parent ctor
  pci_buffer = nullptr;
  pci_usize = 0;

  payload_info_t pi;
  // this calls virtual function that has different implementations for commands and responses
  if (rw.get_payload_info(pi) && pi.pi_buffer != nullptr) {
    c3_assert(!pi.pi_has_errors);
    ci_remains = pi.pi_size;
    if (pi.pi_compressor != CT_NONE) {
      pci_buffer = global_compressor.unpack(pi.pi_compressor, pi.pi_buffer, pi.pi_size, pi.pi_usize,
        rw.get_memory_object());
      if (pci_buffer != nullptr) {
        pci_usize = pi.pi_usize;
        ci_remains = pi.pi_usize;
      } else {
        c3_assert_failure();
        ci_remains = 0; // invalidate
      }
    }
  }
}

PayloadChunkIterator::~PayloadChunkIterator() {
  if (pci_buffer != nullptr) {
    c3_assert(pci_usize);
    ci_rw.get_memory_object().free(pci_buffer, pci_usize);
  }
}

} // CyberCache
