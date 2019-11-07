/*
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
 * Optimizers for FPC and session domains.
 */
#ifndef _HT_OPTIMIZER_H
#define _HT_OPTIMIZER_H

#include "c3lib/c3lib.h"
#include "ht_stores.h"

namespace CyberCache {

/// Expiration modes supported by the optimizer
enum eviction_mode_t {
  /**
   * An invalid mode (placeholder)
   */
  EM_INVALID = 0,
  /**
   * Will not delete a record that has *not* expired unless total memory used by domain exceeds its
   * quota; if memory quota is set to "unlimited", then in this mode the optimizer will never delete a
   * record that is not expired in respective domain.
   */
  EM_STRICT_EXPIRATION_LRU,
  /**
   * This mode (the default for session domain) *does* take into account expiration timestamps and may
   * purge expired records even when the server still has enough free memory in the domain; however, even
   * in this mode the optimizer:
   *
   * - will remove a record with bigger TTL instead of a record that has smaller TTL but that was
   *   accessed more recently if it runs out of memory,
   *
   * - will "revive" expired records that are read or otherwise accessed (if they are have not been
   *   marked as "deleted" yet); to reflect this, `Zend_Cache_Backend_ExtendedInterface::getCapabilities()`
   *   reports the `expired_read` option as `true`.
   */
  EM_EXPIRATION_LRU,
  /**
   * This mode (the default for FPC domain) is "pure" LRU and never removes expired records just because
   * they have expired, although it still honors explicit garbage collection requests from Magento
   * application or server console (`GC` and `CLEAN old` commands, which delete expired records).
   */
  EM_LRU,
  /**
   * This mode works just like `EM_LRU`, except that it ignores even explicit garbage collection requests.
   */
  EM_STRICT_LRU,
  EM_NUMBER_OF_ELEMENTS
};

/// Base class for store optimizers
class Optimizer {

  /**
   * A number of optimization parameters have values that differ depending upon current CPU load. The
   * load is measured as ratio of currently busy connection threads to the number of CPU cores in the
   * system, and is (currently) subdivided into five "slots":
   *
   * 0: there are no active connection threads,
   * 1: number of active connection threads is less than or equal to 33% of CPU cores,
   * 2: number of active connection threads is between 33% and 66% of available CPU cores,
   * 3: number of active connection threads is greater than 66% but less than 100% of CPU cores,
   * 4: number of active connection threads is equal to or exceeds the number of CP cores.
   */
  static constexpr c3_uint_t NUM_LOAD_DEPENDENT_SLOTS = 5;
  /// Maximum number of compression algorithms to try during re-compression attempts
  static constexpr c3_uint_t NUM_COMPRESSORS = 8;
  /// If reported number of CPU cores is zero, use this number by default instead
  static constexpr c3_uint_t DEFAULT_NUM_CPU_CORES = 4;
  /// How much time to wait between runs by default, seconds
  static constexpr c3_uint_t DEFAULT_TIME_BETWEEN_RUNS = 20;
  /// Smallest buffer that the optimizer will attempt to re-compress, bytes
  static constexpr c3_uint_t DEFAULT_MIN_RECOMPRESSION_SIZE = 256;

  /// Number of CPU cores in the system
  static c3_uint_t o_num_cores;

  static const c3_compressor_t o_default_compressors[NUM_COMPRESSORS];
  static const c3_uint_t o_default_num_checks[NUM_LOAD_DEPENDENT_SLOTS];
  static const c3_uint_t o_default_num_comp_attempts[NUM_LOAD_DEPENDENT_SLOTS];

  // initializes the optimizer
  void initialize() C3_FUNC_COLD;
  // make sure GC mode is compatible with memory limit
  void validate_eviction_mode();
  // get human-readable name of an eviction mode
  static const char* get_eviction_mode_name(eviction_mode_t mode);
  // returns value in range [0..NUM_LOAD_DEPENDENT_SLOTS)
  static c3_uint_t get_cpu_load();
  // returns `true` if memory quota had been exceeded
  bool is_above_memory_quota() const;
  // returns `true` if object is subject for size optimization
  bool is_optimizable(const PayloadHashObject* pho) const {
    return pho->flags_are_clear(HOF_BEING_DELETED | HOF_OPTIMIZED) && !pho->has_readers() &&
      pho->get_buffer_usize() >= o_min_recompression_size;
  }

  /// Collection of objects submitted by the same type of user agent
  class ObjectChain {
    PayloadHashObject* oc_first;   // first object in the chain for this user agent type
    PayloadHashObject* oc_last;    // last object in the chain for this user agent type
    c3_uint_t          oc_num;     // number of object in the chain
    c3_uint_t          oc_min_num; // minimum number of object that should *not* be optimized away

  public:
    ObjectChain() {
      oc_first = nullptr;
      oc_last = nullptr;
      oc_num = 0;
      oc_min_num = 1;
    }

    c3_uint_t get_num_objects() const { return oc_num; }
    c3_uint_t get_num_retained_objects() const { return oc_min_num; }
    c3_uint_t set_num_retained_objects(c3_uint_t num) { return oc_min_num = num; }
    PayloadHashObject* get_first() const { return oc_first; }
    void link(PayloadHashObject* pho);
    void promote(PayloadHashObject* pho);
    void unlink(PayloadHashObject* pho);
    void unlink_all();
  };

  /// Helper class for iterating through object chains
  class ObjectChainIterator {
    Optimizer&         oci_optimizer;   // container of the object chains
    PayloadHashObject* oci_next_object; // next object to check during optimization or GC run

    PayloadHashObject* get_next_object(const PayloadHashObject* pho);
    PayloadHashObject* get_gc_object(PayloadHashObject* pho);

  public:
    explicit ObjectChainIterator(Optimizer& optimizer): oci_optimizer(optimizer) {
      oci_next_object = nullptr;
    }
    void prepare_next_object();
    PayloadHashObject* get_first_object();
    PayloadHashObject* get_next_object();
    PayloadHashObject* get_first_gc_object();
    PayloadHashObject* get_next_gc_object();

    void exclude_object(PayloadHashObject* pho) {
      if (pho == oci_next_object) {
        get_next_object();
      }
    }
    void unlink(PayloadHashObject* pho);
    void reset() {
      oci_next_object = nullptr;
    }
  };

protected:
  /// Types of requests that can be sent to the optimizer
  enum optimization_request_t: c3_byte_t {
    OR_INVALID = 0,                    // an invalid request (placeholder)
    OR_WRITE,                          // add a new object, or update an existing one
    OR_READ,                           // optionally "revive" an object and move it to the back of GC list
    OR_DELETE,                         // remove an object
    OR_GC,                             // do garbage collection
    OR_FREE_MEMORY,                    // dispose some objects to free up at least specified memory amount
    OR_CONFIG_WAIT_TIME,               // how many seconds to wait between runs
    OR_CONFIG_NUM_CHECKS,              // how many check (at most) to do during each run
    OR_CONFIG_NUM_COMP_ATTEMPTS,       // how many re-compression attempts (at most) to do during each run
    OR_CONFIG_COMPRESSORS,             // what compressors to use for re-compression attempts
    OR_CONFIG_RETAIN_COUNTS,           // how many objects to retain per each user agent type
    OR_CONFIG_EVICTION_MODE,           // eviction mode to use during optimization and GC runs
    OR_CONFIG_RECOMPRESSION_THRESHOLD, // only attempt re-compression if object buffer is bigger than this
    OR_QUEUE_CAPACITY,                 // queue capacity
    OR_QUEUE_MAX_CAPACITY,             // maximum queue capacity
    OR_QUIT,                           // complete queue processing and then quit
    OR_SESSION_FIRST_WRITE_LIFETIMES,  // lifetimes to set upon first write to session object
    OR_SESSION_FIRST_WRITE_NUMS,       // numbers of steps to use while ramping up lifetime to default
    OR_SESSION_DEFAULT_LIFETIMES,      // lifetimes of session objects after first specified num of writes
    OR_SESSION_READ_EXTRA_LIFETIMES,   // lifetimes to add upon some reads of session objects
    OR_FPC_DEFAULT_LIFETIMES,          // default lifetimes for FPC cache entries
    OR_FPC_READ_EXTRA_LIFETIMES,       // lifetimes to add upon some reads of FPC cache entries
    OR_FPC_MAX_LIFETIMES,              // max allowed lifetimes (clips specified in `SAVE` requests)
    OR_FPC_TOUCH,                      // update FPC record's expiration timestamp
    OR_NUMBER_OF_ELEMENTS
  };

  /// Types of *extra* arguments that can be passed in the optimization request
  enum optimization_argument_t: c3_byte_t {
    OA_INVALID = 0,  // an invalid argument (placeholder)
    OA_NONE,         // there are not extra arguments
    OA_OBJECT,       // a pointer to `PayloadHashObject`
    OA_LONG,         // an unsigned long integer
    OA_BYTE_ARRAY,   // a byte array with up to eight elements (passed within request)
    OA_UINT_ARRAY,   // an array of unsigned integers possibly allocated on the heap (placeholder!)
    OA_UINT_ARRAY_1, // an array containing one unsigned integer (passed within request)
    OA_UINT_ARRAY_2, // an array containing two unsigned integers (passed within request)
    OA_UINT_ARRAY_3, // an array containing three unsigned integers (allocated on heap)
    OA_UINT_ARRAY_4, // an array containing four unsigned integers (allocated on heap)
    OA_UINT_ARRAY_5  // an array containing five unsigned integers (allocated on heap)
  };

  /// Message type for use with optimizer's message queue
  class OptimizerMessage {
    static constexpr c3_uint_t MAX_NUM_RETRIES = 256; // how many times to try to re-post "delete" message

    union {
      PayloadHashObject* om_object;        // pointer to the object being added/updated/deleted
      c3_ulong_t         om_long;          // an unsigned long integer
      c3_uint_t          om_int_array[2];  // array of up to two integers
      c3_uint_t*         om_int_pointer;   // array of three or more integers
      c3_compressor_t    om_byte_array[8]; // array of compression type IDs
    };
    optimization_request_t  om_request;    // request type, an OR_xxx constant
    optimization_argument_t om_argument;   // type of the argument, an OA_xxx constant
    user_agent_t            om_user_agent; // user agent active at the time of request, an UA_xxx constant
    domain_t                om_domain;     // memory domain, a DOMAIN_xxx constant
    c3_uint_t               om_lifetime;   // number of seconds

  public:
    OptimizerMessage() {
      om_request = OR_INVALID;
      om_argument = OA_NONE;
    }
    OptimizerMessage(const OptimizerMessage&) = delete;
    OptimizerMessage(OptimizerMessage&& that) noexcept {
      std::memcpy(this, &that, sizeof(OptimizerMessage));
      that.om_request = OR_INVALID;
      that.om_argument = OA_NONE;
    }
    OptimizerMessage& operator=(const OptimizerMessage& that) = delete;
    OptimizerMessage& operator=(OptimizerMessage&& that) noexcept {
      std::swap(om_object, that.om_object);
      std::swap(om_request, that.om_request);
      std::swap(om_argument, that.om_argument);
      std::swap(om_user_agent, that.om_user_agent);
      std::swap(om_domain, that.om_domain);
      std::swap(om_lifetime, that.om_lifetime);
      return *this;
    }

    explicit OptimizerMessage(optimization_request_t request) {
      c3_assert(request < OR_NUMBER_OF_ELEMENTS);
      om_request = request;
      om_argument = OA_NONE;
    }
    OptimizerMessage(optimization_request_t request, PayloadHashObject* object, user_agent_t user_agent,
      c3_uint_t lifetime) {
      c3_assert(request < OR_NUMBER_OF_ELEMENTS && user_agent < UA_NUMBER_OF_ELEMENTS &&
        object && object->flags_are_set(HOF_PAYLOAD));
      om_object = object;
      om_request = request;
      om_argument = OA_OBJECT;
      om_user_agent = user_agent;
      om_lifetime = lifetime;
    }
    OptimizerMessage(optimization_request_t request, PayloadHashObject* object, c3_uint_t lifetime) {
      c3_assert(request < OR_NUMBER_OF_ELEMENTS && object && object->flags_are_set(HOF_PAYLOAD));
      om_object = object;
      om_request = request;
      om_argument = OA_OBJECT;
      om_user_agent = UA_NUMBER_OF_ELEMENTS;
      om_lifetime = lifetime;
    }
    OptimizerMessage(optimization_request_t request, c3_uint_t lifetime) {
      c3_assert(request < OR_NUMBER_OF_ELEMENTS);
      om_request = request;
      om_argument = OA_NONE;
      om_lifetime = lifetime;
    }
    OptimizerMessage(optimization_request_t request, c3_ulong_t num) {
      c3_assert(request < OR_NUMBER_OF_ELEMENTS);
      om_request = request;
      om_argument = OA_LONG;
      om_long = num;
    }
    OptimizerMessage(optimization_request_t request, const c3_compressor_t* compressors,
      c3_uint_t num = NUM_COMPRESSORS) {
      c3_assert(request < OR_NUMBER_OF_ELEMENTS && compressors && num <= NUM_COMPRESSORS);
      c3_uint_t i = 0;
      while (i < num) {
        om_byte_array[i] = compressors[i];
        i++;
      }
      while (i < NUM_COMPRESSORS) {
        om_byte_array[i] = CT_NONE;
      }
      om_request = request;
      om_argument = OA_BYTE_ARRAY;
    }
    OptimizerMessage(optimization_request_t request, const c3_uint_t* array, c3_uint_t num) {
      c3_assert(request < OR_NUMBER_OF_ELEMENTS && array && (num == 1 || num == 2));
      om_request = request;
      om_argument = (optimization_argument_t)(OA_UINT_ARRAY + num);
      c3_assert(om_argument == OA_UINT_ARRAY_1 || om_argument == OA_UINT_ARRAY_2);
      om_int_array[0] = array[0];
      if (num == 2) {
        om_int_array[1] = array[1];
      }
    }
    OptimizerMessage(optimization_request_t request, Memory& memory, const c3_uint_t* array, c3_uint_t num) {
      c3_assert(request < OR_NUMBER_OF_ELEMENTS && array && num && num <= 5);
      om_request = request;
      om_argument = (optimization_argument_t)(OA_UINT_ARRAY + num);
      switch (num) {
        case 2:
          om_int_array[1] = array[1];
          // fall through
        case 1:
          om_int_array[0] = array[0];
          break;
        default:
          c3_assert(num >= 3 && om_argument >= OA_UINT_ARRAY_3);
          om_domain = memory.get_domain();
          om_int_pointer = (c3_uint_t*) memory.alloc(num * sizeof(c3_uint_t));
          c3_uint_t i = 0;
          do {
            om_int_pointer[i] = array[i];
          } while (++i < num);
      }
    }
    ~OptimizerMessage() {
      if (om_argument >= OA_UINT_ARRAY_3) {
        Memory& memory = Memory::get_memory_object(om_domain);
        memory.free(om_int_pointer, (om_argument - OA_UINT_ARRAY) * sizeof(c3_uint_t));
      }
    }

    bool is_valid() const { return om_request != OR_INVALID; }
    bool is_object_message() const { return om_argument == OA_OBJECT; }
    bool increment_num_retries() {
      c3_assert(om_request == OR_DELETE && om_argument == OA_OBJECT && om_lifetime < MAX_NUM_RETRIES);
      return ++om_lifetime < MAX_NUM_RETRIES;
    }
    optimization_request_t get_request() const { return om_request; }
    PayloadHashObject* get_object() const {
      c3_assert(om_argument == OA_OBJECT && om_object && om_object->flags_are_set(HOF_PAYLOAD));
      return om_object;
    }
    user_agent_t get_user_agent() const {
      c3_assert(om_argument == OA_OBJECT && om_object && om_object->flags_are_set(HOF_PAYLOAD));
      return om_user_agent;
    }
    c3_uint_t get_lifetime() const {
      c3_assert(om_argument == OA_OBJECT && om_object && om_object->flags_are_set(HOF_PAYLOAD));
      return om_lifetime;
    }
    c3_ulong_t get_ulong() const {
      c3_assert(om_argument == OA_LONG);
      return om_long;
    }
    const c3_compressor_t* get_compressors() const {
      c3_assert(om_argument == OA_BYTE_ARRAY);
      return om_byte_array;
    }
    c3_uint_t get_num_uints() const {
      c3_assert(om_argument > OA_UINT_ARRAY);
      return om_argument - OA_UINT_ARRAY;
    }
    c3_uint_t get_uint() const {
      c3_assert(om_argument == OA_UINT_ARRAY_1);
      return om_int_array[0];
    }
    c3_uint_t get_uint(c3_uint_t i) const {
      c3_assert(om_argument > OA_UINT_ARRAY && i < (c3_uint_t)(om_argument - OA_UINT_ARRAY));
      if (om_argument < OA_UINT_ARRAY_3) {
        return om_int_array[i];
      } else {
        c3_assert(om_int_pointer);
        return om_int_pointer[i];
      }
    }
    const c3_uint_t* get_uints() const {
      c3_assert(om_argument > OA_UINT_ARRAY);
      return om_argument < OA_UINT_ARRAY_3? om_int_array: om_int_pointer;
    }
  };

  static_assert(sizeof(OptimizerMessage) == 16, "Invalid 'OptimizerMessage' size");
  typedef CriticalMessageQueue<OptimizerMessage> OptimizerQueue;

  ObjectChain         o_chain[UA_NUMBER_OF_ELEMENTS]; // objects managed by the optimizer
  MemoryInterface*    o_host;                         // object that has to be notified upon deallocation
  PayloadObjectStore* o_store;                        // associated object store
  const char* const   o_name;                         // optimizer identification string
  Memory&             o_memory;                       // memory object
  OptimizerQueue      o_queue;                        // queue of optimization requests
  ObjectChainIterator o_iterator;                     // next object to check during optimization run
  c3_compressor_t     o_compressors[NUM_COMPRESSORS]; // compression algorithms to use for re-compression
  c3_uint_t           o_num_checks[NUM_LOAD_DEPENDENT_SLOTS]; // checks to do during each run
  c3_uint_t           o_num_comp_attempts[NUM_LOAD_DEPENDENT_SLOTS]; // re-compresion attempts to do
  c3_uint_t           o_total_num_objects;            // total num of objects managed by this optimizer
  c3_uint_t           o_wait_time;                    // seconds to wait between optimization runs
  c3_uint_t           o_min_recompression_size;       // smallest buffer that can be re-compressed
  atomic_timestamp_t  o_last_run_time;                // time of the last optimization run
  std::atomic_uint    o_last_run_checks;              // number of checks during last optimization run
  std::atomic_uint    o_last_run_compressions;        // number of re-compressions during last optimization run
  c3_timestamp_t      o_last_save_time;               // last time optimizer requested auto-save from the server
  eviction_mode_t     o_eviction_mode;                // current object eviction strategy
  bool                o_quitting;                     // `true` if `QUIT` request had been received

  void process_write_message(PayloadHashObject* pho, user_agent_t ua, c3_timestamp_t lifetime);
  void process_read_message(PayloadHashObject* pho, user_agent_t ua);
  void process_delete_message(Optimizer::OptimizerMessage& msg);
  void process_gc_message(c3_uint_t seconds);
  void process_free_memory_message(c3_ulong_t min_size, bool direct);

  void process_generic_load_slot_message(const char* what, c3_uint_t* dst, const c3_uint_t* src) C3_FUNC_COLD;
  void process_generic_ua_slot_message(const char* what, c3_uint_t* dst, const c3_uint_t* src) C3_FUNC_COLD;

  void process_config_wait_time_message(c3_uint_t seconds) C3_FUNC_COLD;
  void process_config_compressors_message(const c3_compressor_t* compressors) C3_FUNC_COLD;
  void process_config_retain_counts_message(const c3_uint_t* retain_counts) C3_FUNC_COLD;
  void process_config_eviction_mode_message(eviction_mode_t mode) C3_FUNC_COLD;
  void process_config_recompression_threshold_message(c3_uint_t threshold) C3_FUNC_COLD;
  void process_config_capacity_message(c3_uint_t capacity) C3_FUNC_COLD;
  void process_config_max_capacity_message(c3_uint_t max_capacity) C3_FUNC_COLD;

  void process_message(OptimizerMessage &msg);
  void run(c3_timestamp_t current_time);
  void enter_quit_state() C3_FUNC_COLD;
  void cleanup() C3_FUNC_COLD;

  /**
   * This method is called upon writing (i.e. when contents of its data buffer is updated); the lifetime
   * argument is the one passed to the server -- except that `-1` replaced with `UINT_MAX_VAL`.
   *
   * The object is locked and moved towards the very end of the GC list (for specified user agent) before
   * this method is called.
   *
   * The "last modification time" of the object is updated right before calling this method;
   * implementations can count on this and use this value for calculating expiration time.
   */
  virtual void on_write(PayloadHashObject* pho, c3_timestamp_t lifetime) = 0;
  /**
   * This method is called when an object is accessed in any way other than to store new data into data
   * buffer (with the only exception being the `TOUCH` FPC-only command that is processed separately);
   * object's lifetime can be adjusted according to domain's policy (which for session domain depends on
   * the user agent accessing the record).
   *
   * The object locked and is moved towards the very end of the GC list (for specified user agent) before
   * this method is called.
   */
  virtual void on_read(PayloadHashObject* pho) = 0;
  /**
   * Called on three occasions:
   *
   * - during optimization run when optimizer found an object that has to be garbage collected, or
   * - during garbage collection run, or
   * - when optimizer handles "insufficient memory" request by `Memory` allocator.
   *
   * By the time this method is called, the object is already marked as "deleted", unlinked from internal
   * lists of the optimizer, and (contrary to other `on_xxx()` methods) *unlocked*. Additionally, an
   * attempt had already been made to free up its data buffer.
   *
   * The purpose of this method is to dispatch the object to the next subsystem; session optimizer should
   * post the object to session store's queue of deleted objects, while FPC optimizer should send the
   * object to tag manager.
   */
  virtual void on_delete(PayloadHashObject* pho) = 0;
  /**
   * Called when optimizer's garbage collector stumbles upon an object that is eligible for disposal (not
   * marked as "deleted" yet, and has no readers), BUT there is still enough memory in the domain
   * (otherwise, garbage collector would just dispose the object right away), AND cache eviction strategy
   * is set to "strict expiration LRU" or "expiration LRU" (otherwise, expiration properties of the
   * object would be ignored, and this method would not be called).
   *
   * This method should return `true` if argument object has to be disposed; FPC and session optimizers
   * process this request differently:
   *
   * - FPC optimizer purges records that are "expired" (i.e. have expiration timestamps that are in the
   *   past), while
   * - session optimizer purges records that were not updated during specified number of seconds.
   *
   * Caller ensures that current eviction mode is not "strict LRU" (in which case GC is disabled) or
   * "LRU" (in which case expiration properties are ignored), that the object is locked, not yet marked
   * as "deleted", and does not have active readers.
   */
  virtual bool on_gc(PayloadHashObject* pho, c3_uint_t seconds) = 0;
  /**
   * This method is called when thread proc encounters a message with ID it does not recognize; derived
   * classes should bail our with assertion failure if they do not recognize it either.
   */
  virtual void on_message(OptimizerMessage& msg) = 0;
  /**
   * Get interval between auto-save requests for optimizer's domain (session or FPC).
   *
   * @return Number of secords, or 0 if auto-save is disabled for the domain.
   */
  virtual c3_timestamp_t get_autosave_interval() = 0;
  /**
   * Send "save data for this domain to a file" command to the main thread. This method puts the command to
   * server's message queue and immediately returns.
   */
  virtual void send_autosave_command() = 0;

  Optimizer(const char* name, domain_t domain, eviction_mode_t em, c3_uint_t capacity,
    c3_uint_t max_capacity) C3_FUNC_COLD;

  // interface accessors
  MemoryInterface& get_host() {
    c3_assert(o_host);
    return *o_host;
  }
  void set_host(MemoryInterface* host) {
    c3_assert(host);
    o_host = host;
  }
  PayloadObjectStore& get_store() const {
    c3_assert(o_store);
    return *o_store;
  }
  void set_store(PayloadObjectStore* store) {
    c3_assert(store && o_store == nullptr);
    o_store = store;
  }
  ObjectChain& get_chain(user_agent_t ua) {
    c3_assert(ua < UA_NUMBER_OF_ELEMENTS);
    return o_chain[ua];
  }

public:
  static constexpr c3_uint_t get_num_cpu_load_levels() { return NUM_LOAD_DEPENDENT_SLOTS; }
  static constexpr c3_uint_t get_num_compressors() { return NUM_COMPRESSORS; }
  eviction_mode_t get_eviction_mode() const { return o_eviction_mode; }
  c3_uint_t get_optimization_interval() const { return o_wait_time; }
  const c3_compressor_t* get_compressors() const { return o_compressors; }
  c3_uint_t get_recompression_threshold() const { return o_min_recompression_size; }
  c3_uint_t get_queue_capacity() C3LM_OFF(const) { return o_queue.get_capacity(); }
  c3_uint_t get_max_queue_capacity() C3LM_OFF(const) { return o_queue.get_max_capacity(); }
  void free_memory_chunk(c3_ulong_t min_size) { process_free_memory_message(min_size, true); }
  c3_uint_t reduce_queue_capacity() { return o_queue.reduce_capacity()? 1: 0; }

  c3_timestamp_t get_last_run_time() const { return o_last_run_time.load(std::memory_order_relaxed); }
  c3_uint_t get_last_run_checks() const { return o_last_run_checks.load(std::memory_order_relaxed); }
  c3_uint_t get_last_runs_compressions() const { return o_last_run_compressions.load(std::memory_order_relaxed); }

  bool post_write_message(PayloadHashObject* object, user_agent_t user_agent, c3_uint_t lifetime);
  bool post_read_message(PayloadHashObject* object, user_agent_t user_agent);
  bool post_delete_message(PayloadHashObject* object);
  bool post_gc_message(c3_uint_t seconds);
  bool post_free_memory_message(c3_ulong_t min_size);
  bool post_config_wait_time_message(c3_uint_t wait_time) C3_FUNC_COLD;
  bool post_config_num_checks_message(c3_uint_t* num_checks) C3_FUNC_COLD;
  bool post_config_num_comp_attempts_message(const c3_uint_t* num_attempts) C3_FUNC_COLD;
  bool post_config_compressors_message(const c3_compressor_t* compressors) C3_FUNC_COLD;
  bool post_config_retain_counts_message(const c3_uint_t* retain_counts) C3_FUNC_COLD;
  bool post_config_eviction_mode_message(c3_uint_t mode) C3_FUNC_COLD;
  bool post_config_recompression_threshold_message(c3_uint_t threshold) C3_FUNC_COLD;
  bool post_queue_capacity_message(c3_uint_t capacity) C3_FUNC_COLD;
  bool post_queue_max_capacity_message(c3_uint_t max_capacity) C3_FUNC_COLD;
  bool post_quit_message() C3_FUNC_COLD;

  static void thread_proc(c3_uint_t id, ThreadArgument arg);
};

/// Specialized optimizer for the session domain
class SessionOptimizer: public Optimizer {
  static constexpr c3_uint_t DEFAULT_QUEUE_CAPACITY = 32;
  static constexpr c3_uint_t DEFAULT_MAX_QUEUE_CAPACITY = 1024;

  static const c3_timestamp_t so_default_first_write_lifetimes[UA_NUMBER_OF_ELEMENTS];
  static const c3_uint_t      so_default_first_write_nums[UA_NUMBER_OF_ELEMENTS];
  static const c3_timestamp_t so_default_default_lifetimes[UA_NUMBER_OF_ELEMENTS];
  static const c3_timestamp_t so_default_read_extra_lifetimes[UA_NUMBER_OF_ELEMENTS];

  c3_timestamp_t so_first_write_lifetimes[UA_NUMBER_OF_ELEMENTS]; // lifetimes on first write to session object
  c3_uint_t      so_first_write_nums[UA_NUMBER_OF_ELEMENTS];      // steps from first write lifetime to default
  c3_timestamp_t so_default_lifetimes[UA_NUMBER_OF_ELEMENTS];     // lifetimes after 1st specified num of writes
  c3_timestamp_t so_read_extra_lifetimes[UA_NUMBER_OF_ELEMENTS];  // lifetimes to add upon certain reads

  void on_write(PayloadHashObject* pho, c3_timestamp_t lifetime) override;
  void on_read(PayloadHashObject* pho) override;
  void on_delete(PayloadHashObject* pho) override;
  bool on_gc(PayloadHashObject* pho, c3_uint_t seconds) override;
  void on_message(OptimizerMessage& msg) override C3_FUNC_COLD;
  c3_timestamp_t get_autosave_interval() override;
  void send_autosave_command() override;

public:
  SessionOptimizer() noexcept C3_FUNC_COLD;

  void configure(MemoryInterface* host, PayloadObjectStore* store) C3_FUNC_COLD {
    set_host(host);
    set_store(store);
  }
  const c3_uint_t* get_first_write_lifetimes() const { return so_first_write_lifetimes; }
  const c3_uint_t* get_first_write_nums() const { return so_first_write_nums; }
  const c3_uint_t* get_default_lifetimes() const { return so_default_lifetimes; }
  const c3_uint_t* get_read_extra_lifetimes() const { return so_read_extra_lifetimes; }

  bool post_session_first_write_lifetimes_message(const c3_uint_t* lifetimes) C3_FUNC_COLD;
  bool post_session_first_write_nums_message(c3_uint_t* nums) C3_FUNC_COLD;
  bool post_session_default_lifetimes_message(const c3_uint_t* lifetimes) C3_FUNC_COLD;
  bool post_session_read_extra_lifetimes_message(const c3_uint_t* lifetimes) C3_FUNC_COLD;
};

class TagStore;

/// Specialized optimizer for the session domain
class PageOptimizer: public Optimizer {
  static constexpr c3_uint_t DEFAULT_QUEUE_CAPACITY = 32;
  static constexpr c3_uint_t DEFAULT_MAX_QUEUE_CAPACITY = 1024;

  static const c3_timestamp_t po_default_default_lifetimes[UA_NUMBER_OF_ELEMENTS];
  static const c3_timestamp_t po_default_read_extra_lifetimes[UA_NUMBER_OF_ELEMENTS];
  static const c3_timestamp_t po_default_max_lifetimes[UA_NUMBER_OF_ELEMENTS];

  c3_timestamp_t po_default_lifetimes[UA_NUMBER_OF_ELEMENTS];    // default lifetimes for FPC records
  c3_timestamp_t po_read_extra_lifetimes[UA_NUMBER_OF_ELEMENTS]; // intervals to add to FPC records on reads
  c3_timestamp_t po_max_lifetimes[UA_NUMBER_OF_ELEMENTS];        // maximum allowed lifetimes for FPC records
  TagStore*      po_tag_store;                                   // pointer to associated tag manager

  TagStore& get_tag_manager() const {
    c3_assert(po_tag_store);
    return *po_tag_store;
  }
  void set_tag_manager(TagStore* store) {
    c3_assert(store && po_tag_store == nullptr);
    po_tag_store = store;
  }

  void process_fpc_touch_message(PayloadHashObject* pho, c3_uint_t lifetime);

  void on_write(PayloadHashObject* pho, c3_timestamp_t lifetime) override;
  void on_read(PayloadHashObject* pho) override;
  void on_delete(PayloadHashObject* pho) override;
  bool on_gc(PayloadHashObject* pho, c3_uint_t seconds) override;
  void on_message(OptimizerMessage& msg) override C3_FUNC_COLD;
  c3_timestamp_t get_autosave_interval() override;
  void send_autosave_command() override;

public:
  PageOptimizer() noexcept C3_FUNC_COLD;

  void configure(MemoryInterface* host, PayloadObjectStore* object_store, TagStore* tag_store) C3_FUNC_COLD {
    set_host(host);
    set_store(object_store);
    set_tag_manager(tag_store);
  }
  const c3_uint_t* get_default_lifetimes() const { return po_default_lifetimes; }
  const c3_uint_t* get_read_extra_lifetimes() const { return po_read_extra_lifetimes; }
  const c3_uint_t* get_max_lifetimes() const { return po_max_lifetimes; }

  bool post_fpc_default_lifetimes_message(const c3_uint_t* lifetimes) C3_FUNC_COLD;
  bool post_fpc_read_extra_lifetimes_message(const c3_uint_t* lifetimes) C3_FUNC_COLD;
  bool post_fpc_max_lifetimes_message(const c3_uint_t* lifetimes) C3_FUNC_COLD;
  bool post_fpc_touch_message(PayloadHashObject* object, c3_uint_t lifetime) C3_FUNC_COLD;
};

} // CyberCache

#endif // _HT_OPTIMIZER_H
