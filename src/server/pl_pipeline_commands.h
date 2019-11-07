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
 * I/O pipelines: class that implements parametrized commands to be used in pipeline message queues.
 */
#ifndef _PL_PIPELINE_COMMANDS_H
#define _PL_PIPELINE_COMMANDS_H

#include "c3lib/c3lib.h"

namespace CyberCache {

/**
 * Command messages to be sent using `CommandMessage` to and from socket and file pipelines. Implements
 * all methods necessary to satisfy requirements and constraints of `CommandMessage` container.
 */
class PipelineCommand {
  const void* const pc_null;   // always `NULL` (this is where `ReaderWriter`'s vtable resides)
  const c3_byte_t   pc_id;     // message ID
  const domain_t    pc_domain; // memory domain
  const c3_ushort_t pc_size;   // size of *extra* (i.e. beyond this field) data in the message, bytes

  PipelineCommand(c3_byte_t id, domain_t domain, c3_ushort_t size):
    pc_null(nullptr), pc_id(id), pc_domain(domain), pc_size(size) {
  }

public:
  static PipelineCommand* create(c3_uintptr_t id, domain_t domain, const void* buff, size_t size);

  /////////////////////////////////////////////////////////////////////////////
  // ACCESSORS
  /////////////////////////////////////////////////////////////////////////////

  c3_uintptr_t get_id() const { return pc_id; }

  const void* get_data() const { return (c3_byte_t*) this + sizeof(PipelineCommand); }

  c3_ushort_t get_ushort_data() const {
    c3_assert(pc_size == sizeof(c3_ushort_t));
    return *(c3_ushort_t*) get_data();
  }

  c3_uint_t get_uint_data() const {
    c3_assert(pc_size == sizeof(c3_uint_t));
    return *(c3_uint_t*) get_data();
  }

  c3_ulong_t get_ulong_data() const {
    c3_assert(pc_size == sizeof(c3_ulong_t));
    return *(c3_ulong_t*) get_data();
  }

  c3_uint_t get_size() const { return pc_size; }

  /////////////////////////////////////////////////////////////////////////////
  // METHODS REQUIRED BY MESSAGE CONTAINER
  /////////////////////////////////////////////////////////////////////////////

  bool is_you() const { return pc_null == nullptr; }

  c3_uint_t get_object_size() const { return pc_size + sizeof(PipelineCommand); }

  Memory& get_memory_object() const { return Memory::get_memory_object(pc_domain); }
};

}

#endif // _PL_PIPELINE_COMMANDS_H
