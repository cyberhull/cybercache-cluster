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
 * Main server class.
 */
#ifndef _CC_SERVER_H
#define _CC_SERVER_H

#include "cc_server_queue.h"
#include "ls_system_logger.h"
#include "ht_stores.h"
#include "pl_file_pipelines.h"
#include "pl_socket_pipelines.h"

namespace CyberCache {

/**
 * Possible server states.
 *
 * If an error occurs when server is in "config" state, the server can exit immediately without shutting
 * down its subsystems (which have not been initialized yet). In this state, contrary to other/later
 * states, it is possible to set passwords, hash methods, and numbers of hash tables per store; once
 * server leaves "config" state, altering these particular settings is no longer possible.
 */
enum server_state_t {
  SS_INVALID = 0, // invalid state (placeholder)
  SS_INIT,        // initialization that does not involve configurable parameters
  SS_CONFIG,      // configuring subsystems
  SS_START,       // starting subsystems
  SS_RUN,         // main loop, servicing requests
  SS_SHUTDOWN,    // shutting down
  SS_NUMBER_OF_ELEMENTS
};

/// Helper class that simplifies handling of certain paths by the server
class PathString: protected String {
public:
  PathString() = default;
  bool is_set() const { return not_empty(); }
  const char* get() const;
  void set(const char* buffer);
};

/// Object implementing server startup, shutdown, configuration changes, and health checks.
class Server: public MemoryInterface, public LogInterface, public ThreadInterface, public SystemLogger {
  static constexpr c3_uint_t DEFAULT_HEALTH_CHECK_INTERVAL = minutes2seconds(10);
  static constexpr c3_long_t DEFAULT_FREE_DISK_SPACE_THRESHOLD = megabytes2bytes(64);
  static constexpr c3_long_t DEFAULT_THREAD_ACTIVITY_TIME_THRESHOLD = 5000000; // useconds
  static constexpr c3_uint_t DEFAULT_THREAD_QUIT_TIME = 3000; // milliseconds
  static constexpr c3_uint_t DEFAULT_NUM_CONNECTION_THREADS = 2;
  static constexpr c3_uint_t THREAD_INITIALIZATION_WAIT_TIME = 200; // milliseconds
  static constexpr c3_ulong_t DEFAULT_DEALLOC_CHUNK_SIZE = megabytes2bytes(64);
  static constexpr c3_ulong_t DEFAULT_DEALLOC_MAX_WAIT_TIME = 1500; // milliseconds
  static constexpr c3_uint_t DEFAULT_STORE_DB_DURATION = 5; // seconds
  static constexpr c3_uint_t DEFAULT_STORE_DB_MAX_DURATION = 10 * 60; // seconds
  static constexpr const char* DEFAULT_CONFIG_FILE_PATH = "/etc/cybercache/cybercached.cfg";
  static constexpr const char* DEFAULT_LOG_FILE_PATH = "/var/log/cybercache/cybercached.log";
  static constexpr const char* PID_FILE_PATH = "/var/run/cybercache/cybercached.pid";
  static constexpr const char* DEFAULT_STORE_DB_PATH_PREFIX = "/var/lib/cybercache/";

  ServerMessageQueue      sr_queue;                   // configuration and notification messages
  const char*             sr_exe_file_path;           // path to executable file reported by the system
  c3_timestamp_t          sr_start_time;              // when the server was started
  c3_timestamp_t          sr_last_check;              // time of the last health check
  c3_uint_t               sr_check_interval;          // interval between health checks, seconds
  std::atomic_uint        sr_warning_count;           // warnings encountered far (reported by `CHECK`)
  std::atomic_uint        sr_error_count;             // server errors occurred so far (reported by `CHECK`)
  c3_uint_t               sr_thread_quit_time;        // how long to wait for a thread to quit
  server_state_t          sr_state;                   // current server state
  c3_uint_t               sr_cfg_num_threads;         // worker threads requested in configuration
  String                  sr_cfg_log_path;            // log file path requested in configuration
  String                  sr_cfg_user_password;       // "user" password requested in configuration
  String                  sr_cfg_admin_password;      // "admin" password requested in configuration
  String                  sr_cfg_bulk_password;       // "bulk" password requested in configuration
  c3_long_t               sr_disk_space_threshold;    // warn if less than this many bytes are free on disk
  c3_long_t               sr_thread_active_threshold; // warn if thread is active for this many usecs
  std::atomic_ullong      sr_dealloc_chunk_size;      // free this much if run out of memory
  std::atomic_uint        sr_dealloc_max_wait_time;   // msecs to wait for deallocation
  std::mutex              sr_dealloc_mutex;           // mutex to use during memory deallocation
  std::condition_variable sr_dealloc_notifier;        // notifier about memory deallocation completion
  std::atomic_bool        sr_dealloc_in_progress;     // `true` if deallocation is in progress
  c3_uint_t               sr_store_db_duration;       // how many seconds to wait before [next] binlog saver check
  c3_uint_t               sr_store_db_max_duration;   // how many seconds to wait before aborting save operation
  PathString              sr_session_db_file;         // session database [base] file name
  PathString              sr_fpc_db_file;             // FPC database [base] file name
  sync_mode_t             sr_session_db_sync;         // synchronization mode for writing session db files
  sync_mode_t             sr_fpc_db_sync;             // synchronization mode for writing FPC db files
  user_agent_t            sr_session_db_include;      // persist session records created by this or "higher" users
  user_agent_t            sr_fpc_db_include;          // persist FPC records created by this or "higher" users
  atomic_timestamp_t      sr_session_auto_save;       // session store auto-save interval, seconds
  atomic_timestamp_t      sr_fpc_auto_save;           // FPC store auto-save interval, seconds
  bool                    sr_binlog_saver_ok;         // `false` if an unrecoverable error has ocured
  bool                    sr_pid_file_created;        // whether we have to delete PID file on exit
  bool                    sr_log_level_set;           // `true` if log level had been set using cmd line

  // accessors
  c3_uint_t get_warning_count() const {
    return sr_warning_count.load(std::memory_order_acquire);
  }
  c3_uint_t get_error_count() const {
    return sr_error_count.load(std::memory_order_acquire);
  }
  bool is_dealloc_in_progress() const {
    return sr_dealloc_in_progress.load(std::memory_order_acquire);
  }
  void dealloc_in_progress(bool flag) {
    sr_dealloc_in_progress.store(flag, std::memory_order_release);
  }

  // PID-related methods
  bool check_create_pid_file() C3_FUNC_COLD;
  void delete_pid_file() C3_FUNC_COLD;

  // command line processing utilities
  static char* str_replace(char* buffer, char character, char replacement) C3_FUNC_COLD;
  static char* preprocess_option_name(char* name) C3_FUNC_COLD;
  static char* preprocess_option_value(char* buffer) C3_FUNC_COLD;

  // other utilities
  static void print_information(const char* exe_path, bool print_usage_info) C3_FUNC_COLD;
  void check_memory_quota(const char* type, Memory& memory) C3_FUNC_COLD;
  static c3_uint_t get_current_server_load() C3_FUNC_COLD;
  bool set_password(String& password_string, const char* password) C3_FUNC_COLD;
  bool load_config_file(const char* path) C3_FUNC_COLD;
  void parse_log_level_option(const char* option, char separator) C3_FUNC_COLD;
  bool wait_for_quitting_thread(thread_id_t id) C3_FUNC_COLD;
  void wait_for_deallocation(std::unique_lock<std::mutex>& lock);

  // store persistence support
  /// Structure that holds context of store saving procedure
  struct store_db_context_t {
    PayloadObjectStore&  sdc_store;      // store that is being saved
    FileOutputPipeline&  sdc_pipeline;   // output pipeline that should receive objects being saved
    const c3_timestamp_t sdc_time;       // when operation started
    const user_agent_t   sdc_user_agent; // only objects created by this level and above should be saved
    store_db_context_t(PayloadObjectStore& store, FileOutputPipeline& pipeline, user_agent_t user_agent);
  };
  static bool save_object(void* context, HashObject* ho);
  void set_db_path(char* path, const char* name);
  void load_store(const char* name);
  bool save_store(PayloadObjectStore &store, const char* name, sync_mode_t sync, user_agent_t ua, bool overwrite);
  bool save_session_store();
  bool save_fpc_store();

  // helper methods used by run()
  bool do_health_check(c3_uint_t& num_warnings);
  bool process_id_command(server_command_t command);
  void execute_ping_command(const CommandReader& cr);
  void execute_check_command(const CommandReader& cr);
  void add_memory_info(PayloadListChunkBuilder& list, const char* name, Memory& memory) C3_FUNC_COLD;
  void add_connections_info(PayloadListChunkBuilder& list, const char* name, const SocketPipeline& pipeline)
    C3_FUNC_COLD;
  void add_store_info(PayloadListChunkBuilder& list, const char* name, ObjectStore& store, c3_uint_t bias)
    C3_FUNC_COLD;
  void add_optimizer_info(PayloadListChunkBuilder& list, const char* name, Optimizer& optimizer) C3_FUNC_COLD;
  void add_replicator_info(PayloadListChunkBuilder& list, const char* name, SocketPipeline& pipeline) C3_FUNC_COLD;
  void add_service_info(PayloadListChunkBuilder& list, const char* name, FileBase& service) C3_FUNC_COLD;
  void execute_info_command(const CommandReader& cr);

  #if C3_INSTRUMENTED
  static bool counter_enumeration_callback(const PerfCounter* counter, void* context);
  #endif
  void execute_stats_command(const CommandReader& cr) C3_FUNC_COLD;
  bool execute_shutdown_command(const CommandReader& cr) C3_FUNC_COLD;
  void execute_loadconfig_command(const CommandReader& cr) C3_FUNC_COLD;
  void execute_restore_command(const CommandReader& cr) C3_FUNC_COLD;
  void execute_store_command(const CommandReader& cr) C3_FUNC_COLD;
  static bool option_enumeration_callback(void* context, const char* command) C3_FUNC_COLD;
  void execute_get_command(const CommandReader& cr) C3_FUNC_COLD;
  void execute_set_command(const CommandReader& cr) C3_FUNC_COLD;
  void execute_log_command(const CommandReader& cr) C3_FUNC_COLD;
  void execute_rotate_command(const CommandReader& cr) C3_FUNC_COLD;
  bool process_object_command(const CommandReader& cr) C3_FUNC_COLD;

public:
  Server() noexcept C3_FUNC_COLD;

  // minimal cleanup necessary even when an unrecoverable error occurs
  void on_abort() C3_FUNC_COLD { delete_pid_file(); }

  // configuration/state management
  server_state_t get_state() const { return sr_state; }
  void set_log_level(log_level_t level) C3_FUNC_COLD;
  c3_uint_t get_health_check_interval() const { return sr_check_interval; }
  void set_health_check_interval(c3_uint_t seconds) C3_FUNC_COLD;
  c3_uint_t get_thread_quit_time() const { return sr_thread_quit_time; }
  void set_thread_quit_time(c3_uint_t msecs) { sr_thread_quit_time = msecs; }
  bool set_num_connection_threads(c3_uint_t num) C3_FUNC_COLD;
  bool set_log_file_path(const char* path) C3_FUNC_COLD;
  bool set_user_password(const char* password) { return set_password(sr_cfg_user_password, password); }
  bool set_admin_password(const char* password) { return set_password(sr_cfg_admin_password, password); }
  bool set_bulk_password(const char* password) { return set_password(sr_cfg_bulk_password, password); }
  c3_long_t get_free_disk_space_threshold() const { return sr_disk_space_threshold; }
  void set_free_disk_space_threshold(c3_long_t bytes) { sr_disk_space_threshold = bytes; }
  c3_long_t get_thread_activity_threshold() const { return sr_thread_active_threshold; }
  void set_thread_activity_threshold(c3_long_t usecs) { sr_thread_active_threshold = usecs; }
  c3_ulong_t get_dealloc_chunk_size() const {
    return sr_dealloc_chunk_size.load(std::memory_order_relaxed);
  }
  void set_dealloc_chunk_size(c3_ulong_t chunk_size) {
    sr_dealloc_chunk_size.store(chunk_size, std::memory_order_relaxed);
  }
  c3_uint_t get_dealloc_max_wait_time() const {
    return sr_dealloc_max_wait_time.load(std::memory_order_relaxed);
  }
  void set_dealloc_max_wait_time(c3_uint_t msecs) {
    sr_dealloc_max_wait_time.store(msecs, std::memory_order_relaxed);
  }

  // database configuration (only main thread will use these, so no atomics needed)
  c3_uint_t get_store_wait_time() const { return sr_store_db_duration; }
  void set_store_wait_time(c3_uint_t duration) { sr_store_db_duration = duration; }
  c3_uint_t get_store_max_wait_time() const { return sr_store_db_max_duration; }
  void set_store_max_wait_time(c3_uint_t duration) { sr_store_db_max_duration = duration; }
  bool is_session_db_file_set() const { return sr_session_db_file.is_set(); }
  const char* get_session_db_file_name() const { return sr_session_db_file.get(); }
  void set_session_db_file_name(const char* name) { sr_session_db_file.set(name); }
  bool is_fpc_db_file_set() const { return sr_fpc_db_file.is_set(); }
  const char* get_fpc_db_file_name() const { return sr_fpc_db_file.get(); }
  void set_fpc_db_file_name(const char* name) { sr_fpc_db_file.set(name); }
  sync_mode_t get_session_db_sync_mode() const { return sr_session_db_sync; }
  void set_session_db_sync_mode(sync_mode_t mode) { sr_session_db_sync = mode; }
  sync_mode_t get_fpc_db_sync_mode() const { return sr_fpc_db_sync; }
  void set_fpc_db_sync_mode(sync_mode_t mode) { sr_fpc_db_sync = mode; }
  user_agent_t get_session_db_included_agents() const { return sr_session_db_include; }
  void set_session_db_included_agents(user_agent_t lowest_ua) { sr_session_db_include = lowest_ua; }
  user_agent_t get_fpc_db_included_agents() const { return sr_fpc_db_include; }
  void set_fpc_db_included_agents(user_agent_t lowest_ua) { sr_fpc_db_include = lowest_ua; }

  // auto-save interval getters and setters; used by main thread and optimizers
  c3_timestamp_t get_session_autosave_interval() const {
    return sr_session_auto_save.load(std::memory_order_acquire);
  }
  void set_session_autosave_interval(c3_timestamp_t seconds) {
    sr_session_auto_save.store(seconds, std::memory_order_release);
  }
  c3_timestamp_t get_fpc_autosave_interval() const {
    return sr_fpc_auto_save.load(std::memory_order_acquire);
  }
  void set_fpc_autosave_interval(c3_timestamp_t seconds) {
    sr_fpc_auto_save.store(seconds, std::memory_order_release);
  }

  // queue capacity manipulation
  c3_uint_t get_queue_capacity() C3LM_OFF(const) C3_FUNC_COLD { return sr_queue.get_capacity(); }
  c3_uint_t set_queue_capacity(c3_uint_t capacity) C3_FUNC_COLD { return sr_queue.set_capacity(capacity); }
  c3_uint_t get_max_queue_capacity() C3LM_OFF(const) C3_FUNC_COLD { return sr_queue.get_max_capacity(); }
  c3_uint_t set_max_queue_capacity(c3_uint_t max_capacity) C3_FUNC_COLD {
    return sr_queue.set_max_capacity(max_capacity);
  }

  // messaging
  bool post_id_message(server_command_t id) {
    return sr_queue.post_id_message(id);
  }
  bool post_config_message(const char* message, c3_uint_t length) C3_FUNC_COLD {
    return sr_queue.post_data_message(message, length);
  }
  bool post_object_message(CommandReader* cr) C3_FUNC_COLD {
    // `c3_assert()` commented out to work around "TLS init function" bug in GCC compiler/linker
    // c3_assert(Thread::get_id() >= TI_FIRST_CONNECTION_THREAD);
    return sr_queue.post_object_message(cr);
  }
  bool post_quit_message() C3_FUNC_COLD {
    // `c3_assert()` commented out to work around "TLS init function" bug in GCC compiler/linker
    // c3_assert(Thread::get_id() == TI_SIGNAL_HANDLER);
    return sr_queue.post_id_message(SC_QUIT);
  }

  // server lifecycle
  bool configure(int argc, char** argv) C3_FUNC_COLD;
  bool start() C3_FUNC_COLD;
  void run();
  void shutdown() C3_FUNC_COLD;
  void cleanup() C3_FUNC_COLD;

  // implementations of various interfaces
  void increment_warning_count() override;
  void increment_error_count() override;
  void begin_memory_deallocation(size_t size) override;
  void end_memory_deallocation() override;
  bool thread_is_quitting(c3_uint_t id) override C3_FUNC_COLD;
};

extern Server server;

} // CyberCache

#endif // _CC_SERVER_H
