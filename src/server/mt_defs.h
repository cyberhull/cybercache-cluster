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
 * Multithreading support: common types and constants.
 */
#ifndef _MT_DEFS_H
#define _MT_DEFS_H

#include "c3lib/c3lib.h"

namespace CyberCache {

/// Atomic `float` type for which C++11/17 do not provide specialization
typedef std::atomic<float> atomic_float_t;

/// Types of objects that host/employ synchronization primitives
enum host_object_t {
  HO_INVALID,     // an invalid host (placeholder)
  HO_SERVER,      // server core
  HO_LISTENER,    // incoming traffic listener
  HO_LOGGER,      // logging service
  HO_STORE,       // an object that pertains to a domain's store
  HO_TAG_MANAGER, // tag manager of the FPC store (has to be separate, for queue IDs not to overlap)
  HO_REPLICATOR,  // replication service
  HO_BINLOG,      // binlog service
  HO_OPTIMIZER,   // optimizer
  HO_NUMBER_OF_ELEMENTS
};

/// Types of synchronization objects themselves
enum sync_object_t {
  SO_INVALID,            // an invalid sync object (placeholder)
  SO_SHARED_MUTEX,       // mutex supporting shared lock for reading or exclusive lock for writing
  SO_DOWNGRADABLE_MUTEX, // mutex whose write lock can be downgraded to a read lock
  SO_MESSAGE_QUEUE,      // message queue with synchronized access
  SO_NUMBER_OF_ELEMENTS
};

/// Base class for all synchronization primitives except SpinLock
class SyncObject {
  const domain_t      so_domain; // global / session / fpc
  const host_object_t so_host;   // host object type
  const sync_object_t so_type;   // synchronization object type
  const c3_byte_t     so_id;     // optional unique ID (in case there are multiple SOs of the same type per host)

protected:
  C3_FUNC_COLD SyncObject(domain_t domain, host_object_t host, sync_object_t type, c3_byte_t id):
    so_domain(domain), so_host(host), so_type(type), so_id(id) {
    c3_assert(domain > DOMAIN_INVALID && domain < DOMAIN_NUMBER_OF_ELEMENTS &&
      host > HO_INVALID && host < HO_NUMBER_OF_ELEMENTS &&
      type > SO_INVALID && type < SO_NUMBER_OF_ELEMENTS);
  }

public:
  domain_t get_domain() const { return so_domain; }
  const char* get_domain_name() const { return Memory::get_domain_name(so_domain); }
  Memory& get_memory_object() const { return Memory::get_memory_object(so_domain); }
  host_object_t get_host() const { return so_host; }
  const char* get_host_name() const C3_FUNC_COLD;
  sync_object_t get_type() const { return so_type; }
  const char* get_type_name() const C3_FUNC_COLD;
  c3_byte_t get_id() const { return so_id; }

  /**
   * Size of the buffer that is needed to get text representation of the full type of the
   * synchronization object; current longest theoretically possible combination of components
   * is 34 characters, even though actual length will always be shorter: tag manager (longest
   * host type) does not have a downgradable mutex (longest sync object type).
   */
  static constexpr size_t INFO_BUFF_LENGTH = 36;
  const char* get_text_info(char* buff, size_t length) const C3_FUNC_COLD;
};

}

#endif // _MT_DEFS_H
