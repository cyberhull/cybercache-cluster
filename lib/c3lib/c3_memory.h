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
 * Memory manager and utilities.
 */
#ifndef _C3_MEMORY_H
#define _C3_MEMORY_H

#include "c3_types.h"
#include <atomic>

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// GENERAL-PURPOSE UTILITIES
///////////////////////////////////////////////////////////////////////////////

constexpr c3_ulong_t kilobytes2bytes(size_t num) noexcept { return 1024LL * num; }
constexpr c3_ulong_t megabytes2bytes(size_t num) noexcept { return kilobytes2bytes(1024) * num; }
constexpr c3_ulong_t gigabytes2bytes(size_t num) noexcept { return megabytes2bytes(1024) * num; }
constexpr c3_ulong_t terabytes2bytes(size_t num) noexcept { return gigabytes2bytes(1024) * num; }

///////////////////////////////////////////////////////////////////////////////
// HOST ENVIRONMENT SUPPORT
///////////////////////////////////////////////////////////////////////////////

/// Memory allocation-related methods that host implementation has to provide to the library
class MemoryInterface {
public:
 /**
   * Inform host that a thread ran out of memory; when this method returns, some extra memory should have
   * been made available to the calling thread.
   */
  virtual void begin_memory_deallocation(size_t size) = 0;
  /**
   * Inform host that an extra memory block (of configurable size) had been freed, so threads that
   * previously called `begin_memory_deallocation()` should resume, and try memory allocation again.
   */
  virtual void end_memory_deallocation() = 0;
};

///////////////////////////////////////////////////////////////////////////////
// MEMORY MANAGER
///////////////////////////////////////////////////////////////////////////////

#define C3_MEMORY_DEBUG 0

/**
 * Memory manager whose purpose is to maintain memory quotas for various application "domains", and provide
 * some basic checks. It is possible to allocate a block in one `Memory` object and deallocate it using
 * another; it will work (except in instrumented versions, where it will most probably cause assertion
 * failures), but it would break tracking domain quotas. Therefore, if, say, at the time of the
 * allocation it was not known what domain will eventually use the block, the block should be allocated
 * using `global_memory` object, and then "transferred" to the proper domain using
 *   <another-domain>.transfer_used_size(global_memory, <block-size);
 *
 * We use "relaxed" memory order throughout memory manager for inter-thread synchronization; this order
 * may not achieve exact results (in that, say, `get_used_size()` may return "outdated" value if some
 * other thread just incremented the counter), but a) utmost "precision" it is not important for its
 * intended purpose, and b) it is much faster, which *is* essential.
 */
class Memory {
  // default value for `m_max_size`; must be outside of MIN_QUOTA..MAX_QUOTA range
  static constexpr c3_ulong_t DEFAULT_QUOTA = 0;
  // minimum allowed value for `m_max_size`
  static constexpr c3_ulong_t MIN_QUOTA = megabytes2bytes(8);
  // maximum allowed value for `m_max_size`
  static constexpr c3_ulong_t MAX_QUOTA = LIMITED_MEMORY_QUOTA?
    gigabytes2bytes(32): terabytes2bytes(128);
  // pointers to all memory objects
  static Memory* m_object_refs[DOMAIN_NUMBER_OF_ELEMENTS];
  // pointer to host interface implementation
  static MemoryInterface* m_interface;

  // allocation quota for this `Memory` object
  std::atomic_ullong m_max_size;

  // how much memory was actually allocated using this object; can become
  // bigger than `m_max_size`, code watching this `Memory` object may start
  // deallocating memory until `m_used_size` becomes lower than quota
  std::atomic_ullong m_used_size;

  // domain to which this memory object belongs
  const domain_t m_domain;

  // host interface accessors
  static MemoryInterface& get_interface() {
    c3_assert(m_interface);
    return *m_interface;
  }
  static void set_interface(MemoryInterface* host_interface) {
    c3_assert(host_interface);
    m_interface = host_interface;
  }

  #if C3_MEMORY_DEBUG
  static void report(const Memory &memory, const char *action, void *block, c3_ulong_t change);
  #endif

public:
  explicit Memory(domain_t domain = DOMAIN_GLOBAL) noexcept C3_FUNC_COLD;
  Memory(Memory&) = delete;
  Memory(Memory&&) = delete;
  ~Memory() = default;

  static void configure(MemoryInterface* host_interface) { set_interface(host_interface); }

  // operators
  void operator=(Memory&) = delete;
  void operator=(Memory&&) = delete;

  // identification
  domain_t get_domain() const { return m_domain; }
  static Memory& get_memory_object(domain_t domain) {
    c3_assert(domain < DOMAIN_NUMBER_OF_ELEMENTS);
    return *m_object_refs[domain];
  }
  static const char* get_domain_name(domain_t domain) C3_FUNC_COLD;
  const char* get_domain_name() const { return get_domain_name(m_domain); }

  // limits
  static c3_ulong_t get_min_quota() { return MIN_QUOTA; }
  static c3_ulong_t get_max_quota() { return MAX_QUOTA; }

  // quota management
  bool is_quota_set() const { return m_max_size != DEFAULT_QUOTA; }
  c3_ulong_t get_quota() const { return m_max_size.load(std::memory_order_relaxed); }
  void set_quota(c3_ulong_t new_size) C3_FUNC_COLD;
  c3_ulong_t get_used_size() const { return m_used_size.load(std::memory_order_relaxed); }
  void transfer_used_size(Memory& from, size_t size);

  // heap management
  void* alloc(size_t size) C3_FUNC_MALLOC C3_FUNC_NONNULL_RETURN;
  void* calloc(size_t nelems, size_t esize) C3_FUNC_MALLOC C3_FUNC_NONNULL_RETURN;
  void* optional_calloc(size_t nelems, size_t esize) C3_FUNC_MALLOC;
  void* realloc(void* buff, size_t new_size, size_t old_size) C3_FUNC_NONNULL_ARGS C3_FUNC_NONNULL_RETURN;
  static size_t get_block_size(void* buff);
  void* inplace_realloc(void* buff, size_t new_size, size_t old_size);
  void free(void* buff, size_t size) C3_FUNC_NONNULL_ARGS;
  static bool heap_check() C3_FUNC_COLD;
};

extern Memory global_memory;
extern Memory session_memory;
extern Memory fpc_memory;

///////////////////////////////////////////////////////////////////////////////
// CONVENIENCE FUNCTIONS FOR MEMORY ALLOCATION AND DISPOSAL
///////////////////////////////////////////////////////////////////////////////

inline void* allocate_memory(size_t size) {
  return global_memory.alloc(size);
}
inline void* reallocate_memory(void* buff, size_t new_size, size_t old_size) {
  return global_memory.realloc(buff, new_size, old_size);
}
inline void free_memory(void* buff, size_t size) {
  return global_memory.free(buff, size);
}

template <class T> T* alloc() {
  return (T*) global_memory.alloc(sizeof(T));
}
template <class T> T* alloc(size_t size) {
  return (T*) global_memory.alloc(size);
}
template <class T> T* alloc(Memory& memory) {
  return (T*) memory.alloc(sizeof(T));
}
template <class T> T* alloc(Memory& memory, size_t size) {
  return (T*) memory.alloc(size);
}
template <class T> T* alloc(domain_t domain) {
  return (T*) Memory::get_memory_object(domain).alloc(sizeof(T));
}
template <class T> T* alloc(domain_t domain, size_t size) {
  return (T*) Memory::get_memory_object(domain).alloc(size);
}

template <class T> void dealloc(T* p) {
  global_memory.free(p, sizeof(T));
}
template <class T> void dealloc(Memory& memory, T* p) {
  memory.free(p, sizeof(T));
}
template <class T> void dealloc(domain_t domain, T* p) {
  Memory::get_memory_object(domain).free(p, sizeof(T));
}

template <class T> void dispose(T* p) {
  p->~T();
  dealloc<T>(p);
}
template <class T> void dispose(Memory& memory, T* p) {
  p->~T();
  dealloc<T>(memory, p);
}
template <class T> void dispose(domain_t domain, T* p) {
  p->~T();
  dealloc<T>(domain, p);
}

///////////////////////////////////////////////////////////////////////////////
// SUPPORT FOR DIFFERENT ALLOCATION STRATEGIES
///////////////////////////////////////////////////////////////////////////////

/**
 * Base class for all allocators, i.e. classes that implement non-standard allocations strategies (for
 * instance, when allocationg a buffer requires calling class method, which then handles allocating of
 * the buffer internally, and may not use heap at all).
 */
class Allocator {
public:
  virtual c3_byte_t* alloc(c3_uint_t size) = 0;
  virtual void free(void* buff, c3_uint_t size) = 0;
};

/// Default allocator that uses Memory object
class DefaultAllocator: public Allocator {
  Memory& da_memory;

public:
  explicit DefaultAllocator(Memory& memory = global_memory): da_memory(memory) {}
  c3_byte_t* alloc(c3_uint_t size) override;
  void free(void* buff, c3_uint_t size) override;
};

} // namespace CyberCache

#endif // _C3_MEMORY_H
