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
 * Server commands, messages, and queue.
 */
#ifndef _CC_SERVER_QUEUE_H
#define _CC_SERVER_QUEUE_H

#include "c3lib/c3lib.h"
#include "mt_threads.h"
#include "mt_message_queue.h"

namespace CyberCache {

/// Command IDs that are used in configuration commands and messages; first `MAX_NUM_THREADS` IDs are
/// reserved for "quitting" (notification) messages from subsystems' and connection threads
enum server_command_t: c3_uintptr_t {
  SC_INVALID = 0,            // an invalid command (placeholder)
  SC_QUIT = MAX_NUM_THREADS, // main thread should shut down the server and quit
  SC_SAVE_SESSION_STORE,     // save session store to the file
  SC_SAVE_FPC_STORE,         // save FPC store to the file
  SC_NUMBER_OF_ELEMENTS
};

/// Container for server commands that are mode than simple IDs
class ServerCommand {
  void*     sc_null; // field that distinguishes configuration commands from command reader instances
  c3_uint_t sc_len;  // size of text stored right after this field

public:
  static ServerCommand* create(const char* text, c3_uint_t length) {
    c3_assert(text && length);
    auto cc = alloc<ServerCommand>(global_memory, sizeof(ServerCommand) + length + 1);
    cc->sc_null = nullptr;
    cc->sc_len = length;
    std::memcpy((char*) cc + sizeof(ServerCommand), text, length + 1);
    return cc;
  }

  const char* get_text() const {
    c3_assert(is_you());
    return (const char*) this + sizeof(ServerCommand);
  }
  c3_uint_t get_length() const {
    c3_assert(is_you());
    return sc_len;
  }

  /////////////////////////////////////////////////////////////////////////////
  // METHODS REQUIRED BY MESSAGE CONTAINER
  /////////////////////////////////////////////////////////////////////////////

  bool is_you() const { return sc_null == nullptr; }

  c3_uint_t get_object_size() const { return sc_len + sizeof(ServerCommand); }

  Memory& get_memory_object() const { return global_memory; }

} __attribute__ ((__packed__));

/// Message type for configuration message queue
typedef CommandMessage<server_command_t, ServerCommand, CommandReader, SC_NUMBER_OF_ELEMENTS>
  ServerMessage;

/// Configuration message queue managed by the main thread
class ServerMessageQueue: public MessageQueue<ServerMessage> {
  static constexpr c3_uint_t DEFAULT_CAPACITY = 8;
  static constexpr c3_uint_t DEFAULT_MAX_CAPACITY = 256;

public:
  ServerMessageQueue():
    MessageQueue<ServerMessage>(DOMAIN_GLOBAL, HO_SERVER,
      DEFAULT_CAPACITY, DEFAULT_MAX_CAPACITY, 0) {
  }

  bool post_id_message(server_command_t id) {
    return put(ServerMessage(id));
  }
  bool post_data_message(const char* text, c3_uint_t length) {
    return put(ServerMessage(ServerCommand::create(text, length)));
  }
  bool post_object_message(CommandReader* cr) {
    return put(ServerMessage(cr));
  }
};

} // CyberCache

#endif // _CC_SERVER_QUEUE_H
