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
#include "ht_optimizer.h"
#include "ht_tag_manager.h"
#include "cc_server.h"

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// ObjectChain
///////////////////////////////////////////////////////////////////////////////

void Optimizer::ObjectChain::link(PayloadHashObject* pho) {
  c3_assert(pho && pho->is_locked() && pho->flags_are_clear(HOF_LINKED_BY_OPTIMIZER));
  if (oc_last != nullptr) {
    c3_assert(oc_last->get_opt_next() == nullptr && oc_first);
    oc_last->set_opt_next(pho);
    pho->set_opt_prev(oc_last);
    oc_last = pho;
  } else {
    c3_assert(oc_first == nullptr);
    oc_first = oc_last = pho;
    pho->set_opt_prev(nullptr);
  }
  pho->set_opt_next(nullptr);
  pho->set_flags(HOF_LINKED_BY_OPTIMIZER);
  oc_num++;
}

void Optimizer::ObjectChain::promote(PayloadHashObject* pho) {
  c3_assert(pho && pho->is_locked() && pho->flags_are_set(HOF_LINKED_BY_OPTIMIZER) && oc_num);
  if (oc_last != pho) {
    // unlink
    PayloadHashObject* prev = pho->get_opt_prev();
    PayloadHashObject* next = pho->get_opt_next();
    if (prev != nullptr) {
      c3_assert(prev->get_opt_next() == pho);
      prev->set_opt_next(next);
    } else {
      c3_assert(oc_first == pho);
      oc_first = next;
    }
    c3_assert(next && next->get_opt_prev() == pho);
    next->set_opt_prev(prev);
    // re-link
    c3_assert(oc_last && oc_last->get_opt_next() == nullptr);
    oc_last->set_opt_next(pho);
    pho->set_opt_prev(oc_last);
    pho->set_opt_next(nullptr);
    oc_last = pho;
  }
}

void Optimizer::ObjectChain::unlink(PayloadHashObject* pho) {
  c3_assert(pho && pho->is_locked() && pho->flags_are_set(HOF_LINKED_BY_OPTIMIZER) && oc_num);
  PayloadHashObject* prev = pho->get_opt_prev();
  PayloadHashObject* next = pho->get_opt_next();
  if (prev != nullptr) {
    c3_assert(prev->get_opt_next() == pho);
    prev->set_opt_next(next);
    pho->set_opt_prev(nullptr);
  } else {
    c3_assert(oc_first == pho);
    oc_first = next;
  }
  if (next != nullptr) {
    c3_assert(next->get_opt_prev() == pho);
    next->set_opt_prev(prev);
    pho->set_opt_next(nullptr);
  } else {
    c3_assert(oc_last == pho);
    oc_last = prev;
  }
  pho->clear_flags(HOF_LINKED_BY_OPTIMIZER);
  oc_num--;
}

void Optimizer::ObjectChain::unlink_all() {
  PayloadHashObject* pho = oc_first;
  while (pho != nullptr) {
    PayloadHashObject* next = pho->get_opt_next();
    pho->set_opt_prev(nullptr);
    pho->set_opt_next(nullptr);
    pho->clear_flags(HOF_LINKED_BY_OPTIMIZER);
    pho = next;
  }
  oc_num = 0;
}

///////////////////////////////////////////////////////////////////////////////
// ObjectChainIterator
///////////////////////////////////////////////////////////////////////////////

PayloadHashObject* Optimizer::ObjectChainIterator::get_next_object(const PayloadHashObject* pho) {
  PayloadHashObject* next = nullptr;
  if (pho != nullptr) {
    next = pho->get_opt_next();
    if (next == nullptr) {
      c3_uint_t ua = pho->get_user_agent() + 1;
      while (ua < UA_NUMBER_OF_ELEMENTS) {
        next = oci_optimizer.o_chain[ua++].get_first();
        if (next != nullptr) {
          break;
        }
      }
    }
  }
  return next;
}

PayloadHashObject* Optimizer::ObjectChainIterator::get_gc_object(PayloadHashObject* pho) {
  /*
   * Checks if argument object (OR any of the subsequent object in the chains) is suitable for garbage
   * collection, which means it
   *
   * - should be in a chain that still has more objects than its minimum allowed number,
   * - is not currently being used for data transfer (has zero registered readers),
   * - is not currently locked,
   * - is not marked as "deleted" yet.
   *
   * First checks (and, if check passes, returns) the argument itself, hence no "next" in the method name.
   *
   * This method is necessary to be able to quickly skip entire chains that have large minimum allowed
   * numbers of objects without traversing all their objects one by one.
   */
  if (pho != nullptr) {
    for (c3_uint_t i = pho->get_user_agent(); i < UA_NUMBER_OF_ELEMENTS; i++) {
      ObjectChain& chain = oci_optimizer.o_chain[i];
      if (chain.get_num_objects() > chain.get_num_retained_objects()) {
        if (pho == nullptr) {
          pho = chain.get_first();
          // this must be `true` even if number of retained object is zero
          c3_assert(pho);
        }
        do {
          if (pho->flags_are_clear(HOF_BEING_DELETED) && !pho->is_locked() && !pho->has_readers()) {
            return pho;
          }
          pho = pho->get_opt_next();
        } while (pho != nullptr);
      } else {
        // force setting it to the first object of the chain that fits
        pho = nullptr;
      }
    }
  }
  return nullptr;
}

void Optimizer::ObjectChainIterator::prepare_next_object() {
  PayloadHashObject* next = nullptr;
  for (c3_uint_t i = 0; i < UA_NUMBER_OF_ELEMENTS; i++) {
    next = oci_optimizer.o_chain[i].get_first();
    if (next != nullptr) {
      break;
    }
  }
  oci_next_object = next;
}

PayloadHashObject* Optimizer::ObjectChainIterator::get_first_object() {
  PayloadHashObject* first = nullptr;
  for (c3_uint_t i = 0; i < UA_NUMBER_OF_ELEMENTS; i++) {
    first = oci_optimizer.o_chain[i].get_first();
    if (first != nullptr) {
      break;
    }
  }
  oci_next_object = get_next_object(first);
  return first;
}

PayloadHashObject* Optimizer::ObjectChainIterator::get_next_object() {
  PayloadHashObject* next = oci_next_object;
  oci_next_object = get_next_object(next);
  return next;
}

PayloadHashObject* Optimizer::ObjectChainIterator::get_first_gc_object() {
  PayloadHashObject* first = get_first_object();
  PayloadHashObject* first_gc = get_gc_object(first);
  if (first_gc != first) {
    oci_next_object = get_next_object(first_gc);
  }
  return first_gc;
}

PayloadHashObject* Optimizer::ObjectChainIterator::get_next_gc_object() {
  PayloadHashObject* next = get_next_object();
  PayloadHashObject* next_gc = get_gc_object(next);
  if (next_gc != next) {
    oci_next_object = get_next_object(next_gc);
  }
  return next_gc;
}

void Optimizer::ObjectChainIterator::unlink(PayloadHashObject* pho) {
  c3_assert(pho && pho->get_user_agent() < UA_NUMBER_OF_ELEMENTS && pho != oci_next_object);
  oci_optimizer.o_iterator.exclude_object(pho);
  oci_optimizer.o_chain[pho->get_user_agent()].unlink(pho);
  c3_assert(oci_optimizer.o_total_num_objects);
  oci_optimizer.o_total_num_objects--;
}

///////////////////////////////////////////////////////////////////////////////
// Optimizer
///////////////////////////////////////////////////////////////////////////////

c3_uint_t Optimizer::o_num_cores = 0;

const c3_compressor_t Optimizer::o_default_compressors[NUM_COMPRESSORS] = {
  // re-compression attempts stop when `CT_NONE` is encountered
  #if C3_ENTERPRISE
  CT_ZLIB, CT_ZSTD, CT_BROTLI, CT_NONE, CT_NONE, CT_NONE, CT_NONE, CT_NONE
  #else
  CT_ZLIB, CT_ZSTD, CT_NONE, CT_NONE, CT_NONE, CT_NONE, CT_NONE, CT_NONE
  #endif
};
const c3_uint_t Optimizer::o_default_num_checks[NUM_LOAD_DEPENDENT_SLOTS] = {
  // 0% load  1..33%  33..66%  66..99%  100%
  1000000000, 1000,   500,     200,     100
};
const c3_uint_t Optimizer::o_default_num_comp_attempts[NUM_LOAD_DEPENDENT_SLOTS] = {
  // 0% load  1..33%  33..66%  66..99%  100%
  1000000000, 100,    25,      10,      0
};

Optimizer::Optimizer(const char* name, domain_t domain, eviction_mode_t em, c3_uint_t capacity,
  c3_uint_t max_capacity): o_name(name), o_memory(Memory::get_memory_object(domain)),
  o_queue(domain, HO_OPTIMIZER, capacity, max_capacity), o_iterator(*this) {
  o_host = nullptr;
  o_store = nullptr;
  std::memcpy(o_compressors, o_default_compressors, sizeof o_compressors);
  std::memcpy(o_num_checks, o_default_num_checks, sizeof o_num_checks);
  std::memcpy(o_num_comp_attempts, o_default_num_comp_attempts, sizeof o_num_comp_attempts);
  o_total_num_objects = 0;
  o_wait_time = DEFAULT_TIME_BETWEEN_RUNS;
  o_min_recompression_size = DEFAULT_MIN_RECOMPRESSION_SIZE;
  o_last_run_time.store(Timer::current_timestamp(), std::memory_order_relaxed);
  o_last_run_checks.store(0, std::memory_order_relaxed);
  o_last_run_compressions.store(0, std::memory_order_relaxed);
  o_last_save_time = 0;
  o_eviction_mode = em;
  o_quitting = false;
}

void Optimizer::initialize() {
  if (o_num_cores == 0) { // has not been initialized yet?
    o_num_cores = Thread::get_num_cpu_cores();
    if (o_num_cores == 0) { // system failed to report?
      o_num_cores = DEFAULT_NUM_CPU_CORES;
      get_store().log(LL_WARNING, "System failed to report number of CPU cores, using default value of %d",
        DEFAULT_NUM_CPU_CORES);
    }
  }
}

void Optimizer::validate_eviction_mode() {
  switch (o_eviction_mode) {
    case EM_LRU:
    case EM_STRICT_LRU:
      if (o_memory.is_quota_set() || global_memory.is_quota_set()) {
        get_store().log(LL_WARNING,
          "%s: eviction mode '%s' requires valid memory limit; reverting to '%s'",
          o_name, get_eviction_mode_name(o_eviction_mode), get_eviction_mode_name(EM_EXPIRATION_LRU));
        o_eviction_mode = EM_EXPIRATION_LRU;
      }
      break;
    default:
      c3_assert(o_eviction_mode > EM_INVALID && o_eviction_mode < EM_NUMBER_OF_ELEMENTS);
  }
}

const char* Optimizer::get_eviction_mode_name(eviction_mode_t mode) {
  c3_assert(mode < EM_NUMBER_OF_ELEMENTS);
  static const char* const mode_names[EM_NUMBER_OF_ELEMENTS] = {
    "<INVALID>",
    "strict-expiration-lru",
    "expiration-lru",
    "lru",
    "strict-lru"
  };
  static_assert(EM_NUMBER_OF_ELEMENTS == 5, "Number of eviction modes has changed");
  return mode_names[mode];
}

c3_uint_t Optimizer::get_cpu_load() {
  static_assert(NUM_LOAD_DEPENDENT_SLOTS == 5, "get_cpu_load() assumes 5 levels");
  c3_uint_t num_active_threads = Thread::get_num_active_connection_threads();
  if (num_active_threads > 0) {
    if (num_active_threads < o_num_cores) {
      c3_uint_t load = num_active_threads * 100 / o_num_cores;
      return load <= 33? 1: (load > 66? 3: 2);
    } else {
      return 4;
    }
  } else {
    return 0;
  }
}

bool Optimizer::is_above_memory_quota() const {
  if (o_memory.is_quota_set()) {
    if (o_memory.get_used_size() > o_memory.get_quota()) {
      return true;
    }
  } else if (global_memory.is_quota_set()) {
    if (session_memory.get_used_size() + fpc_memory.get_used_size() > global_memory.get_quota()) {
      return true;
    }
  }
  return false;
}

void Optimizer::process_write_message(PayloadHashObject* pho, user_agent_t ua, c3_timestamp_t lifetime) {
  LockableObjectGuard guard(pho);
  if (pho->flags_are_clear(HOF_BEING_DELETED)) {
    if (pho->flags_are_clear(HOF_LINKED_BY_OPTIMIZER)) {
      pho->set_user_agent(ua);
      get_chain(ua).link(pho);
      o_total_num_objects++;
    } else {
      user_agent_t current_ua = pho->get_user_agent();
      if (current_ua < ua) {
        o_iterator.exclude_object(pho);
        /*
         * this should never happen with session data, as sessions with same ID should be accessed with
         * the same "user agent", but it's still better to have single point where we move object between
         * chains rather than delegate this task to the virtual method
         */
        pho->set_user_agent(ua);
        get_chain(current_ua).unlink(pho);
        get_chain(ua).link(pho);
      } else {
        o_iterator.exclude_object(pho);
        get_chain(current_ua).promote(pho);
      }
    }
    pho->set_modification_time();
    on_write(pho, lifetime);
    c3_assert(pho->flags_are_set(HOF_LINKED_BY_OPTIMIZER));
  } else {
    // the object had already been marked as "deleted"
    if (pho->flags_are_clear(HOF_LINKED_BY_OPTIMIZER)) {
      pho->set_user_agent(ua);
      get_chain(ua).link(pho);
      o_total_num_objects++;
      pho->set_modification_time();
      pho->set_expiration_time(pho->get_last_modification_time());
    }
  }
}

void Optimizer::process_read_message(PayloadHashObject* pho, user_agent_t ua) {
  LockableObjectGuard guard(pho);
  if (pho->flags_are_clear(HOF_BEING_DELETED)) {
    // case 2A)
    // see comment in process_message() method
    if (pho->flags_are_set(HOF_LINKED_BY_OPTIMIZER)) {
      user_agent_t current_ua = pho->get_user_agent();
      if (current_ua < ua) {
        o_iterator.exclude_object(pho);
        // just as with "write" requests, this should never happen to the session data (see above)
        pho->set_user_agent(ua);
        get_chain(current_ua).unlink(pho);
        get_chain(ua).link(pho);
      } else {
        o_iterator.exclude_object(pho);
        get_chain(current_ua).promote(pho);
      }
      on_read(pho);
      c3_assert(pho->flags_are_set(HOF_LINKED_BY_OPTIMIZER));
    } else {
      C3_DEBUG(get_store().log(LL_WARNING, "Optimizer message READ '%.*s' came out of order (ignoring)",
        pho->get_name_length(), pho->get_name()));
    }
  }
}

void Optimizer::process_delete_message(Optimizer::OptimizerMessage& msg) {
  PayloadHashObject* pho = msg.get_object();
  LockableObjectGuard guard(pho);
  c3_assert(pho->flags_are_set(HOF_BEING_DELETED) && pho->flags_are_clear(HOF_LINKED_BY_TM));
  if (pho->flags_are_set(HOF_LINKED_BY_OPTIMIZER)) {
    o_iterator.exclude_object(pho);
    user_agent_t ua = pho->get_user_agent();
    get_chain(ua).unlink(pho);
    c3_assert(o_total_num_objects);
    o_total_num_objects--;
    c3_assert(pho->flags_are_clear(HOF_LINKED_BY_OPTIMIZER));
    /*
     * It is important to unlock the object before it is put into store's queue of deleted objects:
     * otherwise, object's memory might be freed already at the time we unlock it, so we would end up
     * writing to a byte that if no longer part of the object.
     */
    guard.unlock();
    /*
     * This request came either from session store, or from the tag manager; it could not have come from
     * FPC store (so we don't have to unlink tags). Now that we have unlinked the object from optimizer's
     * chains, we can put it into its store's queue of deleted objects.
     */
    get_store().post_unlink_message(pho);
  } else {
    // case 3)
    // see comment in process_message() method
    if (msg.increment_num_retries()) {
      o_queue.put_always(std::move(msg));
    } else {
      C3_DEBUG(get_store().log(LL_ERROR, "Optimizer could not process DELETE '%.*s' message",
        pho->get_name_length(), pho->get_name()));
    }
  }
}

void Optimizer::process_gc_message(c3_uint_t seconds) {
  validate_eviction_mode();
  if (o_eviction_mode != EM_STRICT_LRU) {
    C3_DEBUG(get_store().log(LL_DEBUG, "%s: GC run", o_name));
    c3_assert(o_eviction_mode > EM_INVALID && o_eviction_mode < EM_NUMBER_OF_ELEMENTS);
    ObjectChainIterator iterator(*this);
    PayloadHashObject* pho = iterator.get_first_gc_object();
    while (pho != nullptr) {
      LockableObjectGuard guard(pho);
      if (guard.is_locked()) {
        c3_assert(pho->flags_are_set(HOF_LINKED_BY_OPTIMIZER));
        if (pho->flags_are_clear(HOF_BEING_DELETED)) {
          if (is_above_memory_quota() || (o_eviction_mode <= EM_EXPIRATION_LRU && on_gc(pho, seconds))) {
            C3_DEBUG(get_store().log(LL_DEBUG, "GC: purging '%.*s'",
              pho->get_name_length(), pho->get_name()));
            pho->set_flags(HOF_BEING_DELETED);
            pho->try_dispose_buffer(o_memory);
            iterator.unlink(pho);
            guard.unlock();
            on_delete(pho);
          }
        }
      }
      pho = iterator.get_next_gc_object();
    }
  }
}

void Optimizer::process_free_memory_message(c3_ulong_t min_size, bool direct) {
  c3_ulong_t size = 0;
  ObjectChainIterator iterator(*this);
  PayloadHashObject* pho = iterator.get_first_gc_object();
  PayloadHashObject* deleted = nullptr;
  while (pho != nullptr && size < min_size) {
    LockableObjectGuard guard(pho);
    if (guard.is_locked()) {
      if (pho->flags_are_clear(HOF_BEING_DELETED) && !pho->has_readers()) {
        pho->set_flags(HOF_BEING_DELETED);
        size += pho->dispose_buffer(o_memory);
        iterator.unlink(pho);
        /*
         * Link the object into the chain of deleted objects. We cannot call `on_delete()` right away
         * because that would involve putting the object into some queue and, if the queue is full, it
         * would trigger queue buffer re-allocation, and thus a potential deadlock.
         */
        pho->set_opt_next(deleted);
        deleted = pho;
      }
    }
    pho = iterator.get_next_gc_object();
  }
  while (deleted != nullptr) {
    pho = deleted->get_opt_next();
    deleted->set_opt_next(nullptr);
    on_delete(deleted);
    deleted = pho;
  }
  if (!direct) {
    get_host().end_memory_deallocation();
  }
}

void Optimizer::process_generic_load_slot_message(const char* what, c3_uint_t* dst, const c3_uint_t* src) {
  static_assert(NUM_LOAD_DEPENDENT_SLOTS == 5, "process_generic_load_slot_message() expects 5 slots");
  std::memcpy(dst, src, sizeof(c3_uint_t) * NUM_LOAD_DEPENDENT_SLOTS);
  get_store().log(LL_VERBOSE, "%s: %s set to %u:%u:%u:%u:%u (0%%:1-33%%:34-66%%:67-99%%:100%%)",
    o_name, what, dst[0], dst[1], dst[2], dst[3], dst[4]);
}

void Optimizer::process_generic_ua_slot_message(const char* what, c3_uint_t* dst, const c3_uint_t* src) {
  static_assert(UA_NUMBER_OF_ELEMENTS == 4, "process_generic_ua_slot_message() expects 4 slots");
  std::memcpy(dst, src, sizeof(c3_uint_t) * UA_NUMBER_OF_ELEMENTS);
  get_store().log(LL_VERBOSE, "%s: %s set to %u:%u:%u:%u (unknown:bot:warmer:user)",
    o_name, what, dst[0], dst[1], dst[2], dst[3]);
}

void Optimizer::process_config_wait_time_message(c3_uint_t seconds) {
  o_wait_time = seconds;
  if (o_wait_time > 0) {
    get_store().log(LL_VERBOSE, "%s: time between optimizaton runs set to %u seconds", o_name, seconds);
  } else {
    get_store().log(LL_WARNING, "%s: scheduled optimization runs disabled!", o_name);
  }
}

void Optimizer::process_config_compressors_message(const c3_compressor_t* compressors) {
  static_assert(NUM_COMPRESSORS == 8, "process_config_compressors_message() expects 8 slots");
  c3_assert(compressors);
  std::memcpy(o_compressors, compressors, sizeof o_compressors);
  char names[128]; // longest compressor name is 6 chars
  c3_uint_t pos = 0;
  const char* separator = "";
  for (c3_uint_t i = 0; i < NUM_COMPRESSORS; i++) {
    c3_compressor_t ct = compressors[i];
    if (ct > CT_NONE) {
      const char* name = global_compressor.get_name(ct);
      int length = std::sprintf(names + pos, "%s'%s'", separator, name);
      c3_assert(length > 0);
      pos += length;
      separator = ", ";
    } else {
      // re-compression attempts stop as soon as first "none" is encountered
      break;
    }
  }
  get_store().log(LL_VERBOSE, "%s: compressors set to %s", o_name, names);
}

void Optimizer::process_config_retain_counts_message(const c3_uint_t* retain_counts) {
  c3_assert(retain_counts);
  for (c3_uint_t i = 0; i < UA_NUMBER_OF_ELEMENTS; i++) {
    o_chain[i].set_num_retained_objects(retain_counts[i]);
  }
  static_assert(UA_NUMBER_OF_ELEMENTS == 4, "process_config_retain_counts_message() expects 4 slots");
  get_store().log(LL_VERBOSE, "%s: retain counts set to %u:%u:%u:%u", o_name,
    o_chain[0].get_num_retained_objects(), o_chain[1].get_num_retained_objects(),
    o_chain[2].get_num_retained_objects(), o_chain[3].get_num_retained_objects());
}

void Optimizer::process_config_eviction_mode_message(eviction_mode_t mode) {
  c3_assert(mode > EM_INVALID && mode < EM_NUMBER_OF_ELEMENTS);
  const char* notice = "";
  switch (mode) {
    case EM_LRU:
    case EM_STRICT_LRU:
      notice = " (requires valid memory quota)";
      // fall through
    default:
      o_eviction_mode = mode;
      get_store().log(LL_VERBOSE, "%s: eviction mode set to '%s'%s",
        o_name, get_eviction_mode_name(mode), notice);
  }
}

void Optimizer::process_config_recompression_threshold_message(c3_uint_t threshold) {
  c3_assert(threshold > 0);
  o_min_recompression_size = threshold;
  get_store().log(LL_VERBOSE, "%s: re-compression threshold set to %u bytes", o_name, threshold);
}

void Optimizer::process_config_capacity_message(c3_uint_t capacity) {
  c3_uint_t actual = o_queue.set_capacity(capacity);
  get_store().log(LL_VERBOSE, "%s: queue capacity set to %u (requested: %u)", o_name, actual, capacity);
}

void Optimizer::process_config_max_capacity_message(c3_uint_t max_capacity) {
  c3_uint_t actual = o_queue.store_and_set_max_capacity(max_capacity);
  get_store().log(LL_VERBOSE, "%s: maximum queue capacity set to %u (requested: %u)",
    o_name, actual, max_capacity);
}

void Optimizer::process_message(Optimizer::OptimizerMessage &msg) {
  c3_assert(!msg.is_object_message() || (msg.get_request() == OR_WRITE ||
    msg.get_request() == OR_READ || msg.get_request() == OR_FPC_TOUCH || msg.get_request() == OR_DELETE));
  /*
   * Some of the optimization messages may come out of order; for instance, if, say, `WRITE` and `DELETE`
   * commands are executed one immediately after the other, `DELETE` request can indeed come first,
   * because not only `WRITE` command processing takes more time, but because, if the object is not only
   * updated but created (allocated anew), this command will, at some point, downgrade mutex on the
   * store, which may cause relinquishing control to another thread.
   *
   * When optimization messages that operate on objects come out of sequence, we may find ourselves in a
   * situation when we're told to update/delete an object that has not been linked yet. All in all, we
   * handle out-of-order messages as follows:
   *
   * 1) `OR_WRITE`: we have no way of knowing if it came in order or out, so always process it right away;
   *   we rely on the fact that, if the object has already been removed, it would at least be marked as
   *   "deleted"; if it's not been marked like that yet, we will link it into optimizer's chains of
   *   object if it's not yet there; otherwise, would we will just update it (promote).
   *
   * 2) OR_READ and OR_FPC_TOUCH: if, when these messages come, the object is no longer (or not yet) linked
   *   into optimizer's chains, we ignore them; there can only be two cases: a) if the object is not yet
   *   created, then `OR_WRITE` will soon come and overwrite both chain index (unknown/bot/warmer/user)
   *   and object lifetime, and promote it anyway, or b) if the object had already been destroyed, it
   *   does not matter anymore if it is configured properly.
   *
   * 3) OR_DELETE: this is the most complicated one; if it comes when the object is not in optimizer's
   *   chains, we cannot simply ignore it, because it is optimizer that posts the object into stores'
   *   queues of "deleted" objects, so if we don't do that, there will be a leak (stray object); but we
   *   cannot process it right away either, because putting the object into those queues may lead to its
   *   disposal before other messages that refer to it are processed, with potentially disastrous
   *   consequences; so what we do with `OR_DELETE` (if it came out of order) is pushing it to the back
   *   of the message queue, to retrieve again at a later time when the object is (hopefully) already
   *   linked in; we maintain the count of pushes (using "lifetime" field of the message for that) and,
   *   after it reaches the limit for retries, we give up and ignore the message, logging internal error
   *   and, yes, most probably causing a leak, but that's lesser of the two evils.
   *
   * The code that implements the above logic is in respective message handlers.
   */
  switch (msg.get_request()) {
    case OR_WRITE:
      process_write_message(msg.get_object(), msg.get_user_agent(), msg.get_lifetime());
      return;
    case OR_READ:
      process_read_message(msg.get_object(), msg.get_user_agent());
      return;
    case OR_DELETE:
      process_delete_message(msg);
      return;
    case OR_GC:
      process_gc_message(msg.get_uint());
      return;
    case OR_FREE_MEMORY:
      process_free_memory_message(msg.get_ulong(), false);
      return;
    case OR_CONFIG_WAIT_TIME:
      process_config_wait_time_message(msg.get_uint());
      return;
    case OR_CONFIG_NUM_CHECKS:
      c3_assert(msg.get_num_uints() == NUM_LOAD_DEPENDENT_SLOTS);
      process_generic_load_slot_message("number of checks per run", o_num_checks, msg.get_uints());
      return;
    case OR_CONFIG_NUM_COMP_ATTEMPTS:
      c3_assert(msg.get_num_uints() == NUM_LOAD_DEPENDENT_SLOTS);
      process_generic_load_slot_message("number of re-compression attempts per run", o_num_comp_attempts,
        msg.get_uints());
      return;
    case OR_CONFIG_COMPRESSORS:
      process_config_compressors_message(msg.get_compressors());
      return;
    case OR_CONFIG_RETAIN_COUNTS:
      c3_assert(msg.get_num_uints() == UA_NUMBER_OF_ELEMENTS);
      process_config_retain_counts_message(msg.get_uints());
      return;
    case OR_CONFIG_EVICTION_MODE:
      process_config_eviction_mode_message((eviction_mode_t) msg.get_uint());
      return;
    case OR_CONFIG_RECOMPRESSION_THRESHOLD:
      process_config_recompression_threshold_message(msg.get_uint());
      return;
    case OR_QUEUE_CAPACITY:
      process_config_capacity_message(msg.get_uint());
      return;
    case OR_QUEUE_MAX_CAPACITY:
      process_config_max_capacity_message(msg.get_uint());
      return;
    case OR_QUIT:
      get_store().log(LL_VERBOSE, "%s: QUIT request received", o_name);
      enter_quit_state();
      return;
    default:
      on_message(msg);
  }
}

void Optimizer::run(c3_timestamp_t current_time) {
  PayloadHashObject* pho;

  // 1) Do optional GC pass
  // ======================

  validate_eviction_mode();
  if (is_above_memory_quota()) {
    ObjectChainIterator iterator(*this);
    pho = iterator.get_first_gc_object();
    while (pho != nullptr) {
      LockableObjectGuard guard(pho);
      if (guard.is_locked()) {
        if (pho->flags_are_clear(HOF_BEING_DELETED) && !pho->has_readers()) {
          pho->set_flags(HOF_BEING_DELETED);
          /*
           * Preconditions for calling `dispose_buffer()` (as opposed to `try_dispose_buffer()`) are
           * that the object is already marked as deleted and has no readers; both are met here.
           */
          pho->dispose_buffer(o_memory);
          iterator.unlink(pho);
          guard.unlock();
          on_delete(pho);
        }
      }
      /*
       * there may "appear" more free memory even if we did not delete any object during this loop
       * iteration: in some other thread, a `DELETE` request could have caused release of an object buffer,
       * or a command reader's buffer was successfully sent to a replication server or binlog so that the
       * reader was disposed, etc.; hence this check during each iteration
       */
      if (is_above_memory_quota()) {
        /*
         * the following call will skip all locked objects *past* current one; so, even though `pho` is
         * currently [still] locked, it will not affect method's logic
         */
        pho = iterator.get_next_gc_object();
      } else {
        break;
      }
    }
  }

  // 2) Do optimization pass
  // =======================

  c3_uint_t num_checks = 0;
  c3_uint_t num_compressions = 0;
  while (num_checks < o_total_num_objects) {
    num_checks++;

    // 2A) Get next object to optimize (continue from previous run)
    // ------------------------------------------------------------

    pho = o_iterator.get_next_object();
    if (pho == nullptr) {
      // wrap around
      pho = o_iterator.get_first_object();
      c3_assert(pho);
    }

    // 2B) See if selected object is indeed eligible for optimization
    // --------------------------------------------------------------

    if (is_optimizable(pho)) {
      if (pho->try_lock()) {
        if (is_optimizable(pho)) {
          pho->set_flags(HOF_BEING_OPTIMIZED);

          // 2C) Get payload buffer data and unlock the object so that other threads could use it
          // ------------------------------------------------------------------------------------

          c3_uint_t size = pho->get_buffer_size();
          c3_uint_t usize = pho->get_buffer_usize();
          c3_compressor_t compressor = pho->get_buffer_compressor();
          c3_byte_t* compressed_buffer = pho->get_buffer_bytes(0, size);
          c3_assert(size && usize &&
            ((compressor == CT_NONE && size == usize) || (compressor != CT_NONE && size < usize)) &&
            compressor < CT_NUMBER_OF_ELEMENTS && compressed_buffer);
          c3_byte_t* uncompressed_buffer = compressor == CT_NONE?
            (c3_byte_t*) std::memcpy(o_memory.alloc(usize), compressed_buffer, usize):
            global_compressor.unpack(compressor, compressed_buffer, size, usize, o_memory);
          pho->unlock();

          // 2D) Try to improve compression ratio using engines specified in the configuration
          // ---------------------------------------------------------------------------------

          c3_compressor_t best_compressor = CT_NUMBER_OF_ELEMENTS;
          c3_uint_t best_size = size;
          compressed_buffer = nullptr;
          for (c3_uint_t i = 0; i < NUM_COMPRESSORS; i++) {
            c3_compressor_t try_compressor = o_compressors[i];
            if (try_compressor == CT_NONE) {
              break;
            }
            // default compression strength is "best", so no reason to try the same compressor twice
            if (try_compressor != compressor) {
              c3_uint_t try_size = best_size;
              // compressor returns `NULL` if result is bigger than or equal to `try_size`
              c3_byte_t* try_buff = global_compressor.pack(try_compressor, uncompressed_buffer,
                usize, try_size, o_memory, CL_BEST);
              if (try_buff != nullptr) {
                c3_assert(try_size < best_size);
                if (compressed_buffer != nullptr) {
                  o_memory.free(compressed_buffer, best_size);
                }
                best_compressor = try_compressor;
                best_size = try_size;
                compressed_buffer = try_buff;
                PERF_UPDATE_ARRAY(Recompressions_Succeeded, (c3_uint_t) try_compressor)
              } else {
                PERF_UPDATE_ARRAY(Recompressions_Failed, (c3_uint_t) try_compressor)
              }
            }
          }
          o_memory.free(uncompressed_buffer, usize);

          // 2E) Lock the object again and, if possible/necessary, set new buffer and flags
          // ------------------------------------------------------------------------------

          /*
           * Even if re-compression attempt has failed, we still need to lock the object to
           * at least clear the `HOF_BEING_OPTIMIZED` flag.
           */
          c3_assert_def(bool locked) pho->lock();
          c3_assert(locked);
          if (best_compressor != CT_NUMBER_OF_ELEMENTS && pho->flags_are_clear(HOF_BEING_DELETED) &&
            pho->flags_are_set(HOF_BEING_OPTIMIZED) && !pho->has_readers()) {
            C3_DEBUG(get_store().log(LL_DEBUG, "Optimized '%.*s': %u -> %u bytes (%s -> %s)",
              (int) pho->get_name_length(), pho->get_name(), pho->get_buffer_size(), best_size,
              global_compressor.get_name(pho->get_buffer_compressor()),
              global_compressor.get_name(best_compressor)));
            pho->set_buffer(best_compressor, best_size, usize, compressed_buffer, o_memory);
            pho->set_flags(HOF_OPTIMIZED);
          } else {
            if (pho->flags_are_clear(HOF_BEING_DELETED) && pho->flags_are_set(HOF_BEING_OPTIMIZED) &&
              !pho->has_readers()) {
              // nothing interfered with re-compression, and yet the object could not be optimized; do not try again
              C3_DEBUG(get_store().log(LL_DEBUG, "Object '%.*s' could not be optimized further: %u bytes (%s)",
                (int) pho->get_name_length(), pho->get_name(), pho->get_buffer_size(),
                global_compressor.get_name(pho->get_buffer_compressor())));
              pho->set_flags(HOF_OPTIMIZED);
            }
            if (compressed_buffer != nullptr) {
              o_memory.free(compressed_buffer, best_size);
            }
          }
          pho->clear_flags(HOF_BEING_OPTIMIZED);
          num_compressions++;
        }
        pho->unlock();
      }
    }
    // 2F) See if we should break optimization run for reasons other than having checked all objects
    // ---------------------------------------------------------------------------------------------

    c3_uint_t load = get_cpu_load();
    if (num_checks >= o_num_checks[load] ||
      num_compressions >= o_num_comp_attempts[load] ||
      o_queue.has_messages() ||
      Timer::current_timestamp() >= current_time + o_wait_time) {
      break;
    }
  }

  // 3) See if we have to send auto-save request to the server
  // =========================================================

  c3_timestamp_t autosave_interval = get_autosave_interval();
  if (autosave_interval != 0 && o_last_save_time + autosave_interval <= current_time) {
    o_last_save_time = current_time;
    send_autosave_command();
  }

  // 4) Store key stats from the last pass
  // =====================================

  o_last_run_time.store(current_time, std::memory_order_relaxed);
  o_last_run_checks.store(num_checks, std::memory_order_relaxed);
  o_last_run_compressions.store(num_compressions, std::memory_order_relaxed);
}

void Optimizer::enter_quit_state() {
  Thread::set_state(TS_QUITTING);
  o_quitting = true;
}

void Optimizer::cleanup() {
  c3_uint_t total = 0;
  for (c3_uint_t i = 0; i < UA_NUMBER_OF_ELEMENTS; ++i) {
    ObjectChain& chain = o_chain[i];
    total += chain.get_num_objects();
    chain.unlink_all();
  }
  c3_assert(total == o_total_num_objects);
  o_total_num_objects = 0;
  o_iterator.reset();
}

bool Optimizer::post_write_message(PayloadHashObject* object, user_agent_t user_agent, c3_uint_t lifetime) {
  return o_queue.put(OptimizerMessage(OR_WRITE, object, user_agent, lifetime));
}

bool Optimizer::post_read_message(PayloadHashObject* object, user_agent_t user_agent) {
  return o_queue.put(OptimizerMessage(OR_READ, object, user_agent, 0));
}

bool Optimizer::post_delete_message(PayloadHashObject* object) {
  return o_queue.put(OptimizerMessage(OR_DELETE, object, 0));
}

bool Optimizer::post_gc_message(c3_uint_t seconds) {
  return o_queue.put(OptimizerMessage(OR_GC, &seconds, 1));
}

bool Optimizer::post_free_memory_message(c3_ulong_t min_size) {
  return o_queue.put(OptimizerMessage(OR_FREE_MEMORY, min_size));
}

bool Optimizer::post_config_wait_time_message(c3_uint_t wait_time) {
  return o_queue.put(OptimizerMessage(OR_CONFIG_WAIT_TIME, &wait_time, 1));
}

bool Optimizer::post_config_num_checks_message(c3_uint_t* num_checks) {
  return o_queue.put(OptimizerMessage(OR_CONFIG_NUM_CHECKS, o_memory, num_checks,
    NUM_LOAD_DEPENDENT_SLOTS));
}

bool Optimizer::post_config_num_comp_attempts_message(const c3_uint_t* num_attempts) {
  return o_queue.put(OptimizerMessage(OR_CONFIG_NUM_COMP_ATTEMPTS, o_memory, num_attempts,
    NUM_LOAD_DEPENDENT_SLOTS));
}

bool Optimizer::post_config_compressors_message(const c3_compressor_t* compressors) {
  return o_queue.put(OptimizerMessage(OR_CONFIG_COMPRESSORS, compressors, NUM_COMPRESSORS));
}

bool Optimizer::post_config_retain_counts_message(const c3_uint_t* retain_counts) {
  return o_queue.put(OptimizerMessage(OR_CONFIG_RETAIN_COUNTS, o_memory, retain_counts,
    UA_NUMBER_OF_ELEMENTS));
}

bool Optimizer::post_config_eviction_mode_message(c3_uint_t mode) {
  c3_assert(mode > EM_INVALID && mode < EM_NUMBER_OF_ELEMENTS);
  return o_queue.put(OptimizerMessage(OR_CONFIG_EVICTION_MODE, &mode, 1));
}

bool Optimizer::post_config_recompression_threshold_message(c3_uint_t threshold) {
  c3_assert(threshold > 0);
  return o_queue.put(OptimizerMessage(OR_CONFIG_RECOMPRESSION_THRESHOLD, &threshold, 1));
}

bool Optimizer::post_queue_capacity_message(c3_uint_t capacity) {
  return o_queue.put(OptimizerMessage(OR_QUEUE_CAPACITY, &capacity, 1));
}

bool Optimizer::post_queue_max_capacity_message(c3_uint_t max_capacity) {
  return o_queue.put(OptimizerMessage(OR_QUEUE_MAX_CAPACITY, &max_capacity, 1));
}

bool Optimizer::post_quit_message() {
  return o_queue.put(OptimizerMessage(OR_QUIT));
}

void Optimizer::thread_proc(c3_uint_t id, ThreadArgument arg) {
  Thread::set_state(TS_ACTIVE);
  bool first_run = true;
  auto optimizer = (Optimizer*) arg.get_pointer();
  assert(optimizer);
  optimizer->initialize();
  // shall wait till very first run
  c3_timestamp_t last_run = Timer::current_timestamp();
  for (;;) { // main loop
    // see if main/configuration thread told us to quit
    if (!optimizer->o_quitting && Thread::received_stop_request()) {
      optimizer->enter_quit_state();
    }
    // get next message
    OptimizerMessage msg;
    if (optimizer->o_quitting) {
      msg = optimizer->o_queue.try_get();
    } else {
      c3_uint_t since_last_run = Timer::current_timestamp() - last_run;
      if (since_last_run >= optimizer->o_wait_time) {
        msg = optimizer->o_queue.try_get();
      } else {
        c3_uint_t msecs = (optimizer->o_wait_time - since_last_run) * 1000;
        c3_assert(msecs);
        Thread::set_state(TS_IDLE);
        msg = optimizer->o_queue.get(msecs);
        Thread::set_state(TS_ACTIVE);
      }
    }
    // process message; `msg` should be a valid message at this point
    if (msg.is_valid()) {
      optimizer->process_message(msg);
    } else if (optimizer->o_quitting) {
      // no more messages, we're done
      break;
    }
    if (!optimizer->o_queue.has_messages()) {
      // only do optimization runs if we do not have messages to process
      c3_timestamp_t current_time = Timer::current_timestamp();
      if (current_time - last_run >= optimizer->o_wait_time) {
        last_run = current_time;
        if (first_run) {
          /*
           * this sets "next object" to the very first object in the iterator; if we used
           * `get_first_object()` here, it would have returned the first object *and* set "next object"
           * to the second in the chains, so we'd have skipped the very first one
           */
          optimizer->o_iterator.prepare_next_object();
          first_run = false;
        }
        optimizer->run(current_time);
      }
    }
  }
  optimizer->cleanup();
}

///////////////////////////////////////////////////////////////////////////////
// SessionOptimizer
///////////////////////////////////////////////////////////////////////////////

// default values for: unknown user, known bot, cache warmer, regular site visitor
const c3_timestamp_t SessionOptimizer::so_default_first_write_lifetimes[UA_NUMBER_OF_ELEMENTS] = {
  30, minutes2seconds(1), minutes2seconds(2), minutes2seconds(10)
};
const c3_uint_t SessionOptimizer::so_default_first_write_nums[UA_NUMBER_OF_ELEMENTS] = {
  100, 50, 20, 10
};
const c3_timestamp_t SessionOptimizer::so_default_default_lifetimes[UA_NUMBER_OF_ELEMENTS] = {
  hours2seconds(1), hours2seconds(2), days2seconds(1), weeks2seconds(2)
};
const c3_timestamp_t SessionOptimizer::so_default_read_extra_lifetimes[UA_NUMBER_OF_ELEMENTS] = {
  30, minutes2seconds(1), minutes2seconds(2), weeks2seconds(2)
};

SessionOptimizer::SessionOptimizer() noexcept:
  Optimizer("Session optimizer", DOMAIN_SESSION, EM_EXPIRATION_LRU,
    DEFAULT_QUEUE_CAPACITY, DEFAULT_MAX_QUEUE_CAPACITY) {
  std::memcpy(so_first_write_lifetimes, so_default_first_write_lifetimes, sizeof so_first_write_lifetimes);
  std::memcpy(so_first_write_nums, so_default_first_write_nums, sizeof so_first_write_nums);
  std::memcpy(so_default_lifetimes, so_default_default_lifetimes, sizeof so_default_lifetimes);
  std::memcpy(so_read_extra_lifetimes, so_default_read_extra_lifetimes, sizeof so_read_extra_lifetimes);
}

void SessionOptimizer::on_write(PayloadHashObject* pho, c3_timestamp_t lifetime) {
  auto so = (SessionObject*) pho;
  c3_assert(so && so->is_locked() && so->flags_are_clear(HOF_BEING_DELETED) &&
    so->flags_are_set(HOF_LINKED_BY_OPTIMIZER) &&
    so->get_user_agent() < UA_NUMBER_OF_ELEMENTS && so->get_type() == HOT_SESSION_OBJECT);

  c3_uint_t ua = so->get_user_agent();
  if (ua == UA_USER && lifetime != Timer::MAX_TIMESTAMP) {
    /*
     * Either infinite, or a specific life time (i.e. not default) had been requested. We only
     * honor lifetimes sent along session records of regular users; every other record will get
     * lifetime calculated using options set in server's configuration file.
     */
    if (lifetime != 0) {
      // last modification time had been set right before this call
      c3_ulong_t new_expiration_time = (c3_ulong_t) so->get_last_modification_time() + lifetime;
      if (new_expiration_time >= Timer::MAX_TIMESTAMP) {
        new_expiration_time = Timer::MAX_TIMESTAMP - 1;
      }
      so->set_expiration_time((c3_timestamp_t) new_expiration_time);
    } else {
      so->set_expiration_time(Timer::MAX_TIMESTAMP);
    }
    // we keep number of writes zero to indicate it's a non-default lifetime
  } else {
    /*
     * Either not a regular user (i.e. an unknown user, a bot, or a cache warmer), or default
     * lifetime had been requested.
     */
    c3_uint_t num_writes = so->get_num_writes();
    c3_timestamp_t first_write_lifetime = so_first_write_lifetimes[ua];
    if (num_writes == 0) {
      lifetime = first_write_lifetime;
    } else {
      c3_timestamp_t default_lifetime = so_default_lifetimes[ua];
      c3_uint_t first_num_writes = so_first_write_nums[ua];
      if (first_num_writes == 0) {
        first_num_writes = 1;
      }
      if (num_writes < first_num_writes) {
        if (default_lifetime < first_write_lifetime) {
          // lifetimes should never be set this way in the configuration, but in case someone actually does
          // that, we need to correct it
          default_lifetime = first_write_lifetime;
        }
        lifetime = first_write_lifetime +
          ((default_lifetime - first_write_lifetime) * num_writes) / first_num_writes;
      } else {
        lifetime = default_lifetime;
      }
    }
    // "last modification time" had been set prior to this call (using it is faster than calling timer)
    so->set_expiration_time(so->get_last_modification_time() + lifetime);
    so->increment_num_writes();
  }
}

void SessionOptimizer::on_read(PayloadHashObject* pho) {
  c3_assert(pho && pho->is_locked() && pho->flags_are_clear(HOF_BEING_DELETED) &&
    pho->flags_are_set(HOF_LINKED_BY_OPTIMIZER) &&
    pho->get_user_agent() < UA_NUMBER_OF_ELEMENTS && pho->get_type() == HOT_SESSION_OBJECT);
  if (((SessionObject*) pho)->get_num_writes() > 0) {
    // only extend lifetime if we did not set specific value
    c3_timestamp_t current_time = Timer::current_timestamp();
    c3_timestamp_t expiration_time = pho->get_expiration_time();
    switch (o_eviction_mode) {
      default:
        if (current_time < expiration_time) {
          break;
        }
      case EM_LRU:
      case EM_STRICT_LRU:
        c3_uint_t new_expiration_time = current_time + so_read_extra_lifetimes[pho->get_user_agent()];
        if (new_expiration_time > expiration_time) {
          pho->set_expiration_time(new_expiration_time);
        }
    }
  }
}

void SessionOptimizer::on_delete(PayloadHashObject* pho) {
  c3_assert(pho && !pho->is_locked() && pho->get_type() == HOT_SESSION_OBJECT &&
    pho->flags_are_set(HOF_BEING_DELETED) && pho->flags_are_clear(HOF_LINKED_BY_OPTIMIZER));
  get_store().post_unlink_message(pho);
}

bool SessionOptimizer::on_gc(PayloadHashObject* pho, c3_uint_t seconds) {
  c3_assert(pho && pho->get_type() == HOT_SESSION_OBJECT && pho->is_locked() &&
    pho->flags_are_clear(HOF_BEING_DELETED) && o_eviction_mode <= EM_EXPIRATION_LRU);
  return pho->get_last_modification_time() + seconds < Timer::current_timestamp();
}

void SessionOptimizer::on_message(Optimizer::OptimizerMessage& msg) {
  switch (msg.get_request()) {
    case OR_SESSION_FIRST_WRITE_LIFETIMES:
      c3_assert(msg.get_num_uints() == UA_NUMBER_OF_ELEMENTS);
      process_generic_ua_slot_message("first write lifetimes", so_first_write_lifetimes, msg.get_uints());
      break;
    case OR_SESSION_FIRST_WRITE_NUMS:
      c3_assert(msg.get_num_uints() == UA_NUMBER_OF_ELEMENTS);
      process_generic_ua_slot_message("numbers of first writes", so_first_write_nums, msg.get_uints());
      break;
    case OR_SESSION_DEFAULT_LIFETIMES:
      c3_assert(msg.get_num_uints() == UA_NUMBER_OF_ELEMENTS);
      process_generic_ua_slot_message("default lifetimes", so_default_lifetimes, msg.get_uints());
      break;
    case OR_SESSION_READ_EXTRA_LIFETIMES:
      c3_assert(msg.get_num_uints() == UA_NUMBER_OF_ELEMENTS);
      process_generic_ua_slot_message("extra lifetimes on reads", so_read_extra_lifetimes, msg.get_uints());
      break;
    default:
      c3_assert_failure();
  }
}

c3_timestamp_t SessionOptimizer::get_autosave_interval() {
  return server.get_session_autosave_interval();
}

void SessionOptimizer::send_autosave_command() {
  server.post_id_message(SC_SAVE_SESSION_STORE);
}

bool SessionOptimizer::post_session_first_write_lifetimes_message(const c3_uint_t* lifetimes) {
  return o_queue.put(OptimizerMessage(OR_SESSION_FIRST_WRITE_LIFETIMES, o_memory, lifetimes,
    UA_NUMBER_OF_ELEMENTS));
}

bool SessionOptimizer::post_session_first_write_nums_message(c3_uint_t* nums) {
  return o_queue.put(OptimizerMessage(OR_SESSION_FIRST_WRITE_NUMS, o_memory, nums,
    UA_NUMBER_OF_ELEMENTS));
}

bool SessionOptimizer::post_session_default_lifetimes_message(const c3_uint_t* lifetimes) {
  return o_queue.put(OptimizerMessage(OR_SESSION_DEFAULT_LIFETIMES, o_memory, lifetimes,
    UA_NUMBER_OF_ELEMENTS));
}

bool SessionOptimizer::post_session_read_extra_lifetimes_message(const c3_uint_t* lifetimes) {
  return o_queue.put(OptimizerMessage(OR_SESSION_READ_EXTRA_LIFETIMES, o_memory, lifetimes,
    UA_NUMBER_OF_ELEMENTS));
}

///////////////////////////////////////////////////////////////////////////////
// PageOptimizer
///////////////////////////////////////////////////////////////////////////////

// default values for: unknown user, known bot, cache warmer, regular site visitor
const c3_timestamp_t PageOptimizer::po_default_default_lifetimes[UA_NUMBER_OF_ELEMENTS] = {
  days2seconds(1), days2seconds(2), days2seconds(20), days2seconds(60)
};
const c3_timestamp_t PageOptimizer::po_default_read_extra_lifetimes[UA_NUMBER_OF_ELEMENTS] = {
  days2seconds(1), days2seconds(2), days2seconds(20), days2seconds(60)
};
const c3_timestamp_t PageOptimizer::po_default_max_lifetimes[UA_NUMBER_OF_ELEMENTS] = {
  days2seconds(10), days2seconds(30), days2seconds(60), days2seconds(60)
};

PageOptimizer::PageOptimizer() noexcept:
  Optimizer("FPC optimizer", DOMAIN_FPC, EM_LRU, DEFAULT_QUEUE_CAPACITY, DEFAULT_MAX_QUEUE_CAPACITY) {
  std::memcpy(po_default_lifetimes, po_default_default_lifetimes, sizeof po_default_lifetimes);
  std::memcpy(po_read_extra_lifetimes, po_default_read_extra_lifetimes, sizeof po_read_extra_lifetimes);
  std::memcpy(po_max_lifetimes, po_default_max_lifetimes, sizeof po_max_lifetimes);
}

void PageOptimizer::process_fpc_touch_message(PayloadHashObject* pho, c3_uint_t lifetime) {
  LockableObjectGuard guard(pho);
  if (pho->flags_are_clear(HOF_BEING_DELETED)) {
    // case 2B)
    // see comment in process_message() method
    if (pho->flags_are_set(HOF_LINKED_BY_OPTIMIZER)) {
      o_iterator.exclude_object(pho);
      user_agent_t ua = pho->get_user_agent();
      c3_assert(ua < UA_NUMBER_OF_ELEMENTS);
      get_chain(ua).promote(pho);
      c3_timestamp_t expiration_time = pho->get_expiration_time();
      if (expiration_time != Timer::MAX_TIMESTAMP) {
        c3_uint_t max_lifetime = po_max_lifetimes[ua];
        c3_timestamp_t current_time = Timer::current_timestamp();
        if (expiration_time > current_time) {
          lifetime += expiration_time - current_time;
        }
        if (lifetime > max_lifetime) {
          lifetime = max_lifetime;
        }
        pho->set_expiration_time(current_time + lifetime);
      }
    } else {
      C3_DEBUG(get_store().log(LL_WARNING, "Optimizer message TOUCH '%.*s' came out of order (ignoring)",
        pho->get_name_length(), pho->get_name()));
    }
  }
}

void PageOptimizer::on_write(PayloadHashObject* pho, c3_timestamp_t lifetime) {
  c3_assert(pho && pho->is_locked() && pho->flags_are_clear(HOF_BEING_DELETED) &&
    pho->flags_are_set(HOF_LINKED_BY_OPTIMIZER) &&
    pho->get_user_agent() < UA_NUMBER_OF_ELEMENTS && pho->get_type() == HOT_PAGE_OBJECT);
  if (lifetime != 0) {
    c3_uint_t ua = pho->get_user_agent();
    if (lifetime == Timer::MAX_TIMESTAMP) {
      // "use default lifetime"
      lifetime = po_default_lifetimes[ua];
    }
    c3_timestamp_t max_lifetime = po_max_lifetimes[ua];
    if (lifetime > max_lifetime) {
      lifetime = max_lifetime;
    }
    // "last modification time" had been set prior to this call (using it is faster than calling timer)
    pho->set_expiration_time(pho->get_last_modification_time() + lifetime);
  } else {
    // "infinite" lifetime
    pho->set_expiration_time(Timer::MAX_TIMESTAMP);
  }
}

void PageOptimizer::on_read(PayloadHashObject* pho) {
  c3_assert(pho && pho->is_locked() && pho->flags_are_clear(HOF_BEING_DELETED) &&
    pho->flags_are_set(HOF_LINKED_BY_OPTIMIZER) &&
    pho->get_user_agent() < UA_NUMBER_OF_ELEMENTS && pho->get_type() == HOT_PAGE_OBJECT);
  c3_timestamp_t expiration_time = pho->get_expiration_time();
  if (expiration_time != Timer::MAX_TIMESTAMP) {
    c3_timestamp_t current_time = Timer::current_timestamp();
    switch (o_eviction_mode) {
      default:
        if (current_time < expiration_time) {
          break;
        }
      case EM_LRU:
      case EM_STRICT_LRU:
        c3_uint_t new_expiration_time = current_time + po_read_extra_lifetimes[pho->get_user_agent()];
        if (new_expiration_time > expiration_time) {
          pho->set_expiration_time(new_expiration_time);
        }
    }
  }
}

void PageOptimizer::on_delete(PayloadHashObject* pho) {
  c3_assert(pho && !pho->is_locked() && pho->get_type() == HOT_PAGE_OBJECT &&
    pho->flags_are_set(HOF_BEING_DELETED) && pho->flags_are_clear(HOF_DELETED | HOF_LINKED_BY_OPTIMIZER));
  get_tag_manager().post_unlink_message(pho);
}

bool PageOptimizer::on_gc(PayloadHashObject* pho, c3_uint_t seconds) {
  c3_assert(pho && pho->get_type() == HOT_PAGE_OBJECT && pho->is_locked() &&
    pho->flags_are_clear(HOF_BEING_DELETED) && o_eviction_mode <= EM_EXPIRATION_LRU);
  return pho->get_expiration_time() < Timer::current_timestamp();
}

void PageOptimizer::on_message(Optimizer::OptimizerMessage& msg) {
  switch (msg.get_request()) {
    case OR_FPC_DEFAULT_LIFETIMES:
      c3_assert(msg.get_num_uints() == UA_NUMBER_OF_ELEMENTS);
      process_generic_ua_slot_message("default lifetimes", po_default_lifetimes, msg.get_uints());
      break;
    case OR_FPC_READ_EXTRA_LIFETIMES:
      c3_assert(msg.get_num_uints() == UA_NUMBER_OF_ELEMENTS);
      process_generic_ua_slot_message("extra lifetimes on reads", po_read_extra_lifetimes, msg.get_uints());
      break;
    case OR_FPC_MAX_LIFETIMES:
      c3_assert(msg.get_num_uints() == UA_NUMBER_OF_ELEMENTS);
      process_generic_ua_slot_message("max lifetimes", po_max_lifetimes, msg.get_uints());
      break;
    case OR_FPC_TOUCH:
      process_fpc_touch_message(msg.get_object(), msg.get_lifetime());
      break;
    default:
      c3_assert_failure();
  }
}

c3_timestamp_t PageOptimizer::get_autosave_interval() {
  return server.get_fpc_autosave_interval();
}

void PageOptimizer::send_autosave_command() {
  server.post_id_message(SC_SAVE_FPC_STORE);
}

bool PageOptimizer::post_fpc_default_lifetimes_message(const c3_uint_t* lifetimes) {
  return o_queue.put(OptimizerMessage(OR_FPC_DEFAULT_LIFETIMES, o_memory, lifetimes,
    UA_NUMBER_OF_ELEMENTS));
}

bool PageOptimizer::post_fpc_read_extra_lifetimes_message(const c3_uint_t* lifetimes) {
  return o_queue.put(OptimizerMessage(OR_FPC_READ_EXTRA_LIFETIMES, o_memory, lifetimes,
    UA_NUMBER_OF_ELEMENTS));
}

bool PageOptimizer::post_fpc_max_lifetimes_message(const c3_uint_t* lifetimes) {
  return o_queue.put(OptimizerMessage(OR_FPC_MAX_LIFETIMES, o_memory, lifetimes,
    UA_NUMBER_OF_ELEMENTS));
}

bool PageOptimizer::post_fpc_touch_message(PayloadHashObject* object, c3_uint_t lifetime) {
  return o_queue.put(OptimizerMessage(OR_FPC_TOUCH, object, lifetime));
}

}
