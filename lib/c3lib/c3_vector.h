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
 * Template vector class.
 */
#ifndef _C3_VECTOR_H
#define _C3_VECTOR_H

#include <cstdlib>
#include <cstring>
#include <utility>
#include <new>
#include "c3_memory.h"

namespace CyberCache {

/**
 * Configurable and sortable variable-size array; meant to be used for building temporary collections
 * and/or stacks (*not* queues). The following features have to be taken into account when using it:
 *
 * - `Vector` does *not* "own" elements stored in it. `clear()`-ing or `pop()`ing a particular value
 *   means its removal from the vector; likewise, `clear()`-ing all values means just emptying internal
 *   array. If an element has a destructor, that destructor will *not* be called during any of these
 *   operations. Similarly, creating a copy of an array is implemented as copying internal memory buffer;
 *   constructors will not be called on individual elements being copied.
 *
 * - Array subscript operator returns a reference to contained element. Its callers should therefore
 *   either dereference returned value immediately (by assigning it to a *value* of type `T`), or be
 *   extra-careful and *not* call `push()` while reference returned from a call to subscript operator is
 *   being used, because `push()` may cause re-allocation of internal storage and thus invalidation of
 *   all references to it.
 *
 * - How `clear(<element-index>)` removes the element depends on whether the element is very last in the
 *   array, or not. If it is last, it is removed by simple decrement of internal element count, and
 *   `clear()` returns `false`. If it is *not* last, `clear()` zeroes out its slot, and returns `true` to
 *   indicate that a "hole" had been left in the array; number of elements returned by `get_size()` will
 *   *not* change, only `get_count()` will (if however element count becomes zero, `clear()` will reset
 *   size and return `false`); if calling code then needs to add another element after getting `true`
 *   from `clear()`, it should probably do so by assigning new value to the cleared slot using subscript
 *   operator.
 *
 * - `pop()` does not "know" if an element it is returning had been `clear()`-ed or not. It is advised
 *   not to mix the two intended usage styles, and always use `Vector` as *either* a temporary
 *   collection, *or* a stack; if, however, such mixup is necessary, callers have to check `pop()`-ed
 *   values to see if they are valid. Likewise, `sort()` method "knows" nothing about "holes" in the
 *   array; if `clear()` was used on `Vector` element(s), then user-provided comparator should take that
 *   into account and, if `shrink` flag is specified to `sort()`, make sure that "empty" elements are
 *   "bigger" than valid, so that to push them to the tail of the buffer.
 *
 * - Data type (`class T`) must have default constructor (a constructor without arguments); this is
 *   needed to a) create temporary variable that will be returned by accessors in case data retrieval has
 *   failed for some reason, b) initialize empty slots so that assigning rvalues to them would not cause
 *   assigment operators to "swap" some invalid state into temporary variables that would then cause
 *   those temporary variables to blow everything up when their dtors are called.
 */
template <class T> class Vector {
  template <typename A, typename B> friend class DescriptorVector;

  static const c3_uint_t DEFAULT_INIT_CAPACITY = 32;
  static const c3_uint_t DEFAULT_CAPACITY_INC = 16;

  T*        v_buffer;        // buffer with data; may be NULL
  size_t    v_count;         // number of used ("occupied") slots in `v_buffer`
  size_t    v_size;          // total number of elements (*not* size in bytes)
  size_t    v_capacity;      // how many elements can be held without reallocation
  c3_uint_t v_init_capacity; // capacity for the very first allocation
  c3_uint_t v_capacity_inc;  // capacity increment

  // comparison method to be used in sort()
  static int C3_CDECL comparator(const void* e1, const void* e2)
  {
    const T& r1 = *(const T*) e1;
    const T& r2 = *(const T*) e2;
    // `T` must implement comparison operators
    if (r1 > r2) return 1;
    if (r1 < r2) return -1;
    return 0;
  }

  void init(c3_uint_t init_capacity, c3_uint_t capacity_inc) {
    assert(init_capacity && capacity_inc);
    v_buffer = nullptr;
    v_count = 0;
    v_size = 0;
    v_capacity = 0;
    v_init_capacity = init_capacity;
    v_capacity_inc = capacity_inc;
  }

  void init_with_defaults() {
    init(DEFAULT_INIT_CAPACITY, DEFAULT_CAPACITY_INC);
  }

  void validate() const {
    // using parentheses to satisfy GCC's -Wall
    c3_assert((v_buffer && v_capacity) || (!v_buffer && !v_capacity));
    c3_assert(v_init_capacity && v_capacity_inc);
    c3_assert(v_capacity == 0 || v_capacity >= v_init_capacity);
    c3_assert(v_count <= v_size && v_size <= v_capacity);
  }

  Vector& copy(const Vector& that) {
    that.validate();
    v_init_capacity = that.v_init_capacity;
    v_capacity_inc = that.v_capacity_inc;
    v_count = that.v_count;
    v_size = that.v_size;
    v_capacity = that.v_capacity;
    if (v_capacity) {
      v_buffer = (T*) allocate_memory(v_capacity * sizeof(T));
      if (v_size) {
        std::memcpy(v_buffer, that.v_buffer, v_size * sizeof(T));
      }
    } else {
      v_buffer = nullptr;
    }
    validate();
    return *this;
  }

public:
  // constructors
  Vector() {
    init_with_defaults();
  }
  explicit Vector(c3_uint_t init_capacity) {
    init(init_capacity, DEFAULT_CAPACITY_INC);
  }
  Vector(c3_uint_t init_capacity, c3_uint_t capacity_inc) {
    init(init_capacity, capacity_inc);
  }
  Vector(const Vector& that) {
    copy(that);
  }
  Vector(Vector&& that) {
    that.validate();
    init_with_defaults();
    std::swap(*this, that);
  }
  // destructor
  ~Vector() {
    validate();
    deallocate();
  }

  // operators
  Vector& operator=(const Vector& that) {
    if (this != &that) {
      deallocate();
      return copy(that);
    } else {
      return *this;
    }
  }
  Vector& operator=(Vector&& that) {
    that.validate();
    std::swap(*this, that);
    return *this;
  }
  T& get(size_t i) {
    #ifdef C3_FASTEST
    return v_buffer[i];
    #else // !C3_FASTEST
    validate();
    if (v_buffer && i < v_size) {
      return v_buffer[i];
    } else {
      assert_failure();
      // just to satisfy the compiler
      static T tmp;
      return tmp;
    }
    #endif // C3_FASTEST
  }
  const T& operator[](size_t i) const {
    #ifdef C3_FASTEST
    return v_buffer[i];
    #else // !C3_FASTEST
    validate();
    if (v_buffer && i < v_size) {
      return v_buffer[i];
    } else {
      assert_failure();
      // just to satisfy the compiler
      static T tmp;
      return tmp;
    }
    #endif // C3_FASTEST
  }

  // accessors
  void push(T&& e) {
    validate();
    if (v_capacity == 0) {
      v_buffer = (T*) allocate_memory(v_init_capacity * sizeof(T));
      for (size_t i = 0; i < v_init_capacity; i++) {
        new (v_buffer + i) T();
      }
      v_capacity = v_init_capacity;
    } else if (v_capacity == v_size) {
      size_t new_capacity = v_capacity + v_capacity_inc;
      v_buffer = (T*) reallocate_memory(v_buffer, new_capacity * sizeof(T), v_capacity * sizeof(T));
      for (size_t i = v_capacity; i < new_capacity; i++) {
        new (v_buffer + i) T();
      }
      v_capacity = new_capacity;
    }
    v_buffer[v_size++] = std::move(e);
    v_count++;
    validate();
  }
  T pop() {
    #ifdef C3_FASTEST
    v_count--;
    return std::move(v_buffer[--v_size]);
    #else // !C3_FASTEST
    validate();
    if (v_buffer && v_size) {
      v_count--;
      return std::move(v_buffer[--v_size]);
    } else {
      assert_failure();
      // just to satisfy the compiler
      T tmp;
      return std::move(tmp);
    }
    #endif // C3_FASTEST
  }

  // clearing an element, or entire array
  bool clear(size_t i) {
    validate();
    #ifndef C3_FASTEST
    if (v_buffer && i < v_size) {
    #endif // C3_FASTEST
      new (v_buffer + i) T();
      if (--v_count) {
        if (i == v_size - 1) {
          v_size--;
          return false;
        } else {
          // we left a "hole" in the array
          return true;
        }
      } else { // if was the very last element
        v_size = 0;
        return false;
      }
    #ifndef C3_FASTEST
    } else { // trying to clear() non-existent element
      assert_failure();
      return false;
    }
    #endif // C3_FASTEST
  }
  void clear() {
    validate();
    if (v_count) {
      c3_assert(v_buffer && v_size);
      for (size_t i = 0; i < v_size; i++) {
        new (v_buffer + i) T();
      }
      v_count = 0;
    }
    v_size = 0;
  }
  void deallocate() {
    validate();
    if (v_capacity) {
      free_memory(v_buffer, v_capacity * sizeof(T));
      v_buffer = nullptr;
      v_capacity = 0;
      v_count = 0;
      v_size = 0;
    }
  }

  // utilities
  size_t get_count() const { return v_count; }
  size_t get_size() const { return v_size; }
  void set_init_capacity(c3_uint_t init_capacity) {
    assert(init_capacity);
    c3_assert(v_capacity == 0); // too late?..
    v_init_capacity = init_capacity;
  }
  void set_capacity_inc(c3_uint_t capacity_inc) {
    assert(capacity_inc);
    v_capacity_inc = capacity_inc;
  }

  // sort elements using default or user-provided comparison function
  void sort() C3_FUNC_COLD {
    validate();
    std::qsort(v_buffer, v_size, sizeof(T), comparator);
  }
  void sort(int C3_CDECL (*comp)(const void* e1, const void* e2), bool shrink = false) C3_FUNC_COLD {
    validate();
    std::qsort(v_buffer, v_size, sizeof(T), comp);
    if (shrink) {
      v_size = v_count;
    }
  }
};

/**
 * A fixed-size vector supporting only basic operations available in `Vector` class. Meant to be used
 * as a drop-in replacement of `Vector` when size is known at compile time.
 *
 * @tparam T Type of the vector element; must have default ctor initializing elements to "invalid" state.
 * @tparam N Maximum number of elements in the vector.
 */
template <class T, c3_uint_t N> class FixedVector {
  T         v_buffer[N]; // buffer with data; may be NULL
  c3_uint_t v_count;     // number of used ("occupied") slots in `v_buffer`

public:
  // constructors
  FixedVector() {
    v_count = 0;
  }
  FixedVector(const FixedVector&) = delete;
  FixedVector(FixedVector&&) = delete;
  ~FixedVector() = default;

  // operators
  void operator=(const FixedVector& that) = delete;
  void operator=(FixedVector&& that) = delete;

  T& get(c3_uint_t i) {
    assert(i < v_count);
    return v_buffer[i];
  }
  const T& operator[](c3_uint_t i) const {
    assert(i < v_count);
    return v_buffer[i];
  }

  // accessors
  void push(T&& e) {
    assert(v_count < N);
    v_buffer[v_count++] = std::move(e);
  }
  T pop() {
    assert(v_count > 0);
    return std::move(v_buffer[--v_count]);
  }

  // utilities
  size_t get_count() const { return v_count; }
  void clear() {
    #ifdef C3_SAFEST
    for (c3_uint_t i = 0; i < v_count; i++) {
      new (v_buffer + i) T();
    }
    #endif
    v_count = 0;
  }
};

} // namespace CyberCache

#endif // _C3_VECTOR_H
