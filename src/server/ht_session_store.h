/*
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
 * Container of hash objects with session data.
 */
#ifndef _HT_SESSION_STORE_H
#define _HT_SESSION_STORE_H

#include "c3lib/c3lib.h"
#include "ht_stores.h"

namespace CyberCache {

class Optimizer;
class CommandReader;

/// Global storage of session data
class SessionObjectStore: public PayloadObjectStore {

  static constexpr c3_uint_t DEFAULT_NUM_TABLES = 2;
  static constexpr c3_uint_t DEFAULT_TABLE_CAPACITY = 4096;
  static constexpr c3_uint_t DEFAULT_QUEUE_CAPACITY = 32;
  static constexpr c3_uint_t DEFAULT_MAX_QUEUE_CAPACITY = 1024;

  void destroy_session_record(StringChunk& id);

  bool process_read_command(CommandReader& cr);
  bool process_write_command(CommandReader& cr);
  bool process_destroy_command(CommandReader& cr);
  bool process_gc_command(CommandReader& cr);

public:
  C3_FUNC_COLD SessionObjectStore() noexcept:
    PayloadObjectStore("Session store", DOMAIN_SESSION, DEFAULT_NUM_TABLES, DEFAULT_TABLE_CAPACITY,
      DEFAULT_QUEUE_CAPACITY, DEFAULT_MAX_QUEUE_CAPACITY) {
  }

  FileCommandWriter* create_file_command_writer(PayloadHashObject* pho, c3_timestamp_t time) override;

  void configure(ResponseObjectConsumer* consumer, Optimizer* optimizer) C3_FUNC_COLD {
    set_consumer(consumer);
    set_optimizer(optimizer);
  }
  void allocate() C3_FUNC_COLD {
    // to be called after initial configuration had been loaded
    init_payload_object_store();
  }
  void dispose() C3_FUNC_COLD {
    dispose_payload_object_store();
  }

  bool process_command(CommandReader* cr);
};

}

#endif // _HT_SESSION_STORE_H
