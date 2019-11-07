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
 * Container that manages tags for the FPC using own thread.
 */
#ifndef _HT_TAG_MANAGER_H
#define _HT_TAG_MANAGER_H

#include "c3lib/c3lib.h"
#include "ht_stores.h"

namespace CyberCache {

class PageObjectStore;
class SocketResponseWriter;
class PayloadListChunkBuilder;

/// Concurrent tag manager for the FPC
class TagStore: public ObjectStore {

  static constexpr c3_uint_t DEFAULT_NUM_TABLES = 1;
  static constexpr c3_uint_t DEFAULT_TABLE_CAPACITY = 256;
  static constexpr c3_uint_t DEFAULT_QUEUE_CAPACITY = 32;
  static constexpr c3_uint_t DEFAULT_MAX_QUEUE_CAPACITY = 16384;

  /// Name of the tag that links all pages not tagged with any user tags
  constexpr static const char* NAME_OF_TAG_FOR_UNTAGGED_PAGES = "<<< UNTAGGED >>>";

  /// Commands submitted to the tag manager through its input queue
  enum tag_command_t: c3_uintptr_t {
    TC_INVALID = 0,         // invalid message (placeholder)
    TC_UNLINK_OBJECT,       // the object had been deleted by *optimizer*, unlink it from all tags
    TC_CAPACITY_CHANGE,     // queue capacity change request
    TC_MAX_CAPACITY_CHANGE, // maximum queue capacity change request
    TC_QUIT,                // should process remaining messages and then quit
    TC_NUMBER_OF_ELEMENTS
  };

  /// Message type used by the internal queue of the tag manager
  class TagMessage {
    union {
      tag_command_t      tm_cmd_id;   // numeric (ID-only) command
      CommandReader*     tm_cmd_cr;   // server command passed on by connection thread
    };
    union {
      PayloadHashObject* tm_pho;      // hash object to operate on
      c3_uint_t          tm_capacity; // argument for configuration requests
    };

  public:
    TagMessage() {
      tm_cmd_id = TC_INVALID;
      tm_pho = nullptr;
    }
    TagMessage(tag_command_t cmd, c3_uint_t capacity) {
      tm_cmd_id = cmd;
      tm_capacity = capacity;
    }
    TagMessage(tag_command_t cmd, PayloadHashObject* pho = nullptr) {
      tm_cmd_id = cmd;
      tm_pho = pho;
    }
    TagMessage(CommandReader* cr, PayloadHashObject* pho = nullptr) {
      tm_cmd_cr = cr;
      tm_pho = pho;
    }

    bool is_valid() const { return tm_cmd_id > TC_INVALID; }
    bool is_id_command() const { return tm_cmd_id < TC_NUMBER_OF_ELEMENTS; }
    tag_command_t get_id() const {
      c3_assert(is_id_command());
      return tm_cmd_id;
    }
    CommandReader* get_command() const {
      c3_assert(!is_id_command());
      return tm_cmd_cr;
    }
    PayloadHashObject* get_object() const { return tm_pho; }
    c3_uint_t get_capacity() const {
      c3_assert(is_id_command());
      return tm_capacity;
    }
  };
  /// Type of the queue used for sending messages *to* the tag manager
  typedef MessageQueue<TagMessage> TagQueue;

  TagQueue            ts_queue;      // queue with messages to tag manager
  PayloadObjectStore* ts_page_store; // page store reference for posting "unlink" to its queues
  TagObject*          ts_untagged;   // special tag linking all pages not marked with user tags
  bool                ts_quitting;   // `true` if tag manager thread has received "quit" request

  /// Conditions for selecting an object
  enum tag_select_condition_t {
    TSC_INVALID = 0, // invalid mode (placeholder)
    TSC_ALWAYS,      // select unconditionally (still has to be tagged though)
    TSC_MATCH,       // select if specified number of tags matches those of page object
    TSC_NOT_MATCH    // select if specified number of tags does *not* match those of the object
  };

  /// Structure used to pass extra info to callbacks implementing conditional unlinking
  struct tag_unlink_info_t {
    const TagStore&              tui_tag_store; // reference to the store issuing the request
    TagObject** const            tui_tags;      // array of pointers to tags to compare against
    const c3_uint_t              tui_ntags;     // number of tags in the tag pointer array
    const tag_select_condition_t tui_condition; // match condition

    tag_unlink_info_t(const TagStore& ts, TagObject** tags, c3_uint_t ntags, tag_select_condition_t condition):
      tui_tag_store(ts), tui_tags(tags), tui_ntags(ntags), tui_condition(condition) {
    }
  };

  /// Structure used to pass extra info to callbacks implementing conditional object ID listing
  struct tag_object_info_t {
    PayloadListChunkBuilder&     toi_list;      // list to which object IDs will be added
    TagObject** const            toi_tags;      // array of pointers to tags to compare against
    const c3_uint_t              toi_ntags;     // number of tags in the tag pointer array
    const tag_select_condition_t toi_condition; // match condition

    tag_object_info_t(PayloadListChunkBuilder& list, TagObject** tags, c3_uint_t ntags,
      tag_select_condition_t condition):
      toi_list(list), toi_tags(tags), toi_ntags(ntags), toi_condition(condition) {
    }
  };

  PayloadObjectStore& get_page_store() const {
    c3_assert(ts_page_store);
    return *ts_page_store;
  }

  TagObject* find_tag(const char* name, c3_ushort_t nlen) const;
  TagObject* create_tag(c3_hash_t hash, const char* name, c3_ushort_t nlen, bool untagged = false) const;
  TagObject* find_create_tag(const char* name, c3_ushort_t nlen) const;
  TagObject* extract_tag_names(ListChunk& list, TagObject** tags, c3_uint_t& ntags,
    bool& format_is_ok, bool& all_tags_found) const;
  void dispose_tag(TagObject* to) const;
  void unlink_object_tags(PageObject* po) const;
  void unlink_object(PayloadHashObject* pho) const;
  static bool tag_enum_callback(void* context, HashObject* ho);
  static bool object_cleanup_unlink_callback(void* context, HashObject* ho);
  static bool object_unlink_callback(void* context, HashObject* ho);
  static bool object_enum_callback(void* context, HashObject* ho);
  bool unlink_objects(TagObject** tags, c3_uint_t ntags, tag_select_condition_t condition) const;
  bool unlink_all_objects() const;
  bool enum_objects(PayloadListChunkBuilder& list, TagObject** tags, c3_uint_t ntags,
    tag_select_condition_t condition) const;
  bool enum_all_objects(PayloadListChunkBuilder& list) const;
  static void add_dummy_references(TagObject** tags, c3_uint_t ntags);
  void remove_dummy_references(TagObject** tags, c3_uint_t ntags) const;

  void process_save_command(CommandReader& cr, PayloadHashObject* pho);
  void process_remove_command(PayloadHashObject* pho);
  void process_clean_command(CommandReader& cr);
  void process_getids_command(CommandReader& cr);
  void process_gettags_command(CommandReader& cr);
  void process_getmatchingids_command(c3_byte_t cmd, CommandReader& cr);
  void process_getmetadatas_command(CommandReader& cr, PayloadHashObject* pho);
  void process_id_message(TagMessage &msg);
  void process_command_message(TagMessage& msg);

  void enter_quit_state() C3_FUNC_COLD;
  void allocate_tag_store() C3_FUNC_COLD;
  void dispose_tag_store() C3_FUNC_COLD;

public:
  TagStore() noexcept C3_FUNC_COLD;
  TagStore(const TagStore&) = delete;
  TagStore(TagStore&&) = delete;
  ~TagStore() override C3_FUNC_COLD { dispose_tag_store(); }

  TagStore& operator=(const TagStore&) = delete;
  TagStore& operator=(TagStore&&) = delete;

  void configure(ResponseObjectConsumer* consumer, Optimizer* optimizer, PayloadObjectStore* page_store) C3_FUNC_COLD {
    c3_assert(page_store && ts_page_store == nullptr);
    set_consumer(consumer);
    set_optimizer(optimizer);
    ts_page_store = page_store;
  }

  void allocate() C3_FUNC_COLD {
    allocate_tag_store();
  }

  c3_uint_t get_queue_capacity() C3LM_OFF(const) C3_FUNC_COLD { return ts_queue.get_capacity(); }
  c3_uint_t get_max_queue_capacity() C3LM_OFF(const) C3_FUNC_COLD { return ts_queue.get_max_capacity(); }

  bool post_unlink_message(PayloadHashObject* pho) {
    return ts_queue.put(TagMessage(TC_UNLINK_OBJECT, pho));
  }
  bool post_capacity_change_message(c3_uint_t capacity) C3_FUNC_COLD {
    return ts_queue.put(TagMessage(TC_CAPACITY_CHANGE, capacity));
  }
  bool post_max_capacity_change_message(c3_uint_t max_capacity) C3_FUNC_COLD {
    return ts_queue.put(TagMessage(TC_MAX_CAPACITY_CHANGE, max_capacity));
  }
  bool post_command_message(CommandReader* cr, PayloadHashObject* pho = nullptr) {
    return ts_queue.put(TagMessage(cr, pho));
  }
  bool post_quit_message() C3_FUNC_COLD {
    return ts_queue.put(TagMessage(TC_QUIT));
  }

  static void thread_proc(c3_uint_t id, ThreadArgument arg);
};

}

#endif // _HT_TAG_MANAGER_H
