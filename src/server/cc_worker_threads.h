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
 * Worker threads servicing incoming connections.
 */
#ifndef _CC_WORKER_THREADS_H
#define _CC_WORKER_THREADS_H

#include "cc_subsystems.h"

namespace CyberCache {

/// Types of passwords for information commands
enum password_type_t: c3_byte_t {
  PT_NO_PASSWORD,    // do not use passwords for information commands
  PT_USER_PASSWORD,  // use "user" password (if specified in config file)
  PT_ADMIN_PASSWORD, // use "admin" password (if specified in config file)
  PT_NUMBER_OF_ELEMENTS
};

/**
 * Representation of a thread servicing incoming connections; shared by all worker threads.
 *
 * It is, basically, just a wrapper for various constants, variables, and methods; there is still an
 * instance of this class -- the only reason for its (instance's) existence is to trigger ctor, which
 * initializes static array of command descriptors.
 */
class ConnectionThread {
  static constexpr c3_byte_t CF_CONFIG_HANDLER  = 0x00; // command has to be handled by main thread
  static constexpr c3_byte_t CF_SESSION_HANDLER = 0x01; // command has to be handled by session store
  static constexpr c3_byte_t CF_FPC_HANDLER     = 0x02; // command has to be handled by FPC store
  static constexpr c3_byte_t CF_FPC_TAG_HANDLER = 0x03; // command has to be handled by FPC tag manager
  static constexpr c3_byte_t CF_HANDLER_MASK    = 0x03; // bits used to encode command handler
  static constexpr c3_byte_t CF_NO_PASSWORD     = 0x00; // does not require a password
  static constexpr c3_byte_t CF_USER_PASSWORD   = 0x04; // requires "user" password (if set in server config)
  static constexpr c3_byte_t CF_ADMIN_PASSWORD  = 0x08; // requires "admin" password (if set in server config)
  static constexpr c3_byte_t CF_INFO_PASSWORD   = 0x0C; // may require "user" or "admin" password
  static constexpr c3_byte_t CF_PASSWORD_MASK   = 0x0C; // bits used to encode password requirements
  static constexpr c3_byte_t CF_REPLICATE       = 0x40; // has to be replicated (if services are enabled)
  static constexpr c3_byte_t CF_VALID_COMMAND   = 0x80; // slot corresponds to a valid command

  static c3_byte_t       ct_command_info[BYTE_MAX_VAL + 1]; // command information for selector/executor
  static password_type_t ct_info_password_type;             // password to use for information commands

  static void define_command(c3_byte_t command, c3_byte_t flags) {
    ct_command_info[command] = flags | CF_VALID_COMMAND;
  }
  static c3_byte_t get_command_flags(c3_byte_t command) {
    return ct_command_info[command];
  }
  static void process_command_object(CommandReader* cr);

public:
  ConnectionThread() noexcept C3_FUNC_COLD;

  static password_type_t get_info_password_type() { return ct_info_password_type; }
  static void set_info_password_type(password_type_t type) {
    c3_assert(type < PT_NUMBER_OF_ELEMENTS && Thread::get_id() == TI_MAIN);
    ct_info_password_type = type;
  }

  static bool start_connection_threads(c3_uint_t num) C3_FUNC_COLD;
  static bool stop_connection_threads(c3_uint_t num) C3_FUNC_COLD;

  static void thread_proc(c3_uint_t id, ThreadArgument arg);
};

extern ConnectionThread connection_thread;

} // CyberCache

#endif // _CC_WORKER_THREADS_H
