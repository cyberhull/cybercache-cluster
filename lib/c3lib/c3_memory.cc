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
#include "c3_build.h"

#include <cstdlib>
#include <cstring>
#include <malloc.h>

#include "c3_memory.h"
#include "c3_logger.h"
#include "c3_profiler_defs.h"

namespace CyberCache {

Memory global_memory(DOMAIN_GLOBAL);
Memory session_memory(DOMAIN_SESSION);
Memory fpc_memory(DOMAIN_FPC);

Memory* Memory::m_object_refs[DOMAIN_NUMBER_OF_ELEMENTS] = {
  &global_memory,  // DOMAIN_INVALID (so that we don't blow up in case of error)
  &global_memory,  // DOMAIN_GLOBAL
  &session_memory, // DOMAIN_SESSION
  &fpc_memory      // DOMAIN_FPC
};

MemoryInterface* Memory::m_interface;

///////////////////////////////////////////////////////////////////////////////
// INITIALIZATION
///////////////////////////////////////////////////////////////////////////////

Memory::Memory(domain_t domain) noexcept: m_domain(domain) {
  m_max_size.store(DEFAULT_QUOTA, std::memory_order_relaxed);
 /*
  * We do not set `m_used_size` here because by the time ctor is called, some memory allocations
  * had already been processed; so we rely on it being zero at app startup.
  *
  * It is OK that `m_max_size` is not yet set during first allocations: it is only used by optimizers
  * during scheduled optimization runs much later.
  */
  #if C3_MEMORY_DEBUG
  printf("MEMORY: initialized '%s' domain\n", get_domain_name());
  #endif
}

const char* Memory::get_domain_name(domain_t domain) {
  switch (domain) {
    case DOMAIN_GLOBAL:
      return "global";
    case DOMAIN_SESSION:
      return "session";
    case DOMAIN_FPC:
      return "FPC";
    default:
      return "<INVALID>";
  }
}

///////////////////////////////////////////////////////////////////////////////
// DEBUGGING
///////////////////////////////////////////////////////////////////////////////

#if C3_MEMORY_DEBUG

#define C3M_DEBUG_GLOBAL  1
#define C3M_DEBUG_SESSION 1
#define C3M_DEBUG_FPC     1

#define C3M_TRANSFER_FROM(memory, amount) report(memory, "transfer from domain", nullptr, amount)
#define C3M_TRANSFER_TO(memory, amount) report(memory, "transfer to domain", nullptr, amount)
#define C3M_ALLOC(block, amount) report(*this, "alloc", block, amount)
#define C3M_CALLOC(block, nelems, esize) report(*this, "calloc", block, nelems * esize)
#define C3M_OPT_CALLOC(block, nelems, esize) report(*this, "optional calloc", block, nelems * esize)
#define C3M_REALLOC_GROW(block, amount) report(*this, "realloc (grow)", block, amount)
#define C3M_REALLOC_SHRINK(block, amount) report(*this, "realloc (shrink)", block, amount)
#define C3M_FREE(block, amount) report(*this, "free", block, amount)

void Memory::report(const Memory &memory, const char *action, void *block, c3_ulong_t change) {
  c3_ulong_t used_size = memory.get_used_size();
  size_t block_size = block? get_block_size(block): 0;
  domain_t domain = memory.get_domain();
  const char* domain_name;
  if (domain) {
    domain_name = get_domain_name(domain);
  } else {
    if (&memory == &global_memory) {
      domain = DOMAIN_GLOBAL;
      domain_name = "uninitialized global";
    } else if (&memory == &session_memory) {
      domain = DOMAIN_SESSION;
      domain_name = "uninitialized session";
    } else if (&memory == &fpc_memory) {
      domain = DOMAIN_FPC;
      domain_name = "uninitialized FPC";
    } else {
      domain_name = "INVALID";
    }
  }
  if ((domain == DOMAIN_GLOBAL && C3M_DEBUG_GLOBAL) ||
    (domain == DOMAIN_SESSION && C3M_DEBUG_SESSION) ||
    (domain == DOMAIN_FPC && C3M_DEBUG_FPC)) {
    printf("MEMORY [%s] op: '%s' block: %lu total: %llu change: %llu\n",
           domain_name, action, block_size, used_size, change);
  }
}

#else // !C3_MEMORY_DEBUG

#define C3M_TRANSFER_FROM(memory, amount) ((void)0)
#define C3M_TRANSFER_TO(memory, amount) ((void)0)
#define C3M_ALLOC(block, amount) ((void)0)
#define C3M_CALLOC(block, nelems, esize) ((void)0)
#define C3M_OPT_CALLOC(block, nelems, esize) ((void)0)
#define C3M_REALLOC_GROW(block, amount) ((void)0)
#define C3M_REALLOC_SHRINK(block, amount) ((void)0)
#define C3M_FREE(block, amount) ((void)0)

#endif // C3_MEMORY_DEBUG

///////////////////////////////////////////////////////////////////////////////
// QUOTA MANAGEMENT
///////////////////////////////////////////////////////////////////////////////

void Memory::set_quota(c3_ulong_t new_size) {
  if (new_size != 0) {
    assert(new_size >= MIN_QUOTA && new_size <= MAX_QUOTA);
    m_max_size.store(new_size, std::memory_order_relaxed);
  } else {
    m_max_size.store(DEFAULT_QUOTA, std::memory_order_relaxed);
  }
}

void Memory::transfer_used_size(Memory& from, size_t size) {
  assert(size);
  if (this != &from) {
    C3M_TRANSFER_FROM(from, size);
    c3_assert_def(c3_ulong_t prev) from.m_used_size.fetch_sub(size, std::memory_order_relaxed);
    c3_assert(prev >= size);
    C3M_TRANSFER_TO(*this, size);
    m_used_size.fetch_add(size, std::memory_order_relaxed);
  }
}

///////////////////////////////////////////////////////////////////////////////
// HEAP MANAGEMENT
///////////////////////////////////////////////////////////////////////////////

void* Memory::alloc(size_t size) {
  assert(size);
  PERF_INCREMENT_VAR_DOMAIN_COUNTER(m_domain, Memory_Alloc_Calls)
  PERF_UPDATE_VAR_DOMAIN_RANGE(m_domain, Memory_Alloc_Range, size)
  for (;;) {
    void* buff = std::malloc(size);
    if (buff != nullptr) {
      C3M_ALLOC(buff, size);
      m_used_size.fetch_add(size, std::memory_order_relaxed);
      PERF_UPDATE_VAR_DOMAIN_MAXIMUM(m_domain, Memory_Max_Used, m_used_size.load(std::memory_order_relaxed))
      return buff;
    }
    PERF_INCREMENT_VAR_DOMAIN_COUNTER(m_domain, Memory_Alloc_Purges)
    get_interface().begin_memory_deallocation(size);
  }
}

void* Memory::calloc(size_t nelems, size_t esize) {
  assert(nelems && esize);
  PERF_INCREMENT_VAR_DOMAIN_COUNTER(m_domain, Memory_Calloc_Calls)
  PERF_UPDATE_VAR_DOMAIN_RANGE(m_domain, Memory_Calloc_Range, nelems * esize)
  for (;;) {
    void* buff = std::calloc(nelems, esize);
    if (buff != nullptr) {
      C3M_CALLOC(buff, nelems, esize);
      m_used_size.fetch_add(nelems * esize, std::memory_order_relaxed);
      PERF_UPDATE_VAR_DOMAIN_MAXIMUM(m_domain, Memory_Max_Used, m_used_size.load(std::memory_order_relaxed))
      return buff;
    }
    PERF_INCREMENT_VAR_DOMAIN_COUNTER(m_domain, Memory_Calloc_Purges)
    get_interface().begin_memory_deallocation(nelems * esize);
  }
}

void* Memory::optional_calloc(size_t nelems, size_t esize) {
  assert(nelems && esize);
  PERF_INCREMENT_VAR_DOMAIN_COUNTER(m_domain, Memory_Opt_Calloc_Calls)
  PERF_UPDATE_VAR_DOMAIN_RANGE(m_domain, Memory_Opt_Calloc_Range, nelems * esize)
  void* buff = std::calloc(nelems, esize);
  if (buff != nullptr) {
    C3M_OPT_CALLOC(buff, nelems, esize);
    m_used_size.fetch_add(nelems * esize, std::memory_order_relaxed);
    PERF_UPDATE_VAR_DOMAIN_MAXIMUM(m_domain, Memory_Max_Used, m_used_size.load(std::memory_order_relaxed))
  }
  return buff;
}

void* Memory::realloc(void* buff, size_t new_size, size_t old_size) {
  assert(buff && new_size && old_size && new_size != old_size);
  PERF_INCREMENT_VAR_DOMAIN_COUNTER(m_domain, Memory_Realloc_Calls)
  PERF_UPDATE_VAR_DOMAIN_RANGE(m_domain, Memory_Realloc_Range, new_size)
  for (;;) {
    void* new_buff = std::realloc(buff, new_size);
    if (new_buff != nullptr) {
      if (new_size > old_size) {
        C3M_REALLOC_GROW(new_buff, new_size - old_size);
        m_used_size.fetch_add(new_size - old_size, std::memory_order_relaxed);
        PERF_UPDATE_VAR_DOMAIN_MAXIMUM(m_domain, Memory_Max_Used, m_used_size.load(std::memory_order_relaxed))
      } else {
        c3_ulong_t diff = old_size - new_size;
        C3M_REALLOC_SHRINK(new_buff, diff);
        c3_assert_def(c3_ulong_t prev) m_used_size.fetch_sub(diff, std::memory_order_relaxed);
        c3_assert(prev >= diff);
      }
      return new_buff;
    }
    PERF_INCREMENT_VAR_DOMAIN_COUNTER(m_domain, Memory_Realloc_Purges)
    get_interface().begin_memory_deallocation(new_size);
  }
}

size_t Memory::get_block_size(void* buff) {
  #if C3_CYGWIN
  return 0;
  #else
  return malloc_usable_size(buff);
  #endif
}

void* Memory::inplace_realloc(void* buff, size_t new_size, size_t old_size) {
  // TODO
  return nullptr;
}

void Memory::free(void* buff, size_t size) {
  assert(buff && size);
  PERF_INCREMENT_VAR_DOMAIN_COUNTER(m_domain, Memory_Free_Calls)
  C3M_FREE(buff, size);
  std::free(buff);
  c3_assert_def(c3_ulong_t prev) m_used_size.fetch_sub(size, std::memory_order_relaxed);
  c3_assert(prev >= size);
}

bool Memory::heap_check() {
  // TODO
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// DefaultAllocator
///////////////////////////////////////////////////////////////////////////////

c3_byte_t* DefaultAllocator::alloc(c3_uint_t size) {
  return (c3_byte_t*) da_memory.alloc(size);
}

void DefaultAllocator::free(void* buff, c3_uint_t size) {
  da_memory.free(buff, size);
}

}
