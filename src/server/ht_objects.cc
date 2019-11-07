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
#include "ht_objects.h"
#include "mt_threads.h"

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// HashObject
///////////////////////////////////////////////////////////////////////////////

const char* HashObject::get_flags_state(char* buff, size_t length) const {
  assert(buff != nullptr && length >= FLAGS_STATE_BUFF_LENGTH);

  // take snapshot of flags at the time of the call
  c3_byte_t flags = ho_flags;
  static const char* names[8] = {
    "FPC",
    "PAYLOAD",
    "LINKED_BY_OPTIMIZER",
    "LINKED_BY_TM",
    "BEING_OPTIMIZED",
    "OPTIMIZED",
    "BEING_DELETED",
    "DELETED"
  };
  size_t pos = 0;
  for (int i = 0; i < 8; ++i) {
    if ((flags & (1 << i)) != 0 ) {
      const char* name = names[i];
      size_t len = std::strlen(name);
      if (pos != 0) {
        buff[pos++] = ' ';
      }
      std::memcpy(buff + pos, name, len);
      pos += len;
    }
  }
  buff[pos] = 0;
  return buff;
}

void HashObject::dispose(HashObject* ho) {
  c3_assert(ho && !ho->is_locked() && ho->flags_are_set(HOF_BEING_DELETED | HOF_DELETED) &&
    ho->flags_are_clear(HOF_LINKED_BY_TM | HOF_LINKED_BY_OPTIMIZER | HOF_BEING_OPTIMIZED) &&
    !ho->ho_ht_prev && !ho->ho_ht_next && !ho->ho_prev && !ho->ho_next && ho->ho_length);

  switch (ho->get_type()) {
    case HOT_SESSION_OBJECT: {
      PERF_DECREMENT_DOMAIN_COUNTER(SESSION, Store_Objects_Active)
      auto so = (SessionObject*) ho;
      so->dispose_buffer(session_memory);
      session_memory.free(so, so->get_size());
      return;
    }
    case HOT_PAGE_OBJECT: {
      PERF_DECREMENT_DOMAIN_COUNTER(FPC, Store_Objects_Active)
      auto po = (PageObject*) ho;
      po->dispose_buffer(fpc_memory);
      po->dispose_tag_refs();
      fpc_memory.free(po, po->get_size());
      return;
    }
    case HOT_TAG_OBJECT: {
      PERF_DECREMENT_DOMAIN_COUNTER(GLOBAL, Store_Objects_Active)
      auto to = (TagObject*) ho;
      c3_assert(to->get_num_marked_objects() == 0);
      fpc_memory.free(to, to->get_size());
      return;
    }
    default:
      c3_assert_failure();
  }
}

///////////////////////////////////////////////////////////////////////////////
// PayloadHashObject
///////////////////////////////////////////////////////////////////////////////

constexpr c3_byte_t PayloadHashObject::ZERO_LENGTH_BUFFER[];

void PayloadHashObject::set_buffer(c3_compressor_t compressor, c3_uint_t size, c3_uint_t usize,
  c3_byte_t* buffer, Memory &memory) {
  assert(size <= usize && buffer);
  c3_assert(is_locked() && !has_readers() && flags_are_clear(HOF_BEING_DELETED));
  if (pho_buffer != nullptr) { // replacing an existing buffer?
    c3_assert(pho_size <= pho_usize);
    if (pho_size > 0) {
      c3_assert(pho_buffer != ZERO_LENGTH_BUFFER);
      memory.free(pho_buffer, pho_size);
    } else {
      c3_assert(pho_buffer == ZERO_LENGTH_BUFFER);
    }
    clear_flags(HOF_BEING_OPTIMIZED | HOF_OPTIMIZED);
  }
  pho_buffer = size > 0? buffer: (c3_byte_t*) ZERO_LENGTH_BUFFER;
  pho_size = size;
  pho_usize = usize;
  pho_opt_comp = compressor;
}

c3_uint_t PayloadHashObject::dispose_buffer(Memory& memory) {
  if (pho_buffer != nullptr) {
    c3_assert(pho_size <= pho_usize && flags_are_set(HOF_BEING_DELETED) && !has_readers());
    if (pho_size > 0) {
      c3_assert(pho_buffer != ZERO_LENGTH_BUFFER);
      memory.free(pho_buffer, pho_size);
      pho_buffer = nullptr;
      c3_uint_t result = pho_size;
      pho_size = 0;
      pho_usize = 0;
      return result;
    } else {
      // must be zero-length stub
      c3_assert(pho_buffer == ZERO_LENGTH_BUFFER && pho_usize == 0);
      pho_buffer = nullptr;
      // return zero
    }
  }
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
// SessionObject
///////////////////////////////////////////////////////////////////////////////

std::atomic_uint SessionObject::so_lock_wait_time;

SessionObject::Initializer SessionObject::so_initializer;

session_lock_result_t SessionObject::lock_session(c3_uint_t request_id) {
  c3_assert(is_locked());
  if (request_id != 0) {
    /*
     * Storing wait time in a local variable also ensures we wouldn't wait with zero timeout (indefinitely)
     * if configuration changes between checking and actually using it.
     */
    const c3_uint_t lock_wait_time = get_lock_wait_time();
    if (lock_wait_time != 0) {
      /*
       * We keep track of who was locking the session at the time we started waiting. If the session is locked
       * by some other request, we will wait for session unlocking while our hash object is *unlocked*, so it is
       * theoretically possible that the following sequence of actions/events occurs:
       *
       * -- we wait on timed event (with hash object unlocked),
       * -- current session holder unlocks the session, triggers the event, and unlocks hash object,
       * -- some other thread (with different request ID) acquires hash object lock, and then locks the session,
       * -- we wake up, lock the hash object, and find out that request ID has changed.
       *
       * Similarly, it is theoretically possible that two threads' timeouts expire simultaneously, one of
       * the threads acquires session lock first, and another finds out that yes, waiting has timed out, but
       * request ID has changed...
       *
       * In such cases, we wouldn't want to break other request's session lock; so we should wait again instead.
       */
      c3_uint_t locking_request_id = so_request_id;
      // does some *other* request hold session lock?
      if (locking_request_id != 0 && locking_request_id != request_id) {
        const c3_ulong_t mask = 1ull << Thread::get_id();
        so_threads |= mask;
        for(;;) {
          PERF_INCREMENT_COUNTER(Session_Lock_Waits)
          unlock();
          Thread::wait_for_timed_event(lock_wait_time);
          lock();
          if (so_request_id != 0 && so_request_id != locking_request_id) {
            // some other request acquired session lock; so keep waiting...
            locking_request_id = so_request_id;
          } else {
            // either session is unlocked, or we'll break the lock
            so_threads &= ~mask;
            /*
             * Checking request ID is more reliable than checking return value of `wait_for_timed_event()`
             * because it is (again, theoretically) possible that the timeout expired exactly at the moment when
             * session lock holder finally released session lock.
             */
            bool broke_the_lock = false;
            if (so_request_id != 0) {
              PERF_INCREMENT_COUNTER(Session_Broken_Locks)
              broke_the_lock = true;
            }
            /*
             * While we were waiting for the lock, the record was marked as deleted. It is still safe to
             * manipulate object fields (because there should still be shared lock on the hash table, so the
             * record could not have been disposed), but it is no longer safe to try to access its data buffer,
             * so call site should not try to send back session data.
             */
            if (flags_are_set(HOF_BEING_DELETED)) {
              PERF_INCREMENT_COUNTER(Session_Aborted_Locks)
              so_request_id = 0; // release the lock in case there was a timeout
              return SLR_DELETED;
            }
            so_request_id = request_id; // acquire the lock
            if (broke_the_lock) {
              return SLR_BROKE_LOCK;
            }
            // successfully acquired session lock
            break;
          }
        }
      }
    }
  }
  return SLR_SUCCESS;
}

void SessionObject::unlock_session(c3_uint_t request_id) {
  c3_assert(is_locked());
  if (request_id != 0) {
    // only unlock sessions that were locked with specified request ID
    if (request_id == so_request_id) {
      if (so_threads != 0) {
        for (c3_uint_t i = 0; i < MAX_NUM_THREADS; i++) {
          if ((so_threads & (1ull << i)) != 0) {
            /*
             * Even though we wake up waiting thread, the hash object is still locked, so woken up thread
             * won't examine any fields (until we unlock hash object upon return).
             *
             * We do not clear the bit that corresponds to the thread (that we just woke up) in the mask of
             * waiting threads as that's responsibility of the locking code (in `lock_session()` method).
             */
            Thread::trigger_timed_event(i);
            break;
          }
        }
      }
      so_request_id = 0;
    }
  }
  unlock();
}

///////////////////////////////////////////////////////////////////////////////
// TagRef
///////////////////////////////////////////////////////////////////////////////

void TagRef::link(PageObject* po, TagObject* to) {
  c3_assert(po && po->get_type() == HOT_PAGE_OBJECT && to && to->get_type() == HOT_TAG_OBJECT);

  tr_page = po;
  tr_tag = to;
  tr_prev = nullptr;
  tr_next = to->to_first;
  if (tr_next != nullptr) {
    tr_next->tr_prev = this;
  }
  to->to_first = this;
  to->to_num++;
}

TagObject* TagRef::unlink() {
  c3_assert(tr_page && tr_page->get_type() == HOT_PAGE_OBJECT &&
    tr_tag && tr_tag->get_type() == HOT_TAG_OBJECT && tr_tag->to_num);

  if (tr_prev != nullptr) {
    c3_assert(tr_tag->to_first != this && tr_tag->to_num > 1);
    tr_prev->tr_next = tr_next;
  } else {
    c3_assert(tr_tag->to_first == this);
    tr_tag->to_first = tr_next;
  }
  if (tr_next != nullptr) {
    tr_next->tr_prev = tr_prev;
  }

  TagObject* empty = nullptr;
  if (--tr_tag->to_num == 0 && !tr_tag->to_untagged) {
    empty = tr_tag;
  }

  #ifdef C3_SAFE
  tr_page = nullptr;
  tr_tag = nullptr;
  tr_prev = nullptr;
  tr_next = nullptr;
  #endif

  // tag object that has become empty after removing this reference
  return empty;
}

///////////////////////////////////////////////////////////////////////////////
// PageObject
///////////////////////////////////////////////////////////////////////////////

c3_uint_t PageObject::po_num_internal_tag_refs = 1;

PageObject::PageObject(c3_hash_t hash, const char* name, c3_ushort_t nlen):
  PayloadHashObject(hash, HOF_PAYLOAD | HOF_FPC, name, nlen, calculate_size(nlen)) {

  static_assert(C3_OFFSETOF(PageObject, po_xtags) == sizeof(PayloadHashObject),
    "PageObject::po_tags[] field is not aligned properly");

  PERF_INCREMENT_DOMAIN_COUNTER(FPC, Store_Objects_Created)
  PERF_INCREMENT_DOMAIN_COUNTER(FPC, Store_Objects_Active)
  PERF_UPDATE_DOMAIN_RANGE(FPC, Store_Objects_Length, get_size())
  PERF_UPDATE_DOMAIN_RANGE(FPC, Store_Objects_Name_Length, (c3_uint_t) nlen)

  // the object is created in FPC store; tags will be added later, in tag manager
  set_count(0);
  po_xtags = nullptr;
}

PageObject::~PageObject() {
  if (po_xtags != nullptr) {
    free_tag_xrefs();
  }
}

void PageObject::alloc_tag_xrefs(c3_uint_t ntags) {
  c3_assert(ntags > po_num_internal_tag_refs);
  po_xtags = (TagRef*) fpc_memory.alloc(xrefs_size(ntags));
}

void PageObject::free_tag_xrefs() {
  c3_assert(get_count() > po_num_internal_tag_refs);
  fpc_memory.free(po_xtags, xrefs_size(get_count()));
  po_xtags = nullptr;
}

void PageObject::set_num_tag_refs(c3_uint_t ntags) {
  c3_assert(ntags);
  PERF_UPDATE_ARRAY(Store_Tags_Per_Object, ntags)
  if (ntags != get_count()) {
    PERF_INCREMENT_COUNTER(Store_Tag_Array_Reallocs)
    if (ntags > po_num_internal_tag_refs) {
      if (po_xtags != nullptr) {
        po_xtags = (TagRef*) fpc_memory.realloc(po_xtags, xrefs_size(ntags), xrefs_size(get_count()));
      } else {
        alloc_tag_xrefs(ntags);
      }
    } else {
      if (po_xtags != nullptr) {
        free_tag_xrefs();
      }
    }
    set_count(ntags);
  }
}

void PageObject::dispose_tag_refs() {
  if (po_xtags != nullptr) {
    free_tag_xrefs();
  }
}

bool PageObject::matches_tags(c3_uint_t min_nmatches, TagObject** tags, c3_uint_t ntags) {
  c3_assert(min_nmatches && tags && ntags);
  c3_uint_t nmatches = 0;

  // search internal array of tags
  c3_uint_t num_tags_to_search = min(get_count(), po_num_internal_tag_refs);
  c3_uint_t i = 0;
  do {
    TagObject* to = po_tags[i].get_tag_object();
    c3_uint_t j = 0;
    do {
      if (tags[j] == to) {
        if (++nmatches == min_nmatches) {
          return true;
        }
      }
    } while (++j < ntags);
  } while (++i < num_tags_to_search);

  // search extra array of tags
  if (po_xtags != nullptr) {
    c3_assert(get_count() > po_num_internal_tag_refs);
    num_tags_to_search = get_count() - po_num_internal_tag_refs;
    i = 0;
    do {
      TagObject* to = po_xtags[i].get_tag_object();
      c3_uint_t k = 0;
      do {
        if (tags[k] == to) {
          if (++nmatches == min_nmatches) {
            return true;
          }
        }
      } while (++k < ntags);
    } while (++i < num_tags_to_search);
  }

  return false;
}

} // CyberCache
