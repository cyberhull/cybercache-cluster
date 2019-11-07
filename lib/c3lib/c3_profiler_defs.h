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
 * Macros defining and manipulating performance counters.
 *
 * This header is meant to be included in two "modes": with `C3_PERF_GENERATE_DEFINITIONS` macro undefined (in which
 * case it provides forward declarations of various performance counters), and with that macro set to `1` (so that
 * the header would create definitions of those counters).
 */

#ifndef _C3_PROFILER_DEFS_H
#define _C3_PROFILER_DEFS_H

#if C3_INSTRUMENTED
#define PERF_GLOBAL(name) _Perf_ ## name
#define PERF_LOCAL(name) _perf_ ## name
#else
#define PERF_GLOBAL(name)
#define PERF_LOCAL(name)
#endif

#if !C3_PERF_GENERATE_DEFINITIONS

#include "c3_profiler.h"

///////////////////////////////////////////////////////////////////////////////
// 1A) PARAMETRIZED MACROS DECLARING PERFORMANCE COUNTERS
///////////////////////////////////////////////////////////////////////////////

#if C3_INSTRUMENTED

#define PERF_DEFINE_INT_COUNTER(domain, name) extern PerfNumberCounter<c3_uint_t> PERF_GLOBAL(name);
#define PERF_DEFINE_LONG_COUNTER(domain, name) extern PerfNumberCounter<c3_ulong_t> PERF_GLOBAL(name);
#define PERF_DEFINE_DOMAIN_INT_COUNTER(domain, name) extern PerfDomainNumberCounter<c3_uint_t> PERF_GLOBAL(name);
#define PERF_DEFINE_DOMAIN_LONG_COUNTER(domain, name) extern PerfDomainNumberCounter<c3_ulong_t> PERF_GLOBAL(name);
#define PERF_DEFINE_INT_MAXIMUM(domain, name) extern PerfMaximumCounter<c3_uint_t> PERF_GLOBAL(name);
#define PERF_DEFINE_LONG_MAXIMUM(domain, name) extern PerfMaximumCounter<c3_ulong_t> PERF_GLOBAL(name);
#define PERF_DEFINE_DOMAIN_INT_MAXIMUM(domain, name) extern PerfDomainMaximumCounter<c3_uint_t> PERF_GLOBAL(name);
#define PERF_DEFINE_DOMAIN_LONG_MAXIMUM(domain, name) extern PerfDomainMaximumCounter<c3_ulong_t> PERF_GLOBAL(name);
#define PERF_DEFINE_INT_RANGE(domain, name) extern PerfRangeCounter<c3_uint_t> PERF_GLOBAL(name);
#define PERF_DEFINE_LONG_RANGE(domain, name) extern PerfRangeCounter<c3_ulong_t> PERF_GLOBAL(name);
#define PERF_DEFINE_DOMAIN_INT_RANGE(domain, name) extern PerfDomainRangeCounter<c3_uint_t> PERF_GLOBAL(name);
#define PERF_DEFINE_DOMAIN_LONG_RANGE(domain, name) extern PerfDomainRangeCounter<c3_ulong_t> PERF_GLOBAL(name);
#define PERF_DEFINE_INT_ARRAY(domain, name, size) extern PerfArrayCounter<c3_uint_t, size> PERF_GLOBAL(name);
#define PERF_DEFINE_LONG_ARRAY(domain, name, size) extern PerfArrayCounter<c3_ulong_t, size> PERF_GLOBAL(name);
#define PERF_DECLARE_LOCAL_INT_COUNT(name) c3_uint_t PERF_LOCAL(name) = 0;
#define PERF_DECLARE_LOCAL_LONG_COUNT(name) c3_ulong_t PERF_LOCAL(name) = 0;

#else // !C3_INSTRUMENTED

#define PERF_DEFINE_INT_COUNTER(domain, name)
#define PERF_DEFINE_LONG_COUNTER(domain, name)
#define PERF_DEFINE_DOMAIN_INT_COUNTER(domain, name)
#define PERF_DEFINE_DOMAIN_LONG_COUNTER(domain, name)
#define PERF_DEFINE_INT_MAXIMUM(domain, name)
#define PERF_DEFINE_LONG_MAXIMUM(domain, name)
#define PERF_DEFINE_DOMAIN_INT_MAXIMUM(domain, name)
#define PERF_DEFINE_DOMAIN_LONG_MAXIMUM(domain, name)
#define PERF_DEFINE_INT_RANGE(domain, name)
#define PERF_DEFINE_LONG_RANGE(domain, name)
#define PERF_DEFINE_DOMAIN_INT_RANGE(domain, name)
#define PERF_DEFINE_DOMAIN_LONG_RANGE(domain, name)
#define PERF_DEFINE_INT_ARRAY(domain, name, size)
#define PERF_DEFINE_LONG_ARRAY(domain, name, size)
#define PERF_DECLARE_LOCAL_INT_COUNT(name)
#define PERF_DECLARE_LOCAL_LONG_COUNT(name)

#endif // C3_INSTRUMENTED

///////////////////////////////////////////////////////////////////////////////
// 1B) PARAMETRIZED MACROS MANIPULATING PERFORMANCE COUNTERS
///////////////////////////////////////////////////////////////////////////////

#if C3_INSTRUMENTED

#define PERF_INCREMENT_COUNTER(name) PERF_GLOBAL(name).increment();
#define PERF_DECREMENT_COUNTER(name) PERF_GLOBAL(name).decrement();
#define PERF_INCREMENT_DOMAIN_COUNTER(domain, name) PERF_GLOBAL(name).increment(PD_ ## domain);
#define PERF_DECREMENT_DOMAIN_COUNTER(domain, name) PERF_GLOBAL(name).decrement(PD_ ## domain);
#define PERF_INCREMENT_VAR_DOMAIN_COUNTER(domain, name) PERF_GLOBAL(name).increment(domain);
#define PERF_DECREMENT_VAR_DOMAIN_COUNTER(domain, name) PERF_GLOBAL(name).decrement(domain);
#define PERF_UPDATE_MAXIMUM(name, value) PERF_GLOBAL(name).update(value);
#define PERF_UPDATE_DOMAIN_MAXIMUM(domain, name, value) PERF_GLOBAL(name).update(PD_ ## domain, value);
#define PERF_UPDATE_VAR_DOMAIN_MAXIMUM(domain, name, value) PERF_GLOBAL(name).update(domain, value);
#define PERF_UPDATE_RANGE(name, value) PERF_GLOBAL(name).update(value);
#define PERF_UPDATE_DOMAIN_RANGE(domain, name, value) PERF_GLOBAL(name).update(PD_ ## domain, value);
#define PERF_UPDATE_VAR_DOMAIN_RANGE(domain, name, value) PERF_GLOBAL(name).update(domain, value);
#define PERF_UPDATE_ARRAY(name, value) PERF_GLOBAL(name).increment(value);
#define PERF_INCREMENT_LOCAL_COUNT(name) PERF_LOCAL(name)++;

#else // !C3_INSTRUMENTED

#define PERF_INCREMENT_COUNTER(name) ((void)0);
#define PERF_DECREMENT_COUNTER(name) ((void)0);
#define PERF_INCREMENT_DOMAIN_COUNTER(domain, name) ((void)0);
#define PERF_DECREMENT_DOMAIN_COUNTER(domain, name) ((void)0);
#define PERF_INCREMENT_VAR_DOMAIN_COUNTER(domain, name) ((void)0);
#define PERF_DECREMENT_VAR_DOMAIN_COUNTER(domain, name) ((void)0);
#define PERF_UPDATE_MAXIMUM(name, value) ((void)0);
#define PERF_UPDATE_DOMAIN_MAXIMUM(domain, name, value) ((void)0);
#define PERF_UPDATE_VAR_DOMAIN_MAXIMUM(domain, name, value) ((void)0);
#define PERF_UPDATE_RANGE(name, value) ((void)0);
#define PERF_UPDATE_DOMAIN_RANGE(domain, name, value) ((void)0);
#define PERF_UPDATE_VAR_DOMAIN_RANGE(domain, name, value) ((void)0);
#define PERF_UPDATE_ARRAY(name, value) ((void)0);
#define PERF_INCREMENT_LOCAL_COUNT(name) ((void)0);

#endif // C3_INSTRUMENTED

#else // C3_PERF_GENERATE_DEFINITIONS

///////////////////////////////////////////////////////////////////////////////
// 2) PARAMETRIZED MACROS DEFINING PERFORMANCE COUNTERS' INSTANCES
///////////////////////////////////////////////////////////////////////////////

#if !C3_INSTRUMENTED
#error "Profiler definitions should only be included in instrumented builds"
#endif // C3_INSTRUMENTED

#define PERF_DEFINE_INT_COUNTER(domain, name) \
  PerfNumberCounter<c3_uint_t> PERF_GLOBAL(name)(DM_ ## domain, C3_STRINGIFY_HELPER(name));
#define PERF_DEFINE_LONG_COUNTER(domain, name) \
  PerfNumberCounter<c3_ulong_t> PERF_GLOBAL(name)(DM_ ## domain, C3_STRINGIFY_HELPER(name));
#define PERF_DEFINE_DOMAIN_INT_COUNTER(domain, name) \
  PerfDomainNumberCounter<c3_uint_t> PERF_GLOBAL(name)(DM_ ## domain, C3_STRINGIFY_HELPER(name));
#define PERF_DEFINE_DOMAIN_LONG_COUNTER(domain, name) \
  PerfDomainNumberCounter<c3_ulong_t> PERF_GLOBAL(name)(DM_ ## domain, C3_STRINGIFY_HELPER(name));
#define PERF_DEFINE_INT_MAXIMUM(domain, name) \
  PerfMaximumCounter<c3_uint_t> PERF_GLOBAL(name)(DM_ ## domain, C3_STRINGIFY_HELPER(name));
#define PERF_DEFINE_LONG_MAXIMUM(domain, name) \
  PerfMaximumCounter<c3_ulong_t> PERF_GLOBAL(name)(DM_ ## domain, C3_STRINGIFY_HELPER(name));
#define PERF_DEFINE_DOMAIN_INT_MAXIMUM(domain, name) \
  PerfDomainMaximumCounter<c3_uint_t> PERF_GLOBAL(name)(DM_ ## domain, C3_STRINGIFY_HELPER(name));
#define PERF_DEFINE_DOMAIN_LONG_MAXIMUM(domain, name) \
  PerfDomainMaximumCounter<c3_ulong_t> PERF_GLOBAL(name)(DM_ ## domain, C3_STRINGIFY_HELPER(name));
#define PERF_DEFINE_INT_RANGE(domain, name) \
  PerfRangeCounter<c3_uint_t> PERF_GLOBAL(name)(DM_ ## domain, C3_STRINGIFY_HELPER(name));
#define PERF_DEFINE_LONG_RANGE(domain, name) \
  PerfRangeCounter<c3_ulong_t> PERF_GLOBAL(name)(DM_ ## domain, C3_STRINGIFY_HELPER(name));
#define PERF_DEFINE_DOMAIN_INT_RANGE(domain, name) \
  PerfDomainRangeCounter<c3_uint_t> PERF_GLOBAL(name)(DM_ ## domain, C3_STRINGIFY_HELPER(name));
#define PERF_DEFINE_DOMAIN_LONG_RANGE(domain, name) \
  PerfDomainRangeCounter<c3_ulong_t> PERF_GLOBAL(name)(DM_ ## domain, C3_STRINGIFY_HELPER(name));
#define PERF_DEFINE_INT_ARRAY(domain, name, size) \
  PerfArrayCounter<c3_uint_t, size> PERF_GLOBAL(name)(DM_ ## domain, C3_STRINGIFY_HELPER(name));
#define PERF_DEFINE_LONG_ARRAY(domain, name, size) \
  PerfArrayCounter<c3_ulong_t, size> PERF_GLOBAL(name)(DM_ ## domain, C3_STRINGIFY_HELPER(name));

#endif // C3_PERF_GENERATE_DEFINITIONS

///////////////////////////////////////////////////////////////////////////////
// MACROS DECLARING/DEFINING ACTUAL PERFORMANCE COUNTERS
///////////////////////////////////////////////////////////////////////////////

namespace CyberCache {

/*
 * `STATS` prints out performance counters is reverse order: the higher is a performance counters on
 * the below list, the later it will be printed.
 */
PERF_DEFINE_INT_ARRAY(ALL, Shared_Header_Size, 24)
PERF_DEFINE_LONG_COUNTER(ALL, Shared_Header_Reallocations)

PERF_DEFINE_INT_ARRAY(ALL, Waits_Until_No_Readers, 14);

PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, Local_Queue_Put_Failures)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, Local_Queue_Reallocations)
PERF_DEFINE_DOMAIN_INT_MAXIMUM(ALL, Local_Queue_Max_Capacity)

PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, Queue_Put_Waits)
PERF_DEFINE_DOMAIN_INT_COUNTER(ALL, Queue_Failed_Reallocations)
PERF_DEFINE_DOMAIN_INT_COUNTER(ALL, Queue_Capacity_Reductions)
PERF_DEFINE_DOMAIN_INT_COUNTER(ALL, Queue_Forced_Reallocations)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, Queue_Reallocations)
PERF_DEFINE_DOMAIN_INT_MAXIMUM(ALL, Queue_Max_Capacity)

PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, Cache_Misses)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, Cache_Hits)

PERF_DEFINE_INT_ARRAY(GLOBAL, Recompressions_Failed, 9)
PERF_DEFINE_INT_ARRAY(GLOBAL, Recompressions_Succeeded, 9)

PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, IO_Objects_Cloned)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, IO_Objects_Copied)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, IO_Objects_Created)
PERF_DEFINE_DOMAIN_INT_COUNTER(ALL, IO_Objects_Active)

PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, List_Add_Failures)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, List_Miscalculations)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, List_Short_Reallocs)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, List_Medium_Reallocs)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, List_Long_Reallocs)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, List_Completed_Reallocs)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, List_Added_Strings)

PERF_DEFINE_LONG_COUNTER(FPC, Store_Tag_Array_Reallocs)
PERF_DEFINE_INT_ARRAY(FPC, Store_Tags_Per_Object, 16)
PERF_DEFINE_DOMAIN_INT_RANGE(ALL, Store_Objects_Name_Length)
PERF_DEFINE_DOMAIN_INT_RANGE(ALL, Store_Objects_Length)
PERF_DEFINE_DOMAIN_INT_COUNTER(ALL, Store_Objects_Active)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, Store_Objects_Created)

PERF_DEFINE_DOMAIN_INT_MAXIMUM(ALL, Replicator_Max_Deferred_Commands)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, Replicator_Deferred_Commands)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, Replicator_Reconnections)

PERF_DEFINE_INT_COUNTER(GLOBAL, Sockets_Accept_Error_Other)
PERF_DEFINE_INT_COUNTER(GLOBAL, Sockets_Accept_Error_IP)
PERF_DEFINE_INT_COUNTER(GLOBAL, Sockets_Accept_Error_Address)
PERF_DEFINE_INT_COUNTER(GLOBAL, Sockets_Accept_Try_NoConn)
PERF_DEFINE_LONG_COUNTER(GLOBAL, Sockets_Closed)
PERF_DEFINE_INT_RANGE(GLOBAL, Sockets_Received_Data_Range)
PERF_DEFINE_INT_RANGE(GLOBAL, Sockets_Sent_Data_Range)
PERF_DEFINE_LONG_COUNTER(GLOBAL, Socket_Inbound_Connections)
PERF_DEFINE_LONG_COUNTER(GLOBAL, Socket_Outbound_Connections)
PERF_DEFINE_LONG_COUNTER(GLOBAL, Sockets_Bound)
PERF_DEFINE_LONG_COUNTER(GLOBAL, Sockets_Created)
PERF_DEFINE_LONG_COUNTER(GLOBAL, Socket_Hosts_Resolved)

PERF_DEFINE_DOMAIN_LONG_MAXIMUM(ALL, Memory_Max_Used)
PERF_DEFINE_DOMAIN_INT_COUNTER(ALL, Memory_Realloc_Purges)
PERF_DEFINE_DOMAIN_INT_COUNTER(ALL, Memory_Calloc_Purges)
PERF_DEFINE_DOMAIN_INT_COUNTER(ALL, Memory_Alloc_Purges)
PERF_DEFINE_DOMAIN_LONG_RANGE(ALL, Memory_Realloc_Range)
PERF_DEFINE_DOMAIN_LONG_RANGE(ALL, Memory_Opt_Calloc_Range)
PERF_DEFINE_DOMAIN_LONG_RANGE(ALL, Memory_Calloc_Range)
PERF_DEFINE_DOMAIN_LONG_RANGE(ALL, Memory_Alloc_Range)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, Memory_Free_Calls)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, Memory_Realloc_Calls)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, Memory_Opt_Calloc_Calls)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, Memory_Calloc_Calls)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, Memory_Alloc_Calls)

PERF_DEFINE_DOMAIN_INT_MAXIMUM(ALL, SpinLock_Max_Waits)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, SpinLock_Total_Waits)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, SpinLock_Acquisitions)

PERF_DEFINE_INT_COUNTER(SESSION, Session_Aborted_Locks)
PERF_DEFINE_INT_COUNTER(SESSION, Session_Broken_Locks)
PERF_DEFINE_LONG_COUNTER(SESSION, Session_Lock_Waits)
PERF_DEFINE_LONG_ARRAY(GLOBAL, Hash_Object_Waits, 12)
PERF_DEFINE_LONG_COUNTER(GLOBAL, Hash_Object_Lock_Try_Failures)
PERF_DEFINE_LONG_COUNTER(GLOBAL, Hash_Object_Lock_Try_Successes)
PERF_DEFINE_LONG_COUNTER(GLOBAL, Hash_Object_Locks)

PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, Pipeline_Connection_Events)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, Pipeline_Object_Events)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, Pipeline_Socket_Events)
PERF_DEFINE_DOMAIN_LONG_COUNTER(ALL, Pipeline_Queue_Events)
PERF_DEFINE_LONG_COUNTER(GLOBAL, Incoming_Connections)

} // CyberCache

#endif // _C3_PROFILER_DEFS_H
