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
 * A vector that is very similar to `Vector` except that:
 *
 * - it has `add()` and `remove()` methods instead of `push()` and
 *   `clear(<index>)`, respectively, and does not have a counterpart of `pop()`;
 *   its `add()` method returns index of the newly inserted element,
 *
 * - it can "fill the holes" (from elements removed from the middle of the
 *   vector) when adding new elements; to do that, it maintains internal vector
 *   of indices of removed elements.
 *
 * These properties make it especially suitable for the implementation of
 * various descriptors, hence the name.
 */
#ifndef _C3_DESCRIPTOR_VECTOR_H
#define _C3_DESCRIPTOR_VECTOR_H

#include "c3_vector.h"

namespace CyberCache {

template <class T, class N> class DescriptorVector
{
  Vector<T> dv_data;    // actual data that DescriptorVector holds
  Vector<N> dv_removed; // indices of removed elements

  void validate() const {
    c3_assert(!(dv_removed.v_count && (dv_data.v_count == dv_data.v_size || dv_data.v_count == 0)));
  }

  void copy(const DescriptorVector& that) {
    dv_data.copy(that.dv_data);
    dv_removed.copy(that.dv_removed);
    validate();
  }

public:
  // constructors
  DescriptorVector() = default;
  DescriptorVector(c3_uint_t init_data_capacity, c3_uint_t init_removed_capacity):
    dv_data(init_data_capacity), dv_removed(init_removed_capacity) {
  }
  DescriptorVector(c3_uint_t init_data_capacity, c3_uint_t data_capacity_inc,
    c3_uint_t init_removed_capacity, c3_uint_t removed_capacity_inc):
    dv_data(init_data_capacity, data_capacity_inc),
    dv_removed(init_removed_capacity, removed_capacity_inc) {
  }
  DescriptorVector(const DescriptorVector& that):
    dv_data(that.dv_data), dv_removed(that.dv_removed) {
    validate();
  }
  DescriptorVector(DescriptorVector&& that) noexcept {
    validate();
    that.validate();
    std::swap(dv_data, that.dv_data);
    std::swap(dv_removed, that.dv_removed);
  }
  // destructor
  ~DescriptorVector() {
    validate();
  }

  // operators
  DescriptorVector& operator=(const DescriptorVector& that) {
    if (this != &that) {
      dv_data = that.dv_data;
      dv_removed = that.dv_removed;
      validate();
    }
    return *this;
  }
  DescriptorVector& operator=(DescriptorVector&& that) noexcept {
    validate();
    that.validate();
    std::swap(dv_data, that.dv_data);
    std::swap(dv_removed, that.dv_removed);
    return *this;
  }
  T& operator[](N i) const {
    #ifdef C3_FASTEST
    return dv_data.v_buffer[i];
    #else // !C3_FASTEST
    validate();
    if (dv_data.v_buffer && i < (N) dv_data.v_size) {
      return dv_data.v_buffer[i];
    } else {
      assert_failure();
      // just to satisfy the compiler
      static T tmp;
      return tmp;
    }
    #endif // C3_FASTEST
  }

  // accessors
  N add(T&& e) {
    validate();
    N i;
    if (dv_removed.v_count) {
      i = dv_removed.pop();
      dv_data[i] = std::move(e);
      dv_data.v_count++;
    } else {
      i = dv_data.v_size;
      dv_data.push(std::move(e));
    }
    validate();
    return i;
  }
  void remove(N i) {
    #ifndef C3_FASTEST
    validate();
    if (i < (N) dv_data.v_size) {
    #endif // C3_FASTEST
      if (dv_data.clear(i)) {
        // we just made a "hole" in the data vector
        dv_removed.push(std::move(i));
      } else if (dv_data.v_size == 0) {
        // it was the very last used data slot
        dv_removed.clear();
      }
    #ifndef C3_FASTEST
    } else {
      assert_failure();
    }
    validate();
    #endif // C3_FASTEST
  }

  // clear or deallocate internal structures
  void clear() {
    validate();
    dv_data.clear();
    dv_removed.clear();
  }
  void deallocate() {
    validate();
    dv_data.deallocate();
    dv_removed.deallocate();
  }

  // utilities
  size_t get_count() const { return dv_data.v_count; }
  size_t get_size() const { return dv_data.v_size; }
  void set_init_data_capacity(c3_uint_t init_capacity) {
    dv_data.set_init_capacity(init_capacity);
  }
  void set_data_capacity_inc(c3_uint_t capacity_inc) {
    dv_data.set_capacity_inc(capacity_inc);
  }
  void set_init_removed_capacity(c3_uint_t init_capacity) {
    dv_removed.set_init_capacity(init_capacity);
  }
  void set_removed_capacity_inc(c3_uint_t capacity_inc) {
    dv_removed.set_capacity_inc(capacity_inc);
  }

  // sort elements using default or user-provided comparison function
  void sort() {
    validate();
    dv_data.sort();
  }
  void sort(int C3_CDECL (*comp)(const void* e1, const void* e2), bool shrink = false) {
    validate();
    dv_data.sort(comp, shrink);
  }
};

} // namespace CyberCache

#endif // _C3_DESCRIPTOR_VECTOR_H
