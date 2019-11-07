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
 * Multithreading support: messages and shared message queues.
 */
#ifndef _MT_MESSAGE_QUEUE_H
#define _MT_MESSAGE_QUEUE_H

#include "c3lib/c3lib.h"
#include "mt_thread_guards.h"

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <utility>

namespace CyberCache {

/// Type of the contents stored in the `CommandMessage` object
enum command_message_type_t {
  CMT_INVALID = 0,  // either unsuccessful `try_get()` from a queue, or a disposed message
  CMT_ID_COMMAND,   // message contains simple numeric command ID
  CMT_DATA_COMMAND, // message contains instance of the data command class (command with arguments)
  CMT_OBJECT        // message contains instance of the "main object" class
};

/**
 * This template class is the core of CyberCache messaging system, although it is not the only type of
 * objects that can be stored in a `MessageQueue`.
 *
 * It represents a message that contains one of the following:
 * - a simple command that is just some integer ID (an element of enumeration E), or
 * - a command with arguments (a.k.a. data command, a pointer to an instance of structure/class D), or
 * - an object (a.k.a. "main object", a pointer to an instance of class/structure T).
 *
 * The queue that uses an instantiation of this template class as its message type is supposed to be used
 * for queueing objects of type T; however, every now and then it might be necessary to pass on a command
 * (with or without arguments), and this is where ID and data commands it supports come in handy.
 *
 * `CommandMessage` puts the following restrictions on enums and classes used for instantiation:
 * - E must be an `enum` of size `c3_uintptr_t`,
 * - class/structure D (data command) must have `bool is_you()` method (which "recognizes" layout of D),
 * - both D (data command) and T (main object) classes must have `c3_uint_t get_object_size()` method.
 * - both D (data command) and T (main object) classes must have `Memory& get_memory_object()` method.
 *
 * `CommandMessage` figures out what contents it currently stores as follows:
 * - if value is zero, the object is in "invalid" state (e.g. result of a failed try_get() from a queue),
 * - if value is small, it is a simple ID command of type E; otherwise, it is a pointer to D or T,
 * - if `is_you()` called on D* returns `true`, it is a "data command" of type D,
 * - otherwise, it is a "main object" of type T.
 *
 * Two things are important for these tests:
 * - maximum value of E (passed as N argument) must be small, guaranteedly smaller than any pointer, and
 * - the 'is_you()' method of structure/class D must be coded with an assumption that it could also be
 *   called on pointer to an instance of class T; so it must be aware of both its own layout and the
 *   layout of T, be able to tell these apart, and not do anything that would cause crash.
 */
template <class E, class D, class T, c3_uintptr_t N> class CommandMessage {
  union {
    E  cm_id_command;   // an `enum` value, a command that is just an ID
    D* cm_data_command; // a pointer to a structure with data, a command having some extra data/arguments
    T* cm_object;       // a pointer of to "main object" sent through the queue
  };

  static_assert(sizeof(E) == sizeof(D*), "class 'CommandMessage': enum must be of size 'c3_uintptr_t'");

public:
  CommandMessage() { cm_object = nullptr; } // sets "invalid" state
  explicit CommandMessage(E id_command) { cm_id_command = id_command; }
  explicit CommandMessage(D* data_command) { cm_data_command = data_command; }
  explicit CommandMessage(T* object) { cm_object = object; }
  ~CommandMessage() { dispose(); }

  CommandMessage(const CommandMessage&) = delete;
  CommandMessage(CommandMessage&& cm) noexcept {
    cm_id_command = cm.cm_id_command;
    cm.cm_id_command = (E) 0;
  }
  CommandMessage& operator=(const CommandMessage&) = delete;
  CommandMessage& operator=(CommandMessage&& cm) noexcept {
    std::swap(cm_id_command, cm.cm_id_command);
    return *this;
  }

  /////////////////////////////////////////////////////////////////////////////
  // IDENTIFICATION AND CHECKS
  /////////////////////////////////////////////////////////////////////////////

  command_message_type_t get_type() const {
    if (cm_id_command < N) {
      if (cm_id_command > 0) {
        return CMT_ID_COMMAND;
      } else {
        return CMT_INVALID;
      }
    } else {
      if (cm_data_command->is_you()) {
        return CMT_DATA_COMMAND;
      } else {
        return CMT_OBJECT;
      }
    }
  }
  bool is_valid() const { return cm_id_command != 0; }
  bool is_id_command() const { return cm_id_command > 0 && cm_id_command < N; }
  bool is_data_command() const { return cm_id_command >= N && cm_data_command->is_you(); }
  bool is_object() const { return cm_id_command >= N && !cm_data_command->is_you(); }

  /////////////////////////////////////////////////////////////////////////////
  // DATA RETRIEVAL
  /////////////////////////////////////////////////////////////////////////////

  E get_id_command() const {
    c3_assert(is_id_command());
    return cm_id_command;
  }

  const D& get_data_command() const {
    c3_assert(is_data_command());
    return *cm_data_command;
  }

  D* fetch_data_command() {
    c3_assert(is_data_command());
    D* data_dommand = cm_data_command;
    cm_data_command = nullptr; // transfer ownership to the caller
    return data_dommand;
  }

  const T& get_const_object() const {
    c3_assert(is_object());
    return *cm_object;
  }

  T& get_object() const {
    c3_assert(is_object());
    return *cm_object;
  }

  T* fetch_object() {
    c3_assert(is_object());
    T* object = cm_object;
    cm_object = nullptr; // transfer ownership to the caller
    return object;
  }

  /////////////////////////////////////////////////////////////////////////////
  // CLEANUP
  /////////////////////////////////////////////////////////////////////////////

  static void dispose(D* data_command) {
    assert(data_command);
    c3_uint_t size = data_command->get_object_size();
    Memory& memory = data_command->get_memory_object();
    data_command->~D(); // call dtor
    memory.free(data_command, size);
  }

  static void dispose(T* object) {
    assert(object);
    c3_uint_t size = object->get_object_size();
    Memory& memory = object->get_memory_object();
    object->~T(); // call dtor
    memory.free(object, size);
  }

  void dispose() {
    if (cm_id_command >= N) {
      if (cm_data_command->is_you()) {
        dispose(cm_data_command);
      } else {
        dispose(cm_object);
      }
    }
    cm_object = nullptr; // invalidate
  }
};

/**
 * Synchronized queue of dynamic capacity: if the queue is full *and* maximum capacity (supplied in ctor
 * argument OR set later with a method call) is bigger than current capacity, the queue will resize
 * itself by doubling its size; otherwise, the method adding new element to the queue will wait till some
 * other thread removes at least one element.
 *
 * The queue may also be sized down with method calls setting capacity or maximum capacity; in such
 * cases, new size is limited from the bottom by the current count of the elements in the queue (if, say,
 * there are 10 elements, the size cannot be set to anything less than 16); this rounding up happens
 * silently, not generating any errors.
 *
 * Queue capacity must be greater than or equal to 1, less than or equal to 65536, and be a power of 2;
 * if specified capacity does not meet those requirements, actual capacity will be silently rounded up
 * (or down, if specified value is greater than 64k) to the nearest power of 2.
 *
 * Elements stored in the queue must have default ctors creating elements in some "invalid" state, and must
 * have means of distinguishing between valid and invalid states, because retrieval methods return
 * elements created using default ctors on failure (i.e. when the queue is empty).
 *
 * @see Queue
 *
 * @tparam T Type of the message that this queue contains.
 */
template <class T> class MessageQueue: public SyncObject {
  static constexpr c3_uint_t  MQ_MIN_ALLOWED_CAPACITY = 1;
  static constexpr c3_uint_t  MQ_MAX_ALLOWED_CAPACITY = USHORT_MAX_VAL + 1;

protected:
  std::mutex              mq_mutex;        // common access mutex
  std::condition_variable mq_not_empty;    // notifications about added object
  std::condition_variable mq_not_full;     // notifications about removed object
  T*                      mq_buffer;       // ring buffer with queued elements
  c3_uint_t               mq_max_capacity; // currently allowed max queue capacity
  c3_uint_t               mq_capacity;     // current queue capacity (max number of elements)
  c3_uint_t               mq_count;        // current number of elements in the queue
  c3_uint_t               mq_put_index;    // next element will be added at this index
  c3_uint_t               mq_get_index;    // next element will be retrieved at this index
  c3_uint_t               mq_index_mask;   // bit mask for indices

  static constexpr c3_uint_t validate_capacity(c3_uint_t capacity) {
    if (capacity < MQ_MIN_ALLOWED_CAPACITY) {
      capacity = MQ_MIN_ALLOWED_CAPACITY;
    }
    if (capacity > MQ_MAX_ALLOWED_CAPACITY) {
      capacity = MQ_MAX_ALLOWED_CAPACITY;
    }
    return get_next_power_of_2(capacity);
  }

  void configure_capacity(c3_uint_t capacity, bool force) {
    if (!force) {
      // validate requested capacity
      capacity = validate_capacity(capacity);
      if (capacity > mq_max_capacity) {
        capacity = mq_max_capacity;
      }
      c3_uint_t min_possible_capacity = get_next_power_of_2(mq_count);
      if (capacity < min_possible_capacity) {
        capacity = min_possible_capacity;
      }
    }
    PERF_UPDATE_VAR_DOMAIN_MAXIMUM(get_domain(), Queue_Max_Capacity, capacity)

    // see if we actually have to resize the queue
    if (capacity != mq_capacity) {
      Memory& memory = get_memory_object();
      auto buffer = (T*) memory.optional_calloc(capacity, sizeof(T));
      /*
       * If `optional_calloc()` returned `NULL`, it means that we're in the process of reclaiming memory
       * triggered by some thread that ran out of memory. Most likely (but not necessarily), server's
       * memory recovery procedure tried to post "free memory block" message to the optimizer, while
       * optimizer's queue was already full, so we got here... Since there is (again, only "likely") no
       * room for the "free memory block" message, there will be a delay before optimizer actually
       * receives it, but that's OK: if optimizer itself runs out of memory, server's memory recovery
       * procedure will immediately call its deallocation method directly; the worst thing that can
       * happen is optimizer processing reallocation requests twice (once directly, and another time when
       * the message finally gets through).
       */
      if (buffer) {
        if (mq_buffer) {
          PERF_INCREMENT_VAR_DOMAIN_COUNTER(get_domain(), Queue_Reallocations)
          for (c3_uint_t i = 0; i < mq_count; i++) {
            c3_uint_t j = (mq_get_index + i) & mq_index_mask;
            buffer[i] = std::move(mq_buffer[j]);
          }
          memory.free(mq_buffer, mq_capacity * sizeof(T));
        }
        mq_buffer = buffer;
        mq_capacity = capacity;
        mq_index_mask = capacity - 1;
        mq_get_index = 0;
        mq_put_index = mq_count & mq_index_mask; // in case count == capacity
      }
    }
  }

  void configure_max_capacity(c3_uint_t max_capacity) C3_FUNC_COLD {
    max_capacity = validate_capacity(max_capacity);
    if (max_capacity < mq_capacity) {
      /**
       * We try to shrink the queue just one time; if it fails (because there are more elements in the
       * queue than the maximum capacity we're trying to set), we simply adjust max capacity. BTW, the
       * queue could still be shrunk, just not to the level we requested.
       *
       * We cannot get here if the method is called from within ctor: queue capacity is zero at that point.
       */
      configure_capacity(max_capacity, false);
      if (max_capacity < mq_capacity) {
        max_capacity = mq_capacity;
      }
    }
    mq_max_capacity = max_capacity;
  }

  void reset_fields() C3_FUNC_COLD {
    mq_buffer = nullptr;
    mq_capacity = 0;
    mq_max_capacity = 0;
    mq_count = 0;
    mq_put_index = 0;
    mq_get_index = 0;
    mq_index_mask = 0;
  }

public:
  C3_FUNC_COLD MessageQueue(domain_t domain, host_object_t host, c3_uint_t capacity,
    c3_uint_t max_capacity = 0, c3_byte_t id = 0):
    SyncObject(domain, host, SO_MESSAGE_QUEUE, id) {
    reset_fields();
    configure_max_capacity(max_capacity > 0? max_capacity: capacity);
    configure_capacity(capacity, false);
  }
  MessageQueue(const MessageQueue&) = delete;
  MessageQueue(MessageQueue&&) = delete;
  ~MessageQueue() C3_FUNC_COLD { dispose(); }

  // not copyable
  MessageQueue& operator=(const MessageQueue&) = delete;
  MessageQueue& operator=(MessageQueue&&) = delete;

  void dispose() C3_FUNC_COLD {
    if (mq_buffer) {
      c3_assert(mq_capacity);
      get_memory_object().free(mq_buffer, mq_capacity * sizeof(T));
      reset_fields();
    }
  }

  // this method is meant to be used w/o locking, but it is still safe
  bool has_messages() const { return mq_count != 0; }

  /////////////////////////////////////////////////////////////////////////////
  // QUEUE CAPACITY MANIPULATION
  /////////////////////////////////////////////////////////////////////////////

  /*
   * Queue capacity (both current and maximum) is only retrieved for information purposes: "outer world"
   * does not need to know queue capacity to be able to efficiently use the queue. Retrieving ordinary fields
   * like we do below is similar to making those fields atomics and using `load()` with `memory_order_relaxed`;
   * in other words, it is safe (for our purposes) for all Linux platforms CyberCache is supposed to run on.
   */

  c3_uint_t get_capacity() C3LM_OFF(const) C3_FUNC_COLD {
    #if C3LM_ENABLED
    ThreadMessageQueueGetCapacityGuard guard(this);
    if (guard.check_passed()) {
      std::lock_guard<std::mutex> lock(mq_mutex);
      return mq_capacity;
    }
    return 0;
    #else
    return mq_capacity;
    #endif // C3LM_ENABLED
  }

  c3_uint_t get_max_capacity() C3LM_OFF(const) C3_FUNC_COLD {
    #if C3LM_ENABLED
    ThreadMessageQueueGetMaxCapacityGuard guard(this);
    if (guard.check_passed()) {
      std::lock_guard<std::mutex> lock(mq_mutex);
      return mq_max_capacity;
    }
    return 0;
    #else
    return mq_max_capacity;
    #endif // C3LM_ENABLED
  }

  c3_uint_t set_capacity(c3_uint_t capacity) C3_FUNC_COLD {
    c3_assert(mq_buffer);
    ThreadMessageQueueSetCapacityGuard guard(this);
    if (guard.check_passed()) {
      std::lock_guard<std::mutex> lock(mq_mutex);
      configure_capacity(capacity, false);
      return mq_capacity;
    }
    return 0;
  }

  c3_uint_t set_max_capacity(c3_uint_t max_capacity) C3_FUNC_COLD {
    c3_assert(mq_buffer);
    ThreadMessageQueueSetMaxCapacityGuard guard(this);
    if (guard.check_passed()) {
      std::lock_guard<std::mutex> lock(mq_mutex);
      configure_max_capacity(max_capacity);
      return mq_max_capacity;
    }
    return 0;
  }

  /////////////////////////////////////////////////////////////////////////////
  // QUEUE CONTENT MANIPULATION
  /////////////////////////////////////////////////////////////////////////////

  bool put(T&& o) {
    c3_assert(mq_buffer);
    ThreadMessageQueuePutGuard guard(this);
    if (guard.check_passed()) {
      std::unique_lock<std::mutex> lock(mq_mutex);
      if (mq_count == mq_capacity) {
        if (mq_capacity < mq_max_capacity) {
          configure_capacity(mq_capacity * 2, false);
        }
        if (mq_count == mq_capacity) { // did not succeed?
          PERF_INCREMENT_VAR_DOMAIN_COUNTER(get_domain(), Queue_Put_Waits)
          c3_assert(mq_get_index == mq_put_index);
          auto condition = [this]{ return mq_count < mq_capacity; };
          mq_not_full.wait(lock, condition);
        }
      }
      mq_buffer[mq_put_index++] = std::move(o);
      mq_put_index &= mq_index_mask;
      mq_count++;
      mq_not_empty.notify_one();
      return true;
    }
    return false;
  }

  bool put(T&& o, c3_uint_t msecs) {
    bool ready = false;
    c3_assert(mq_buffer);
    ThreadMessageQueuePutGuard guard(this);
    if (guard.check_passed()) {
      std::unique_lock<std::mutex> lock(mq_mutex);
      ready = true;
      if (mq_count == mq_capacity) {
        if (mq_capacity < mq_max_capacity) {
          configure_capacity(mq_capacity * 2, false);
        }
        if (mq_count == mq_capacity) { // did not succeed?
          PERF_INCREMENT_VAR_DOMAIN_COUNTER(get_domain(), Queue_Put_Waits)
          c3_assert(mq_get_index == mq_put_index);
          auto condition = [this]{ return mq_count < mq_capacity; };
          if (msecs > 0) {
            ready = mq_not_full.wait_for(lock, std::chrono::milliseconds(msecs), condition);
          } else {
            mq_not_full.wait(lock, condition);
          }
        }
      }
      if (ready) {
        mq_buffer[mq_put_index++] = std::move(o);
        mq_put_index &= mq_index_mask;
        mq_count++;
        mq_not_empty.notify_one();
      }
    }
    return ready;
  }

  T try_get() {
    c3_assert(mq_buffer);
    ThreadMessageQueueTryGetGuard guard(this);
    if (guard.check_passed()) {
      std::lock_guard<std::mutex> lock(mq_mutex);
      if (mq_count) {
        T result = std::move(mq_buffer[mq_get_index++]);
        mq_get_index &= mq_index_mask;
        mq_count--;
        mq_not_full.notify_one();
        return std::move(result);
      }
    }
    return std::move(T());
  }

  T get() {
    c3_assert(mq_buffer);
    ThreadMessageQueueGetGuard guard(this);
    if (guard.check_passed()) {
      std::unique_lock<std::mutex> lock(mq_mutex);
      if (mq_count == 0) {
        c3_assert(mq_get_index == mq_put_index);
        auto condition = [this]{ return mq_count > 0; };
        mq_not_empty.wait(lock, condition);
      }
      T result = std::move(mq_buffer[mq_get_index++]);
      mq_get_index &= mq_index_mask;
      mq_count--;
      mq_not_full.notify_one();
      return std::move(result);
    }
    return std::move(T());
  }

  T get(c3_uint_t msecs) {
    c3_assert(mq_buffer);
    ThreadMessageQueueGetGuard guard(this);
    if (guard.check_passed()) {
      std::unique_lock<std::mutex> lock(mq_mutex);
      bool ready = true;
      if (mq_count == 0) {
        c3_assert(mq_get_index == mq_put_index);
        auto condition = [this]{ return mq_count > 0; };
        if (msecs > 0) {
          ready = mq_not_empty.wait_for(lock, std::chrono::milliseconds(msecs), condition);
        } else {
          mq_not_empty.wait(lock, condition);
        }
      }
      if (ready) {
        T result = std::move(mq_buffer[mq_get_index++]);
        mq_get_index &= mq_index_mask;
        mq_count--;
        mq_not_full.notify_one();
        return std::move(result);
      }
    }
    return std::move(T());
  }
};

/**
 * This class differs from its base in that it can grow its message buffer beyond not only set maximum
 * capacity, but also hardcoded queue size limits -- if it is necessary to store a message "no matter what".
 *
 * A special method can then be used to contract the buffer if current number of objects in it permits that.
 *
 * Note: we have to use `this->` notation to access protected members of the base class because of this:
 *   https://isocpp.org/wiki/faq/templates#nondependent-name-lookup-members
 *
 * @tparam T Type of the message that this queue contains.
 */
template <class T> class CriticalMessageQueue: public MessageQueue<T> {
  c3_uint_t cmq_set_max_capacity; // maximum capacity set in the configuration

public:
  C3_FUNC_COLD CriticalMessageQueue(domain_t domain, host_object_t host, c3_uint_t capacity, c3_uint_t max_capacity = 0,
    c3_byte_t id = 0): MessageQueue<T>(domain, host, capacity, max_capacity, id) {
    cmq_set_max_capacity = this->mq_max_capacity;
  }

  bool put_always(T&& o) {
    c3_assert(this->mq_buffer);
    ThreadMessageQueuePutGuard guard(this);
    if (guard.check_passed()) {
      std::unique_lock<std::mutex> lock(this->mq_mutex);
      if (this->mq_count == this->mq_capacity) {
        if (this->mq_capacity == this->mq_max_capacity) {
          if (this->mq_max_capacity < (c3_uint_t) INT_MAX_VAL + 1) {
            PERF_INCREMENT_VAR_DOMAIN_COUNTER(this->get_domain(), Queue_Forced_Reallocations)
            this->mq_max_capacity *= 2;
          } else {
            PERF_INCREMENT_VAR_DOMAIN_COUNTER(this->get_domain(), Queue_Failed_Reallocations)
            /*
             * However much installed RAM we have, we cannot grow the queue any further since maximum
             * queue capacity would not fit its `c3_uint_t` type, and we would just lose entire queue
             * contents. Here, we go for a lesser evil, and just lose the record we were told to put.
             */
            return false;
          }
        }
        this->configure_capacity(this->mq_capacity * 2, true);
        c3_assert(this->mq_count < this->mq_capacity);
      }
      this->mq_buffer[this->mq_put_index++] = std::move(o);
      this->mq_put_index &= this->mq_index_mask;
      this->mq_count++;
      this->mq_not_empty.notify_one();
    }
    return true;
  }

  c3_uint_t store_and_set_max_capacity(c3_uint_t max_capacity) {
    cmq_set_max_capacity = this->validate_capacity(max_capacity);
    return this->set_max_capacity(cmq_set_max_capacity);
  }

  bool reduce_capacity() {
    /*
     * We do first check without locking the queue for the sake of efficiency: in vast majority of cases, queue
     * capacity will be already below or at the set threshold; even if max capacity gets changed by another thread
     * between the check and the `set_max_capacity()` call, in either "direction", it will do no harm.
     */
    c3_uint_t current_max_capacity = this->mq_max_capacity;
    if (cmq_set_max_capacity < current_max_capacity) {
      c3_uint_t new_max_capacity = this->set_max_capacity(cmq_set_max_capacity);
      if (new_max_capacity < current_max_capacity) {
        PERF_INCREMENT_VAR_DOMAIN_COUNTER(this->get_domain(), Queue_Capacity_Reductions)
        return true;
      }
    }
    return false;
  }
};

}

#endif // _MT_MESSAGE_QUEUE_H
