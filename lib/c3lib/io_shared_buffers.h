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
 * Implementation of the `SharedBuffers` class.
 */
#ifndef _IO_SHARED_BUFFERS_H
#define _IO_SHARED_BUFFERS_H

#include "io_data_buffer.h"
#include "io_payload.h"
#include "c3_compressor.h"

namespace CyberCache {

/**
 * Class implementing data store that can be passed on from object to object in the server pipelines
 * without any data duplications. Basically, it encapsulates all data that are not device-specific.
 * Classes using this one maintain their own indices into its data, so data stored in the buffers
 * maintained (or, if it links a lockable hash object, referenced) by this class can be simultaneously,
 * say, written to a binlog and sent to several replication servers. Maintains reference count and allows
 * for concurrent read access to the data it stores.
 */
class SharedBuffers {
  /**
   * Size of the "alternative" buffer for small headers and short responses. This constant should never be
   * smaller than 5, as that is combined size of command descriptor and command header length bytes
   * (command readers must store those somewhere before `sb_data` is set to full header size).
   */
  static constexpr c3_uint_t AUX_DATA_SIZE = 12;

protected:
  Memory&             sb_memory;             // memory object for `sb_data` and `sb_payload`
  DataBuffer          sb_data;               // command header or response data
  DataBuffer          sb_payload;            // payload buffer
  c3_byte_t           sb_aux[AUX_DATA_SIZE]; // "alternative" storage for small headers or responses
  std::atomic_uint    sb_nrefs;              // reference count: current number of users of this buffer

  c3_uint_t get_num_refs() const { return sb_nrefs.load(std::memory_order_acquire); }
  void set_num_refs(c3_uint_t num) { sb_nrefs.store(num, std::memory_order_release); }
  c3_uint_t increment_num_refs() { return sb_nrefs.fetch_add(1, std::memory_order_acq_rel); }
  c3_uint_t decrement_num_refs() { return sb_nrefs.fetch_sub(1, std::memory_order_acq_rel); }

  explicit SharedBuffers(Memory& memory): sb_memory(memory) {
    set_num_refs(0);
  }
  virtual ~SharedBuffers();

  virtual c3_uint_t get_object_size() const;
  virtual void clone_payload(SharedBuffers* cloned_sb) const;

public:
  SharedBuffers(const SharedBuffers&) = delete;
  SharedBuffers(SharedBuffers&&) = delete;

  SharedBuffers& operator=(const SharedBuffers&) = delete;
  SharedBuffers& operator=(SharedBuffers&&) = delete;

  /**
   * Creates instance of the SharedBuffers class; ctor is private, so this method is the only way to
   * obtain an instance.
   *
   * @param memory Memory domain that should be used to allocate all buffers
   * @return Instance of the SharedBuffers class
   */
  static SharedBuffers* create(Memory& memory);

  /**
   * Create copy of the instance on which this method is called.
   *
   * @param full If `true`, created full copy; if `false`, only copies header data, leaving payload empty.
   * @return New instance of the `SharedBuffers` object.
   */
  SharedBuffers* clone(bool full) const;

  /**
   * Increments usage count of the instance. This method should *only* be called when copying a pointer
   * to an existing imstance; when a new instance is created using static create() method, that method
   * sets reference count to 1 by itself.
   */
  void add_reference() { increment_num_refs(); }

  /**
   * Decrements usage count of specified instance; if the count becomes zero, the instance is deleted. If
   * the instance being deleted has non-NULL `sb_object` field, the instance will be deregistered as a
   * reader of the lockable hash object pointed to by `sb_object`.
   *
   * @param sb Instance to delete
   * @return `true` if instance was deleted, `false` otherwise
   */
  static bool remove_reference(SharedBuffers* sb);

  /**
   * Get reference to the internal memory object.
   *
   * @return Reference to the Memory object.
   */
  Memory& get_memory_object() { return sb_memory; }

  /////////////////////////////////////////////////////////////////////////////
  // ACCESSORS: HEADER
  /////////////////////////////////////////////////////////////////////////////

  bool using_static_header() const { return sb_data.get_size() == 0; }

  void configure_header(c3_uint_t used_size, c3_uint_t full_size);

  c3_uint_t get_available_header_size() const {
    c3_uint_t size = sb_data.get_size();
    return size > 0? size: AUX_DATA_SIZE;
  }

  c3_byte_t* get_header_bytes(c3_uint_t offset, c3_uint_t size) {
    if (sb_data.get_size() > 0) {
      return sb_data.get_bytes(offset, size);
    } else {
      assert(offset + size <= AUX_DATA_SIZE);
      return sb_aux + offset;
    }
  }

  /////////////////////////////////////////////////////////////////////////////
  // ACCESSORS: PAYLOAD
  /////////////////////////////////////////////////////////////////////////////

  virtual c3_uint_t get_payload_size() const;

  virtual c3_uint_t get_payload_usize() const;

  virtual c3_compressor_t get_payload_compressor() const;

  virtual c3_byte_t* get_payload_bytes(c3_uint_t offset, c3_uint_t size) const;

  virtual c3_byte_t* set_payload_size(c3_uint_t size);

  /**
   * Attaches payload hash object to the shared buffer. Can only be called if:
   * - payload hash is currently locked,
   * - payload hash object is not yet marked as "deleted",
   * - another payload hash object had not been attached to the shared object already,
   * - internal payload buffer is empty.
   *
   * @param payload Pointer to an instance of PayloadHashObject
   */
  virtual void attach_payload(Payload* payload);

  /**
   * Transfers payload to a payload hash object and registers itself as a reader of that hash object.
   * Can only be called if:
   * - payload hash is currently locked,
   * - payload hash object is not yet marked as "deleted",
   * - payload hash object does not currently have other readers,
   * - another payload hash object had not been attached to the shared object already.
   *
   * @param payload Pointer to an instance of PayloadHashObject
   * @param domain Target memory domain (to which payload is to be transferred)
   * @param usize Size of uncompressed data in the buffer
   * @param compressor Compressor type
   */
  virtual void transfer_payload(Payload* payload, domain_t domain, c3_uint_t usize, c3_compressor_t compressor);
};

} // CyberCache

#endif // _IO_SHARED_BUFFERS_H
