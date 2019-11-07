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
 * Hash tables and structures that contain them.
 */
#ifndef _HT_STORES_H
#define _HT_STORES_H

#include "c3lib/c3lib.h"
#include "ht_objects.h"
#include "mt_message_queue.h"
#include "mt_mutexes.h"

#include <cstdarg>

namespace CyberCache {

/// Function pointer used for enumeration; if `false` is returned, enumeration should stop
typedef bool object_callback_t(void* context, HashObject* ho);

///////////////////////////////////////////////////////////////////////////////
// Store
///////////////////////////////////////////////////////////////////////////////

/// Base class for all object stores
class Store: public virtual AbstractLogger {
  static constexpr float MIN_FACTOR = 0.5f;
  static constexpr float MAX_FACTOR = 10.0f;

  const char* const s_name;        // human-readable name of the store
  Memory&           s_memory;      // reference to a `Memory` object
  atomic_float_t    s_fill_factor; // fill ratio that contained hash tables should maintain
  c3_byte_t         s_index_shift; // how many bits in object hashes are used as hash table indices

protected:
  Store(const char* name, domain_t domain) C3_FUNC_COLD;
  virtual ~Store() C3_FUNC_COLD = default;
  void set_index_shift(c3_uint_t ntables) C3_FUNC_COLD;

public:
  Store(const Store&) = delete;
  Store(Store&&) = delete;

  Store& operator=(const Store&) = delete;
  Store& operator=(Store&&) = delete;

  const char* get_name() const { return s_name; }
  domain_t get_domain() const { return s_memory.get_domain(); }
  Memory& get_memory_object() const { return s_memory; }

  static constexpr float get_min_fill_factor() { return MIN_FACTOR; }
  static constexpr float get_max_fill_factor() { return MAX_FACTOR; }
  float get_fill_factor() const { return s_fill_factor.load(std::memory_order_relaxed); }
  void set_fill_factor(float factor) C3_FUNC_COLD {
    c3_assert(factor >= MIN_FACTOR && factor <= MAX_FACTOR);
    s_fill_factor.store(factor, std::memory_order_relaxed);
  }

  c3_uint_t get_base_index(c3_hash_t hash) const {
    c3_assert(hash != INVALID_HASH_VALUE);
    return (c3_uint_t)(hash >> s_index_shift);
  }
  c3_uint_t get_base_index(const HashObject* ho) const {
    c3_assert(ho);
    return get_base_index(ho->get_hash_code());
  }
};

///////////////////////////////////////////////////////////////////////////////
// HashTable
///////////////////////////////////////////////////////////////////////////////

/**
 * Container that stores hash objects.
 *
 * Its capacity is defined as number of buckets times fill factor; when count of elements contained in
 * the table is about to exceed table capacity (number of buckets times fill factor), the number of
 * buckets is doubled, and the table gets re-built.
 */
class HashTable {
  static constexpr c3_uint_t MIN_NUM_BUCKETS = 64;
  static constexpr c3_uint_t MAX_NUM_BUCKETS = 1u << 31;

  Store&           ht_store;    // reference to the container
  HashObject**     ht_buckets;  // array of buckets
  HashObject*      ht_first;    // first object in the chain of all objects in this table
  c3_uint_t        ht_nbuckets; // current number of buckets in the table (size of bucket array)
  std::atomic_uint ht_count;    // total number of objects in the table

  c3_uint_t get_bucket_index(const HashObject* ho) const {
    return ht_store.get_base_index(ho) & (ht_nbuckets - 1);
  }
  c3_uint_t get_bucket_index(c3_hash_t hash) const {
    return ht_store.get_base_index(hash) & (ht_nbuckets - 1);
  }
  void allocate_buckets();
  void free_buckets();
  bool resize_table();

public:
  HashTable(Store& store, c3_uint_t init_capacity) C3_FUNC_COLD;
  HashTable(const HashTable&) = delete;
  HashTable(HashTable&&) = delete;
  ~HashTable() C3_FUNC_COLD { dispose(); }

  HashTable& operator=(const HashTable&) = delete;
  HashTable& operator=(HashTable&&) = delete;

  c3_uint_t get_num_elements() const { return ht_count.load(std::memory_order_relaxed); }
  HashObject* find(c3_hash_t hash, const char* name, c3_ushort_t len) const;
  bool add(HashObject* ho);
  void remove(HashObject* ho);
  bool enumerate(void* context, object_callback_t callback) const;
  void dispose() C3_FUNC_COLD;
};

///////////////////////////////////////////////////////////////////////////////
// ObjectStore
///////////////////////////////////////////////////////////////////////////////

class ResponseObjectConsumer;
class Optimizer;
class PayloadListChunkBuilder;

/// Base class for stores that do not require locking individual hash tables
class ObjectStore: public Store {
  friend class TableLock;
  ResponseObjectConsumer* os_consumer;  // where to post objects with response data
  Optimizer*              os_optimizer; // optimizer of this object store
  HashTable*              os_tables;    // array of hash table objects, *not* pointers
  c3_uint_t               os_ntables;   // number of tables in the array; can't change after initialization
  c3_uint_t               os_capacity;  // initial capacity of each individual table

protected:
  ObjectStore(const char* name, domain_t domain, c3_uint_t default_ntables,
    c3_uint_t default_capacity) C3_FUNC_COLD;
  ~ObjectStore() override C3_FUNC_COLD { dispose_object_store(); }

  void init_object_store() C3_FUNC_COLD;
  void dispose_object_store() C3_FUNC_COLD;

  ResponseObjectConsumer& get_consumer() const {
    c3_assert(os_consumer);
    return *os_consumer;
  }
  void set_consumer(ResponseObjectConsumer* consumer) C3_FUNC_COLD {
    c3_assert(consumer && os_consumer == nullptr);
    os_consumer = consumer;
  }
  Optimizer& get_optimizer() const {
    c3_assert(os_optimizer);
    return *os_optimizer;
  }
  void set_optimizer(Optimizer* optimizer) C3_FUNC_COLD {
    c3_assert(optimizer && os_optimizer == nullptr);
    os_optimizer = optimizer;
  }

  c3_uint_t get_table_index(c3_hash_t hash) const {
    c3_assert(hash != INVALID_HASH_VALUE && os_ntables);
    return (c3_uint_t) hash & (os_ntables - 1);
  }
  c3_uint_t get_table_index(const HashObject* ho) const {
    c3_assert(ho);
    return get_table_index(ho->get_hash_code());
  }
  HashTable& table(c3_uint_t i) const {
    c3_assert(os_tables && i < os_ntables);
    return os_tables[i];
  }

public:
  ObjectStore(const ObjectStore&) = delete;
  ObjectStore(ObjectStore&&) = delete;

  ObjectStore& operator=(const ObjectStore&) = delete;
  ObjectStore& operator=(ObjectStore&&) = delete;

  bool is_initialized() const { return os_tables != nullptr; }
  c3_uint_t get_num_tables() const { return os_ntables; }
  bool set_num_tables(c3_uint_t ntables) C3_FUNC_COLD;

  c3_uint_t get_num_elements() const;
  void set_table_capacity(c3_uint_t capacity) C3_FUNC_COLD;

  bool enumerate_all(void* context, object_callback_t callback) const;

  virtual c3_uint_t get_num_deleted_objects() const;
};

///////////////////////////////////////////////////////////////////////////////
// PayloadObjectStore
///////////////////////////////////////////////////////////////////////////////

/// Base class for stores that implement concurrent access to their hash tables
class PayloadObjectStore: public ObjectStore {
  friend class TableLock;
  static constexpr c3_uint_t DEFAULT_DISPOSE_COUNT_IF_RESIZED = 4;
  static constexpr c3_uint_t DEFAULT_DISPOSE_COUNT_IF_NOT_RESIZED = 64;

  typedef Pointer<PayloadHashObject> PayloadObjectMessage;
  typedef CriticalMessageQueue<PayloadObjectMessage> PayloadObjectQueue;
  PayloadObjectQueue* pos_queues;                 // array of queue objects, not pointers
  DynamicMutex*       pos_mutexes;                // array of dynamic mutex objects, not pointers
  std::atomic_uint    pos_dispose_if_resized;     // how many objects to delete if table was rebuilt
  std::atomic_uint    pos_dispose_if_not_resized; // --"-- if table was not rebuilt during add()
  c3_uint_t           pos_init_capacity;          // initial queue capacity
  c3_uint_t           pos_init_max_capacity;      // initial max queue capacity
  std::atomic_uint    pos_num_deleted_objects;    // total number of objects in "queues of deleted objects"

  PayloadObjectQueue& get_queue(c3_uint_t index) const {
    c3_assert(pos_queues && index < get_num_tables());
    return pos_queues[index];
  }

  void increment_num_deleted_objects() {
    pos_num_deleted_objects.fetch_add(1, std::memory_order_relaxed);
  }
  void decrement_num_deleted_objects() {
    pos_num_deleted_objects.fetch_sub(1, std::memory_order_relaxed);
  }

protected:
  PayloadObjectStore(const char* name, domain_t domain, c3_uint_t ntables, c3_uint_t table_capacity,
    c3_uint_t queue_capacity, c3_uint_t max_queue_capacity) C3_FUNC_COLD;
  ~PayloadObjectStore() override C3_FUNC_COLD { dispose_payload_object_store(); }

  void init_payload_object_store() C3_FUNC_COLD;
  void dispose_payload_object_store() C3_FUNC_COLD;

public:
  PayloadObjectStore(const PayloadObjectStore&) = delete;
  PayloadObjectStore(PayloadObjectStore&&) = delete;

  PayloadObjectStore& operator=(const PayloadObjectStore&) = delete;
  PayloadObjectStore& operator=(PayloadObjectStore&&) = delete;

  virtual FileCommandWriter* create_file_command_writer(PayloadHashObject* pho, c3_timestamp_t time) = 0;

  void set_queue_capacity(c3_uint_t capacity) C3_FUNC_COLD;
  void set_max_queue_capacity(c3_uint_t max_capacity) C3_FUNC_COLD;
  c3_uint_t reduce_queue_capacity();

  // configuration of removal policy
  void get_unlinking_quotas(c3_uint_t& while_rebuilding, c3_uint_t& while_not_rebuilding) {
    while_rebuilding = pos_dispose_if_resized.load(std::memory_order_relaxed);
    while_not_rebuilding = pos_dispose_if_not_resized.load(std::memory_order_relaxed);
  }
  void set_unlinking_quotas(c3_uint_t while_rebuilding, c3_uint_t while_not_rebuilding) C3_FUNC_COLD;

  bool post_unlink_message(PayloadHashObject* pho);
  bool lock_enumerate_all(void* context, object_callback_t callback) const;

  c3_uint_t get_num_deleted_objects() const override;
  void dispose_deleted_objects(c3_uint_t index, c3_uint_t num);
};

///////////////////////////////////////////////////////////////////////////////
// TableLock
///////////////////////////////////////////////////////////////////////////////

/// Helper class for locking tables while manupulating them
class TableLock {
  PayloadObjectStore& tl_store;          // store on which table lock operates
  const c3_uint_t     tl_index;          // index of the table being locked
  bool                tl_table_resized;  // `true` if table had been resized while adding an object
  bool                tl_queue_procesed; // `true` if queue of deleted objects had already been processed

  DynamicMutex& get_mutex() const {
    c3_assert(tl_store.pos_mutexes && tl_index < tl_store.os_ntables);
    return tl_store.pos_mutexes[tl_index];
  }
  void process_deleted_objects();

public:
  TableLock(PayloadObjectStore& store, c3_hash_t hash, bool exclusive = false);
  TableLock(const TableLock&) = delete;
  TableLock(TableLock&&) = delete;
  ~TableLock();

  TableLock& operator=(const TableLock&) = delete;
  TableLock& operator=(TableLock&&) = delete;

  /*
   * This method is to be used in cases when the lock is never downgraded, so dtor will have to somehow
   * know whether the table was re-built (otherwise, if the lock *was* downgraded, respective method
   * would get this information from its argument).
   */
  void table_was_resized(bool resized = true) { tl_table_resized = resized; }
  HashTable& get_table() const {
    c3_assert(tl_store.os_tables && tl_index < tl_store.os_ntables);
    return tl_store.os_tables[tl_index];
  }
  bool downgrade_lock(bool resized);
  bool upgrade_lock() const;
};

}

#endif // _HT_STORES_H
