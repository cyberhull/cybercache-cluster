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

// for placement new to work with non-default ctors
#include <new>
#include <cstring>

#include "io_shared_buffers.h"
#include "c3_profiler_defs.h"

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// CREATION AND MAINTENANCE
///////////////////////////////////////////////////////////////////////////////

SharedBuffers::~SharedBuffers() {
  c3_assert(get_num_refs() == 0 && sb_data.get_size() == 0 && sb_payload.get_size() == 0);
}

c3_uint_t SharedBuffers::get_object_size() const {
  return sizeof(SharedBuffers);
}

SharedBuffers* SharedBuffers::create(Memory& memory) {
  auto sb = alloc<SharedBuffers>(memory);
  new (sb) SharedBuffers(memory);
  c3_assert(sb->get_num_refs() == 0);
  return sb;
}

void SharedBuffers::clone_payload(SharedBuffers* cloned_sb) const {
  c3_assert(cloned_sb);
  c3_uint_t size = sb_payload.get_size();
  if (size > 0) {
    std::memcpy(cloned_sb->sb_payload.set_size(cloned_sb->sb_memory, size), sb_payload.get_bytes(0), size);
  }
}

SharedBuffers* SharedBuffers::clone(bool full) const {
  SharedBuffers* sb = create(sb_memory);
  c3_uint_t size = sb_data.get_size();
  if (size > 0) {
    c3_assert(size > AUX_DATA_SIZE);
    std::memcpy(sb->sb_data.set_size(sb->sb_memory, size), sb_data.get_bytes(0), size);
  } else {
    std::memcpy(sb->sb_aux, sb_aux, AUX_DATA_SIZE);
  }
  if (full) {
    clone_payload(sb);
  }
  return sb;
}

bool SharedBuffers::remove_reference(SharedBuffers* sb) {
  c3_assert(sb && sb->get_num_refs() >= 1);
  if (sb->decrement_num_refs() == 1) { // checks *previous* value
    /*
     * The following two empty() calls must be made before dtor is called, because dtors of the
     * member objects run before containing object's dtor, and those member objects' dtors check that
     * their memory buffers are already freed (i.e. we cannot free them in SharedBuffers dtor).
     */
    Memory& memory = sb->sb_memory;
    sb->sb_data.empty(memory);
    sb->sb_payload.empty(memory);
    c3_uint_t size = sb->get_object_size();
    sb->~SharedBuffers();
    memory.free(sb, size);
    return true;
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// HEADER CONFIGURATION
///////////////////////////////////////////////////////////////////////////////

void SharedBuffers::configure_header(c3_uint_t used_size, c3_uint_t full_size) {
  assert(used_size <= AUX_DATA_SIZE && full_size > used_size);
  PERF_UPDATE_ARRAY(Shared_Header_Size, full_size)
  if (full_size > AUX_DATA_SIZE) {
    PERF_INCREMENT_COUNTER(Shared_Header_Reallocations)
    c3_byte_t* buffer = sb_data.set_size(sb_memory, full_size);
    c3_assert(buffer);
    std::memcpy(buffer, sb_aux, used_size);
  }
}

///////////////////////////////////////////////////////////////////////////////
// PAYLOAD ACCESSORS
///////////////////////////////////////////////////////////////////////////////

c3_uint_t SharedBuffers::get_payload_size() const {
  return sb_payload.get_size();
}

c3_uint_t SharedBuffers::get_payload_usize() const {
  // this method should only be called on an instance of a derived class
  c3_assert_failure();
  return 0;
}

c3_compressor_t SharedBuffers::get_payload_compressor() const {
  // this method should only be called on an instance of a derived class
  c3_assert_failure();
  return CT_NONE;
}

c3_byte_t* SharedBuffers::get_payload_bytes(c3_uint_t offset, c3_uint_t size) const {
  return sb_payload.get_bytes(offset, size);
}

c3_byte_t* SharedBuffers::set_payload_size(c3_uint_t size) {
  return sb_payload.set_size(sb_memory, size);
}

///////////////////////////////////////////////////////////////////////////////
// ADVANCED PAYLOAD CONFIGURATION
///////////////////////////////////////////////////////////////////////////////

void SharedBuffers::attach_payload(Payload* payload) {
  // this method should only be called on an instance of a derived class
  c3_assert_failure();
}

void SharedBuffers::transfer_payload(Payload* payload, domain_t domain, c3_uint_t usize, c3_compressor_t compressor) {
  // this method should only be called on an instance of a derived class
  c3_assert_failure();
}

}
