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
#include "cc_worker_threads.h"
#include "cc_server.h"
#include "pl_net_configuration.h"

namespace CyberCache {

ConnectionThread connection_thread;

c3_byte_t ConnectionThread::ct_command_info[BYTE_MAX_VAL + 1];
password_type_t ConnectionThread::ct_info_password_type = PT_NO_PASSWORD;

ConnectionThread::ConnectionThread() noexcept {
  define_command(CMD_PING, CF_CONFIG_HANDLER | CF_INFO_PASSWORD);
  define_command(CMD_CHECK, CF_CONFIG_HANDLER | CF_INFO_PASSWORD);
  define_command(CMD_INFO, CF_CONFIG_HANDLER | CF_INFO_PASSWORD);
  define_command(CMD_STATS, CF_CONFIG_HANDLER | CF_INFO_PASSWORD);
  define_command(CMD_SHUTDOWN, CF_CONFIG_HANDLER | CF_ADMIN_PASSWORD);
  define_command(CMD_LOADCONFIG, CF_CONFIG_HANDLER | CF_ADMIN_PASSWORD);
  define_command(CMD_RESTORE, CF_CONFIG_HANDLER | CF_ADMIN_PASSWORD);
  define_command(CMD_STORE, CF_CONFIG_HANDLER | CF_ADMIN_PASSWORD);
  define_command(CMD_GET, CF_CONFIG_HANDLER | CF_ADMIN_PASSWORD);
  define_command(CMD_SET, CF_CONFIG_HANDLER | CF_ADMIN_PASSWORD);
  define_command(CMD_LOG, CF_CONFIG_HANDLER | CF_ADMIN_PASSWORD);
  define_command(CMD_ROTATE, CF_CONFIG_HANDLER | CF_ADMIN_PASSWORD);

  define_command(CMD_READ, CF_SESSION_HANDLER | CF_USER_PASSWORD);
  define_command(CMD_WRITE, CF_SESSION_HANDLER | CF_USER_PASSWORD | CF_REPLICATE);
  define_command(CMD_DESTROY, CF_SESSION_HANDLER | CF_USER_PASSWORD | CF_REPLICATE);
  define_command(CMD_GC, CF_SESSION_HANDLER | CF_USER_PASSWORD | CF_REPLICATE);

  define_command(CMD_LOAD, CF_FPC_HANDLER | CF_USER_PASSWORD);
  define_command(CMD_TEST, CF_FPC_HANDLER | CF_USER_PASSWORD);
  define_command(CMD_SAVE, CF_FPC_HANDLER | CF_USER_PASSWORD | CF_REPLICATE);
  define_command(CMD_REMOVE, CF_FPC_HANDLER | CF_USER_PASSWORD | CF_REPLICATE);
  define_command(CMD_CLEAN, CF_FPC_TAG_HANDLER | CF_USER_PASSWORD | CF_REPLICATE);
  define_command(CMD_GETIDS, CF_FPC_TAG_HANDLER | CF_USER_PASSWORD);
  define_command(CMD_GETTAGS, CF_FPC_TAG_HANDLER | CF_USER_PASSWORD);
  define_command(CMD_GETIDSMATCHINGTAGS, CF_FPC_TAG_HANDLER | CF_USER_PASSWORD);
  define_command(CMD_GETIDSNOTMATCHINGTAGS, CF_FPC_TAG_HANDLER | CF_USER_PASSWORD);
  define_command(CMD_GETIDSMATCHINGANYTAGS, CF_FPC_TAG_HANDLER | CF_USER_PASSWORD);
  define_command(CMD_GETFILLINGPERCENTAGE, CF_FPC_HANDLER | CF_USER_PASSWORD);
  define_command(CMD_GETMETADATAS, CF_FPC_HANDLER | CF_USER_PASSWORD);
  define_command(CMD_TOUCH, CF_FPC_HANDLER | CF_USER_PASSWORD | CF_REPLICATE);
}

bool ConnectionThread::start_connection_threads(c3_uint_t num) {
  c3_assert(Thread::get_id() == TI_MAIN && num &&
    Thread::get_num_connection_threads() + num <= MAX_NUM_CONNECTION_THREADS);
  c3_uint_t num_to_start = num;
  ThreadArgument arg(&connection_thread);
  for (c3_uint_t i = TI_FIRST_CONNECTION_THREAD; i < MAX_NUM_THREADS; i++) {
    /*
     * Previous requests to lower the number of connection threads may have left "holes" in the pool
     * of connection threads, so we have to go though entire array of connection threads to find slots
     * that are currently "unused".
     */
    if (Thread::get_state(i) == TS_UNUSED) {
      Thread::start(i, thread_proc, arg);
      if (--num_to_start == 0) {
        break;
      }
    }
  }
  if (num_to_start > 0) {
    server_logger.log(LL_ERROR, "Could NOT start %u out of %u connection threads", num_to_start, num);
    return false;
  } else {
    server_logger.log(LL_NORMAL, "Started %u connection threads", num);
    return true;
  }
}

bool ConnectionThread::stop_connection_threads(c3_uint_t num) {
  c3_assert(Thread::get_id() == TI_MAIN && num && num <= Thread::get_num_connection_threads());
  c3_uint_t num_sent_messages = 0;
  for (c3_uint_t i = 0; i < num; i++) {
    /*
     * We do not know in advance which thread(s) will receive these `SOC_QUIT` commands, and in what order;
     * each thread receiving such command will quit, and thread proc wrapper will post thread ID to the
     * configuration queue as an "ID message", causing main thread to `join()` quitting thread.
     */
    if (server_listener.post_processors_quit_command()) {
      num_sent_messages++;
    }
  }
  if (num_sent_messages == num) {
    return true;
  } else {
    server_logger.log(LL_ERROR, "Could not send stop request to %u out of %u threads",
      num - num_sent_messages, num);
    return false;
  }
}

void ConnectionThread::process_command_object(CommandReader* cr) {

  // 1) Check that the command is a valid one
  // ----------------------------------------
  command_t command = cr->get_command_id();
  c3_byte_t flags = get_command_flags(command);
  if (flags != 0) {

    C3_DEBUG(server_logger.log(LL_DEBUG, "> RECEIVED command '%s' FROM [%d]",
      c3_get_command_name(command), cr->get_fd()));

    // 2) Authenticate
    // ---------------
    c3_byte_t required_password_type = flags & CF_PASSWORD_MASK;

    // 2a) if it's an information retrieval command, see what authentication level is really required
    if (required_password_type == CF_INFO_PASSWORD) {
      switch (ct_info_password_type) {
        case PT_NO_PASSWORD:
          required_password_type = CF_NO_PASSWORD;
          break;
        case PT_USER_PASSWORD:
          required_password_type = CF_USER_PASSWORD;
          break;
        case PT_ADMIN_PASSWORD:
          required_password_type = CF_ADMIN_PASSWORD;
          break;
        default:
          c3_assert_failure();
      }
    }

    // 2b) get all passwords specified in configuration file
    c3_hash_t user_password = server_net_config.get_user_password();
    c3_hash_t admin_password = server_net_config.get_admin_password();
    c3_hash_t bulk_password = server_net_config.get_bulk_password();

    // 2c) see if required password is actually specified in server configuration
    if ((required_password_type == CF_USER_PASSWORD && user_password == INVALID_HASH_VALUE) ||
      (required_password_type == CF_ADMIN_PASSWORD && admin_password == INVALID_HASH_VALUE)) {
        required_password_type = CF_NO_PASSWORD;
    }

    // 2d) see what password was actually set in the command, and whether it's "enough"
    bool pwd_check_passed = false;
    c3_hash_t provided_password;
    command_password_type_t provided_password_type = cr->get_command_pwd_hash(provided_password);
    switch (provided_password_type) {
      case CPT_NO_PASSWORD:
        if (required_password_type == CF_NO_PASSWORD) {
          pwd_check_passed = true;
        }
        break;
      case CPT_USER_PASSWORD:
        if (required_password_type == CF_NO_PASSWORD ||
          (required_password_type == CF_USER_PASSWORD && provided_password == user_password)) {
          pwd_check_passed = true;
        }
        break;
      case CPT_ADMIN_PASSWORD:
        if (required_password_type == CF_NO_PASSWORD || provided_password == admin_password) {
          pwd_check_passed = true;
        }
        break;
      case CPT_BULK_PASSWORD:
        if (required_password_type == CF_NO_PASSWORD ||
          (required_password_type == CF_USER_PASSWORD && provided_password == bulk_password)) {
          pwd_check_passed = true;
        }
    }

    // 2e) see whether the command has passed authentication
    if (pwd_check_passed) {

      // 3) Handle replication and binlog
      // --------------------------------
      if ((flags & CF_REPLICATE) != 0) {

        // 3a) optionally replace password
        c3_assert(provided_password_type != CPT_ADMIN_PASSWORD);
        if (provided_password_type == CPT_USER_PASSWORD) {
          cr->set_command_pwd_hash(CPT_BULK_PASSWORD, bulk_password);
        }

        // 3b) try sending copies to replication and binlog services
        if ((flags & CF_FPC_HANDLER) != 0) {
          if (fpc_replicator.is_service_active()) {
            auto fpc_replication_copy = alloc<SocketCommandWriter>(fpc_memory);
            new (fpc_replication_copy) SocketCommandWriter(fpc_memory, *cr);
            fpc_replicator.send_input_object(fpc_replication_copy);
          }
          if (fpc_binlog.is_service_active() && cr->is_set(IO_FLAG_NETWORK)) {
            auto fpc_binlog_copy = alloc<FileCommandWriter>(fpc_memory);
            // passing zero `fd` sets "valid, but not active" object state
            new (fpc_binlog_copy) FileCommandWriter(fpc_memory, *cr, 0);
            fpc_binlog.send_object(fpc_binlog_copy);
          }
        } else {
          if (session_replicator.is_service_active()) {
            auto session_replication_copy = alloc<SocketCommandWriter>(session_memory);
            new (session_replication_copy) SocketCommandWriter(session_memory, *cr);
            session_replicator.send_input_object(session_replication_copy);
          }
          if (session_binlog.is_service_active() && cr->is_set(IO_FLAG_NETWORK)) {
            auto session_binlog_copy = alloc<FileCommandWriter>(session_memory);
            // passing zero `fd` sets "valid, but not active" object state
            new (session_binlog_copy) FileCommandWriter(session_memory, *cr, 0);
            session_binlog.send_object(session_binlog_copy);
          }
        }
      }

      // 4) Dispatch the command
      bool result;
      switch (flags & CF_HANDLER_MASK) {
        case CF_CONFIG_HANDLER:
          result = server.post_object_message(cr);
          break;
        case CF_SESSION_HANDLER:
          result = session_store.process_command(cr);
          break;
        case CF_FPC_HANDLER:
          result = fpc_store.process_command(cr);
          break;
        case CF_FPC_TAG_HANDLER:
          result = tag_manager.post_command_message(cr);
          break;
        default:
          // just to satisfy the compiler and CLion (we have 4 `case`s on 2-bit `switch` here...)
          c3_assert_failure();
          result = false;
      }

      // 5) Report internal server error if the command was not processed
      if (result) {
        // response had already been sent, and command reader had already been disposed by a handler
        return;
      }
      server_listener.post_internal_error_response(*cr);
      // proceed with disposing the object

    } else {
      server_logger.log(LL_ERROR, "Authentication failed for command [%02X] received from '%s'",
        command, c3_ip2address(cr->get_ipv4()));
      server_listener.post_error_response(*cr, "Authentication failed for command [%02X]", command);
    }
  } else {
    server_logger.log(LL_ERROR, "Unknown command [%02X] received from '%s'",
      command, c3_ip2address(cr->get_ipv4()));
    server_listener.post_error_response(*cr, "Unknown command [%02X]", command);
  }
  ReaderWriter::dispose(cr);
}

void ConnectionThread::thread_proc(c3_uint_t id, ThreadArgument arg) {
  Thread::set_state(TS_ACTIVE);
  server_logger.log(LL_VERBOSE, "Started connection thread [%u]", id);
  for (;;) {
    Thread::set_state(TS_IDLE);
    OutputSocketMessage msg = server_listener.get_output_message();
    command_message_type_t type = msg.get_type();
    if (type == CMT_ID_COMMAND) {
      Thread::set_state(TS_QUITTING);
      c3_assert(msg.get_id_command() == SOC_QUIT);
      server_logger.log(LL_VERBOSE, "Connection thread [%u] is quitting", id);
      break;
    } else if (type == CMT_OBJECT) {
      Thread::set_state(TS_ACTIVE);
      ReaderWriter* rw = msg.fetch_object();
      c3_assert(rw && rw->is_active() && rw->is_set(IO_FLAG_IS_READER) &&
        rw->is_clear(IO_FLAG_IS_RESPONSE)); // the object could have come from binlog loader
      process_command_object((SocketCommandReader*) rw);
    }
  }
}

} // CyberCache
