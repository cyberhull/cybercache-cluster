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
#include "mt_threads.h"
#include "ht_objects.h"

#include <new>

namespace CyberCache {

c3_uint_t           Thread::t_num_cpu_cores;
c3_uint_t           Thread::t_num_connection_threads;
std::atomic_uint    Thread::t_num_active_connection_threads;
ThreadInterface*    Thread::t_host;
Thread::Initializer Thread::t_initializer;

/*
 * This variable was originally a private `t_id` field in the `Thread` class. However, due to a bug in GCC
 * linker, the a `thread_local` static field could not be used in any inline methods.
 *
 * See e.g. https://gcc.gnu.org/bugzilla/show_bug.cgi?id=64697
 *
 * Some solutions (defining the variable in this or that module) would work for a while, but then would
 * still break when modules were added or removed from the project. The only reliable work-around was to
 * move this field out of the class *and* making it local for the module (which involved turning inline
 * methods that used it into regular ones).
 *
 * TODO: restore `Thread::t_id` field
 */
thread_local c3_uint_t local_thread_id;
static Thread thread_pool[MAX_NUM_THREADS];

///////////////////////////////////////////////////////////////////////////////
// THREAD SCHEDULING
///////////////////////////////////////////////////////////////////////////////

Thread::Initializer::Initializer() noexcept {
  // sets number of cores to 0 if the value cannot be retrieved
  t_num_cpu_cores = c3_get_num_cpus();
}

void Thread::thread_proc_wrapper(thread_function_t proc, c3_uint_t id, ThreadArgument arg) {
  Thread::initialize(id);
  assert(proc && id && id < MAX_NUM_THREADS);

  // initializes thread-local compressors' data
  global_compressor.initialize();
  proc(id, arg);
  // clean up thread-local compressors' data
  global_compressor.cleanup();
  /*
   * We do *not* mark the slot as "unused" here yet: this has to be done in `wait_stop()`, after the thread
   * is `join()`ed: only after that the slot becomes truly "unused".
   */
  #ifdef C3_SAFEST
  C3LM_ON(Thread* thread = thread_pool + id);
  #endif
  C3LM_ON(c3_assert(thread->t_mutex_ref == nullptr && thread->t_mutex_state == MXS_UNLOCKED &&
    thread->t_object_ref == nullptr && thread->t_object_state == LOS_UNLOCKED &&
    thread->t_queue_ref == nullptr && thread->t_queue_state == MQS_UNUSED));
  set_state(TS_QUITTING); // in case the thread didn't do it itself
  get_host().thread_is_quitting(id);
}

void Thread::initialize(c3_uint_t id) {
  static_assert((C3LM_ENABLED && sizeof(Thread) == 64) ||
    (C3LM_DISABLED && sizeof(Thread) == 32), "The 'Thread' structure is not packed properly");

  assert(id < MAX_NUM_THREADS);
  local_thread_id = id;
  Thread& thread = thread_pool[id];
  thread.t_start_time = get_current_time();
  C3LM_ON(thread.t_mutex_ref = nullptr);
  C3LM_ON(thread.t_object_ref = nullptr);
  C3LM_ON(thread.t_queue_ref = nullptr);
  thread.t_state = TS_IDLE; // otherwise, count of active connection threads will break
  C3LM_ON(thread.t_mutex_state = MXS_UNLOCKED);
  C3LM_ON(thread.t_object_state = LOS_UNLOCKED);
  C3LM_ON(thread.t_event_state = EVS_NOT_WAITING);
  C3LM_ON(thread.t_timed_event_state = EVS_NOT_WAITING);
  C3LM_ON(thread.t_queue_state = MQS_UNUSED);
  thread.t_quit_request = false;
}

void Thread::start(c3_uint_t id, thread_function_t proc, ThreadArgument arg) {
  assert(id != local_thread_id && id > TI_MAIN && id < MAX_NUM_THREADS);
  Thread* thread = thread_pool + id;
  c3_assert(thread->t_state == TS_UNUSED);
  new (thread) Thread(thread_proc_wrapper, proc, id, arg);
  if (id >= TI_FIRST_CONNECTION_THREAD) {
    t_num_connection_threads++;
  }
}

void Thread::request_stop(c3_uint_t id) {
  assert(id != local_thread_id && id > TI_MAIN && id < MAX_NUM_THREADS);
  Thread& thread = thread_pool[id];
  c3_assert(thread.t_state != TS_UNUSED);
  thread.t_quit_request = true;
}

bool Thread::received_stop_request() {
  c3_assert(local_thread_id > TI_MAIN && local_thread_id < MAX_NUM_THREADS);
  return thread_pool[local_thread_id].t_quit_request;
}

void Thread::wait_stop(c3_uint_t id) {
  assert(id != local_thread_id && id > TI_MAIN && id < MAX_NUM_THREADS);
  Thread& thread = thread_pool[id];
  c3_assert(thread.t_quit_request || thread.t_state == TS_QUITTING);
  thread.t_thread.join();
  thread.t_state = TS_UNUSED;
  if (id >= TI_FIRST_CONNECTION_THREAD) {
    t_num_connection_threads--;
  }
}

///////////////////////////////////////////////////////////////////////////////
// UTILITIES
///////////////////////////////////////////////////////////////////////////////

void Thread::sleep(c3_uint_t msecs) {
  std::this_thread::sleep_for(std::chrono::milliseconds(msecs));
}

///////////////////////////////////////////////////////////////////////////////
// EVENT MANIPULATION
///////////////////////////////////////////////////////////////////////////////

void Thread::wait_for_event() {
  c3_assert(local_thread_id < MAX_NUM_THREADS);
  Thread& thread = thread_pool[local_thread_id];
  C3LM_ON(thread.t_event_state = EVS_IS_WAITING);
  thread.t_event.wait();
  C3LM_ON(thread.t_event_state = EVS_NOT_WAITING);
}

void Thread::trigger_event(c3_uint_t id) {
  assert(id != local_thread_id && id < MAX_NUM_THREADS);
  /*
   * We're not checking if thread `id` is waiting for the event: it is possible (and
   * allowed) that it is only about to enter waiting state: while trying to lock hash
   * object, it could have added itself to the mask of waiting threads (of the hash object)
   * already, but did not call `wait_for_event()` yet.
   */
  Thread& thread = thread_pool[id];
  thread.t_event.notify();
}

bool Thread::wait_for_timed_event(c3_uint_t milliseconds) {
  c3_assert(local_thread_id < MAX_NUM_THREADS);
  Thread& thread = thread_pool[local_thread_id];
  C3LM_ON(thread.t_timed_event_state = EVS_IS_WAITING);
  bool result = thread.t_timed_event.wait(milliseconds);
  C3LM_ON(thread.t_timed_event_state = EVS_NOT_WAITING);
  return result;
}

void Thread::trigger_timed_event(c3_uint_t id) {
  assert(id != local_thread_id && id < MAX_NUM_THREADS);
  /*
   * We're not checking if thread `id` is waiting for the "timed" event: it is possible (and
   * allowed) that it is only about to enter waiting state: while trying to lock the session,
   * it could have added itself to the mask of waiting threads (of the session object) already,
   * but did not call `wait_for_timed_event()` yet.
   */
  Thread& thread = thread_pool[id];
  thread.t_timed_event.notify();
}

///////////////////////////////////////////////////////////////////////////////
// IDENTIFICATION
///////////////////////////////////////////////////////////////////////////////

const char* Thread::get_name(c3_uint_t id) {
  static_assert(TI_FIRST_CONNECTION_THREAD == 13, "Number of service threads has changed");
  switch (id) {
    case TI_MAIN:
      return "Main thread";
    case TI_SIGNAL_HANDLER:
      return "Signal handler";
    case TI_LISTENER:
      return "Listener";
    case TI_LOGGER:
      return "Logger";
    case TI_SESSION_BINLOG:
      return "Session binlog";
    case TI_FPC_BINLOG:
      return "FPC binlog";
    case TI_BINLOG_LOADER:
      return "Binlog loader";
    case TI_BINLOG_SAVER:
      return "Binlog saver";
    case TI_SESSION_REPLICATOR:
      return "Session replicator";
    case TI_FPC_REPLICATOR:
      return "FPC replicator";
    case TI_SESSION_OPTIMIZER:
      return "Session optimizer";
    case TI_FPC_OPTIMIZER:
      return "FPC optimizer";
    case TI_TAG_MANAGER:
      return "Tag manager";
    default:
      c3_assert(id >= TI_FIRST_CONNECTION_THREAD && id < MAX_NUM_THREADS);
      return "Connection thread";
  }
}

const char* Thread::get_name() { return get_name(local_thread_id); }

const char* Thread::get_state_name(thread_state_t state) {
  switch (state) {
    case TS_UNUSED:
      return "unused";
    case TS_ACTIVE:
      return "active";
    case TS_IDLE:
      return "idle";
    case TS_QUITTING:
      return "quitting";
    default:
      return "<ERROR>";
  }
}

///////////////////////////////////////////////////////////////////////////////
// THREAD STATE MANAGEMENT
///////////////////////////////////////////////////////////////////////////////

thread_state_t Thread::get_state(c3_uint_t id) {
  assert(id < MAX_NUM_THREADS);
  return thread_pool[id].t_state;
}

void Thread::get_state(c3_uint_t id, extended_thread_state_t& state) {
  static_assert(sizeof extended_thread_state_t::ets_object_flags == HashObject::FLAGS_STATE_BUFF_LENGTH,
    "Size of object flags buffer in thread state must match that of 'HashObject'");
  assert(id < MAX_NUM_THREADS);
  Thread& thread = thread_pool[id];
  #if C3LM_ENABLED
  // all refs are copied to local vars in case running thread changes them
  auto ho = (const HashObject*) thread.t_object_ref;
  if (ho != nullptr) {
    ho->get_flags_state(state.ets_object_flags, sizeof state.ets_object_flags);
  } else {
  #endif
    std::strcpy(state.ets_object_flags, "-");
  #if C3LM_ENABLED
  }
  const Mutex* mutex = thread.t_mutex_ref;
  if (mutex != nullptr) {
    mutex->get_text_info(state.ets_mutex_info, sizeof state.ets_mutex_info);
  } else {
  #endif
    std::strcpy(state.ets_mutex_info, "-");
  #if C3LM_ENABLED
  }
  const SyncObject* queue = thread.t_queue_ref;
  if (queue != nullptr) {
    queue->get_text_info(state.ets_queue_info, sizeof state.ets_queue_info);
  } else {
  #endif
    std::strcpy(state.ets_queue_info, "-");
  #if C3LM_ENABLED
  }
  #endif
  state.ets_state = thread.t_state;
  state.ets_mutex_state = C3LM_IF(thread.t_mutex_state, MXS_UNLOCKED);
  state.ets_object_state = C3LM_IF(thread.t_object_state, LOS_UNLOCKED);
  state.ets_event_state = C3LM_IF(thread.t_event_state, EVS_NOT_WAITING);
  state.ets_timed_event_state = C3LM_IF(thread.t_timed_event_state, EVS_NOT_WAITING);
  state.ets_queue_state = C3LM_IF(thread.t_queue_state, MQS_UNUSED);
  state.ets_quit_request = thread.t_quit_request;
}

bool Thread::is_running(c3_uint_t id) {
  assert(id < MAX_NUM_THREADS);
  thread_state_t state = thread_pool[id].t_state;
  return state == TS_IDLE || state == TS_ACTIVE;
}

c3_long_t Thread::get_time_in_current_state(c3_uint_t id) {
  assert(id < MAX_NUM_THREADS);
  return get_current_time() - thread_pool[id].t_start_time;
}

void Thread::set_state(thread_state_t state) {
  c3_assert(local_thread_id < MAX_NUM_THREADS);
  Thread& thread = thread_pool[local_thread_id];
  switch (state) {
    case TS_IDLE:
    case TS_QUITTING:
      if (local_thread_id >= TI_FIRST_CONNECTION_THREAD) {
        // this condition will be `false` when connection thread enters "idle" state for the 1st time
        if (thread.t_state == TS_ACTIVE) {
          t_num_active_connection_threads.fetch_sub(1, std::memory_order_relaxed);
        }
      }
      break;
    case TS_ACTIVE:
      if (local_thread_id >= TI_FIRST_CONNECTION_THREAD) {
        c3_assert(thread.t_state != TS_ACTIVE);
        t_num_active_connection_threads.fetch_add(1, std::memory_order_relaxed);
      }
      break;
    default:
      assert_failure();
  }
  thread.t_state = state;
  thread.t_start_time = get_current_time();
}

///////////////////////////////////////////////////////////////////////////////
// THREAD SYNCHRONIZATION OBJECTS STATE MANAGEMENT
///////////////////////////////////////////////////////////////////////////////

#if C3LM_ENABLED

bool Thread::set_mutex_state(const Mutex* mutex, thread_mutex_state_t state, bool skip_spinlock_check) {
  assert(mutex);
  c3_assert(local_thread_id < MAX_NUM_THREADS);
  Thread& thread = thread_pool[local_thread_id];

  if ((skip_spinlock_check || // spinlock can be active during upgrade or downgrade
    (thread.t_object_state == LOS_UNLOCKED && thread.t_object_ref == nullptr)) &&
    thread.t_queue_state == MQS_UNUSED && thread.t_queue_ref == nullptr) {
    switch (state) {
      case MXS_UNLOCKED:
        if (thread.t_mutex_ref == mutex && (thread.t_mutex_state == MXS_BEGIN_SHARED_UNLOCK ||
          thread.t_mutex_state == MXS_BEGIN_EXCLUSIVE_UNLOCK)) {
          thread.t_mutex_ref = nullptr;
          thread.t_mutex_state = MXS_UNLOCKED;
          return true;
        } else {
          c3_assert_failure();
          return false;
        }
      case MXS_BEGIN_SHARED_LOCK:
      case MXS_BEGIN_EXCLUSIVE_LOCK:
        if (thread.t_mutex_ref == nullptr && thread.t_mutex_state == MXS_UNLOCKED) {
          thread.t_mutex_ref = mutex;
          thread.t_mutex_state = state;
          return true;
        } else {
          c3_assert_failure();
          return false;
        }
      case MXS_BEGIN_DOWNGRADE:
      case MXS_BEGIN_EXCLUSIVE_UNLOCK:
        if (thread.t_mutex_ref == mutex && thread.t_mutex_state == MXS_ACQUIRED_EXCLUSIVE_LOCK) {
          thread.t_mutex_state = state;
          return true;
        } else {
          c3_assert_failure();
          return false;
        }
      case MXS_BEGIN_UPGRADE:
      case MXS_BEGIN_SHARED_UNLOCK:
        if (thread.t_mutex_ref == mutex && thread.t_mutex_state == MXS_ACQUIRED_SHARED_LOCK) {
          thread.t_mutex_state = state;
          return true;
        } else {
          c3_assert_failure();
          return false;
        }
      case MXS_ACQUIRED_SHARED_LOCK:
        if (thread.t_mutex_ref == mutex && (thread.t_mutex_state == MXS_BEGIN_SHARED_LOCK ||
          thread.t_mutex_state == MXS_BEGIN_DOWNGRADE)) {
          thread.t_mutex_state = MXS_ACQUIRED_SHARED_LOCK;
          return true;
        } else {
          c3_assert_failure();
          return false;
        }
      case MXS_ACQUIRED_EXCLUSIVE_LOCK:
        if (thread.t_mutex_ref == mutex && (thread.t_mutex_state == MXS_BEGIN_EXCLUSIVE_LOCK ||
          thread.t_mutex_state == MXS_BEGIN_UPGRADE)) {
          thread.t_mutex_state = MXS_ACQUIRED_EXCLUSIVE_LOCK;
          return true;
        } else {
          c3_assert_failure();
          return false;
        }
      case MXS_SHARED_LOCK_FAILED:
        if (thread.t_mutex_ref == mutex && thread.t_mutex_state == MXS_BEGIN_SHARED_LOCK) {
          thread.t_mutex_ref = nullptr;
          thread.t_mutex_state = MXS_UNLOCKED;
          return true;
        } else {
          c3_assert_failure();
          return false;
        }
      case MXS_EXCLUSIVE_LOCK_FAILED:
        if (thread.t_mutex_ref == mutex && thread.t_mutex_state == MXS_BEGIN_EXCLUSIVE_LOCK) {
          thread.t_mutex_ref = nullptr;
          thread.t_mutex_state = MXS_UNLOCKED;
          return true;
        } else {
          c3_assert_failure();
          return false;
        }
      case MXS_DOWNGRADE_FAILED:
        if (thread.t_mutex_ref == mutex && thread.t_mutex_state == MXS_BEGIN_DOWNGRADE) {
          /*
           * If current state is "begin downgrade", it means initial *thread* check passed successfully
           * and, from thread's standpoint, the mutex was indeed locked in write mode. On the other
           * hand, since we're here, it also means that from the standpoint of the mutex itself it was
           * not in exclusive mode. A classic "can't happen!!!111". There isn't much that we can do to
           * recover here... Setting state to what the mutex itself thinks it is is all we can do
           * (which, again, does not help much).
           */
          thread.t_mutex_state = MXS_ACQUIRED_SHARED_LOCK;
          return true;
        } else {
          c3_assert_failure();
          return false;
        }
      case MXS_UPGRADE_FAILED:
        if (thread.t_mutex_ref == mutex && thread.t_mutex_state == MXS_BEGIN_UPGRADE) {
          // Situation is very similar to that with `MXS_DOWNGRADE_FAILED`; see notes above
          thread.t_mutex_state = MXS_ACQUIRED_EXCLUSIVE_LOCK;
          return true;
        } else {
          c3_assert_failure();
          return false;
        }
      default:
        assert_failure();
        return false;
    }
  } else {
    c3_assert_failure();
    return false;
  }
}

bool Thread::set_object_state(const LockableObject* lo, thread_object_state_t state) {
  assert(lo);
  c3_assert(local_thread_id < MAX_NUM_THREADS);
  Thread& thread = thread_pool[local_thread_id];

  if (thread.t_queue_state == MQS_UNUSED && thread.t_queue_ref == nullptr) {
    switch (state) {
      case LOS_UNLOCKED:
        if (thread.t_object_ref == lo && thread.t_object_state == LOS_BEGIN_UNLOCK) {
          thread.t_object_ref = nullptr;
          thread.t_object_state = LOS_UNLOCKED;
          return true;
        } else {
          c3_assert_failure();
          return false;
        }
      case LOS_BEGIN_TRY_LOCK:
      case LOS_BEGIN_LOCK:
        if (thread.t_object_ref == nullptr && thread.t_object_state == LOS_UNLOCKED) {
          thread.t_object_ref = lo;
          thread.t_object_state = state;
          return true;
        } else {
          c3_assert_failure();
          return false;
        }
      case LOS_BEGIN_UNLOCK:
        if (thread.t_object_ref == lo && thread.t_object_state == LOS_ACQUIRED_LOCK) {
          thread.t_object_state = LOS_BEGIN_UNLOCK;
          return true;
        } else {
          c3_assert_failure();
          return false;
        }
      case LOS_ACQUIRED_LOCK:
        if (thread.t_object_ref == lo && (thread.t_object_state == LOS_BEGIN_TRY_LOCK ||
          thread.t_object_state == LOS_BEGIN_LOCK)) {
          thread.t_object_state = LOS_ACQUIRED_LOCK;
          return true;
        } else {
          c3_assert_failure();
          return false;
        }
      case LOS_LOCK_FAILED:
        if (thread.t_object_ref == lo && thread.t_object_state == LOS_BEGIN_TRY_LOCK) {
          thread.t_object_ref = nullptr;
          thread.t_object_state = LOS_UNLOCKED;
          return true;
        } else {
          c3_assert_failure();
          return false;
        }
      default:
        assert_failure();
        return false;
    }
  } else {
    c3_assert_failure();
    return false;
  }
}

bool Thread::set_queue_state(const SyncObject* queue, thread_queue_state_t state) {
  assert(queue);
  c3_assert(local_thread_id < MAX_NUM_THREADS);
  Thread& thread = thread_pool[local_thread_id];

  switch (state) {
    case MQS_UNUSED:
      if (thread.t_queue_ref == queue && thread.t_queue_state != MQS_UNUSED) {
        thread.t_queue_ref = nullptr;
        thread.t_queue_state = MQS_UNUSED;
        return true;
      } else {
        c3_assert_failure();
        return false;
      }
    case MQS_IN_TRY_GET:
    case MQS_IN_GET:
    case MQS_IN_PUT:
    case MQS_IN_GET_CAPACITY:
    case MQS_IN_GET_MAX_CAPACITY:
    case MQS_IN_SET_CAPACITY:
    case MQS_IN_SET_MAX_CAPACITY:

      if (thread.t_queue_ref == nullptr && thread.t_queue_state == MQS_UNUSED) {
        thread.t_queue_ref = queue;
        thread.t_queue_state = state;
        return true;
      } else {
        c3_assert_failure();
        return false;
      }
    default:
      assert_failure();
      return false;
  }
}

#endif // C3LM_ENABLED

}
