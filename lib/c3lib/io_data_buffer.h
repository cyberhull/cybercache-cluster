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
 * Resizable data buffer for various server command and response objects.
 */
#ifndef _IO_DATA_BUFFER_H
#define _IO_DATA_BUFFER_H

#include "c3_build.h"
#include "c3_memory.h"

namespace CyberCache {

/**
 * Class that stores data for various server commands.
 *
 * All methods implementing access to the underlying memory buffer check their arguments to make sure
 * users do not go beoynd buffer bounds.
 */
class DataBuffer {
  c3_byte_t*  db_buffer; // stored data
  c3_uint_t   db_size;   // size of the data (memory block) in `db_buffer`

  void validate() const {
    // using parentheses to satisfy GCC's -Wall
    c3_assert((db_buffer == nullptr && db_size == 0) || (db_buffer && db_size));
  }

public:
  DataBuffer() {
    db_buffer = nullptr;
    db_size = 0;
  }
  DataBuffer(const DataBuffer&) = delete;
  DataBuffer(DataBuffer&&) = delete;
  ~DataBuffer() {
    if (db_buffer != nullptr) {
      free_memory(db_buffer, db_size);
      // should have been freed by its container by now
      c3_assert_failure();
    }
  }

  // disabled operators
  void* operator new(size_t) = delete;
  DataBuffer& operator=(const DataBuffer&) = delete;
  DataBuffer& operator=(DataBuffer&&) = delete;
  void operator delete(void*) = delete;

  /**
   * Checks if the buffer is empty.
   *
   * @return `true` if the buffer is empty, `false` otherwise
   */
  bool is_empty() const {
    validate();
    return db_buffer == nullptr;
  }

  /**
   * Checks if the buffer is not empty.
   *
   * @return `true` if the buffer is not empty, `false` otherwise
   */
  bool is_not_empty() const {
    validate();
    return db_buffer != nullptr;
  }

  /**
   * Releases memory block used by the buffer and resets buffer size.
   */
  void empty(Memory& memory);

  /**
   * Set buffer pointer and size to zero *without* deallocation; the name of the method is
   * intentionally long to stress that it's a special operation.
   */
  void reset_buffer_transferred_to_another_object();

  /**
   * Get current size of the buffer.
   *
   * @return Current buffer size
   */
  c3_uint_t get_size() const { return db_size; }

  /**
   * Expand of contract data buffer.
   *
   * @param size New size of the buffer.
   * @return Pointer to the resized buffer.
   */
  c3_byte_t* set_size(Memory& memory, c3_uint_t size);

  /**
   * Get pointer to the specified location in the buffer; the location must point to a byte that is
   * contained in the buffer (i.e. maximum allowed offset that can be passed to this method is buffer
   * size minus one).
   *
   * @param offset Offset into the buffer, bytes; default is 0.
   * @return Pointer to the specified part of the buffer, or NULL on failure.
   */
  c3_byte_t* get_bytes(c3_uint_t offset = 0) const {
    #ifdef C3_FASTEST
    return db_buffer + offset;
    #else
    validate();
    assert(offset < db_size);
    c3_assert(db_buffer);
    return db_buffer + offset;
    #endif
  }

  /**
   * Get pointer to the specified segment of the buffer; entire segment must lie within the buffer (i.e.
   * the sum of offset and size passed to this method must be leff then of equal to current buffer size).
   *
   * @param offset Offset into the buffer, bytes
   * @param size Size of the segment, bytes
   * @return Pointer to the specified segment of the buffer, or NULL on failure.
   */
  c3_byte_t* get_bytes(c3_uint_t offset, c3_uint_t size) const {
    #ifdef C3_FASTEST
    return db_buffer + offset;
    #else
    validate();
    assert(offset + size <= db_size);
    c3_assert(db_buffer);
    return db_buffer + offset;
    #endif
  }

  /**
   * Get reference to a byte in the buffer.
   *
   * @param i Offset to the byte
   * @return Reference to the byte at specified offset
   */
  c3_byte_t& operator[](c3_uint_t i) const { return *get_bytes(i); }
};

}

#endif // _IO_DATA_BUFFER_H
