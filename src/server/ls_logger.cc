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
#include "ls_logger.h"
#include "ls_utils.h"

#include <cstdarg>

namespace CyberCache {

const char* Logger::l_level_names[LL_NUMBER_OF_ELEMENTS] = {
  "<INVALID>", // LL_INVALID
  "EXPLICIT",  // LL_EXPLICIT
  "FATAL",     // LL_FATAL
  "ERROR",     // LL_ERROR
  "WARNING",   // LL_WARNING
  "TERSE",     // LL_TERSE
  "NORMAL",    // LL_NORMAL
  "VERBOSE",   // LL_VERBOSE
  "DEBUG",     // LL_DEBUG
};

///////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION: INTERNAL CLASSES
///////////////////////////////////////////////////////////////////////////////

Logger::LogString* Logger::LogString::create(log_level_t level, const char* msg, size_t length) {
  assert(msg);
  if (length > LOG_STRING_MAX_SIZE) {
    length = LOG_STRING_MAX_SIZE;
  }
  size_t full_length = length + 1;
  auto ls = alloc<LogString>(LOG_STRING_OVERHEAD + full_length);
  ls->ls_length = (c3_ushort_t) full_length;
  ls->ls_level = level;
  std::memcpy(ls->ls_text, msg, length);
  ls->ls_text[length] = 0; // the string may be not 0-terminated
  return ls;
}

Logger::LogCommand* Logger::LogCommand::create(log_subcommand_t cmd, const void* arg, size_t size) {
  assert(cmd > 0 && cmd < LS_NUMBER_OF_ELEMENTS && arg && size < LOG_COMMAND_MAX_SIZE);
  auto lc = alloc<LogCommand>(LOG_COMMAND_OVERHEAD + size);
  lc->lc_length = -(c3_short_t) size;
  lc->lc_cmd = cmd;
  std::memcpy(lc->lc_arg.lca_path, arg, size);
  return lc;
}

Logger::LogCommand* Logger::LogCommand::create(Logger::log_subcommand_t cmd, const char* arg) {
  assert(arg);
  return create(cmd, arg, std::strlen(arg) + 1);
}

///////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION: LOGGER
///////////////////////////////////////////////////////////////////////////////

void Logger::write_data(log_level_t level, const void* buffer, size_t size) {
  if (is_fd_valid()) {
    assert(buffer && size > 0);

    // 1) see if we should provide extra prefix (severity)
    size_t prefix_size;
    const char* prefix;
    switch (level) {
      case LL_WARNING: {
        static const char warning_prefix[] = "[WARNING] ";
        prefix = warning_prefix;
        prefix_size = sizeof warning_prefix - 1;
        break;
      }
      case LL_ERROR: {
        static const char error_prefix[] = "[ERROR] ";
        prefix = error_prefix;
        prefix_size = sizeof error_prefix - 1;
        break;
      }
      case LL_FATAL: {
        static const char error_prefix[] = "[FATAL ERROR] ";
        prefix = error_prefix;
        prefix_size = sizeof error_prefix - 1;
        break;
      }
      default:
        prefix = nullptr;
        prefix_size = 0;
    }

    // 2) compose log message
    const size_t total_message_size = TIMER_FORMAT_STRING_LENGTH + prefix_size + size + 1;
    char message_buffer[total_message_size];
    Timer timer;
    timer.to_ascii(true, message_buffer);
    message_buffer[TIMER_FORMAT_STRING_LENGTH - 1] = ' ';
    if (prefix_size > 0) {
      std::memcpy(message_buffer + TIMER_FORMAT_STRING_LENGTH, prefix, prefix_size);
    }
    std::memcpy(message_buffer + TIMER_FORMAT_STRING_LENGTH + prefix_size, buffer, size);
    message_buffer[total_message_size - 1] = '\n';

    // 3) write message to the log file
    c3_write_file(get_fd(), message_buffer, total_message_size);

    // 4) update current log file size
    increment_current_size(total_message_size);

    /*
     * We cannot reliably force log rotation within this method:
     *
     * a) if we call log rotation method here directly, we get into infinite recursion, because that
     * method would call write_string() (and thus write_date()) itself, we will again see that log file
     * size limit exceeded, etc. ad infinitum;
     *
     * b) if we try to send "log-rotate" message to ourselves, the queue might happen to be full, and
     * we'd end up waiting infinitely, because the only thread that pulls messages from that queue is
     * the logger itself.
     *
     * So we just increment current file size, and leave it to the main thread function to notice
     * overage and trigger log rotation.
     */
  }
}

void Logger::write_string(log_level_t level, const char* format, ...) {
  if (level <= l_level && is_fd_valid()) {
    char buffer[LOG_STRING_MAX_SIZE];
    va_list args;
    va_start(args, format);
    int length = std::vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);
    if (length > 0) {
      write_data(level, buffer, (size_t) length);
    }
  }
}

void Logger::write_header_strings() {
  write_string(LL_EXPLICIT, "%s", c3lib_full_version_string);
  if (l_level >= LL_TERSE) {
    char sys_info[C3_SYSTEM_INFO_BUFFER_SIZE];
    write_string(LL_TERSE, "System: %s", c3_get_system_info(sys_info, sizeof sys_info));
    write_string(LL_TERSE, "Available memory: %llu bytes total, %llu bytes free",
      c3_get_total_memory(), c3_get_available_memory());
    write_string(LL_TERSE, "Process PID: %d (parent: %d)", getpid(), getppid());
    write_string(LL_TERSE, "Log file: '%s'", l_path.get_chars());
    write_string(LL_TERSE, "Log level: %s", l_level_names[l_level]);
  }
}

void Logger::process_log_path_change_command(const char* path) {
  bool new_path_ok = path != nullptr && path[0] != '\0';
  if (is_fd_valid()) {
    if (l_level >= LL_NORMAL) {
      write_string(LL_NORMAL, "Configuration change request received:");
      if (new_path_ok) {
        write_string(LL_NORMAL, "New log file: '%s'", path);
      } else {
        write_string(LL_NORMAL, "Disabling logger");
      }
    }
    // even if this call fails, we do not have any acceptable recovery strategy
    close_file();
    l_path.empty();
  }
  if (new_path_ok) {
    file_mode_t mode = c3_file_access(path, AM_WRITABLE)? FM_APPEND: FM_CREATE;
    if (open_file(path, mode)) {
      l_path.set(DOMAIN_GLOBAL, path);
      write_header_strings();
    }
  }
}

void Logger::process_rotation_threshold_change_command(c3_ulong_t threshold) {
  if (threshold >= MIN_LOG_FILE_SIZE && threshold <= MAX_LOG_FILE_SIZE) {
    set_max_size(threshold);
    write_string(LL_NORMAL, "Log size threshold set to %lld bytes", threshold);
  } else {
    write_string(LL_ERROR, "Log size threshold out of %lld..%lld range: %lld",
      MIN_LOG_FILE_SIZE, MAX_LOG_FILE_SIZE, threshold);
  }
}

void Logger::process_rotation_path_change_command(const char* path) {
  if (path != nullptr && path[0] != '\0') {
    rotation_type_t type = LogUtils::get_log_rotation_type(path);
    if (type != RT_INVALID) {
      l_rot_path.set(DOMAIN_GLOBAL, path);
      write_string(LL_NORMAL, "Log rotation path set to '%s'", path);
    } else {
      write_string(LL_ERROR, "Ill-formed log rotation path: '%s'", path);
    }
  } else {
    l_rot_path.empty();
    write_string(LL_NORMAL, "Log rotation disabled");
  }
}

void Logger::process_rotate_command(const char* reason) {
  if (is_fd_valid() && l_path) {
    if (l_rot_path) {
      write_string(LL_NORMAL, "Rotating log file (%s)", reason);
      c3_ulong_t prev_size = get_current_size();
      if (close_file()) {
        char path[MAX_FILE_PATH_LENGTH];
        rotation_result_t result = LogUtils::rotate_log(l_path, l_rot_path, path);
        switch (result) {
          case RR_SUCCESS:
          case RR_SUCCESS_RND:
            open_file(l_path, FM_CREATE);
            write_header_strings();
            write_string(LL_NORMAL, "Previous log file moved to '%s'", path);
            break;
          default:
            c3_open_file(l_path, FM_APPEND);
            if (prev_size >= get_max_size()) {
              set_current_size(prev_size);
              // if we got here because of hitting threshold, increase it so that every logging attempt
              // does not lead to another (failed...) rotation attempt
              set_max_size(prev_size + LOG_FILE_SIZE_PADDING);
            }
            write_string(LL_ERROR, "Log rotation to '%s' FAILED", l_rot_path.get_chars());
            write_string(LL_ERROR, "Continuing with current log file");
        }
      } else {
        write_string(LL_ERROR, "Could not close log file for rotation: %s", c3_get_error_message());
      }
    } else {
      write_string(LL_ERROR, "Cannot rotate log: rotation path not set");
    }
  }
}

void Logger::process_capacity_change_command(c3_uint_t capacity) {
  c3_uint_t new_capacity = l_queue.set_capacity(capacity);
  write_string(LL_VERBOSE, "Queue capacity change: %u (requested: %u)", new_capacity, capacity);
}

void Logger::process_max_capacity_change_command(c3_uint_t max_capacity) {
  c3_uint_t new_capacity = l_queue.set_max_capacity(max_capacity);
  write_string(LL_VERBOSE, "Max queue capacity change: %u (requested: %u, capacity: %u)",
    new_capacity, max_capacity, l_queue.get_capacity());
}

bool Logger::send_command(log_command_t cmd) {
  if (l_level != LL_INVALID) {
    assert(cmd && cmd < LC_NUMBER_OF_ELEMENTS);
    return l_queue.put(LogMessage(cmd));
  }
  return false;
}

bool Logger::send_subcommand(log_subcommand_t cmd, const void* data, size_t size) {
  if (l_level != LL_INVALID) {
    return l_queue.put(LogMessage(LogCommand::create(cmd, data, size)));
  }
  return false;
}

bool Logger::send_subcommand(log_subcommand_t cmd, const char* data) {
  if (l_level != LL_INVALID) {
    return l_queue.put(LogMessage(LogCommand::create(cmd, data)));
  }
  return false;
}

void Logger::enter_quit_state() {
  l_quitting = true;
  Thread::set_state(TS_QUITTING);
}

void Logger::shut_down() {
  // disable logging
  process_log_path_change_command(nullptr);
  // disable sending commands
  l_level = LL_INVALID;
  #ifdef C3_SAFE
  // deplete message queue
  for (;;) {
    LogMessage msg = l_queue.try_get();
    if (!msg.is_valid()) {
      // an invalid message: the queue is empty, we're done
      break;
    }
  }
  #endif
  // empty message queue
  l_queue.dispose();
  // clear paths
  l_path.empty();
  l_rot_path.empty();
  // tell outer world logger is quitting (to make join() possible)
  Thread::set_state(TS_QUITTING);
}

///////////////////////////////////////////////////////////////////////////////
// INTERFACE
///////////////////////////////////////////////////////////////////////////////

void Logger::set_level(log_level_t level) {
  c3_assert(level > LL_INVALID && level < LL_NUMBER_OF_ELEMENTS);
  l_level = level;
  if (is_fd_valid()) {
    // this will record level change in the log file
    send_command((log_command_t) level);
  }
}

bool Logger::log(log_level_t level, const char* format, ...) {
  c3_assert(level > LL_INVALID && format);
  increment_counts(level);
  if (l_level >= level) { // is logging [for this level] enabled?
    char buffer[LOG_STRING_MAX_SIZE];
    va_list args;
    va_start(args, format);
    int length = std::vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);
    if (length > 0) {
      return l_queue.put(LogMessage(LogString::create(level, buffer, (size_t) length)));
    }
  }
  return false;
}

bool Logger::log_string(log_level_t level, const char* str, int length) {
  c3_assert(level > LL_INVALID && str && length > 0);
  increment_counts(level);
  if (l_level >= level) { // is logging [for this level] enabled?
    return l_queue.put(LogMessage(LogString::create(level, str, (size_t) length)));
  }
  return false;
}

bool Logger::send_path_change_command(const char* path) {
  if (path != nullptr && path[0] != '\0') {
    return send_subcommand(LS_LOG_PATH_CHANGE, path);
  } else {
    return send_command(LC_DISABLE);
  }
}

bool Logger::send_rotation_threshold_change_command(c3_long_t threshold) {
  return send_subcommand(LS_ROTATION_THRESHOLD_CHANGE, &threshold, sizeof threshold);
}

bool Logger::send_rotation_path_change_command(const char* path) {
  if (path != nullptr && path[0] != '\0') {
    return send_subcommand(LS_ROTATION_PATH_CHANGE, path);
  } else {
    return send_command(LC_DISABLE_ROTATION);
  }
}

bool Logger::send_capacity_change_command(c3_uint_t capacity) {
  return send_subcommand(LS_SET_CAPACITY, &capacity, sizeof capacity);
}

bool Logger::send_max_capacity_change_command(c3_uint_t max_capacity) {
  return send_subcommand(LS_SET_MAX_CAPACITY, &max_capacity, sizeof max_capacity);
}

void Logger::thread_proc(c3_uint_t id, ThreadArgument arg) {
  Thread::set_state(TS_ACTIVE);
  auto logger = (Logger*) arg.get_pointer();
  assert(logger && logger->is_fd_invalid());
  for (;;) {
    // see if we have to enter "quit" state
    if (!logger->l_quitting && Thread::received_stop_request()) {
      logger->enter_quit_state();
    }

    // get next message
    LogMessage msg;
    if (logger->l_quitting) {
      msg = logger->l_queue.try_get();
      if (!msg.is_valid()) {
        break;
      }
    } else {
      // see if we have to rotate the log
      if (logger->get_current_size() >= logger->get_max_size()) {
        logger->process_rotate_command("log file size exceeded");
      }
      Thread::set_state(TS_IDLE);
      msg = logger->l_queue.get();
      Thread::set_state(TS_ACTIVE);
    }
    switch (msg.get_type()) {
      case CMT_ID_COMMAND: {
        log_command_t cmd = msg.get_id_command();
        switch (cmd) {
          case LC_DISABLE:
            logger->process_log_path_change_command(nullptr);
            break;
          case LC_DISABLE_ROTATION:
            logger->process_rotation_path_change_command(nullptr);
            break;
          case LC_ROTATE:
            logger->process_rotate_command("received rotation request");
            break;
          case LC_QUIT:
            /*
             * Disable logging permanently, shut down logger
             *
             * This command *MUST* be sent *after* Thread::request_stop() for logger to take notice, or
             * else it could be sitting in MessageQueue::get() waiting for new log messages or commands
             * that are not going to arrive as the server is winding down.
             *
             * We don't have to do anything here: next while() iteration will notice there's "quit"
             * request, exit the loop, and proceed with shutdown actions.
             */
            logger->write_string(LL_DEBUG, "Received QUIT request");
            break;
          default:
            c3_assert(cmd >= LC_LEVEL_FIRST_ID && cmd <= LC_LEVEL_LAST_ID);
            auto level = (log_level_t) cmd;
            /*
             * It might as well be the same level, but we still report it so that people checking logs
             * after issuing a command are not confused.
             */
            logger->write_string(LL_TERSE, "Log level set to %s", l_level_names[level]);
        }
        break;
      }
      case CMT_DATA_COMMAND: {
        const LogCommand& lc = msg.get_data_command();
        switch (lc.lc_cmd) {
          case LS_LOG_PATH_CHANGE:
            logger->process_log_path_change_command(lc.lc_arg.lca_path);
            break;
          case LS_ROTATION_THRESHOLD_CHANGE:
            logger->process_rotation_threshold_change_command(lc.lc_arg.lca_size);
            break;
          case LS_ROTATION_PATH_CHANGE:
            logger->process_rotation_path_change_command(lc.lc_arg.lca_path);
            break;
          case LS_SET_CAPACITY:
            logger->process_capacity_change_command(lc.lc_arg.lca_capacity);
            break;
          case LS_SET_MAX_CAPACITY:
            logger->process_max_capacity_change_command(lc.lc_arg.lca_capacity);
            break;
          default:
            c3_assert_failure();
        }
        break;
      }
      case CMT_OBJECT: {
        const LogString& ls = msg.get_const_object();
        logger->write_data(ls.ls_level, ls.ls_text, (size_t) (ls.ls_length - 1));
        break;
      }
      default:
        ; // do nothing
    }
  }
  // disable logging *and* sending commands, deplete message queue etc.
  logger->shut_down();
}

}
