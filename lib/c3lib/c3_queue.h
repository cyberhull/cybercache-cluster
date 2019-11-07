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
#ifndef _C3_QUEUE_H
#define _C3_QUEUE_H

#include "c3_memory.h"

namespace CyberCache {

/**
 * Unsynchronized queue of dynamic capacity: if the queue if full *and* maximum capacity (supplied in ctor
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
 * @see MessageQueue
 *
 * @tparam T Type of the message that this queue contains.
 */
template <class T> class Queue {
  static constexpr c3_uint_t  Q_MIN_ALLOWED_CAPACITY = 1;
  static constexpr c3_uint_t  Q_MAX_ALLOWED_CAPACITY = USHORT_MAX_VAL + 1;

  Memory&   q_memory;       // memory object used for buffer allocation/freeing
  T*        q_buffer;       // ring buffer with queued elements
  c3_uint_t q_max_capacity; // currently allowed max queue capacity
  c3_uint_t q_capacity;     // current queue capacity (max number of elements)
  c3_uint_t q_count;        // current number of elements in the queue
  c3_uint_t q_put_index;    // next element will be added at this index
  c3_uint_t q_get_index;    // next element will be retrieved at this index
  c3_uint_t q_index_mask;   // bit mask for indices

  static constexpr c3_uint_t validate_capacity(c3_uint_t capacity) {
    if (capacity < Q_MIN_ALLOWED_CAPACITY) {
      capacity = Q_MIN_ALLOWED_CAPACITY;
    }
    if (capacity > Q_MAX_ALLOWED_CAPACITY) {
      capacity = Q_MAX_ALLOWED_CAPACITY;
    }
    return get_next_power_of_2(capacity);
  }

  void configure_capacity(c3_uint_t capacity) {
    // validate requested capacity
    capacity = validate_capacity(capacity);
    if (capacity > q_max_capacity) {
      capacity = q_max_capacity;
    }
    c3_uint_t min_possible_capacity = get_next_power_of_2(q_count);
    if (capacity < min_possible_capacity) {
      capacity = min_possible_capacity;
    }
    PERF_UPDATE_VAR_DOMAIN_MAXIMUM(get_domain(), Local_Queue_Max_Capacity, capacity)

    // see if we actually have to resize the queue
    if (capacity != q_capacity) {
      auto buffer = (T*) q_memory.calloc(capacity, sizeof(T));
      if (q_buffer) {
        PERF_INCREMENT_VAR_DOMAIN_COUNTER(get_domain(), Local_Queue_Reallocations)
        for (c3_uint_t i = 0; i < q_count; i++) {
          c3_uint_t j = (q_get_index + i) & q_index_mask;
          buffer[i] = std::move(q_buffer[j]);
        }
        q_memory.free(q_buffer, q_capacity * sizeof(T));
      }
      q_buffer = buffer;
      q_capacity = capacity;
      q_index_mask = capacity - 1;
      q_get_index = 0;
      q_put_index = q_count & q_index_mask; // in case count == capacity
    }
  }

  void configure_max_capacity(c3_uint_t max_capacity) C3_FUNC_COLD {
    max_capacity = validate_capacity(max_capacity);
    if (max_capacity < q_capacity) {
      /**
       * We try to shrink the queue just one time; if it fails (because there are more elements in the
       * queue than the maximum capacity we're trying to set), we simply adjust max capacity. BTW, the
       * queue could still be shrunk, just not to the level we requested.
       *
       * We cannot get here if the method is called from within ctor: queue capacity is zero at that point.
       */
      configure_capacity(max_capacity);
      if (max_capacity < q_capacity) {
        max_capacity = q_capacity;
      }
    }
    q_max_capacity = max_capacity;
  }

  void reset_fields() C3_FUNC_COLD {
    q_buffer = nullptr;
    q_capacity = 0;
    q_max_capacity = 0;
    q_count = 0;
    q_put_index = 0;
    q_get_index = 0;
    q_index_mask = 0;
  }

public:
  C3_FUNC_COLD Queue(domain_t domain, c3_uint_t capacity, c3_uint_t max_capacity = 0):
    q_memory(Memory::get_memory_object(domain)) {
    reset_fields();
    configure_max_capacity(max_capacity > 0? max_capacity: capacity);
    configure_capacity(capacity);
  }
  Queue(const Queue&) = delete;
  Queue(Queue&&) = delete;
  ~Queue() C3_FUNC_COLD { dispose(); }

  // not copyable
  Queue& operator=(const Queue&) = delete;
  Queue& operator=(Queue&&) = delete;

  void dispose() C3_FUNC_COLD {
    if (q_buffer) {
      c3_assert(q_capacity);
      q_memory.free(q_buffer, q_capacity * sizeof(T));
      reset_fields();
    }
  }

  domain_t get_domain() const { return q_memory.get_domain(); }
  bool has_messages() const { return q_count != 0; }
  c3_uint_t get_count() const { return q_count; }

  /////////////////////////////////////////////////////////////////////////////
  // QUEUE CAPACITY MANIPULATION
  /////////////////////////////////////////////////////////////////////////////

  c3_uint_t get_capacity() const { return q_capacity; }

  c3_uint_t get_max_capacity() const { return q_max_capacity; }

  c3_uint_t set_capacity(c3_uint_t capacity) C3_FUNC_COLD {
    c3_assert(q_buffer);
    configure_capacity(capacity);
    return q_capacity;
  }

  c3_uint_t set_max_capacity(c3_uint_t max_capacity) C3_FUNC_COLD {
    c3_assert(q_buffer);
    configure_max_capacity(max_capacity);
    return q_max_capacity;
  }

  /////////////////////////////////////////////////////////////////////////////
  // QUEUE CONTENT MANIPULATION
  /////////////////////////////////////////////////////////////////////////////

  bool put(T&& o) {
    c3_assert(q_buffer);
      if (q_count == q_capacity) {
        if (q_capacity < q_max_capacity) {
          configure_capacity(q_capacity * 2);
        }
        if (q_count == q_capacity) { // did not succeed?
          c3_assert(q_get_index == q_put_index);
          PERF_INCREMENT_VAR_DOMAIN_COUNTER(get_domain(), Local_Queue_Put_Failures)
          return false;
        }
      }
      q_buffer[q_put_index++] = std::move(o);
      q_put_index &= q_index_mask;
      q_count++;
      return true;
  }

  T get() {
    c3_assert(q_buffer);
    if (q_count == 0) {
      c3_assert(q_get_index == q_put_index);
      return std::move(T());
    }
    T result = std::move(q_buffer[q_get_index++]);
    q_get_index &= q_index_mask;
    q_count--;
    return std::move(result);
  }
};

}

#endif // _C3_QUEUE_H
