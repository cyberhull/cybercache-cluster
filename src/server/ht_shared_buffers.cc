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
#include "ht_shared_buffers.h"

// for placement new to work with non-default ctors
#include <new>

namespace CyberCache {

constexpr c3_byte_t SharedObjectBuffers::ZERO_LENGTH_BUFFER[];

SharedObjectBuffers::SharedObjectBuffers(Memory& memory):
  SharedBuffers(memory)
  #if C3_INSTRUMENTED
  // in non-instrumented mode, `SpinLock` ctor does not take any arguments
  , sob_lock(memory.get_domain())
  #endif
{
  sob_object = nullptr;
}

SharedObjectBuffers::~SharedObjectBuffers() {
  c3_assert(sob_lock.is_unlocked());
  if (sob_object != nullptr) {
    sob_object->unregister_reader();
  }
}

void SharedObjectBuffers::clone_payload(SharedBuffers* cloned_sb) const {
  if (sob_object != nullptr) {
    auto sob = (SharedObjectBuffers*) cloned_sb;
    c3_assert(sob && sob_object->is_locked());
    sob_object->register_reader();
    SpinLockGuard guard(sob->sob_lock);
    sob->sob_object = sob_object;
  } else {
    SharedBuffers::clone_payload(cloned_sb);
  }
}

c3_uint_t SharedObjectBuffers::get_object_size() const {
  return sizeof(SharedObjectBuffers);
}

SharedObjectBuffers* SharedObjectBuffers::create_object(Memory& memory) {
  auto sob = alloc<SharedObjectBuffers>(memory);
  new (sob) SharedObjectBuffers(memory);
  c3_assert(sob->sob_object == nullptr && sob->sb_payload.is_empty() && sob->get_num_refs() == 0);
  return sob;
}

c3_uint_t SharedObjectBuffers::get_payload_size() const {
  SpinLockGuard guard(sob_lock);
  if (sob_object != nullptr) {
    c3_assert(sob_object->has_readers());
    return sob_object->get_buffer_size();
  } else {
    return sb_payload.get_size();
  }
}

c3_uint_t SharedObjectBuffers::get_payload_usize() const {
  SpinLockGuard guard(sob_lock);
  c3_assert(sob_object && sob_object->has_readers());
  return sob_object->get_buffer_usize();
}

c3_compressor_t SharedObjectBuffers::get_payload_compressor() const {
  SpinLockGuard guard(sob_lock);
  c3_assert(sob_object && sob_object->has_readers());
  return sob_object->get_buffer_compressor();
}

c3_byte_t* SharedObjectBuffers::get_payload_bytes(c3_uint_t offset, c3_uint_t size) const {
  SpinLockGuard guard(sob_lock);
  if (sob_object != nullptr) {
    return sob_object->get_buffer_bytes(offset, size);
  } else {
    if (size > 0) {
      return sb_payload.get_bytes(offset, size);
    } else {
      c3_assert(offset == 0 && sb_payload.is_empty());
      return (c3_byte_t*) ZERO_LENGTH_BUFFER;
    }
  }
}

c3_byte_t* SharedObjectBuffers::set_payload_size(c3_uint_t size) {
  SpinLockGuard guard(sob_lock);
  c3_assert(sob_object == nullptr);
  return sb_payload.set_size(sb_memory, size);
}

void SharedObjectBuffers::attach_payload(Payload* payload) {
  SpinLockGuard guard(sob_lock);
  auto pho = (PayloadHashObject*) payload;
  c3_assert(is_usable(pho) && sob_object == nullptr && sb_payload.is_empty());
  pho->register_reader();
  sob_object = pho;
}

void SharedObjectBuffers::transfer_payload(Payload* payload, domain_t domain, c3_uint_t usize,
  c3_compressor_t compressor) {
  SpinLockGuard guard(sob_lock);
  auto pho = (PayloadHashObject*) payload;
  c3_uint_t size = sb_payload.get_size();
  c3_byte_t* buffer = size != 0? sb_payload.get_bytes(): (c3_byte_t*) ZERO_LENGTH_BUFFER;
  c3_assert(is_usable(pho) && !pho->has_readers() && sob_object == nullptr && buffer != nullptr && usize >= size);
  Memory& memory = Memory::get_memory_object(domain); // TARGET memory object
  pho->set_buffer(compressor, size, usize, buffer, memory);
  // attach object to these shared buffers and register them as a reader
  pho->register_reader();
  sob_object = pho;
  if (size != 0) {
    memory.transfer_used_size(sb_memory, size);
    sb_payload.reset_buffer_transferred_to_another_object();
  }
}

}
