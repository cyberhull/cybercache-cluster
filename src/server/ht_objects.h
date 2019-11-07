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
 * Data structures stored in CyberCache hash tables.
 */
#ifndef _HT_OBJECTS_H
#define _HT_OBJECTS_H

#include "c3lib/c3lib.h"
#include "mt_lockable_object.h"
#include "mt_quick_semaphore.h"

#include <atomic>
#include <cstring>
#include <c3lib/c3lib.h>

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// HashObject
///////////////////////////////////////////////////////////////////////////////

/*
 * Any valid combination of these flags is non-zero:
 *
 *   Session object ==> HOF_PAYLOAD
 *   FPC object     ==> HOF_PAYLOAD + HOF_FPC
 *   Tag object     ==> HOF_FPC
 *
 * The HOF_BEING_DELETED flag is (currently) only set in the following methods:
 *
 *   TagStore::object_unlink_callback()
 *   TagStore::process_clean_command()
 *   Optimizer::process_gc_message()
 *   Optimizer::process_free_memory_message()
 *   Optimizer::run()
 *   SessionObjectStore::destroy_session_record()
 *   PageObjectStore::remove_fpc_record()
 *   HashTable::dispose()
 *
 * After that flag is set, the object can still be safely used by `ReaderWriter`-derived objects
 * that registered themselves as "readers" before the object was marked for deletion.
 */
constexpr c3_byte_t HOF_FPC                 = 0x01; // belongs to FPC domain (SESSION domain if clear)
constexpr c3_byte_t HOF_PAYLOAD             = 0x02; // is actually an instance of PayloadHashObject
constexpr c3_byte_t HOF_LINKED_BY_OPTIMIZER = 0x04; // linked into optimizer's list of objects
constexpr c3_byte_t HOF_LINKED_BY_TM        = 0x08; // linked into tag manager's list of objects
constexpr c3_byte_t HOF_BEING_OPTIMIZED     = 0x10; // object is being re-compressed by optimization thread
constexpr c3_byte_t HOF_OPTIMIZED           = 0x20; // `pho_opt_comp` is an optimal compressor for this object
constexpr c3_byte_t HOF_BEING_DELETED       = 0x40; // object is being deleted (can still be revived)
constexpr c3_byte_t HOF_DELETED             = 0x80; // object'd been put into deleted objects queue (can't be revived)
constexpr c3_byte_t HOF_TYPE_BITS           = HOF_FPC | HOF_PAYLOAD;

/// Valid hash object types
enum hash_object_type_t: c3_byte_t {
  HOT_INVALID = 0,                        // an invalid object (placeholder)
  HOT_SESSION_OBJECT = HOF_PAYLOAD,       // session data object
  HOT_TAG_OBJECT = HOF_FPC,               // an FPC tag object
  HOT_PAGE_OBJECT = HOF_PAYLOAD | HOF_FPC // FPC data object
};

/**
 * Base class for all dynamically-allocated objects that can be put into a `HashTable`.
 */
class HashObject: public LockableObject {
  friend class HashTable;
  HashObject*       ho_ht_prev; // previous object in the bucket chain, or NULL
  HashObject*       ho_ht_next; // next object in the bucket chain, or NULL
  HashObject*       ho_prev;    // previous object in the chain of objects of its type, or NULL
  HashObject*       ho_next;    // next object in the chain of objects of its type, or NULL
  const c3_hash_t   ho_hash;    // hash code for the object
  const c3_uint_t   ho_length;  // total record size, bytes
  const c3_ushort_t ho_nlength; // record name length; there is *no* terminating `\0` byte
  c3_byte_t         ho_flags;   // various flags (a combination of HO_xxx constants)
  c3_byte_t         ho_xflags;  // additional flags (currently unused; TODO: turn into `ho_type`)

protected:
  HashObject(c3_hash_t hash, c3_byte_t flags, const char* name, c3_ushort_t nlen, c3_uint_t size):
    ho_hash(hash), ho_length(size), ho_nlength(nlen) {

    // there has to be some extra data between `HashObject` and its name
    c3_assert(sizeof(HashObject) + ho_nlength < ho_length &&
      flags && name && nlen && hash != INVALID_HASH_VALUE);

    #ifdef C3_SAFE
    // just in case (these fields are onwed and will be initialized by hash table anyway)
    ho_ht_prev = ho_ht_next = ho_prev = ho_next = nullptr;
    #endif

    std::memcpy((char*) get_name(), name, ho_nlength);
    ho_flags = flags;
  }

public:
  hash_object_type_t get_type() const { return (hash_object_type_t)(ho_flags & HOF_TYPE_BITS); }
  c3_hash_t get_hash_code() const { return ho_hash; }
  c3_uint_t get_size() const { return ho_length; }
  const char* get_name() const { return ((const char*) this) + ho_length - ho_nlength; }
  c3_ushort_t get_name_length() const { return ho_nlength; }
  domain_t get_domain() const { return (ho_flags & HOF_FPC) != 0? DOMAIN_FPC: DOMAIN_SESSION; }
  Memory& get_memory_object() const { return Memory::get_memory_object(get_domain()); }

  /**
   * Size of the buffer required to hold textual descriptions of object flags. Currently, maximum
   * length of the status text (in case *all* flags are set) is 92 characters.
   */
  static constexpr size_t FLAGS_STATE_BUFF_LENGTH = 96;
  const char* get_flags_state(char* buff, size_t length) const;

  /*
   * None of the following methods use memory fences (i.e. the fields are not atomic, and special
   * load/store methods are not used to access them), which is OK provided that:
   *
   * 1) either these methods are only used by callers that have successfully locked the object, OR
   * 2) the callers do checks again once they acquired the lock (e.g. optimization thread is looking for
   *    objects that a) are not locked, b) do not have active readers, and c) do not have "deleted" flag
   *    set; once optimization thread finds such an object, it should lock it and re-check both number of
   *    readers and the "deleted" flag).
   */
  bool flags_are_set(c3_byte_t flags) const { return (ho_flags & flags) == flags; }
  bool some_flags_are_set(c3_byte_t flags) const { return (ho_flags & flags) != 0; }
  bool flags_are_clear(c3_byte_t flags) const { return (ho_flags & flags) == 0; }
  void set_flags(c3_byte_t flags) { ho_flags |= flags; }
  void clear_flags(c3_byte_t flags) { ho_flags &= ~flags; }

  /*
   * Free *all* memory occupied by any derived object. This method is called from two sites:
   *
   * 1) TableLock::process_deleted_objects(): right before the table is unlocked, this method scans
   *    queue of deleted objects of that table, and disposes those that do not have attached readers.
   *
   * 2) HashTable::dispose(): when CyberCache is shutting down, this method disposes all the object
   *    that are still linked in the hash table.
   */
  static void dispose(HashObject* ho);
};

///////////////////////////////////////////////////////////////////////////////
// PayloadHashObject
///////////////////////////////////////////////////////////////////////////////

/**
 * Base class for objects that store data, with buffers attached to them, such as
 * session data, or an FPC data chunk.
 */
class PayloadHashObject: public HashObject {
  constexpr static c3_byte_t ZERO_LENGTH_BUFFER[] = "PHO_ZeroLengthBuffer";

  c3_byte_t*          pho_buffer;        // buffer with data associated with the object
  PayloadHashObject*  pho_opt_prev;      // previous object in optimizer's list of objects, or NULL
  PayloadHashObject*  pho_opt_next;      // next object in optimizer's list of objects, or NULL
  c3_uint_t           pho_size;          // buffer size, bytes (size of the data in `pho_buffer`)
  c3_uint_t           pho_usize;         // size of uncompressed data, bytes
  c3_timestamp_t      pho_mod_time;      // last modification timestamp
  c3_timestamp_t      pho_exp_time;      // expiration timestamp
  QuickSemaphore      pho_semaphore;     // number of readers (and possibly index of waiting writer's thread)
  c3_ushort_t         pho_count;         // session writes for session object, tag records for `PageObject`
  user_agent_t        pho_opt_useragent; // user agent type
  c3_compressor_t     pho_opt_comp;      // type of compressor used on `pho_buffer` contents

protected:
  PayloadHashObject(c3_hash_t hash, c3_byte_t flags, const char* name, c3_ushort_t nlen, c3_uint_t size):
    HashObject(hash, flags, name, nlen, size) {

    c3_assert(size >= sizeof(PayloadHashObject) + nlen);

    pho_buffer = nullptr;
    pho_size = 0;
    pho_usize = 0;
    pho_count = 0;

    // just in case (these are owned and will be initialized by the optimizer anyway)
    pho_mod_time = Timer::current_timestamp();
    pho_exp_time = Timer::MAX_TIMESTAMP;
    pho_opt_prev = pho_opt_next = nullptr;
    pho_opt_useragent = UA_NUMBER_OF_ELEMENTS;
    pho_opt_comp = CT_NUMBER_OF_ELEMENTS;
  }

  // helper methods manipulating counts
  c3_uint_t get_count() const { return pho_count; }
  void set_count(c3_uint_t count) {
    c3_assert(count <= USHORT_MAX_VAL);
    pho_count = (c3_ushort_t) count;
  }
  void increment_count() {
    if (pho_count < USHORT_MAX_VAL) {
      pho_count++;
    }
  }

public:
  // metadata accessors
  c3_timestamp_t get_last_modification_time() const { return pho_mod_time; }
  void set_modification_time() { pho_mod_time = Timer::current_timestamp(); }
  c3_timestamp_t get_expiration_time() const { return pho_exp_time; }
  void set_expiration_time(c3_timestamp_t time) { pho_exp_time = time; }
  user_agent_t get_user_agent() const { return pho_opt_useragent; }
  void set_user_agent(user_agent_t user_agent) {
    assert(user_agent < UA_NUMBER_OF_ELEMENTS);
    pho_opt_useragent = user_agent;
  }
  void reset_user_agent() { pho_opt_useragent = UA_NUMBER_OF_ELEMENTS; }

  // optimization chain accessors
  PayloadHashObject* get_opt_prev() const { return pho_opt_prev; }
  void set_opt_prev(PayloadHashObject* pho) { pho_opt_prev = pho; }
  PayloadHashObject* get_opt_next() const { return pho_opt_next; }
  void set_opt_next(PayloadHashObject* pho) { pho_opt_next = pho; }

  // buffer handling
  c3_uint_t get_buffer_size() const { return pho_size; }
  c3_uint_t get_buffer_usize() const { return pho_usize; }
  c3_compressor_t get_buffer_compressor() const { return pho_opt_comp; }
  c3_byte_t* get_buffer_bytes(c3_uint_t offset, c3_uint_t size) const {
    assert(offset + size <= pho_size);
    c3_assert(pho_buffer);
    return pho_buffer + offset;
  }
  void set_buffer(c3_compressor_t compressor, c3_uint_t size, c3_uint_t usize, c3_byte_t* buffer, Memory &memory);
  c3_uint_t dispose_buffer(Memory& memory);
  void try_dispose_buffer(Memory& memory) {
    if (!has_readers()) {
      dispose_buffer(memory);
    }
  }

  // readers' handling
  bool has_readers() const { return pho_semaphore.has_readers(); }
  void wait_until_no_readers() {
    c3_assert(is_locked() && flags_are_clear(HOF_BEING_DELETED));
    pho_semaphore.wait_until_no_readers();
  }
  void register_reader() {
    c3_assert(is_locked() && flags_are_clear(HOF_BEING_DELETED) && pho_buffer && pho_usize >= pho_size);
    pho_semaphore.register_reader();
  }
  void unregister_reader() {
    c3_assert(pho_buffer && pho_usize >= pho_size);
    pho_semaphore.unregister_reader();
  }
};

///////////////////////////////////////////////////////////////////////////////
// SessionObject
///////////////////////////////////////////////////////////////////////////////

/// Possible results of attempts to acquire session lock
enum session_lock_result_t {
  SLR_SUCCESS,    // session record successfully locked
  SLR_BROKE_LOCK, // session record had been locked, but we broke existing lock for that
  SLR_DELETED     // while we were waiting for the lock, session record had been marked as deleted
};

/**
 * Object that stores session data; supports session locking.
 */
class SessionObject: public PayloadHashObject {
  static constexpr c3_uint_t DEFAULT_LOCK_WAIT_TIME = 8000; // 8 seconds
  static std::atomic_uint so_lock_wait_time; // number of milliseconds to wait for session lock

  struct Initializer {
    Initializer() noexcept { so_lock_wait_time.store(DEFAULT_LOCK_WAIT_TIME, std::memory_order_relaxed); }
  };
  static Initializer so_initializer;

  /*
   * The following fields are only accessed by methods that are executed by threads holding
   * locks on the hash object (hence no `atomic` etc.).
   */
  c3_ulong_t so_threads;    // mask of threads waiting for session lock
  c3_uint_t  so_request_id; // ID of the request currently holding session lock

public:
  static c3_uint_t calculate_size(c3_uint_t name_length) { return name_length + sizeof(SessionObject); }

  SessionObject(c3_hash_t hash, const char* name, c3_ushort_t nlen):
    PayloadHashObject(hash, HOF_PAYLOAD, name, nlen, calculate_size(nlen)) {

    so_threads = 0;
    so_request_id = 0;

    PERF_INCREMENT_DOMAIN_COUNTER(SESSION, Store_Objects_Created)
    PERF_INCREMENT_DOMAIN_COUNTER(SESSION, Store_Objects_Active)
    PERF_UPDATE_DOMAIN_RANGE(SESSION, Store_Objects_Length, get_size())
    PERF_UPDATE_DOMAIN_RANGE(SESSION, Store_Objects_Name_Length, (c3_uint_t) nlen)
  }

  c3_uint_t get_num_writes() const { return get_count(); }
  void increment_num_writes() { increment_count(); }

  static c3_uint_t get_lock_wait_time() { return so_lock_wait_time.load(std::memory_order_relaxed); }
  static void set_lock_wait_time(c3_uint_t milliseconds) {
    so_lock_wait_time.store(milliseconds, std::memory_order_relaxed);
  }

  /**
   * Locks the session: any subsequent `lock_session()` calls with different request IDs will be waiting
   * until the session is unlocked, or until waiting times outs.
   *
   * If either session lock wait time or request ID is zero, does nothing.
   *
   * Called on an object that is already locked (with hash object lock); upon return, the object remains locked.
   *
   * @param request_id Request ID for which session lock should be obtained.
   * @return Result of the attempt to acquire session lock, an `SLR_xxx` constant.
   */
  session_lock_result_t lock_session(c3_uint_t request_id);
  /**
   * If session was locked by request with specified ID, unlocks the session and wakes up one of the threads
   * waiting for session lock.
   *
   * If the session was not locked by specified request or request ID (argument) is zero, then this method does
   * not try to unlock the session.
   *
   * Called on an object that is locked (with hash object lock); upon return, always releases hash object
   * lock (whether or not the session was unlocked).
   *
   * @param request_id ID of the request.
   */
  void unlock_session(c3_uint_t request_id);
};

///////////////////////////////////////////////////////////////////////////////
// TagObject
///////////////////////////////////////////////////////////////////////////////

// bug in GCC prevents it from treating "friend class TagRef" as proper forward reference
class TagRef;

/**
 * Object that represents FPC "tag" with which individual FPC entries can be marked.
 */
class TagObject: public HashObject {
  friend class TagRef;
  TagRef*    to_first;    // first page reference object (in the chain) marked with this tag, or NULL
  c3_uint_t  to_num;      // number of page objects marked with this tag
  const bool to_untagged; // if `true`, the tag is actually a list of untagged `PageObject`s

public:
  static c3_uint_t calculate_size(c3_uint_t name_length) { return name_length + sizeof(TagObject); }

  TagObject(c3_hash_t hash, const char* name, c3_ushort_t nlen, bool untagged):
    HashObject(hash, HOF_FPC, name, nlen, calculate_size(nlen)), to_untagged(untagged) {

    PERF_INCREMENT_DOMAIN_COUNTER(GLOBAL, Store_Objects_Created)
    PERF_INCREMENT_DOMAIN_COUNTER(GLOBAL, Store_Objects_Active)
    PERF_UPDATE_DOMAIN_RANGE(GLOBAL, Store_Objects_Length, get_size())
    PERF_UPDATE_DOMAIN_RANGE(GLOBAL, Store_Objects_Name_Length, (c3_uint_t) nlen)

    to_first = nullptr;
    to_num = 0;
  }

  bool is_untagged() const { return to_untagged; }
  TagRef* get_first_ref() const { return to_first; }
  c3_uint_t get_num_marked_objects() const { return to_num; }

  /// Supposed to be used to prevent tag disposal after the last "real" reference had been removed
  void add_reference() {
    to_num++;
  }
  /// Supposed to be used to remove "dummy" references added to prevent premature tag disposal
  bool remove_reference() {
    c3_assert(to_num);
    return --to_num == 0;
  }

} __attribute__ ((__packed__));

///////////////////////////////////////////////////////////////////////////////
// TagRef
///////////////////////////////////////////////////////////////////////////////

class PageObject;

/**
 * Reference to a `TagObject` from within a `PageObject`; the latter contains an array of tag references.
 */
class TagRef {
  friend class TagObject;
  PageObject* tr_page; // Page object that contains this reference
  TagObject*  tr_tag;  // The tag with which containing object is marked,
  TagRef*     tr_prev; // Previous object in the list of the tag, or `NULL`
  TagRef*     tr_next; // Next object in the list of the tag, or `NULL`

public:
  PageObject* get_page_object() const { return tr_page; }
  TagObject* get_tag_object() const { return tr_tag; }
  TagRef* get_next_ref() const { return tr_next; }
  void link(PageObject* po, TagObject* to);
  TagObject* unlink();
};

///////////////////////////////////////////////////////////////////////////////
// PageObject
///////////////////////////////////////////////////////////////////////////////

/**
 * Object that stores full page cache data chunks that are optionally tagged.
 */
class PageObject: public PayloadHashObject {
  static c3_uint_t po_num_internal_tag_refs;

  TagRef* po_xtags;   // array with tag references in case their number exceeds max for internal tag refs
  TagRef  po_tags[1]; // array of up to `po_num_internal_tag_refs` "built-in" tag references

  c3_uint_t xrefs_size(c3_uint_t ntags) const {
    return (ntags - po_num_internal_tag_refs) * sizeof(TagRef);
  }
  void alloc_tag_xrefs(c3_uint_t ntags);
  void free_tag_xrefs();

public:
  static c3_uint_t calculate_size(c3_uint_t name_length) {
    return name_length + po_num_internal_tag_refs * sizeof(TagRef) + C3_OFFSETOF(PageObject, po_tags);
  }

  PageObject(c3_hash_t hash, const char* name, c3_ushort_t nlen);
  PageObject(const PageObject&) = delete;
  PageObject(PageObject&&) = delete;
  ~PageObject();

  PageObject& operator=(const PageObject&) = delete;
  PageObject& operator=(PageObject&&) = delete;

  static c3_uint_t get_num_internal_tag_refs() {
    return po_num_internal_tag_refs;
  }
  static void set_num_internal_tag_refs(c3_uint_t num) {
    // configuration manager should issue warning and *not* call this method if user attempts to set
    // number of internal tag references exceeding the edition-dependent limit
    c3_assert(num <= MAX_NUM_INTERNAL_TAG_REFS);
    po_num_internal_tag_refs = num;
  }
  c3_uint_t get_num_tag_refs() const { return get_count(); }
  void set_num_tag_refs(c3_uint_t ntags);
  void dispose_tag_refs();

  TagRef& get_tag_ref(c3_uint_t i) {
    c3_assert(i < get_count());
    if (i < po_num_internal_tag_refs) {
      return po_tags[i];
    } else {
      c3_assert(po_xtags);
      return po_xtags[i - po_num_internal_tag_refs];
    }
  }

  /**
   * Finds out if page object is marked with *at least* `min_nmatches` of the specified tags.
   *
   * @param min_nmatches Minimum number of matching tags
   * @param tags Array of pointers to tag objects
   * @param ntags Number of tag pointers in the `tags` arrays
   * @return `true` if the object is marked with at least `min_nmatches` of specified tags
   */
  bool matches_tags(c3_uint_t min_nmatches, TagObject** tags, c3_uint_t ntags);
};

} // CyberCache

#endif // _HT_OBJECTS_H
