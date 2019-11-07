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
#include "ht_tag_manager.h"
#include "pl_socket_pipelines.h"

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// Store
///////////////////////////////////////////////////////////////////////////////

Store::Store(const char* name, domain_t domain):
  s_name(name), s_memory(Memory::get_memory_object(domain)) {
  set_fill_factor(1.5f);
  s_index_shift = 0;
}

void Store::set_index_shift(c3_uint_t ntables) {
  assert(is_power_of_2(ntables));
  s_index_shift = 0;
  while ((ntables & (1 << s_index_shift)) == 0) {
    s_index_shift++;
  }
}

///////////////////////////////////////////////////////////////////////////////
// HashTable
///////////////////////////////////////////////////////////////////////////////

HashTable::HashTable(Store& store, c3_uint_t init_capacity): ht_store(store) {
  c3_uint_t nbuckets = (c3_uint_t)(init_capacity / store.get_fill_factor());
  if (nbuckets < MIN_NUM_BUCKETS) {
    nbuckets = MIN_NUM_BUCKETS;
  }
  ht_nbuckets = get_next_power_of_2(nbuckets);
  if (ht_nbuckets == 0) {
    ht_nbuckets = MAX_NUM_BUCKETS;
  }
  allocate_buckets();
  ht_first = nullptr;
  ht_count.store(0, std::memory_order_relaxed);
}

void HashTable::allocate_buckets() {
  c3_assert(ht_buckets == nullptr);
  ht_buckets = (HashObject**) ht_store.get_memory_object().calloc(ht_nbuckets, sizeof(HashObject*));
}

void HashTable::free_buckets() {
  c3_assert(ht_buckets);
  ht_store.get_memory_object().free(ht_buckets, ht_nbuckets * sizeof(HashObject*));
  ht_buckets = nullptr;
}

bool HashTable::resize_table() {
  if (ht_nbuckets < MAX_NUM_BUCKETS) {
    free_buckets();
    ht_nbuckets <<= 1;
    allocate_buckets();
    HashObject* ho = ht_first;
    while (ho != nullptr) {
      ho->ho_ht_prev = nullptr;
      c3_uint_t index = get_bucket_index(ho->ho_hash);
      if ((ho->ho_ht_next = ht_buckets[index]) != nullptr) {
        ho->ho_ht_next->ho_ht_prev = ho;
      }
      ht_buckets[index] = ho;
      ho = ho->ho_next;
    }
    return true;
  }
  return false;
}

HashObject* HashTable::find(c3_hash_t hash, const char* name, c3_ushort_t len) const {
  assert(hash != INVALID_HASH_VALUE && name && len);
  c3_uint_t index = get_bucket_index(hash);
  HashObject* ho = ht_buckets[index];
  while (ho != nullptr) {
    if (ho->ho_hash == hash && ho->ho_nlength == len && std::memcmp(ho->get_name(), name, len) == 0) {
      return ho;
    }
    ho = ho->ho_ht_next;
  }
  return nullptr;
}

bool HashTable::add(HashObject* ho) {
  // resize the table if needed
  bool table_resized = false;
  if (ht_count.load(std::memory_order_relaxed) >= (c3_uint_t)(ht_nbuckets * ht_store.get_fill_factor())) {
    table_resized = resize_table();
  }

  // link the object into the global chain
  ho->ho_prev = nullptr;
  if ((ho->ho_next = ht_first) != nullptr) {
    ho->ho_next->ho_prev = ho;
  }
  ht_first = ho;

  // link the object into the bucket chain
  c3_uint_t index = get_bucket_index(ho->ho_hash);
  ho->ho_ht_prev = nullptr;
  if ((ho->ho_ht_next = ht_buckets[index]) != nullptr) {
    ho->ho_ht_next->ho_ht_prev = ho;
  }
  ht_buckets[index] = ho;

  // increment table object count (more efficient than atomic overload for "++")
  ht_count.fetch_add(1, std::memory_order_relaxed);
  return table_resized;
}

void HashTable::remove(HashObject* ho) {
  // unlink from the global chain
  if (ho->ho_prev != nullptr) {
    assert(ho->ho_prev->ho_next == ho);
    ho->ho_prev->ho_next = ho->ho_next;
  } else {
    assert(ht_first == ho);
    ht_first = ho->ho_next;
  }
  if (ho->ho_next != nullptr) {
    assert(ho->ho_next->ho_prev == ho);
    ho->ho_next->ho_prev = ho->ho_prev;
  }
  #ifdef C3_SAFE
  ho->ho_prev = ho->ho_next = nullptr;
  #endif

  // unlink from the bucket chain
  if (ho->ho_ht_prev != nullptr) {
    assert(ho->ho_ht_prev->ho_ht_next == ho);
    ho->ho_ht_prev->ho_ht_next = ho->ho_ht_next;
  } else {
    c3_uint_t index = get_bucket_index(ho->ho_hash);
    assert(ht_buckets[index] == ho);
    ht_buckets[index] = ho->ho_ht_next;
  }
  if (ho->ho_ht_next != nullptr) {
    assert(ho->ho_ht_next->ho_ht_prev == ho);
    ho->ho_ht_next->ho_ht_prev = ho->ho_ht_prev;
  }
  #ifdef C3_SAFE
  ho->ho_ht_prev = ho->ho_ht_next = nullptr;
  #endif

  // decrement table object count (more efficient than atomic overload for "--")
  c3_assert_def(c3_uint_t prev_count) ht_count.fetch_sub(1, std::memory_order_relaxed);
  c3_assert(prev_count);
}

bool HashTable::enumerate(void* context, object_callback_t callback) const {
  HashObject* ho = ht_first;
  while (ho != nullptr) {
    if (!callback(context, ho)) {
      return false;
    }
    ho = ho->ho_next;
  }
  return true;
}

void HashTable::dispose() {
  HashObject* ho = ht_first;
  while (ho != nullptr) {
    HashObject* next = ho->ho_next;
    if (ho->is_locked()) {
      ht_store.log(LL_WARNING, "%s: skipping object '%.*s' disposal because it is locked",
        ht_store.get_name(), ho->get_name_length(), ho->get_name());
    } else if (ho->some_flags_are_set(HOF_LINKED_BY_OPTIMIZER | HOF_LINKED_BY_TM)) {
      /*
       * These flags should have been cleared during cleanup procedure: disposing a store (and thus its
       * hash tables) is the very last step of the cleanup. If we are here, it means that some server
       * subsystem reacted to "force quit" request and abandoned normal cleanup sequence.
       *
       * Not knowing what exactly had happened, we just log this object and skip freeing up its memory,
       * which will result in memory manager reporting a "leak", but that's a lesser of evils here...
       */
      char flags_state[HashObject::FLAGS_STATE_BUFF_LENGTH];
      ht_store.log(LL_WARNING, "%s: skipping object '%.*s' disposal because it is linked [%s]",
        ht_store.get_name(), ho->get_name_length(), ho->get_name(),
        ho->get_flags_state(flags_state, sizeof flags_state));
    } else if (ho->flags_are_set(HOF_PAYLOAD) && ((PayloadHashObject*) ho)->has_readers()) {
      ht_store.log(LL_WARNING, "%s: skipping object '%.*s' disposal because it has readers",
        ht_store.get_name(), ho->get_name_length(), ho->get_name());
    } else {
      // dispose() will check all this...
      ho->set_flags(HOF_BEING_DELETED | HOF_DELETED);
      ho->ho_prev = nullptr;
      ho->ho_next = nullptr;
      ho->ho_ht_prev = nullptr;
      ho->ho_ht_next = nullptr;
      HashObject::dispose(ho);
    }
    ho = next;
  }
  ht_first = nullptr;
  free_buckets();
  ht_nbuckets = 0;
  ht_count.store(0, std::memory_order_relaxed);
}

///////////////////////////////////////////////////////////////////////////////
// ObjectStore
///////////////////////////////////////////////////////////////////////////////

ObjectStore::ObjectStore(const char* name, domain_t domain, c3_uint_t default_ntables,
  c3_uint_t default_capacity):
  Store(name, domain) {
  c3_assert(default_ntables <= MAX_NUM_TABLES_PER_STORE && is_power_of_2(default_ntables));
  os_consumer = nullptr;
  os_optimizer = nullptr;
  os_tables = nullptr;
  os_ntables = default_ntables;
  set_index_shift(os_ntables);
  os_capacity = default_capacity;
}

void ObjectStore::init_object_store() {
  c3_assert(os_tables == nullptr && os_ntables > 0);
  os_tables = (HashTable*) get_memory_object().calloc(os_ntables, sizeof(HashTable));
  for (c3_uint_t i = 0; i < os_ntables; ++i) {
    new (os_tables + i) HashTable(*this, os_capacity);
  }
}

void ObjectStore::dispose_object_store() {
  if (os_tables != nullptr) {
    for (c3_uint_t i = 0; i < os_ntables; ++i) {
      os_tables[i].dispose(); // this is equivalent to calling table dtor
    }
    get_memory_object().free(os_tables, os_ntables * sizeof(HashTable));
    os_tables = nullptr;
  }
}

bool ObjectStore::set_num_tables(c3_uint_t ntables) {
  assert(ntables >= 1 && ntables <= MAX_NUM_TABLES_PER_STORE);
  if (os_tables == nullptr) {
    c3_assert(is_power_of_2(ntables));
    os_ntables = ntables;
    set_index_shift(os_ntables);
    return true;
  } else {
    log(LL_ERROR, "%s: number of tables per store cannot be changed after server startup", get_name());
    return false;
  }
}

c3_uint_t ObjectStore::get_num_elements() const {
  c3_uint_t num = 0;
  c3_assert(os_tables != nullptr);
  for (c3_uint_t i = 0; i < os_ntables; ++i) {
    num += os_tables[i].get_num_elements();
  }
  return num;
}

void ObjectStore::set_table_capacity(c3_uint_t capacity) {
  // configuration manager should have blocked this request
  c3_assert(os_tables == nullptr);
  os_capacity = capacity;
}

bool ObjectStore::enumerate_all(void* context, object_callback_t callback) const {
  if (is_initialized()) {
    for (c3_uint_t i = 0; i < get_num_tables(); i++) {
      if (!table(i).enumerate(context, callback)) {
        return false;
      }
    }
  }
  return true;
}

c3_uint_t ObjectStore::get_num_deleted_objects() const {
  // only payload object stores have specialized queues that keep pointer to object marked as "deleted"
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
// PayloadObjectStore
///////////////////////////////////////////////////////////////////////////////

PayloadObjectStore::PayloadObjectStore(const char* name, domain_t domain, c3_uint_t ntables, c3_uint_t table_capacity,
    c3_uint_t queue_capacity, c3_uint_t max_queue_capacity):
  ObjectStore(name, domain, ntables, table_capacity) {
  pos_queues = nullptr;
  pos_mutexes = nullptr;
  pos_dispose_if_resized.store(DEFAULT_DISPOSE_COUNT_IF_RESIZED, std::memory_order_relaxed);
  pos_dispose_if_not_resized.store(DEFAULT_DISPOSE_COUNT_IF_NOT_RESIZED, std::memory_order_relaxed);
  pos_init_capacity = queue_capacity;
  pos_init_max_capacity = max_queue_capacity;
  pos_num_deleted_objects.store(0, std::memory_order_relaxed);
}

void PayloadObjectStore::init_payload_object_store() {
  c3_assert(pos_queues == nullptr && pos_mutexes == nullptr);
  init_object_store();
  pos_queues = (PayloadObjectQueue*) get_memory_object().calloc(get_num_tables(), sizeof(PayloadObjectQueue));
  pos_mutexes = (DynamicMutex*) get_memory_object().calloc(get_num_tables(), sizeof(DynamicMutex));
  for (c3_uint_t i = 0; i < get_num_tables(); ++i) {
    new (pos_queues + i) PayloadObjectQueue(get_domain(), HO_STORE,
      pos_init_capacity, pos_init_max_capacity, (c3_byte_t) i);
    new (pos_mutexes + i) DynamicMutex(get_domain(), HO_STORE, (c3_byte_t) i);
  }
}

void PayloadObjectStore::dispose_payload_object_store() {
  c3_uint_t num = get_num_tables();
  /*
   * The following call disposes all hash tables along with the objects contained in them.
   *
   * The queues of deleted objects (disposed below) then end up containing pointers to the
   * objects that had already been disposed, so we can simply delete the queues without
   * processing the pointers contained in them.
   */
  dispose_object_store();
  if (pos_queues != nullptr) {
    for (c3_uint_t i = 0; i < num; ++i) {
      pos_queues[i].dispose(); // this is equivalent to calling dtor
    }
    get_memory_object().free(pos_queues, num * sizeof(PayloadObjectQueue));
    pos_queues = nullptr;
  }
  if (pos_mutexes != nullptr) {
    #if C3_SAFEST
    for (c3_uint_t i = 0; i < num; ++i) {
      DynamicMutex& dm = pos_mutexes[i];
      c3_assert(!dm.is_locked_exclusively() && !dm.get_num_readers());
    }
    #endif // C3_SAFEST
    get_memory_object().free(pos_mutexes, num * sizeof(DynamicMutex));
    pos_mutexes = nullptr;
  }
}

void PayloadObjectStore::set_queue_capacity(c3_uint_t capacity) {
  if (pos_queues != nullptr) {
    for (c3_uint_t i = 0; i < get_num_tables(); ++i) {
      pos_queues[i].set_capacity(capacity);
    }
  } else {
    pos_init_capacity = capacity;
  }
}

void PayloadObjectStore::set_max_queue_capacity(c3_uint_t max_capacity) {
  if (pos_queues != nullptr) {
    for (c3_uint_t i = 0; i < get_num_tables(); ++i) {
      pos_queues[i].store_and_set_max_capacity(max_capacity);
    }
  } else {
    pos_init_max_capacity = max_capacity;
  }
}

c3_uint_t PayloadObjectStore::reduce_queue_capacity() {
  c3_uint_t num_reduced = 0;
  if (pos_queues != nullptr) {
    for (c3_uint_t i = 0; i < get_num_tables(); ++i) {
      if (pos_queues[i].reduce_capacity()) {
        num_reduced++;
      }
    }
  }
  return num_reduced;
}

void PayloadObjectStore::set_unlinking_quotas(c3_uint_t while_rebuilding, c3_uint_t while_not_rebuilding) {
  pos_dispose_if_resized.store(while_rebuilding, std::memory_order_relaxed);
  pos_dispose_if_not_resized.store(while_not_rebuilding, std::memory_order_relaxed);
}

bool PayloadObjectStore::post_unlink_message(PayloadHashObject* pho) {
  c3_assert(pho && pho->get_user_agent() < UA_NUMBER_OF_ELEMENTS && pho->flags_are_set(HOF_BEING_DELETED) &&
    pho->flags_are_clear(HOF_LINKED_BY_OPTIMIZER | HOF_LINKED_BY_TM | HOF_DELETED));

  // the following renders the object unusable for any store
  pho->reset_user_agent();
  pho->set_flags(HOF_DELETED);
  PayloadObjectQueue& queue = get_queue(get_table_index(pho));
  /*
   * The call could have been made by optimizer processing the "delete object" message; if that's the case,
   * the optimizer cannot afford to wait on the queue of deleted objects, because the optimizer itself could
   * have been triggered by a call from a thread that is doing bulk removal, and that thread
   * -- has a read (shared) lock on the table to which the queue of deleted object belong,
   * -- posts more and more "delete object" messages to the optimizer.
   *
   * So if optimizer itself waits, its queue may hit capacity limit, the thread doing bulk removal will start
   * waiting on optimizer's queue (so it would never release shared lock on the table), and we'd have a deadlock.
   *
   * Therefore, in order to avoid deadlocks, we put messages to the queue of deleted objects using `put_always()`.
   */
  if (queue.put_always(PayloadObjectMessage(pho))) {
    increment_num_deleted_objects();
    return true;
  }
  return false;
}

bool PayloadObjectStore::lock_enumerate_all(void* context, object_callback_t callback) const {
  if (is_initialized()) {
    c3_assert(pos_mutexes);
    for (c3_uint_t i = 0; i < get_num_tables(); i++) {
      DynamicMutexLock lock(pos_mutexes[i]);
      if (!table(i).enumerate(context, callback)) {
        return false;
      }
    }
  }
  return true;
}

c3_uint_t PayloadObjectStore::get_num_deleted_objects() const {
  return pos_num_deleted_objects.load(std::memory_order_relaxed);
}

void PayloadObjectStore::dispose_deleted_objects(c3_uint_t index, c3_uint_t num) {
  if (num > 0) {
    HashTable& the_table = table(index);
    PayloadObjectQueue& the_queue = get_queue(index);
    do {
      PayloadObjectMessage msg = the_queue.try_get();
      if (msg.is_valid()) {
        PayloadHashObject* pho = msg.get();
        c3_assert(pho->flags_are_clear(HOF_LINKED_BY_TM | HOF_LINKED_BY_OPTIMIZER) &&
          pho->flags_are_set(HOF_BEING_DELETED | HOF_DELETED) && !pho->is_locked());
        if (pho->has_readers()) {
          /*
           * Defer disposal until later.
           *
           * Since this method is the only one pulling messages from the queue of deleted objects, we
           * use queue method that is guaranteed to work even if the queue is not only full, but also
           * reached its max capacity (in which case max capacity would be automatically doubled).
           */
          the_queue.put_always(PayloadObjectMessage(pho));
        } else {
          the_table.remove(pho);
          HashObject::dispose(pho);
          decrement_num_deleted_objects();
        }
      } else {
        break;
      }
    } while (--num != 0);
  }
}

///////////////////////////////////////////////////////////////////////////////
// TableLock
///////////////////////////////////////////////////////////////////////////////

TableLock::TableLock(PayloadObjectStore& store, c3_hash_t hash, bool exclusive):
  tl_store(store), tl_index(store.get_table_index(hash)) {
  tl_table_resized = false;
  tl_queue_procesed = false;
  DynamicMutex& mutex = get_mutex();
  if (exclusive) {
    mutex.lock_exclusive();
  } else {
    mutex.lock_shared();
  }
}

TableLock::~TableLock() {
  DynamicMutex& mutex = get_mutex();
  if (mutex.is_locked_exclusively()) {
    process_deleted_objects();
    mutex.unlock_exclusive();
  } else {
    mutex.unlock_shared();
  }
}

void TableLock::process_deleted_objects() {
  if (!tl_queue_procesed) {
    c3_uint_t num = tl_table_resized?
      tl_store.pos_dispose_if_resized.load(std::memory_order_relaxed):
      tl_store.pos_dispose_if_not_resized.load(std::memory_order_relaxed);
    tl_store.dispose_deleted_objects(tl_index, num);
    tl_queue_procesed = true;
  }
}

bool TableLock::downgrade_lock(bool resized) {
  table_was_resized(resized);
  process_deleted_objects();
  DynamicMutex& mutex = get_mutex();
  c3_assert(mutex.is_locked_exclusively());
  return mutex.downgrade_lock();
}

bool TableLock::upgrade_lock() const {
  DynamicMutex& mutex = get_mutex();
  c3_assert(!mutex.is_locked_exclusively());
  return mutex.upgrade_lock();
}

}
