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
#ifndef _HT_PAGE_STORE_H
#define _HT_PAGE_STORE_H

#include "c3lib/c3lib.h"
#include "ht_stores.h"

namespace CyberCache {

class Optimizer;
class PageOptimizer;
class TagStore;
class CommandReader;

/// Global storage of FPC data
class PageObjectStore: public PayloadObjectStore {

  static constexpr c3_uint_t DEFAULT_NUM_TABLES = 4;
  static constexpr c3_uint_t DEFAULT_TABLE_CAPACITY = 8192;
  static constexpr c3_uint_t DEFAULT_QUEUE_CAPACITY = 32;
  static constexpr c3_uint_t DEFAULT_MAX_QUEUE_CAPACITY = 2048;

  TagStore* pos_tag_manager; // reference to the tag manager of the FPC domain

  TagStore& get_tag_manager() const {
    c3_assert(pos_tag_manager);
    return *pos_tag_manager;
  }
  PageOptimizer& get_page_optimizer() const {
    return *(PageOptimizer*)&(get_optimizer());
  }

  bool remove_fpc_record(const StringChunk& id, CommandReader& cr);

  bool process_load_command(CommandReader& cr);
  bool process_test_command(CommandReader& cr);
  bool process_save_command(CommandReader& cr);
  bool process_remove_command(CommandReader& cr);
  bool process_getfillingpercentage_command(CommandReader& cr);
  bool process_getmetadatas_command(CommandReader& cr);
  bool process_touch_command(CommandReader& cr);

public:
  C3_FUNC_COLD PageObjectStore() noexcept:
    PayloadObjectStore("FPC store", DOMAIN_FPC, DEFAULT_NUM_TABLES, DEFAULT_TABLE_CAPACITY,
      DEFAULT_QUEUE_CAPACITY, DEFAULT_MAX_QUEUE_CAPACITY) {
    pos_tag_manager = nullptr;
  }

  FileCommandWriter* create_file_command_writer(PayloadHashObject* pho, c3_timestamp_t time) override;

  void configure(ResponseObjectConsumer* consumer, Optimizer* optimizer, TagStore* tag_manager) C3_FUNC_COLD {
    c3_assert(tag_manager && pos_tag_manager == nullptr);
    set_consumer(consumer);
    set_optimizer(optimizer);
    pos_tag_manager = tag_manager;
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

#endif // _HT_PAGE_STORE_H
