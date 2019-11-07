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
 * Multithreading support: the thread object supporting deadlock detection and recovery.
 */
#ifndef _MT_THREADS_H
#define _MT_THREADS_H

#include "c3lib/c3lib.h"
#include "mt_monitoring.h"
#include "mt_quick_event.h"
#include "mt_lockable_object.h"
#include "mt_mutexes.h"

/*
 * "mt_message_queue.h" cannot be included here as has to include this header itself as it is a template
 * class; we have to work with `SyncObject` pointers whenever we need a pointer to a message queue.
 */
#include <thread>

namespace CyberCache {

extern thread_local c3_uint_t local_thread_id;

/// Methods that host implementation has to provide to the thread pool
class ThreadInterface {
public:
/**
 * Inform host that the thread with given ID has just returned from its thread proc, and is good to
 * `join()`. Before this method is called, host implementation cannot assume that the thread with given
 * ID will indeed return from its thread proc, even if it set "quitting" status: something can always
 * happen at the last moment, and main thread waiting on `join()` might hang in there forever...
 *
 * @param id ID of the thread that has just returned from its thread proc
 * @return `true` if host has accepted notification, `false` otherwise (there isn't much that the
 *   thread can do upon `false`, other than report a critical error using *system* logging facility).
 */
  virtual bool thread_is_quitting(c3_uint_t id) = 0;
};

/// Thread IDs, used as indices into global array of thread objects
enum thread_id_t {
  TI_MAIN = 0,               // main application thread
  TI_SIGNAL_HANDLER,         // thread that handles various signals (errors, interruptions etc.)
  TI_LISTENER,               // incoming connections listener
  TI_LOGGER,                 // server logger
  TI_SESSION_BINLOG,         // binlog of the "session" domain
  TI_FPC_BINLOG,             // binlog of the "fpc" domain
  TI_BINLOG_LOADER,          // shared binlog loader
  TI_BINLOG_SAVER,           // cache database saver, used by main server thread only
  TI_SESSION_REPLICATOR,     // replicator of rhe "session" domain
  TI_FPC_REPLICATOR,         // replicator of the "fpc" domain
  TI_SESSION_OPTIMIZER,      // optimizer of the "session" domain
  TI_FPC_OPTIMIZER,          // optimizer of the "fpc" domain
  TI_TAG_MANAGER,            // concurrent tag manager for the FPC
  TI_FIRST_CONNECTION_THREAD // ID of the first thread from the pool of threads handling incoming commands
};

static_assert(TI_FIRST_CONNECTION_THREAD == 13, "Adjust 'Waits_Until_No_Readers' perf counter array size");

/// Maximum total number of threads supported by the server
constexpr c3_uint_t MAX_NUM_THREADS = TI_FIRST_CONNECTION_THREAD + MAX_NUM_CONNECTION_THREADS;

/// Type of the second argument to the thread procedure
union ThreadArgument {
private:
  void*       ta_pointer; // used if it necessary to pass a pointer or reference
  c3_intptr_t ta_number;  // used if it is necessary to pass an integer value

public:
  ThreadArgument() { ta_pointer = nullptr; }
  explicit ThreadArgument(void* pointer) { ta_pointer = pointer; }
  explicit ThreadArgument(c3_intptr_t number) { ta_number = number; }

  c3_intptr_t get_number() const { return ta_number; }
  void* get_pointer() const { return ta_pointer; }
};

/// Type name for the procedures run by threads
typedef void (*thread_function_t)(c3_uint_t id, ThreadArgument arg);

/// Thread status
enum thread_state_t: c3_byte_t {
  TS_UNUSED = 0, // unused slot in the array of threads
  TS_ACTIVE,     // a running thread (i.e. not waiting on a queue for next job)
  TS_IDLE,       // waiting on a message queue for next job
  TS_QUITTING    // the thread acknowledged quit request and is about to exit its thread proc
};

/*
 * The below three `enum`s list states of synchronization objects that a thread can track. A thread is only
 * allowed to lock (or wait on) *up to* one mutex (shared *or* downgradable), one spinlock, and one
 * queue; and, if more than one synchronization object is accessed by thread at any given moment, then
 * accesses must go **ONLY** in that order! Some lock(s) in that sequence can be "skipped" though;
 * therefore:
 *
 * - if a mutex is being locked, there must be no active spin lock or a message queue lock,
 * - if a spin lock is being acquired, there must be no active message queue lock,
 * - if a mutex, a spin lock, or a message queue is being locked, there must not be another active lock
 *   on a synchronization object of the same type.
 *
 *   If a thread is hung somewhere, then it is possible to examine its status and see what
 *   synchronization resources it owns. Sync object statuses allow for clear distinguishing of threads
 *   that are IN THE SYNC OBJECT (trying to acquire it) and those that acquired synchronization object and
 *   are now doing something else.
 */

/**
 * State of a mutex in relation to the current thread (i.e. "unlocked" means "not locked by *current*
 * thread", "acquired" means "acquired by *current* thread" etc.)
 */
enum thread_mutex_state_t: c3_byte_t {
  MXS_UNLOCKED = 0,            // mutex is not currently being used
  MXS_BEGIN_SHARED_LOCK,       // entered method that will try to get shared lock on a mutex
  MXS_BEGIN_EXCLUSIVE_LOCK,    // entered method that will try to get exclusive lock on a mutex
  MXS_BEGIN_DOWNGRADE,         // entered method that will try to downgrade exclusive lock to shared
  MXS_BEGIN_UPGRADE,           // entered method that will try to upgrade shared lock to exclusive
  MXS_BEGIN_SHARED_UNLOCK,     // entered method that will release a shared lock
  MXS_BEGIN_EXCLUSIVE_UNLOCK,  // entered method that will release an exclusive lock
  MXS_ACQUIRED_SHARED_LOCK,    // successively acquired shared lock on a mutex
  MXS_ACQUIRED_EXCLUSIVE_LOCK, // successively acquired exclusive lock on a mutex

  // the following states can be passed to the state-setting method, but will never be stored
  MXS_SHARED_LOCK_FAILED,      // attempt to acquire shared lock timed out
  MXS_EXCLUSIVE_LOCK_FAILED,   // attempt to acquire exclusive lock timed out
  MXS_DOWNGRADE_FAILED,        // attempt to downgrade exclusive lock has failed
  MXS_UPGRADE_FAILED           // attempt to upgrade shared lock has failed
};

/**
 * State of a quick mutex in relation to the current thread (i.e. "unlocked" means "not locked by *current*
 * thread", "acquired" means "acquired by *current* thread" etc.)
 */
enum thread_object_state_t: c3_byte_t {
  LOS_UNLOCKED = 0,   // lockable object is not currently being used
  LOS_BEGIN_TRY_LOCK, // entered method that will try to lock an object (will *not* wait)
  LOS_BEGIN_LOCK,     // entered method that will lock an object (*will* wait if needed)
  LOS_BEGIN_UNLOCK,   // entered method that will release locked object
  LOS_ACQUIRED_LOCK,  // successfully locked an object

  // the following state can be passed to the state-setting method, but `LOS_UNLOCKED` state will be stored instead
  LOS_LOCK_FAILED     // attempt to acquire lock on an object has failed (the object was already locked)
};

/**
 * State of a message queue in relation to the current thread (i.e. "unused" means "not locked by
 * *current* thread", etc.)
 */
enum thread_queue_state_t: c3_byte_t {
  MQS_UNUSED = 0,          // message queue is not currently being used
  MQS_IN_TRY_GET,          // entered method that gets a message from a queue (will *not* wait)
  MQS_IN_GET,              // entered method that gets a message from a queue (will wait if needed)
  MQS_IN_PUT,              // entered method that puts a message into a queue
  MQS_IN_GET_CAPACITY,     // entered method that retrieves current queue capacity
  MQS_IN_GET_MAX_CAPACITY, // entered method that retrieves current maximum queue capacity
  MQS_IN_SET_CAPACITY,     // entered method that sets new queue capacity
  MQS_IN_SET_MAX_CAPACITY, // entered method that sets new maximum queue capacity
};

/**
 * State of the events associated with each thread, on which a thread can wait with or
 * without a timeout.
 */
enum thread_event_state_t: c3_byte_t {
  EVS_NOT_WAITING = 0, // the thread is not waiting on the event
  EVS_IS_WAITING       // the thread is waiting on the event
};

/// Extended thread status used for diagnostics (but *not* recovery)
struct extended_thread_state_t {
  char                  ets_object_flags[96];   // buffer for object flags; size is checked with static assert
  char                  ets_mutex_info[SyncObject::INFO_BUFF_LENGTH]; // buffer for mutex info
  char                  ets_queue_info[SyncObject::INFO_BUFF_LENGTH]; // buffer for message queue info
  thread_state_t        ets_state;              // current state of the thread, a `TS_xxx` constant
  thread_mutex_state_t  ets_mutex_state;        // current mutex use mode (`MXS_xxx` constant)
  thread_object_state_t ets_object_state;       // current object lock use mode (`LOS_xxx` constant)
  thread_event_state_t  ets_event_state;        // current state of the non-timed event (`EVS_xxx` constant)
  thread_event_state_t  ets_timed_event_state;  // current state of the non-timed event (`EVS_xxx` constant)
  thread_queue_state_t  ets_queue_state;        // current message queue use mode (`MQS_xxx` constant)
  bool                  ets_quit_request;       // whether the thread got request to terminate
};

/// State of the thread of execution
class Thread {
  /// Type name for the wrapper procedure running actual thread procs
  typedef void (*thread_wrapper_t)(thread_function_t proc, c3_uint_t id, ThreadArgument arg);

  static c3_uint_t        t_num_cpu_cores;                 // number of CPU cores reported by the system
  static c3_uint_t        t_num_connection_threads;        // current number of worker threads
  static std::atomic_uint t_num_active_connection_threads; // current number of active worker threads
  static ThreadInterface* t_host;                          // implementation of the host interface

  struct Initializer {
    Initializer() noexcept C3_FUNC_COLD;
  };
  static Initializer t_initializer;

  std::thread                    t_thread;            // system object representing the thread
  c3_long_t                      t_start_time;        // when the thread entered its current state
  C3LM_DEF(const Mutex*          t_mutex_ref)         // pointer to a mutex used by current thread
  C3LM_DEF(const LockableObject* t_object_ref)        // pointer to a lockable object used by current thread
  C3LM_DEF(const SyncObject*     t_queue_ref)         // pointer to a message queue used by current thread
  QuickEvent                     t_event;             // used for hash object locks & waiting until there're no readers
  QuickTimedEvent                t_timed_event;       // used for session locks
  thread_state_t                 t_state;             // current state of the thread, a `TS_xxx` constant
  C3LM_DEF(thread_mutex_state_t  t_mutex_state)       // current use mode of the mutex (`MXS_xxx` constant)
  C3LM_DEF(thread_object_state_t t_object_state)      // current use mode of the spin lock (`SLS_xxx` constant)
  C3LM_DEF(thread_event_state_t  t_event_state)       // state of the non-timed event
  C3LM_DEF(thread_event_state_t  t_timed_event_state) // state of the timed event
  C3LM_DEF(thread_queue_state_t  t_queue_state)       // current use mode of the message queue (`MQS_xxx` constant)
  bool                           t_quit_request;      // the thread got request to terminate
  C3LM_DEF(c3_byte_t             t_reserved[9])       // unused field (padding to 64 bytes for the whole structure)

  C3_FUNC_COLD Thread(thread_wrapper_t wrapper, thread_function_t proc, c3_uint_t id, ThreadArgument arg):
    t_thread(wrapper, proc, id, arg) {}

  static ThreadInterface& get_host() {
    c3_assert(t_host);
    return *t_host;
  }
  static void set_host(ThreadInterface* host) {
    c3_assert(t_host == nullptr);
    t_host = host;
  }
  static void initialize(c3_uint_t id) C3_FUNC_COLD;
  static void thread_proc_wrapper(thread_function_t proc, c3_uint_t id, ThreadArgument arg);
  static c3_long_t get_current_time() { return PrecisionTimer::microseconds_since_epoch(); }

public:
  // default ctor to build array elements
  Thread() noexcept {}
  Thread(const Thread&) = delete;
  Thread(Thread&&) = delete;
  ~Thread() = default;

  Thread& operator=(const Thread&) = delete;
  Thread& operator=(Thread&&) = delete;

  static void initialize_main(ThreadInterface* host) C3_FUNC_COLD {
    set_host(host);
    initialize(TI_MAIN);
    /*
     * even though it's not checked currently (it's main thread who check states of other threads),
     * we still set it for consistency
     */
    set_state(TS_ACTIVE);
  }
  static void start(c3_uint_t id, thread_function_t proc, ThreadArgument arg) C3_FUNC_COLD;
  static void request_stop(c3_uint_t id) C3_FUNC_COLD;
  static bool received_stop_request();
  static void wait_stop(c3_uint_t id) C3_FUNC_COLD;

  // utilities
  static void sleep(c3_uint_t msecs);

  // event manipulation
  static void wait_for_event();
  static void trigger_event(c3_uint_t id);
  static bool wait_for_timed_event(c3_uint_t milliseconds);
  static void trigger_timed_event(c3_uint_t id);

  // stats
  static c3_uint_t get_num_cpu_cores() { return t_num_cpu_cores; }
  static c3_uint_t get_num_connection_threads() { return t_num_connection_threads; }
  static c3_uint_t get_num_active_connection_threads() {
    return t_num_active_connection_threads.load(std::memory_order_relaxed);
  }

  // thread identification
  static c3_uint_t get_id() { return local_thread_id; };
  static const char* get_name(c3_uint_t id) C3_FUNC_COLD;
  static const char* get_name() C3_FUNC_COLD;
  static const char* get_state_name(thread_state_t state) C3_FUNC_COLD;

  // thread state management
  static thread_state_t get_state(c3_uint_t id);
  static void get_state(c3_uint_t id, extended_thread_state_t& state) C3_FUNC_COLD;
  static bool is_running(c3_uint_t id) C3_FUNC_COLD;
  static c3_long_t get_time_in_current_state(c3_uint_t id);
  static void set_state(thread_state_t state);

  // synchronization objects state management
  static bool set_mutex_state(const Mutex* mutex, thread_mutex_state_t state, bool skip_spinlock_check)
    C3LM_IF(;,{ return true; })
  static bool set_object_state(const LockableObject* lo, thread_object_state_t state)
    C3LM_IF(;,{ return true; })
  static bool set_queue_state(const SyncObject* queue, thread_queue_state_t state)
    C3LM_IF(;,{ return true; })
};

} // CyberCache

#endif // _MT_THREADS_H
