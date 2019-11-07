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
#include "cc_server.h"
#include "cc_signal_handler.h"
#include "cc_worker_threads.h"
#include "cc_configuration.h"
#include "pl_net_configuration.h"

namespace CyberCache {

Server server;

///////////////////////////////////////////////////////////////////////////////
// PathString
///////////////////////////////////////////////////////////////////////////////

const char* PathString::get() const {
  const char* buffer = get_chars();
  return buffer != nullptr? buffer: "";
}

void PathString::set(const char* buffer) {
  if (buffer != nullptr && *buffer != '\0') {
    String::set(DOMAIN_GLOBAL, buffer);
  } else {
    empty();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Server
///////////////////////////////////////////////////////////////////////////////

Server::Server() noexcept {
  sr_start_time = Timer::current_timestamp();
  sr_exe_file_path = nullptr;
  sr_last_check = 0; // "start of epoch"
  sr_check_interval = DEFAULT_HEALTH_CHECK_INTERVAL;
  sr_warning_count.store(0, std::memory_order_relaxed);
  sr_error_count.store(0, std::memory_order_relaxed);
  sr_thread_quit_time = DEFAULT_THREAD_QUIT_TIME;
  sr_state = SS_INVALID;
  sr_cfg_num_threads = DEFAULT_NUM_CONNECTION_THREADS;
  sr_cfg_log_path.set(DOMAIN_GLOBAL, DEFAULT_LOG_FILE_PATH);
  sr_disk_space_threshold = DEFAULT_FREE_DISK_SPACE_THRESHOLD;
  sr_thread_active_threshold = DEFAULT_THREAD_ACTIVITY_TIME_THRESHOLD;
  set_dealloc_chunk_size(DEFAULT_DEALLOC_CHUNK_SIZE);
  set_dealloc_max_wait_time(DEFAULT_DEALLOC_MAX_WAIT_TIME);
  sr_dealloc_in_progress.store(false, std::memory_order_relaxed);
  sr_store_db_duration = DEFAULT_STORE_DB_DURATION;
  sr_store_db_max_duration = DEFAULT_STORE_DB_MAX_DURATION;
  sr_session_db_sync = SM_DATA_ONLY;
  sr_fpc_db_sync = SM_NONE;
  sr_session_db_include = UA_USER;
  sr_fpc_db_include = UA_BOT;
  sr_session_auto_save.store(0, std::memory_order_relaxed); // disabled
  sr_fpc_auto_save.store(0, std::memory_order_relaxed); // disabled
  sr_binlog_saver_ok = true;
  sr_pid_file_created = false;
  sr_log_level_set = false;
}

///////////////////////////////////////////////////////////////////////////////
// PID MANIPULATION
///////////////////////////////////////////////////////////////////////////////

bool Server::check_create_pid_file() {
  if (c3_file_access(PID_FILE_PATH) && c3_get_file_size(PID_FILE_PATH) > 0) {
    log(LL_ERROR, "Non-empty PID file '%s' already exists, CyberCache server will not start", PID_FILE_PATH);
    return false;
  } else {
    char pid[16];
    int length = std::snprintf(pid, sizeof pid, "%d", getpid());
    if (length > 0 && c3_save_file(PID_FILE_PATH, pid, (size_t) length)) {
      sr_pid_file_created = true;
      return true;
    } else {
      log(LL_ERROR, "Could not create PID file '%s' [%d], CyberCache server will not start (%s)",
        PID_FILE_PATH, length, c3_get_error_message());
      return false;
    }
  }
}

void Server::delete_pid_file() {
  if (sr_pid_file_created) {
    if (!c3_delete_file(PID_FILE_PATH)) {
      syslog_message(LL_ERROR, "Could not delete PID file '%s' (%s)",
        PID_FILE_PATH, c3_get_error_message());
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// COMMAND LINE PROCESSING
///////////////////////////////////////////////////////////////////////////////

char* Server::str_replace(char* buffer, char character, char replacement) {
  char* c = buffer;
  while ((c = std::strchr(c, character)) != nullptr) {
    *c++ = replacement;
  }
  return buffer;
}

char* Server::preprocess_option_name(char* name) {
  return str_replace(name, '-', '_');
}

char* Server::preprocess_option_value(char* value) {
  if (*value != '\'' && *value != '\"' && *value != '`') {
    // only do this replacement in an unquoted string
    return str_replace(value, ',', ' ');
  }
  return value;
}

///////////////////////////////////////////////////////////////////////////////
// MISCELLANEOUS UTILITIES
///////////////////////////////////////////////////////////////////////////////

void Server::print_information(const char* exe_path, bool print_usage_info) {
  printf("%s\n"
    "Written by Vadim Sytnikov.\n"
    "Copyright (C) 2016-2019 CyberHULL. All rights reserved.\n"
    "This program is free software distributed under GPL v2+ license.\n", c3lib_full_version_string);

  if (print_usage_info) {
    constexpr char usage[] = R"C3USAGE(
Use: %s [<option> [<option> [...]]] [<config-file-name>]

where <option> is either a "short option" ("-" followed by a single letter),
or a "long option" ("--" followed by full option name). If an option takes
arguments, they can be specified after ':' (for a short option), or '=' (for
a long option). If an option takes more than one argument, they must be
separated with commas (','); if option argument is a string that contains
commas, it must be quoted using single or double quotes, or backticks ('`').

A long option is essentially any configuration file option prefixed with
double hyphen. If configuration option contains underscores ('_'), in command
line option they can be substituted with hyphens ('-'). Note that if an option
takes more than one argument those arguments are separated with spaces in the
configuration file, but on command line they have to be separated with commas.
Please see configuration file for the full list of available options, their
formats, and effect.

In addition to the configuration file options, the following command line-only
options are recognized (server will NOT process any other options, will NOT
load any config files, and will exit if any of these options are specified):
  --version    Print out version information,
  --help       Print out this help message.

Main configuration file is always loaded first, then command line options are
processed from left to right, overwriting their counterparts already set by
the configuration file. Here, "main" means either file specified as very last
command line argument, or (if no file was specified), the default
configuration file ('%s').

If name of the configuration file does NOT start with '.' or '/', then the
server will search "standard locations" for the file, in this order:
  1) "/etc/cybercache/" directory,
  2) directory where server's executable file is located,
  3) current directory.

Recognized "short" options (and their "long" counterparts) are:
  -v  --version (cannot be used in the configuration file),
  -h  --help (cannot be used in the configuration file),
  -i  --include,
  -l  --log-level,
  -n  --num-connection-threads,
  -m  --max-memory,
  -s  --max-session-memory,
  -f  --max-fpc-memory,
  -a  --listener-addresses,
  -p  --listener-port.

Please see '/usr/share/doc/cybercache-<edition>/copyright' file for the
licensing information on CyberCache Cluster and its components.
)C3USAGE";
    printf(usage, exe_path, DEFAULT_CONFIG_FILE_PATH);
  }
}

void Server::check_memory_quota(const char* type, Memory& memory) {
  if (memory.is_quota_set()) {
    c3_ulong_t ram_size = c3_get_total_memory();
    c3_ulong_t quota = memory.get_quota();
    if (quota > ram_size) {
      log(LL_WARNING, "%s set memory quota appears to be bigger than installed RAM size: %llu > %llu",
        type, quota, ram_size);
    }
  }
}

c3_uint_t Server::get_current_server_load() {
  c3_uint_t connection_threads = Thread::get_num_connection_threads();
  return connection_threads > 0? Thread::get_num_active_connection_threads() * 100 / connection_threads: 0;
}

bool Server::set_password(String& password_string, const char* password) {
  if (sr_state <= SS_CONFIG) {
    if (password != nullptr && password[0] != '\0') {
      password_string.set(DOMAIN_GLOBAL, password);
    } else {
      password_string.empty();
    }
    return true;
  }
  return false;
}

bool Server::load_config_file(const char* path) {
  char full_path[MAX_FILE_PATH_LENGTH];
  bool config_file_found;
  switch (path[0]) {
    case '.':
    case '/':
      config_file_found = c3_file_access(std::strcpy(full_path, path), AM_READABLE);
      break;
    default:
      std::snprintf(full_path, sizeof full_path, "/etc/cybercache/%s", path);
      config_file_found = c3_file_access(full_path, AM_READABLE);
      if (!config_file_found && sr_exe_file_path != nullptr) {
        const char* forward_slash_pos = std::strrchr(sr_exe_file_path, '/');
        const char* back_slash_pos= std::strrchr(sr_exe_file_path, '\\'); // for Windows...
        char* slash_pos = (char*) max(forward_slash_pos, back_slash_pos);
        if (slash_pos != nullptr) {
          size_t dir_length = slash_pos - sr_exe_file_path + 1;
          std::memcpy(full_path, sr_exe_file_path, dir_length);
          std::strcpy(full_path + dir_length, path);
          config_file_found = c3_file_access(full_path, AM_READABLE);
        }
      }
      if (!config_file_found) {
        std::strcpy(full_path, path);
        config_file_found = c3_file_access(full_path, AM_READABLE);
      }
  }
  if (config_file_found) {
    if (configuration.load_file(full_path)) {
      return true;
    } else {
      log(LL_ERROR, "Configuration file '%s' had errors", full_path);
      return false;
    }
  } else {
    log(LL_ERROR, "Could not locate configuration file '%s'", path);
    return false;
  }
}

void Server::parse_log_level_option(const char* option, char separator) {
  // if option is ill-formed, it will be caught (and diagnostics provided) later
  if (*option == separator) {
    c3_uint_t length = (c3_uint_t) std::strlen(option + 1);
    configuration.set_option('l', option + 1, length);
    sr_log_level_set = true;
  }
}

bool Server::wait_for_quitting_thread(thread_id_t id) {
  bool gave_extra_time = false;
  for (;;) {
    ServerMessage msg = sr_queue.get(sr_thread_quit_time);
    switch (msg.get_type()) {
      case CMT_INVALID: // timeout has expired
        if (gave_extra_time) {
          log(LL_WARNING, "%s (%u) did not quit on time", Thread::get_name(id), id);
          return false;
        } else {
          if (Thread::get_state(id) == TS_QUITTING) {
            // the thread acknowledged "quit" request, but did not manage to quit yet
            gave_extra_time = true;
            break;
          } else {
            log(LL_WARNING, "%s (%u) did not respond to shutdown request on time",
              Thread::get_name(id), id);
            return false;
          }
        }
      case CMT_ID_COMMAND: {
        server_command_t command = msg.get_id_command();
        switch (command) {
          case SC_QUIT:
            // some subsystem (signal handler?) has sent "quit" request while the server is already
            // shutting down; ignoring...
          case SC_SAVE_SESSION_STORE:
          case SC_SAVE_FPC_STORE:
            // optimized requested session/FPC save while server is being shut down; the store will be saved
            // anyway in due course, so we ignore the request
            break;
          default: {
            c3_uint_t thread_id = command; // notification: thread with this ID is quitting
            Thread::wait_stop(thread_id);
            if (thread_id == id) {
              // yep, that's the thread we were waiting for
              return true;
            } else {
              // we do not wait specifically for connection threads to quit; their notifications arrive
              // while we're waiting for other threads, so we do not report them
              if (thread_id < TI_FIRST_CONNECTION_THREAD) {
                // some thread we tried to stop earlier was too late to respond, but finally did it...
                log(LL_NORMAL, "%s (%u) finally responded to shutdown request",
                  Thread::get_name(thread_id), thread_id);
              }
            }
          }
        }
        break;
      }
      case CMT_OBJECT: {
        CommandReader& cr = msg.get_object();
        if (id == TI_LISTENER) {
          // the listener could be waiting for response to this command...
          server_listener.post_error_response(cr, "Server is shutting down");
        }
        break;
      }
      default:
        c3_assert_failure();
    }
  }
}

void Server::wait_for_deallocation(std::unique_lock<std::mutex>& lock) {
  auto condition = [this]{
    return !sr_dealloc_in_progress.load(std::memory_order_relaxed);
  };
  sr_dealloc_notifier.wait_for(lock,
    std::chrono::milliseconds(get_dealloc_max_wait_time()), condition);
}

///////////////////////////////////////////////////////////////////////////////
// STORE PERSISTENCE SUPPORT
///////////////////////////////////////////////////////////////////////////////

Server::store_db_context_t::store_db_context_t(PayloadObjectStore &store, FileOutputPipeline &pipeline,
  user_agent_t user_agent): sdc_store(store), sdc_pipeline(pipeline), sdc_time(Timer::current_timestamp()),
  sdc_user_agent(user_agent) {
}

bool Server::save_object(void* context, HashObject* ho) {
  auto db_context = (Server::store_db_context_t*) context;
  c3_assert(db_context && db_context->sdc_user_agent < UA_NUMBER_OF_ELEMENTS && ho && ho->flags_are_set(HOF_PAYLOAD));
  auto pho = (PayloadHashObject*) ho;
  if (pho->flags_are_clear(HOF_BEING_DELETED) && pho->get_user_agent() >= db_context->sdc_user_agent) {
    LockableObjectGuard lock(pho);
    if (lock.is_locked()) {
      if (pho->flags_are_clear(HOF_BEING_DELETED) && pho->get_user_agent() >= db_context->sdc_user_agent) {
        FileCommandWriter* fcw = db_context->sdc_store.create_file_command_writer(pho, db_context->sdc_time);
        if (fcw != nullptr) {
          db_context->sdc_pipeline.send_object(fcw);
        }
      }
    }
  }
  return true;
}

void Server::set_db_path(char* path, const char* name) {
  if (std::strchr(name, '/') != nullptr) {
    std::strcpy(path, name);
  } else {
    std::snprintf(path, MAX_FILE_PATH_LENGTH, "%s/%s", DEFAULT_STORE_DB_PATH_PREFIX, name);
  }
}

void Server::load_store(const char* name) {
  char path[MAX_FILE_PATH_LENGTH];
  set_db_path(path, name);
  if (c3_file_access(path, AM_READABLE)) {
    binlog_loader.send_load_file_command(path);
  } else {
    log(LL_WARNING, "Cannot load database file '%s': it does not exist, or is not readable", path);
  }
}

bool Server::save_store(PayloadObjectStore &store, const char* name, sync_mode_t sync, user_agent_t ua,
  bool overwrite) {
  c3_assert(name && sync < SM_NUMBER_OF_ELEMENTS && ua < UA_NUMBER_OF_ELEMENTS);
  bool result = false;
  if (sr_binlog_saver_ok) {
    char path[MAX_FILE_PATH_LENGTH];
    set_db_path(path, name);
    // if database file already exists and we're told to overwrite it, remove it
    if (overwrite && c3_file_access(path, AM_EXISTS)) {
      if (!c3_delete_file(path)) {
        log(LL_ERROR, "Could not overwrite database file: '%s' (%s)",
          path, c3_get_error_message());
        return false;
      }
    }
    /*
     * sync mode should be sent before opening binlog; otherwise, if the command is sent when binlog
     * is already open *and* sent mode does not match current, binlog will have to be re-opened; will
     * still work, but inefficiently
     */
    if (binlog_saver.send_set_sync_mode_command(sync)) {
      if (binlog_saver.send_open_binlog_command(path)) {
        /*
         * if we opened binlog, we will have to also close it *and* wait for closing notification (that
         * is, eat it up), otherwise binlog saver becomes unusable
         */
        store_db_context_t context(store, binlog_saver, ua);
        if (store.enumerate_all(&context, save_object)) {
          result = true;
        } else {
          log(LL_ERROR, "While storing `%s` not all records were saved", name);
        }
        if (binlog_saver.send_close_binlog_command()) {
          c3_timestamp_t started = Timer::current_timestamp();
          bool warned = false;
          while (!binlog_saver.wait_for_notification(sr_store_db_duration)) {
            if (Timer::current_timestamp() - started >= sr_store_db_max_duration) {
              log(LL_ERROR, "Could not store `%s`: operation took more than %u seconds",
                name, sr_store_db_max_duration);
              result = sr_binlog_saver_ok = false;
              break;
            }
            if (!warned) {
              log(LL_WARNING, "Storing '%s' took more than %u seconds", name, sr_store_db_duration);
              warned = true;
            }
          }
          log(LL_NORMAL, "Finished saving %s to '%s'", store.get_name(), name);
        } else {
          result = sr_binlog_saver_ok = false;
          log(LL_ERROR, "Storing `%s` failed: could not send 'close binlog' command", name);
        }
      } else {
        log(LL_ERROR, "Cannot store `%s`: could not send 'open binlog' command", name);
      }
    } else {
      log(LL_ERROR, "Cannot store `%s`: could not send 'sync mode change' command", name);
    }
  } else {
    log(LL_ERROR, "Cannot store `%s`: saving service is in error state after previous operation", name);
  }
  return result;
}

bool Server::save_session_store() {
  if (is_session_db_file_set()) {
    return save_store(session_store, get_session_db_file_name(),
                      get_session_db_sync_mode(), get_session_db_included_agents(), true);
  }
  return false;
}

bool Server::save_fpc_store() {
  if (is_fpc_db_file_set()) {
    return save_store(fpc_store, get_fpc_db_file_name(), get_fpc_db_sync_mode(),
                      get_fpc_db_included_agents(),
      // only overwrite if different paths were given for session and FPC databases
                      std::strcmp(get_session_db_file_name(), get_fpc_db_file_name()) != 0);
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// HELPER METHODS USED BY run()
///////////////////////////////////////////////////////////////////////////////

bool Server::do_health_check(c3_uint_t& num_warnings) {
  // starting health check
  log(LL_NORMAL, "Starting health check...");
  num_warnings = 0;

  // check that the server is listening to incoming connections
  if (!server_listener.is_service_active()) {
    log(LL_WARNING, "Server is NOT listening to incoming connections (IPs/port not set?)");
    num_warnings++;
  }

  // shrink critical message queues if they grew beyond their set limits
  c3_uint_t num_shrunk_queues =
    session_store.reduce_queue_capacity() +
    fpc_store.reduce_queue_capacity() +
    session_optimizer.reduce_queue_capacity() +
    fpc_optimizer.reduce_queue_capacity();
  if (num_shrunk_queues > 0) {
    log(LL_VERBOSE, "Reduced capacities of %u overgrown critical message queues", num_shrunk_queues);
  }

  // optionally check remaining free disk space
  if (sr_disk_space_threshold != 0) {
    c3_long_t bytes = server_logger.get_available_space();
    if (bytes >= 0 && bytes < sr_disk_space_threshold) {
      log(LL_WARNING, "Logger has only %lld free bytes on disk", bytes);
      num_warnings++;
    }
    bytes = session_binlog.get_available_space();
    if (bytes >= 0 && bytes < sr_disk_space_threshold) {
      log(LL_WARNING, "Session binlog has only %lld free bytes on disk", bytes);
      num_warnings++;
    }
    bytes = fpc_binlog.get_available_space();
    if (bytes >= 0 && bytes < sr_disk_space_threshold) {
      log(LL_WARNING, "FPC binlog has only %lld free bytes on disk", bytes);
      num_warnings++;
    }
  }

  // optionally check the threads
  if (sr_thread_active_threshold > 0) {
    for (c3_uint_t i = 1; i < MAX_NUM_THREADS; i++) {
      if (i != TI_BINLOG_LOADER) { // loading a binlog can take *really* long time...
        thread_state_t state = Thread::get_state(i);
        if (state == TS_ACTIVE || state == TS_QUITTING) {
          c3_long_t usecs = Thread::get_time_in_current_state(i);
          if (usecs >= sr_thread_active_threshold) {
            extended_thread_state_t ets;
            Thread::get_state(i, ets);
            c3_long_t msecs = usecs / 1000;
            log(LL_WARNING,
              "%s [%u] exceeded run time limit: %lld msecs in '%s' state: M%u[%s] O%u[%s] E%u T%u Q%u[%s] R%u",
              Thread::get_name(i), i, msecs, Thread::get_state_name(ets.ets_state),
              ets.ets_mutex_state, ets.ets_mutex_info,
              ets.ets_object_state, ets.ets_object_flags,
              ets.ets_event_state, ets.ets_timed_event_state,
              ets.ets_queue_state, ets.ets_queue_info,
              (c3_uint_t) ets.ets_quit_request);
            num_warnings++;
          }
        }
      }
    }
  }

  // currently, we do not treat any errors/warnings as critical
  return true;
}

bool Server::process_id_command(server_command_t command) {
  switch (command) {
    case SC_QUIT:
      log(LL_NORMAL, "Server is shutting down due to QUIT request");
      return false;
    case SC_SAVE_SESSION_STORE:
      save_session_store();
      break;
    case SC_SAVE_FPC_STORE:
      save_fpc_store();
      break;
    default:
      c3_assert(command && command < MAX_NUM_THREADS);
      c3_uint_t id = command;
      Thread::wait_stop(id);
      log(LL_VERBOSE, "%s [%u] has quit", Thread::get_name(id), id);
  }
  return true;
}

void Server::execute_ping_command(const CommandReader& cr) {
  if (ChunkIterator::has_any_data(cr)) {
    server_listener.post_format_error_response(cr);
  } else {
    server_listener.post_ok_response(cr);
  }
}

void Server::execute_check_command(const CommandReader& cr) {
  if (ChunkIterator::has_any_data(cr)) {
    server_listener.post_format_error_response(cr);
  } else {
    server_listener.post_data_response(cr, "UUU",
      get_current_server_load(), get_warning_count(), get_error_count());
  }
}

void Server::add_memory_info(PayloadListChunkBuilder& list, const char* name, Memory& memory) {
  list.addf("%s memory: %llu / %llu bytes (used / quota)",
    name, memory.get_used_size(), memory.get_quota());
}

void Server::add_connections_info(PayloadListChunkBuilder &list, const char* name, const SocketPipeline &pipeline) {
  list.addf("Active %s connections: %u", name, pipeline.get_num_connections());
}

void Server::add_store_info(PayloadListChunkBuilder& list, const char* name, ObjectStore& store,
  c3_uint_t bias) {
  c3_uint_t num_records = store.get_num_elements() - bias;
  c3_uint_t num_tables = store.get_num_tables();
  c3_uint_t num_deleted = store.get_num_deleted_objects();
  list.addf("%s store: %u record%s in %u table%s (%u record%s marked as 'deleted')", name,
    num_records, plural(num_records), num_tables, plural(num_tables), num_deleted, plural(num_deleted));
}

void Server::add_optimizer_info(PayloadListChunkBuilder& list, const char* name, Optimizer& optimizer) {
  char time[TIMER_FORMAT_STRING_LENGTH];
  c3_timestamp_t timestamp = optimizer.get_last_run_time();
  if (timestamp > 0) {
    Timer::to_ascii(timestamp, true, time);
  } else {
    std::strcpy(time, "none");
  }
  c3_uint_t nchecks = optimizer.get_last_run_checks();
  c3_uint_t ncompressions = optimizer.get_last_runs_compressions();
  list.addf("%s optimizer last run: %s, %u check%s, %u re-compression%s", name,
    time, nchecks, plural(nchecks), ncompressions, plural(ncompressions));
}

void Server::add_replicator_info(PayloadListChunkBuilder& list, const char* name,
  SocketPipeline& pipeline) {
  list.addf("%s replicator: %s", name, pipeline.is_service_active()? "ON": "OFF");
}

void Server::add_service_info(PayloadListChunkBuilder& list, const char* name, FileBase& service) {
  if (service.is_service_active()) {
    list.addf("%s: %llu / %llu [%llu] bytes (written / max [available])",
      name, service.get_current_size(), service.get_max_size(), service.get_available_space());
  } else {
    list.addf("%s: OFF", name);
  }
}

void Server::execute_info_command(const CommandReader& cr) {
  command_status_t status = CS_FORMAT_ERROR;
  CommandHeaderIterator iterator(cr);
  if (iterator.get_next_chunk_type() == CHUNK_NUMBER) {
    NumberChunk domain = iterator.get_number();
    if (domain.is_in_range(1, DM_ALL) && !iterator.has_more_chunks() &&
      !PayloadChunkIterator::has_payload_data(cr)) {

      // get domain mode
      c3_uint_t dm = domain.get_uint();
      // create response object and [payload] list
      SocketResponseWriter* rw = ResponseObjectConsumer::create_response(cr);
      PayloadListChunkBuilder info_list(*rw, server_net_config, 0, 0, 0);

      // collect global information
      if ((dm & DM_GLOBAL) != 0) {
        c3_uint_t num_warnings;
        do_health_check(num_warnings);
        info_list.addf("Version: %s", c3lib_full_version_string);
        info_list.addf("Started: %s", Timer::to_ascii(sr_start_time));
        char sys_info[C3_SYSTEM_INFO_BUFFER_SIZE];
        info_list.addf("System: %s", c3_get_system_info(sys_info, sizeof sys_info));
        info_list.addf("Health check: %u warning%s", num_warnings, plural(num_warnings));
        c3_uint_t total_errors = get_error_count();
        c3_uint_t total_warnings = get_warning_count();
        info_list.addf("Since start: %u error%s, %u warning%s",
          total_errors, plural(total_errors), total_warnings, plural(total_warnings));
        info_list.addf("Global memory: %llu bytes used", global_memory.get_used_size());
        if (global_memory.is_quota_set()) {
          info_list.addf("Combined memory quota: %llu bytes", global_memory.get_quota());
        } else {
          info_list.add("Combined memory quota: not set");
        }
        info_list.addf("Current load: %u%% (active / total worker threads)", get_current_server_load());
        add_service_info(info_list, "Logger", server_logger);
        add_connections_info(info_list, "inbound", server_listener);
        if (binlog_loader.is_service_active()) {
          info_list.addf("Binlog loader: %llu / %llu bytes (processed / total)",
            binlog_loader.get_current_size(), binlog_loader.get_max_size());
        } else {
          info_list.add("Binlog loader: on standby");
        }
      }

      // collect session domain information
      if ((dm & DM_SESSION) != 0) {
        add_memory_info(info_list, "Session", session_memory);
        add_store_info(info_list, "Session", session_store, 0);
        add_optimizer_info(info_list, "Session", session_optimizer);
        add_replicator_info(info_list, "Session", session_replicator);
        add_connections_info(info_list, "session replicator", session_replicator);
        add_service_info(info_list, "Session binlog", session_binlog);
      }

      // collect FPC domain information
      if ((dm & DM_FPC) != 0) {
        add_memory_info(info_list, "FPC", fpc_memory);
        add_store_info(info_list, "FPC", fpc_store, 0);
        add_store_info(info_list, "Tag", tag_manager, 1); // exclude "list of untagged objects" record
        add_optimizer_info(info_list, "FPC", fpc_optimizer);
        add_replicator_info(info_list, "FPC", fpc_replicator);
        add_connections_info(info_list, "FPC replicator", fpc_replicator);
        add_service_info(info_list, "FPC binlog", fpc_binlog);
      }

      // send response
      status = CS_FAILURE;
      if (server_listener.post_list_response(rw, info_list)) {
        status = CS_SUCCESS;
      }
    }
  }

  // process result
  switch (status) {
    case CS_FORMAT_ERROR:
      server_listener.post_format_error_response(cr);
      break;
    case CS_FAILURE:
      server_listener.post_internal_error_response(cr);
      // fall through
    case CS_SUCCESS:
      break;
    default:
      c3_assert_failure();
  }
}

#if C3_INSTRUMENTED
struct perf_enumeration_context_t {
  PayloadListChunkBuilder& pec_list;    // list to which name/value pairs should be added
  const c3_byte_t          pec_domains; // mask of domains whose data should be retrieved

  perf_enumeration_context_t(PayloadListChunkBuilder& list, c3_byte_t domains):
    pec_list(list), pec_domains(domains) {
  }
};

bool Server::counter_enumeration_callback(const PerfCounter* counter, void* context) {
  c3_assert(counter && context);
  auto data = (perf_enumeration_context_t*) context;
  char buffer[1024];
  return data->pec_list.addf("%s: %s", counter->get_name(),
    counter->get_values(data->pec_domains, buffer, sizeof buffer));
}
#endif

void Server::execute_stats_command(const CommandReader& cr) {
  command_status_t status = CS_FORMAT_ERROR;
  CommandHeaderIterator iterator(cr);
  if (iterator.get_next_chunk_type() == CHUNK_NUMBER) {
    NumberChunk domains = iterator.get_number();
    if (domains.is_in_range(1, DM_ALL) && iterator.get_next_chunk_type() == CHUNK_STRING) {
      StringChunk mask = iterator.get_string();
      if (mask.is_valid() && !iterator.has_more_chunks() && !PayloadChunkIterator::has_payload_data(cr)) {
        status = CS_FAILURE;

        #if C3_INSTRUMENTED
        // create response object and [payload] list
        SocketResponseWriter* rw = ResponseObjectConsumer::create_response(cr);
        PayloadListChunkBuilder counter_list(*rw, server_net_config, 0, 0, 0);
        // extract counter mask (we don't have counters with names this long, so we can safely ignore longer masks)
        char name_mask[128];
        mask.to_cstring(name_mask, sizeof name_mask);
        // collect counter name:value pairs
        c3_byte_t domain_mask = (c3_byte_t) domains.get_uint();
        perf_enumeration_context_t context(counter_list, domain_mask);
        if (PerfCounter::enumerate(domain_mask, name_mask, counter_enumeration_callback, &context) &&
          server_listener.post_list_response(rw, counter_list)) {
          status = CS_SUCCESS;
        } else {
          ReaderWriter::dispose(rw);
        }
        #else // !C3_INSTRUMENTED
        if (server_listener.post_error_response(cr,
          "The 'STATS' command is only available in instrumented versions")) {
          status = CS_SUCCESS;
        }
        #endif
      }
    }
  }
  // report errors, if any
  switch (status) {
    case CS_FORMAT_ERROR:
      server_listener.post_format_error_response(cr);
      break;
    case CS_FAILURE:
      server_listener.post_internal_error_response(cr);
      // fall through
    case CS_SUCCESS:
      break;
    default:
      c3_assert_failure();
  }
}

bool Server::execute_shutdown_command(const CommandReader& cr) {
  if (ChunkIterator::has_any_data(cr)) {
    server_listener.post_format_error_response(cr);
    return true;
  } else {
    // if we can't send confirmation, we don't shut down
    return !server_listener.post_ok_response(cr);
  }
}

void Server::execute_loadconfig_command(const CommandReader& cr) {
  CommandHeaderIterator iterator(cr);
  StringChunk name = iterator.get_string();
  if (name.is_valid() && !iterator.has_more_chunks() && !PayloadChunkIterator::has_payload_data(cr)) {
    c3_uint_t length = name.get_length() + 1;
    char path[length];
    name.to_cstring(path, length);
    if (configuration.load_file(path)) {
      server_listener.post_ok_response(cr);
    } else {
      server_listener.post_error_response(cr, "Error loading config file '%s'", path);
    }
  } else {
    server_listener.post_format_error_response(cr);
  }
}

void Server::execute_restore_command(const CommandReader& cr) {
  CommandHeaderIterator iterator(cr);
  StringChunk name = iterator.get_string();
  if (name.is_valid() && !iterator.has_more_chunks() && !PayloadChunkIterator::has_payload_data(cr)) {
    c3_uint_t length = name.get_length() + 1;
    char path[length];
    name.to_cstring(path, length);
    if (c3_file_access(path, AM_READABLE)) {
      if (binlog_loader.send_load_file_command(path)) {
        server_listener.post_ok_response(cr);
      } else {
        server_listener.post_error_response(cr, "Error triggering binlog restoration from '%s'", path);
      }
    } else {
      server_listener.post_error_response(cr, "Binlog file '%s' does not exist or is not readable", path);
    }
  } else {
    server_listener.post_format_error_response(cr);
  }
}

void Server::execute_store_command(const CommandReader &cr) {
  CommandHeaderIterator iterator(cr);
  NumberChunk domain_chunk = iterator.get_number();
  if (domain_chunk.is_in_range(DM_SESSION, DM_SESSION | DM_FPC)) {
    StringChunk name_chunk = iterator.get_string();
    if (name_chunk.is_valid()) {
      NumberChunk ua_chunk = iterator.get_number();
      if (ua_chunk.is_in_range(UA_UNKNOWN, UA_USER)) {
        NumberChunk sync_chunk = iterator.get_number();
        if (sync_chunk.is_in_range(SM_NONE, SM_FULL) && !iterator.has_more_chunks() &&
          !PayloadChunkIterator::has_payload_data(cr)) {
          // format checks passed; now extract data and convert to proper types
          auto domains = (c3_byte_t) domain_chunk.get_value();
          c3_uint_t length = name_chunk.get_length() + 1;
          char path[length];
          name_chunk.to_cstring(path, length);
          auto ua = (user_agent_t) ua_chunk.get_value();
          auto mode = (sync_mode_t) sync_chunk.get_value();
          if ((domains & DM_SESSION) != 0) {
            if (!save_store(session_store, path, mode, ua, true)) {
              server_listener.post_error_response(cr, "Could not save session database to '%s', see log file", path);
              return;
            }
          }
          if ((domains & DM_FPC) != 0) {
            if (!save_store(fpc_store, path, mode, ua, (domains & DM_SESSION) == 0)) {
              server_listener.post_error_response(cr, "Could not save FPC database to '%s', see log file", path);
              return;
            }
          }
          server_listener.post_ok_response(cr);
          return;
        }
      }
    }
  }
  server_listener.post_format_error_response(cr);
}

bool Server::option_enumeration_callback(void* context, const char* command) {
  c3_assert(context && command);
  const size_t command_length = std::strlen(command);
  char value_buffer[MAX_COMMAND_LINE_OPTION_LENGTH];
  ssize_t value_length = configuration.get_option(command, value_buffer, sizeof value_buffer);
  if (value_length == 0) {
    const char empty_stub[] = "''";
    value_length = sizeof empty_stub;
    std::memcpy(value_buffer, empty_stub, sizeof empty_stub);
  } else if (value_length < 0) {
    const char unavailable_stub[] = "<value could not be retrieved>";
    value_length = sizeof unavailable_stub;
    std::memcpy(value_buffer, unavailable_stub, sizeof unavailable_stub);
  }
  const size_t total_length = command_length + 4 + value_length;
  char buffer[total_length];
  std::snprintf(buffer, total_length, "%s : %s", command, value_buffer);
  PayloadListChunkBuilder& list = *(PayloadListChunkBuilder*) context;
  list.add(buffer);
  return true; // == continue enumeration
}

void Server::execute_get_command(const CommandReader& cr) {
  command_status_t status = CS_FORMAT_ERROR;
  CommandHeaderIterator iterator(cr);
  ListChunk mask_list = iterator.get_list();
  if (mask_list.is_valid() && !PayloadChunkIterator::has_payload_data(cr)) {
    c3_uint_t num_elemements = mask_list.get_count();
    if (num_elemements > 0) {
      // create response object and [payload] list
      SocketResponseWriter* rw = ResponseObjectConsumer::create_response(cr);
      PayloadListChunkBuilder option_list(*rw, server_net_config, 0, 0, 0);
      // collect option values
      c3_uint_t i = 0; do {
        StringChunk mask = mask_list.get_string();
        if (mask.is_valid()) {
          char mask_buff[128]; // we do not have options that are this long, so we ignore too long masks
          if (mask.get_length() < sizeof mask_buff) {
            mask.to_cstring(mask_buff, sizeof mask_buff);
            configuration.enumerate_options(mask_buff, option_enumeration_callback, &option_list);
          }
        } else {
          break;
        }
      } while (++i < num_elemements);
      // did we go through all masks, or was there a format error?
      if (i == num_elemements) {
        status = CS_FAILURE;
        if (server_listener.post_list_response(rw, option_list)) {
          status = CS_SUCCESS;
        }
      } else {
        ReaderWriter::dispose(rw);
      }
    }
  }
  // report errors, if any
  switch (status) {
    case CS_FORMAT_ERROR:
      server_listener.post_format_error_response(cr);
      break;
    case CS_FAILURE:
      server_listener.post_internal_error_response(cr);
      // fall through
    case CS_SUCCESS:
      break;
    default:
      c3_assert_failure();
  }
}

void Server::execute_set_command(const CommandReader& cr) {
  CommandHeaderIterator iterator(cr);
  StringChunk name = iterator.get_string();
  if (name.is_valid() && !iterator.has_more_chunks() && !PayloadChunkIterator::has_payload_data(cr)) {
    if (configuration.set_option(name.get_chars(), name.get_length(), true)) {
      server_listener.post_ok_response(cr);
    } else {
      server_listener.post_error_response(cr,
        "Could not set option '%.*s' to specified value", name.get_length(), name.get_chars());
    }
  } else {
    server_listener.post_format_error_response(cr);
  }
}

void Server::execute_log_command(const CommandReader& cr) {
  CommandHeaderIterator iterator(cr);
  StringChunk message = iterator.get_string();
  if (message.is_valid() && !iterator.has_more_chunks() && !PayloadChunkIterator::has_payload_data(cr)) {
    if (server_logger.log_string(LL_EXPLICIT, message.get_chars(), message.get_length())) {
      server_listener.post_ok_response(cr);
    } else {
      server_listener.post_internal_error_response(cr);
    }
  } else {
    server_listener.post_format_error_response(cr);
  }
}

void Server::execute_rotate_command(const CommandReader& cr) {
  enum rotation_status_t {
    RS_NOT_REQUESTED = 0,
    RS_SUCCEEDED,
    RS_SERVICE_INACTIVE,
    RS_FAILED
  };
  command_status_t status = CS_FORMAT_ERROR;
  rotation_status_t log_result = RS_NOT_REQUESTED;
  rotation_status_t session_result = RS_NOT_REQUESTED;
  rotation_status_t fpc_result = RS_NOT_REQUESTED;

  CommandHeaderIterator iterator(cr);
  if (iterator.get_next_chunk_type() == CHUNK_NUMBER) {
    NumberChunk domain = iterator.get_number();
    if (domain.is_in_range(1, DM_ALL) && !iterator.has_more_chunks() && !PayloadChunkIterator::has_payload_data(cr)) {
      c3_uint_t dm = domain.get_uint();
      status = CS_SUCCESS;

      // process log rotation
      if ((dm & DM_GLOBAL) != 0) {
        if (server_logger.is_service_active()) {
          if (server_logger.send_rotate_command()) {
            log_result = RS_SUCCEEDED;
          } else {
            log_result = RS_FAILED;
            status = CS_FAILURE;
          }
        } else {
          log_result = RS_SERVICE_INACTIVE;
          status = CS_FAILURE;
        }
      }

      // process session binlog rotation
      if ((dm & DM_SESSION) != 0) {
        if (session_binlog.is_service_active()) {
          if (session_binlog.send_rotate_binlog_command()) {
            session_result = RS_SUCCEEDED;
          } else {
            session_result = RS_FAILED;
            status = CS_FAILURE;
          }
        } else {
          session_result = RS_SERVICE_INACTIVE;
          status = CS_FAILURE;
        }
      }

      // process FPC binlog rotation
      if ((dm & DM_FPC) != 0) {
        if (fpc_binlog.is_service_active()) {
          if (fpc_binlog.send_rotate_binlog_command()) {
            fpc_result = RS_SUCCEEDED;
          } else {
            fpc_result = RS_FAILED;
            status = CS_FAILURE;
          }
        } else {
          fpc_result = RS_SERVICE_INACTIVE;
          status = CS_FAILURE;
        }
      }
    }
  }

  // send back command result
  switch (status) {
    case CS_FORMAT_ERROR:
      server_listener.post_format_error_response(cr);
      break;
    case CS_SUCCESS:
      server_listener.post_ok_response(cr);
      break;
    case CS_FAILURE: {
      static const char* results[] = {
        "not_requested",
        "ok",
        "service inactive",
        "failed"
      };
      server_listener.post_error_response(cr,
        "Rotation error: log=%s, session binlog=%s, FPC binlog=%s",
         results[log_result], results[session_result], results[fpc_result]);
      break;
    }
    default:
      c3_assert_failure();
  }
}

bool Server::process_object_command(const CommandReader& cr) {
  switch (cr.get_command_id()) {
    case CMD_PING:
      execute_ping_command(cr);
      break;
    case CMD_CHECK:
      execute_check_command(cr);
      break;
    case CMD_INFO:
      execute_info_command(cr);
      break;
    case CMD_STATS:
      execute_stats_command(cr);
      break;
    case CMD_SHUTDOWN:
      return execute_shutdown_command(cr);
    case CMD_LOADCONFIG:
      execute_loadconfig_command(cr);
      break;
    case CMD_RESTORE:
      execute_restore_command(cr);
      break;
    case CMD_STORE:
      execute_store_command(cr);
      break;
    case CMD_GET:
      execute_get_command(cr);
      break;
    case CMD_SET:
      execute_set_command(cr);
      break;
    case CMD_LOG:
      execute_log_command(cr);
      break;
    case CMD_ROTATE:
      execute_rotate_command(cr);
      break;
    default:
      c3_assert_failure();
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// CONFIGURATION/STATE MANAGEMENT
///////////////////////////////////////////////////////////////////////////////

void Server::set_log_level(log_level_t level) {
  /*
   * If we're parsing configuration file (during `CONFIG` state), but log level had already been set using
   * command line option, this request will be ignored.
   *
   * Command line is first parsed (and log level option gets processed) during `INIT` state, so if there
   * are several -l/--log-level options, the very last one will take effect. Similarly, if we're parsing
   * configuration file and log level had not been specified on the command line, the very last
   * encountered `log_level` option will take effect.
   */
  if (sr_state != SS_CONFIG || !sr_log_level_set) {
    server_logger.set_level(level);
  }
}

void Server::set_health_check_interval(c3_uint_t seconds) {
  sr_check_interval = seconds;
  log(LL_VERBOSE, "Health check interval set to %u seconds", seconds);
}

bool Server::set_num_connection_threads(c3_uint_t num) {
  c3_assert(sr_state && num <= MAX_NUM_CONNECTION_THREADS);
  if (sr_state <= SS_CONFIG) {
    sr_cfg_num_threads = num;
    return true;
  } else {
    c3_uint_t current_num = Thread::get_num_connection_threads();
    if (current_num > num) {
      return ConnectionThread::stop_connection_threads(current_num - num);
    } else if (num > current_num) {
      return ConnectionThread::start_connection_threads(num - current_num);
    }
    return true;
  }
}

bool Server::set_log_file_path(const char* path) {
  c3_assert(sr_state && path);
  if (sr_state <= SS_CONFIG) {
    sr_cfg_log_path.set(DOMAIN_GLOBAL, path);
    return true;
  } else {
    return server_logger.send_path_change_command(path);
  }
}

///////////////////////////////////////////////////////////////////////////////
// SERVER LIFE CYCLE
///////////////////////////////////////////////////////////////////////////////

bool Server::configure(int argc, char** argv) {
  // enter initialization state
  sr_state = SS_INIT;

  // set asynchronous I/O mode
  NetworkConfiguration::set_sync_io(false);

  // set executable file path
  sr_exe_file_path = argv[0];
  c3_assert(sr_exe_file_path);

  // initialize system log and signal handler
  syslog_open("CyberCache", true, this);
  log(LL_NORMAL, "Initializing %s...", c3lib_full_version_string);
  signal_handler.block_signals();

  // option types
  enum option_type_t: c3_byte_t {
    OT_INVALID = 0, // invalid option (placeholder)
    OT_SHORT,       // short option (one-letter, starts with single hyphen, value separator is ':')
    OT_LONG,        // long option (starts with double hyphen, value separator is '=')
    OT_SEPARATOR,   // lone double hyphen ("--") before config file name
    OT_FILE         // configuration file name
  };

  // pre-parse/validate options
  int last_option_index = -1;
  bool version_requested = false;
  bool help_requested = false;
  option_type_t option_types[argc];
  option_types[0] = OT_INVALID;
  for (int i = 1; i < argc; i++) {
    const char* option = argv[i];
    size_t option_len = std::strlen(option);
    if (option[0] == '-') { // an option?
      if (option_len > 1 && option[1] == '-') { // a long option or separator?
        if (option_len == 2) { // an option separator?
          if (i == argc - 1) {
            option_types[i] = OT_SEPARATOR; // trailing "--"; doesn't make sense, but it's OK...
          } else if (i == argc - 2) {
            option_types[i] = OT_SEPARATOR;
            option_types[i + 1] = OT_FILE; // whatever name of the very last argument starts with...
            break; // ... we do not even check it (because previous argument was "--")
          } else {
            log(LL_ERROR, "Only single config file name may follow '--' on the command line");
            return false;
          }
        } else { // a long option
          option_types[i] = OT_LONG;
          last_option_index = i;
          const char* l_option = option + 2;
          if (std::strcmp(l_option, "version") == 0) {
            version_requested = true;
          } else if (std::strcmp(l_option, "help") == 0) {
            help_requested = true;
          } else if (std::strncmp(l_option, "log_level", 9) == 0 || std::strncmp(l_option, "log-level", 9) == 0) {
            parse_log_level_option(l_option + 9, '=');
          }
        }
      } else if (option_len == 2 || (option_len >= 4 && option[2] == ':')) { // a short option?
        option_types[i] = OT_SHORT;
        last_option_index = i;
        const char s_option = option[1];
        if (s_option == 'v') {
          version_requested = true;
        } else if (s_option == 'h') {
          help_requested = true;
        } else if (s_option == 'l') {
          parse_log_level_option(option + 2, ':');
        }
        if ((version_requested || help_requested) && option_len > 2) {
          log(LL_ERROR, "'version' and 'help' options take no arguments: %s", option);
          return false;
        }
      } else {
        log(LL_ERROR, "Ill-formed option: %s", option);
        return false;
      }
    } else if (i == argc - 1) { // a config file name?
      option_types[i] = OT_FILE;
    } else {
      log(LL_ERROR, "Ill-formed option or multiple config files specified: %s", option);
      return false;
    }
  }

  // see if only version and/or usage information had been requested
  if (version_requested || help_requested) {
    print_information(argv[0], help_requested);
    return false;
  }

  // see if any set memory quota exceeds physical memory
  check_memory_quota("Global (combined)", global_memory);
  check_memory_quota("Session", session_memory);
  check_memory_quota("FPC", fpc_memory);

  // check if the server had already been started (should only be done after "-v"/"-h" are processed)
  if (!check_create_pid_file()) {
    return false;
  }

  /*
   * initialize references in global objects (replicators do not need this; neither does `Thread`: it had
   * received pointer to host object during main thread initialization)
   */
  Memory::configure(this);
  server_logger.configure(this);
  session_store.configure(&server_listener, &session_optimizer);
  fpc_store.configure(&server_listener, &fpc_optimizer, &tag_manager);
  tag_manager.configure(&server_listener, &fpc_optimizer, &fpc_store);
  session_optimizer.configure(this, &session_store);
  fpc_optimizer.configure(this, &fpc_store, &tag_manager);
  binlog_loader.configure(&server_listener);

  if (!server_listener.initialize() || !session_replicator.initialize() || !fpc_replicator.initialize()) {
    log(LL_ERROR, "Could not initialize socket pipelines");
    return false;
  }

  // enter configuration state
  sr_state = SS_CONFIG;

  // load main configuration file
  const char* config_file_path = DEFAULT_CONFIG_FILE_PATH;
  if (option_types[argc - 1] == OT_FILE) {
    config_file_path = argv[argc - 1];
  }
  log(LL_NORMAL, "Loading configuration file '%s'...", config_file_path);
  if (!load_config_file(config_file_path)) {
    // reason for the failure had already been logged
    return false;
  }

  // process command line options
  char buffer[MAX_COMMAND_LINE_OPTION_LENGTH];
  for (int j = 1; j <= last_option_index; j++) {
    std::strncpy(buffer, argv[j], MAX_COMMAND_LINE_OPTION_LENGTH);
    buffer[MAX_COMMAND_LINE_OPTION_LENGTH - 1] = 0; // in case we hit the limit
    bool result;
    switch (option_types[j]) {
      case OT_SHORT:
        if (buffer[2] != '\0') { // short option with argument(s)?
          c3_assert(buffer[2] == ':');
          char* value = buffer + 3;
          result = configuration.set_option(buffer[1],
            preprocess_option_value(value), (c3_uint_t) std::strlen(value));
        } else {
          result = configuration.set_option(buffer[1], nullptr, 0);
        }
        break;
      case OT_LONG: {
        char* opt_name = buffer + 2;
        char* separator = std::strchr(opt_name, '=');
        if (separator != nullptr) { // option with argument(s)?
          *separator = 0;
          preprocess_option_name(opt_name);
          *separator = ' ';
          preprocess_option_value(separator + 1);
        } else { // option w/o arguments
          preprocess_option_name(opt_name);
        }
        result = configuration.set_option(opt_name, (c3_uint_t) std::strlen(opt_name), false);
        break;
      }
      default:
        c3_assert_failure();
        result = false;
    }
    if (!result) {
      log(LL_ERROR, "Ill-formed command line option: '%s'", argv[j]);
      return false;
    }
  }

  // set passwords
  if (sr_cfg_user_password.not_empty()) {
    if (sr_cfg_admin_password.is_empty()) {
      log(LL_WARNING, "Admin password is empty while user password is set");
    }
    server_net_config.set_user_password(sr_cfg_user_password.get_chars());
    sr_cfg_user_password.empty();
  }
  if (sr_cfg_admin_password.not_empty()) {
    server_net_config.set_admin_password(sr_cfg_admin_password.get_chars());
    sr_cfg_admin_password.empty();
  }
  if (sr_cfg_bulk_password.not_empty()) {
    server_net_config.set_bulk_password(sr_cfg_bulk_password.get_chars());
    sr_cfg_bulk_password.empty();
  }

  // initialize object stores
  session_store.allocate();
  fpc_store.allocate();
  tag_manager.allocate();

  // configuration had been completed successfully
  return true;
}

bool Server::start() {
  // enter [subsystems] start state
  sr_state = SS_START;
  log(LL_NORMAL, "Starting server subsystems...");

  // start signal handler thread
  Thread::start(TI_SIGNAL_HANDLER, SignalHandler::thread_proc,
    ThreadArgument((SignalHandler*) &signal_handler));

  // start logger thread
  Thread::start(TI_LOGGER, Logger::thread_proc,
    ThreadArgument((Logger*) &server_logger));
  set_log_file_path(sr_cfg_log_path.get_chars());

  // start binlog threads
  Thread::start(TI_SESSION_BINLOG, FileOutputPipeline::thread_proc,
    ThreadArgument((FileOutputPipeline*) &session_binlog));
  Thread::start(TI_FPC_BINLOG, FileOutputPipeline::thread_proc,
    ThreadArgument((FileOutputPipeline*) &fpc_binlog));

  // start replication threads
  Thread::start(TI_SESSION_REPLICATOR, SocketPipeline::thread_proc,
    ThreadArgument((SocketPipeline*) &session_replicator));
  Thread::start(TI_FPC_REPLICATOR, SocketPipeline::thread_proc,
    ThreadArgument((SocketPipeline*) &fpc_replicator));

  // start binlog loader thread
  Thread::start(TI_BINLOG_LOADER, FileInputPipeline::thread_proc,
    ThreadArgument((FileInputPipeline*) &binlog_loader));

  // start cache database saver thread
  Thread::start(TI_BINLOG_SAVER, FileOutputPipeline::thread_proc,
    ThreadArgument((FileOutputPipeline*) &binlog_saver));

  // start optimization threads
  Thread::start(TI_SESSION_OPTIMIZER, Optimizer::thread_proc,
    ThreadArgument((Optimizer*) &session_optimizer));
  Thread::start(TI_FPC_OPTIMIZER, Optimizer::thread_proc,
    ThreadArgument((Optimizer*) &fpc_optimizer));

  // start tag manager's thread
  Thread::start(TI_TAG_MANAGER, TagStore::thread_proc,
    ThreadArgument((TagStore*) &tag_manager));

  // start connection threads
  c3_assert(sr_cfg_num_threads);
  set_num_connection_threads(sr_cfg_num_threads);

  // start listener for incoming connections
  Thread::start(TI_LISTENER, SocketPipeline::thread_proc,
    ThreadArgument((SocketPipeline*) &server_listener));

  // give threads some time to initialize their states
  Thread::sleep(THREAD_INITIALIZATION_WAIT_TIME);

  // see if there were initialization errors
  c3_uint_t num_errors = sr_error_count.load(std::memory_order_acquire);
  if (num_errors > 0) {
    log(LL_ERROR, "Some subsystems did not initialize properly (%u error%s), exiting...",
      num_errors, plural(num_errors));
    return false;
  }

  // do an extra check to make sure that all threads did indeed start (this is more like an `assert()`
  // since all errors should have been reported and caught already)
  #ifdef C3_SAFE
  bool all_threads_started = true;
  for (c3_uint_t i = 1; i < TI_FIRST_CONNECTION_THREAD + sr_cfg_num_threads; i++) {
    if (!Thread::is_running(i)) {
      log(LL_ERROR, "Thread %s (%u) did not start", Thread::get_name(i), i);
      all_threads_started = false;
    }
  }
  return all_threads_started;
  #else
  return true;
  #endif // C3_SAFE
}

void Server::run() {
  // logging before "formally" entering `RUN` stage to use syslog/stdio
  log(LL_TERSE, "Initialization completed successfully");

  // enter active state
  sr_state = SS_RUN;

  // see if we have to restore cache databases
  if (is_session_db_file_set()) {
    load_store(get_session_db_file_name());
  }
  if (is_fpc_db_file_set() && std::strcmp(get_session_db_file_name(), get_fpc_db_file_name()) != 0) {
    // only attempt to load the file if it has different name
    load_store(get_fpc_db_file_name());
  }

  // main application loop
  bool keep_going;
  do {
    c3_uint_t time_since_last_check = Timer::current_timestamp() - sr_last_check;
    c3_uint_t msecs;
    if (time_since_last_check >= sr_check_interval) {
      /*
       * Normally, we only get here once: when the server is just starting. We can also get here if check
       * interval is set to some very small value, and some command processing took very long time...
       */
      msecs = 1;
    } else {
      /*
       * Wait time can, theoretically, be set to one year (longest allowed duration), but 32-bit uint
       * cannot hold that many milliseconds (the limit is a bit more than a month and half)... so we have
       * to check for an overflow.
       */
      c3_ulong_t lmsecs = (c3_ulong_t)(sr_check_interval - time_since_last_check) * 1000;
      msecs = lmsecs > UINT_MAX_VAL? UINT_MAX_VAL: (c3_uint_t) lmsecs;
    }
    /*
     * Even though main thread's state is never checked (it's main thread that checks states of other
     * threads, and when it does so it "knows" it's active...), we still set it for consistency.
     */
    Thread::set_state(TS_IDLE);
    ServerMessage msg = sr_queue.get(msecs);
    Thread::set_state(TS_ACTIVE);
    switch (msg.get_type()) {
      case CMT_INVALID: { // wait time elapsed
        sr_last_check = Timer::current_timestamp();
        c3_uint_t dummy;
        keep_going = do_health_check(dummy);
        break;
      }
      case CMT_ID_COMMAND:
        keep_going = process_id_command(msg.get_id_command());
        break;
      case CMT_OBJECT:
        keep_going = process_object_command(msg.get_const_object());
        break;
      default:
        c3_assert_failure();
        keep_going = false; // internal error
    }
  } while (keep_going);
}

void Server::shutdown() {
  // enter "winding down" state
  sr_state = SS_SHUTDOWN;

  // logging after "formally" entering `SHUTDOWN` phase to use syslog/stdio
  log(LL_NORMAL, "Shutting down the server...");

  // stop incoming connections listener
  Thread::request_stop(TI_LISTENER);
  server_listener.send_quit_command();
  wait_for_quitting_thread(TI_LISTENER);

  // stop binlog loader
  Thread::request_stop(TI_BINLOG_LOADER);
  binlog_loader.send_quit_command();
  wait_for_quitting_thread(TI_BINLOG_LOADER);

  // stop connection threads
  set_num_connection_threads(0);

  // stop session replicator
  Thread::request_stop(TI_SESSION_REPLICATOR);
  session_replicator.send_quit_command();
  wait_for_quitting_thread(TI_SESSION_REPLICATOR);

  // stop FPC replicator
  Thread::request_stop(TI_FPC_REPLICATOR);
  fpc_replicator.send_quit_command();
  wait_for_quitting_thread(TI_FPC_REPLICATOR);

  // stop session binlog
  Thread::request_stop(TI_SESSION_BINLOG);
  session_binlog.send_quit_command();
  wait_for_quitting_thread(TI_SESSION_BINLOG);

  // stop FPC binlog
  Thread::request_stop(TI_FPC_BINLOG);
  fpc_binlog.send_quit_command();
  wait_for_quitting_thread(TI_FPC_BINLOG);

  // stop session optimizer (this unlinks all session objects from optimizer's chains)
  Thread::request_stop(TI_SESSION_OPTIMIZER);
  session_optimizer.post_quit_message();
  wait_for_quitting_thread(TI_SESSION_OPTIMIZER);

  // stop FPC optimizer (this unlinks all FPC objects from optimizer's chains)
  Thread::request_stop(TI_FPC_OPTIMIZER);
  fpc_optimizer.post_quit_message();
  wait_for_quitting_thread(TI_FPC_OPTIMIZER);

  // optionally save cache databases
  save_session_store();
  save_fpc_store();

  // stop cache database saver
  Thread::request_stop(TI_BINLOG_SAVER);
  binlog_saver.send_quit_command();
  wait_for_quitting_thread(TI_BINLOG_SAVER);

  // stop tag manager
  Thread::request_stop(TI_TAG_MANAGER);
  tag_manager.post_quit_message();
  wait_for_quitting_thread(TI_TAG_MANAGER);

  // dispose object stores
  session_store.dispose();
  fpc_store.dispose();

  // stop main logger
  Thread::request_stop(TI_LOGGER);
  server_logger.send_quit_command();
  wait_for_quitting_thread(TI_LOGGER);

  // stop signal handler
  if (Thread::is_running(TI_SIGNAL_HANDLER)) {
    SignalHandler::send_quit_message();
    wait_for_quitting_thread(TI_SIGNAL_HANDLER);
  }

  // check that all connection threads quit
  c3_uint_t num_worker_threads = Thread::get_num_connection_threads();
  if (num_worker_threads > 0) {
    log(LL_WARNING, "Shutting down with %u hung (?) connection threads", num_worker_threads);
  }
}

void Server::cleanup() {
  // optionally remove PID file (does nothing if the file was not created)
  delete_pid_file();

  // log completion of server shutdown
  syslog_message(LL_NORMAL, "%s: shutdown completed", c3lib_full_version_string);
}

///////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF INTERFACES PROVIDED BY THE SERVER
///////////////////////////////////////////////////////////////////////////////

void Server::increment_warning_count() {
  sr_warning_count.fetch_add(1, std::memory_order_release);
}

void Server::increment_error_count() {
  sr_error_count.fetch_add(1, std::memory_order_release);
}

void Server::begin_memory_deallocation(size_t size) {
  /*
   * If it is session or FPC optimizer that runs out of memory, then this method will call their
   * deallocation procedures directly, and mutex will stay locked during entire memory freeing process.
   * If meanwhile some other thread runs out of memory, calls this method and waits for mutex lock, then
   * by the time the mutex is released it would "see" that deallocation is not in progress, and would try
   * to start deallocation itself -- even though it would be done (just finished) by then...
   *
   * To prevent that from happening, we save deallocation flag right upon method entry, and then, after
   * we acquire the mutex and find out that deallocation is not in progress, we check if it was in
   * progress at the time of the call; if it was, we do not re-start it and simply quit to try allocating
   * memory again.
   */
  bool dealloc_was_in_progress = is_dealloc_in_progress();
  std::unique_lock<std::mutex> lock(sr_dealloc_mutex);
  if (sr_dealloc_in_progress.load(std::memory_order_relaxed)) {
    wait_for_deallocation(lock);
  } else if (!dealloc_was_in_progress) {
    dealloc_in_progress(true);
    // decide which domain we will "shrink"
    domain_t domain = DOMAIN_INVALID;
    // if a domain is above its memory quota, it's our first candidate
    if (fpc_memory.is_quota_set() && fpc_memory.get_used_size() > fpc_memory.get_quota()) {
      domain = DOMAIN_FPC;
    } else if (session_memory.is_quota_set() &&
      session_memory.get_used_size() > session_memory.get_quota()) {
      domain = DOMAIN_SESSION;
    }
    // otherwise, we pick domain with more actually allocated memory
    if (domain == DOMAIN_INVALID) {
      if (fpc_memory.get_used_size() > session_memory.get_used_size()) {
        domain = DOMAIN_FPC;
      } else {
        domain = DOMAIN_SESSION;
      }
    }
    // figure out how much memory we should free up
    c3_ulong_t configured = get_dealloc_chunk_size();
    c3_ulong_t needed = size * 2;
    c3_ulong_t chunk_size = max(configured, needed);
    // try to downsize selected domain
    if (domain == DOMAIN_FPC) {
      if (Thread::get_id() == TI_FPC_OPTIMIZER) {
        fpc_optimizer.free_memory_chunk(chunk_size);
        /*
         * the below two calls are essentially `end_memory_deallocation()` without trying to lock
         * `sr_dealloc_mutex`, which we already own
         */
        dealloc_in_progress(false);
        sr_dealloc_notifier.notify_all();
      } else {
        fpc_optimizer.post_free_memory_message(chunk_size);
        wait_for_deallocation(lock);
      }
    } else {
      if (Thread::get_id() == TI_SESSION_OPTIMIZER) {
        session_optimizer.free_memory_chunk(chunk_size);
        dealloc_in_progress(false);
        sr_dealloc_notifier.notify_all();
      } else {
        session_optimizer.post_free_memory_message(chunk_size);
        wait_for_deallocation(lock);
      }
    }
  }
}

void Server::end_memory_deallocation() {
  std::unique_lock<std::mutex> lock(sr_dealloc_mutex);
  dealloc_in_progress(false);
  sr_dealloc_notifier.notify_all();
}

bool Server::thread_is_quitting(c3_uint_t id) {
  c3_assert(id > TI_MAIN && id < MAX_NUM_THREADS);
  return sr_queue.post_id_message((server_command_t) id);
}

}
