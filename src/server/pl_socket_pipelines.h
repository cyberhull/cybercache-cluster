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
 * I/O pipelines: classes implementing networking (TCP/IP) pipelines.
 */
#ifndef _PL_SOCKET_PIPELINES_H
#define _PL_SOCKET_PIPELINES_H

#include "c3lib/c3lib.h"
#include "ht_objects.h"
#include "mt_message_queue.h"
#include "pl_pipeline_commands.h"
#include "pl_socket_events.h"

#include <cstdarg>

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// SOCKET PIPELINE
///////////////////////////////////////////////////////////////////////////////

/// Socket pipeline input commands
enum socket_input_command_t: c3_uintptr_t {
  SIC_INVALID = 0,                      // an invalid command (placeholder)
  SIC_IP_SET_CHANGE,                    // must listen or connect to a [different] set of IPs
  SIC_PORT_CHANGE,                      // must listen or connect to a [different] port
  SIC_INPUT_QUEUE_CAPACITY_CHANGE,      // should change capacity of the input queue
  SIC_INPUT_QUEUE_MAX_CAPACITY_CHANGE,  // should change input queue capacity limit
  SIC_OUTPUT_QUEUE_CAPACITY_CHANGE,     // should change capacity of the output queue
  SIC_OUTPUT_QUEUE_MAX_CAPACITY_CHANGE, // should change output queue capacity limit
  SIC_LOCAL_QUEUE_CAPACITY_CHANGE,      // should change capacity of the internal queue of deferred objects
  SIC_LOCAL_QUEUE_MAX_CAPACITY_CHANGE,  // should change internal queue of deferred objects limit
  SIC_PERSISTENT_CONNECTIONS_ON,        // should use persistent connections
  SIC_PERSISTENT_CONNECTIONS_OFF,       // should use per-command connections
  SIC_QUIT,                             // must complete outstanding actions and then quit
  SIC_NUMBER_OF_ELEMENTS
};

/// Socket pipeline output commands
enum socket_output_command_t: c3_uintptr_t {
  SOC_INVALID, // an invalid command (placeholder)
  SOC_QUIT,    // server is shutting down; output queue processors (connection threads) must quit
  SOC_NUMBER_OF_ELEMENTS
};

/// Message type for socket pipeline's input message queue
typedef CommandMessage<socket_input_command_t, PipelineCommand, ReaderWriter, SIC_NUMBER_OF_ELEMENTS>
  InputSocketMessage;

/// Message type for socket pipeline's output message queue
typedef CommandMessage<socket_output_command_t, PipelineCommand, ReaderWriter, SOC_NUMBER_OF_ELEMENTS>
  OutputSocketMessage;

/// Base class for all socket (networking) pipelines
class SocketPipeline: public virtual AbstractLogger {

  virtual void process_input_queue_object(ReaderWriter* rw) = 0;
  void process_queue_event();
  virtual void process_socket_event(const pipeline_event_t& event) = 0;
  virtual void process_object_event(const pipeline_event_t& event) = 0;
  virtual void process_connection_event(const pipeline_event_t& event) = 0;
  virtual void process_ip_set_change() = 0;
  virtual void process_port_change() = 0;
  virtual void process_port_and_ip_set_change() = 0;
  virtual void process_persistent_connections_change(bool persistent) = 0;
  virtual void process_local_capacity_change(c3_uint_t capacity) = 0;
  virtual void process_local_max_capacity_change(c3_uint_t max_capacity) = 0;
  virtual void reset_event_processor() = 0;
  virtual void cleanup() C3_FUNC_COLD { cleanup_socket_pipeline(); }

protected:
  typedef MessageQueue<InputSocketMessage> InputSocketQueue;
  typedef MessageQueue<OutputSocketMessage> OutputSocketQueue;

  const char* const    sp_name;            // pipeline name
  InputSocketQueue     sp_input_queue;     // input message queue
  OutputSocketQueue*   sp_output_queue;    // output message queue (optional)
  SocketEventProcessor sp_event_processor; // wrapper around `epoll` services
  c3_uint_t            sp_num_connections; // number of readers/writers currently being processed
  PipelineCommand*     sp_socket_change;   // socket set change command is being processed
  c3_ushort_t          sp_port_change;     // port change command is being processed
  bool                 sp_persistent;      // `true` if connections are persistent
  bool                 sp_quitting;        // `true` if "quit" request had been received

  SocketPipeline(const char* name, domain_t domain, host_object_t host,
    c3_uint_t input_capacity, c3_uint_t output_capacity, c3_byte_t base_id) C3_FUNC_COLD;
  virtual ~SocketPipeline() C3_FUNC_COLD;

  bool is_active() const {
    return !sp_quitting && sp_port_change == 0 && sp_socket_change == nullptr;
  }
  domain_t get_domain() const { return sp_input_queue.get_domain(); }
  Memory& get_memory_object() const { return sp_input_queue.get_memory_object(); }
  void cleanup_socket_pipeline() C3_FUNC_COLD;

  static c3_uint_t get_num_ips(const PipelineCommand* pc) {
    c3_assert(pc && pc->get_size() % sizeof(c3_ipv4_t) == 0);
    return pc->get_size() / sizeof(c3_ipv4_t);
  }
  static c3_ipv4_t* get_ip_array(const PipelineCommand* pc) {
    c3_assert(pc);
    return (c3_ipv4_t*) pc->get_data();
  }

  bool send_output_command(socket_output_command_t cmd) C3_FUNC_COLD;
  bool send_output_object(ReaderWriter* object);
  bool send_input_command(socket_input_command_t cmd) C3_FUNC_COLD;
  bool send_input_command(socket_input_command_t cmd, const void* data, size_t size) C3_FUNC_COLD;

  bool log_object(log_level_t level, ReaderWriter* rw, const char* msg) C3_FUNC_COLD;
  bool log_connection(log_level_t level, PipelineConnectionEvent* pce, const char* msg) C3_FUNC_COLD;
  void enter_quit_state() C3_FUNC_COLD;

public:
  SocketPipeline(const SocketPipeline&) = delete;
  SocketPipeline(SocketPipeline&&) = delete;

  SocketPipeline& operator=(const SocketPipeline&) = delete;
  SocketPipeline& operator=(SocketPipeline&&) = delete;

  // queue manipulation
  c3_uint_t get_input_queue_capacity() C3LM_OFF(const) C3_FUNC_COLD { return sp_input_queue.get_capacity(); }
  c3_uint_t get_max_input_queue_capacity() C3LM_OFF(const) C3_FUNC_COLD { return sp_input_queue.get_max_capacity(); }
  c3_uint_t get_output_queue_capacity() C3LM_OFF(const) C3_FUNC_COLD {
    return sp_output_queue != nullptr? sp_output_queue->get_capacity(): 0;
  }
  c3_uint_t get_max_output_queue_capacity() C3LM_OFF(const) C3_FUNC_COLD {
    return sp_output_queue != nullptr? sp_output_queue->get_max_capacity(): 0;
  }

  // to be used by the application
  bool is_using_persistent_connections() const { return sp_persistent; }
  bool is_service_active() const { return sp_event_processor.get_num_sockets() != 0; }
  bool is_initialized() const { return sp_event_processor.is_initialized(); }
  bool initialize() C3_FUNC_COLD { return sp_event_processor.initialize_processor(); }
  c3_uint_t get_num_connections() const { return sp_num_connections; }

  // messaging
  bool send_input_object(ReaderWriter* rw);
  bool send_ip_set_change_command(const c3_ipv4_t* ips, c3_uint_t num) C3_FUNC_COLD {
    return send_input_command(SIC_IP_SET_CHANGE, ips, sizeof(c3_ipv4_t) * num);
  }
  bool send_port_change_command(c3_ushort_t port) C3_FUNC_COLD {
    return send_input_command(SIC_PORT_CHANGE, &port, sizeof port);
  }
  bool send_input_queue_capacity_change_command(c3_uint_t capacity) C3_FUNC_COLD {
    return send_input_command(SIC_INPUT_QUEUE_CAPACITY_CHANGE, &capacity, sizeof(capacity));
  }
  bool send_max_input_queue_capacity_change_command(c3_uint_t max_capacity) C3_FUNC_COLD {
    return send_input_command(SIC_INPUT_QUEUE_MAX_CAPACITY_CHANGE, &max_capacity, sizeof max_capacity);
  }
  bool send_output_queue_capacity_change_command(c3_uint_t capacity) C3_FUNC_COLD {
    return send_input_command(SIC_OUTPUT_QUEUE_CAPACITY_CHANGE, &capacity, sizeof(capacity));
  }
  bool send_max_output_queue_capacity_change_command(c3_uint_t max_capacity) C3_FUNC_COLD {
    return send_input_command(SIC_OUTPUT_QUEUE_MAX_CAPACITY_CHANGE, &max_capacity, sizeof max_capacity);
  }
  bool send_local_queue_capacity_change_command(c3_uint_t capacity) C3_FUNC_COLD {
    return send_input_command(SIC_LOCAL_QUEUE_CAPACITY_CHANGE, &capacity, sizeof(capacity));
  }
  bool send_max_local_queue_capacity_change_command(c3_uint_t max_capacity) C3_FUNC_COLD {
    return send_input_command(SIC_LOCAL_QUEUE_MAX_CAPACITY_CHANGE, &max_capacity, sizeof max_capacity);
  }
  bool send_set_persistent_connections_command(bool enable) C3_FUNC_COLD {
    return send_input_command(enable? SIC_PERSISTENT_CONNECTIONS_ON: SIC_PERSISTENT_CONNECTIONS_OFF);
  }
  bool send_quit_command() C3_FUNC_COLD { return send_input_command(SIC_QUIT); }

  OutputSocketMessage get_output_message();

  // this method must *NOT* be called directly: its name should be passed to Thread::start()
  static void thread_proc(c3_uint_t id, ThreadArgument arg);
};

///////////////////////////////////////////////////////////////////////////////
// COMMAND OBJECT CONSUMER
///////////////////////////////////////////////////////////////////////////////

/**
 * Interface that defines methods that can be used to access output queue of the server entry point,
 * which (the queue) is constantly being listened by the connection threads.
 *
 * Main server thread may use this interface to send "quit" requests to the connection threads, while
 * binlog loader may post there `FileCommandReader` objects. Connection threads have to:
 *
 * - always check whether command reader they retrieve is "file" or "socket" object; in case of former,
 *   they must *not* send it to replication and/or binlog processors,
 *
 * - upon reception of "quit" request, never "get()" from the queue again, so that other/remaining "quit"
 *   requests wouldn't be consumed by mistake (for main server thread, it should be enough to send just
 *   as many "quit" requests as there are connection threads).
 */
class CommandObjectConsumer {
public:
  // low-level command handlers
  virtual bool post_processors_quit_command() = 0;
  virtual bool post_command_reader(CommandReader* cr) = 0;
};

///////////////////////////////////////////////////////////////////////////////
// RESPONSE OBJECT CONSUMER
///////////////////////////////////////////////////////////////////////////////

class PayloadListChunkBuilder;

/**
 * Command processing status.
 *
 * Command processors must *always* send some kind of response back to the socket pipeline. This type is
 * meant to facilitate that: upon start of handling a command, processors should set status to "format
 * error"; then, if parsing is done successfully, they should change status to "failure"; then, if command
 * processing succeeds, they should change status to "success". At the very end of the method, processors
 * should check status and, if it's not "success", send appropriate failure/error response. A "failure"
 * is not necassary an "error"; the processor could, for intance, fail to find an object, in which case
 * protocol may require to send an `OK` response.
 */
enum command_status_t {
  CS_FORMAT_ERROR,   // ill-formed command
  CS_INTERNAL_ERROR, // internal server error
  CS_FAILURE,        // the command could not be executed for some reason (not necessarily an "error")
  CS_SUCCESS         // the command had been executed, and response had been sent
};

/**
 * Interface that defines method to be used to access input queue of the server entry point. Object
 * stores can use this method to send response writers back through the socket pipeline.
 */
class ResponseObjectConsumer {

  bool write_post_response_writer(SocketResponseWriter* srw);
  bool post_error_response_impl(SocketResponseWriter* srw, const char* format, va_list args);
  bool post_data_response_impl(SocketResponseWriter* srw, PayloadHashObject* pho, const char* format, va_list args);

public:
  /*
   * Static helpers for response object creation.
   */
  static SocketResponseWriter* create_response(const CommandReader& cr);
  static SocketResponseWriter* create_object_response(const CommandReader& cr);
  /*
   * High-level response handlers.
   *
   * For all but one type of responses separate overloads are provided for use with `CommandReader`
   * reference or `SocketResponseWriter` pointer; the latter will free up passed object if sending it
   * failed for some reason. The only type of response for which there is no overload using
   * `ConnameReader` is `LIST` response; reason for that it that `SocketResponseWriter` is needed to
   * create and configure its second argument anyway.
   *
   * The `ERROR`-type response sending methods take regular `fprintf()`-style format string, while
   * `DATA`-type responses take special format strings that may only contain the following characters:
   *
   * - 'N' : respective argument is a signed 32-bit integer number (`c3_int_t`),
   * - 'U' : respective argument is an unsigned 32-bit integer number (`c3_uint_t`),
   * - 'S' : respective argument is a string; IMPORTANT: two arguments have to be passed for each such
   *         format specifier: first is a pointer to string characters (character sequence does *not*
   *         have to be '\0'-terminated), second is a 32-bit unsigned string length (`c3_uint_t`),
   * - 'C' : respective argument is a "C" string ('\0'-terminated sequence of characters),
   * - 'L' : respective argument is a string list (pointer to a `HeaderListChunkBuilder` object); this
   *         one is to be used for sending uncompressed lists within response headers; if it's necessary to
   *         send a potentially big list as part of [compressed] payload, a separate list-sending method
   *         has to be used.
   *
   * All methods posting response object into the socket pipeline's input queue will write response data
   * once first (to ensure minimum response delay), and only then queue the object. The object will be
   * queued even if all its data had been sent during first write attempt, because it is the consumer
   * (server listener) who's responsible for the housekeeping:
   *
   * - closing respective connection,
   * - counting.maintaining number of live connections,
   * - disposing response object.
   */
  bool post_ok_response(const CommandReader& cr);
  bool post_ok_response(SocketResponseWriter* srw);
  bool post_error_response(const CommandReader& cr, const char* format, ...) C3_FUNC_PRINTF(3) C3_FUNC_COLD;
  bool post_format_error_response(const CommandReader& cr) C3_FUNC_COLD;
  bool post_internal_error_response(const CommandReader& cr) C3_FUNC_COLD;
  bool post_data_response(const CommandReader& cr, const char* format, ...);
  bool post_data_response(SocketResponseWriter* srw, const char* format, ...);
  bool post_data_response(const CommandReader& cr, PayloadHashObject* pho, const char* format, ...);
  bool post_list_response(SocketResponseWriter* srw, const PayloadListChunkBuilder& list);

  // low-level response handlers
  virtual bool post_response_writer(ResponseWriter* rw) = 0;
  virtual bool log_error_response(const char* message, int length) = 0;
};

///////////////////////////////////////////////////////////////////////////////
// SOCKET INPUT PIPELINE
///////////////////////////////////////////////////////////////////////////////

/// Server entry point
class SocketInputPipeline: public SocketPipeline,
  public CommandObjectConsumer, public ResponseObjectConsumer {

  PipelineCommand* sip_last_ipv4_set; // last used IP set (in case we need to change port)

  void process_input_queue_object(ReaderWriter* rw) override;
  void process_socket_event(const pipeline_event_t& event) override;
  void process_object_event(const pipeline_event_t& event) override;
  void process_connection_event(const pipeline_event_t& event) override;
  void process_ip_set_change() override C3_FUNC_COLD;
  void process_port_change() override C3_FUNC_COLD;
  void process_port_and_ip_set_change() override C3_FUNC_COLD;
  void process_persistent_connections_change(bool persistent) override C3_FUNC_COLD;
  void process_local_capacity_change(c3_uint_t capacity) override C3_FUNC_COLD;
  void process_local_max_capacity_change(c3_uint_t max_capacity) override C3_FUNC_COLD;
  void reset_event_processor() override C3_FUNC_COLD;
  void cleanup() override C3_FUNC_COLD;

  void cleanup_socket_input_pipeline() C3_FUNC_COLD;

public:
  bool post_processors_quit_command() override C3_FUNC_COLD;
  bool post_command_reader(CommandReader* cr) override;
  bool post_response_writer(ResponseWriter* rw) override;
  bool log_error_response(const char* message, int length) override C3_FUNC_COLD;

  C3_FUNC_COLD SocketInputPipeline(const char* name, domain_t domain, host_object_t host,
    c3_uint_t input_capacity, c3_uint_t output_capacity, c3_byte_t base_id):
    SocketPipeline(name, domain, host, input_capacity, output_capacity, base_id) {
    sip_last_ipv4_set = nullptr;
  }
  SocketInputPipeline(const SocketInputPipeline&) = delete;
  SocketInputPipeline(SocketInputPipeline&&) = delete;
  ~SocketInputPipeline() override C3_FUNC_COLD;

  SocketInputPipeline& operator=(const SocketInputPipeline&) = delete;
  SocketInputPipeline& operator=(SocketInputPipeline&&) = delete;
};

///////////////////////////////////////////////////////////////////////////////
// SOCKET OUTPUT PIPELINE
///////////////////////////////////////////////////////////////////////////////

/// Server replicator or a client "command sender"
class SocketOutputPipeline: public SocketPipeline {

  static constexpr c3_uint_t SOP_DEFAULT_QUEUE_CAPACITY = 16;
  static constexpr c3_uint_t SOP_MAX_QUEUE_CAPACITY = 1024;

  typedef Pointer<ReaderWriter> ReaderWriterPointer;
  typedef Queue<ReaderWriterPointer> ReaderWriterQueue;

  ReaderWriterQueue sop_deferred_objects; // queue of pointers to deferred `ReaderWriter` objects

  void process_input_queue_object(ReaderWriter* rw) override;
  void process_socket_event(const pipeline_event_t &event) override;
  void process_object_event(const pipeline_event_t &event) override;
  void process_connection_event(const pipeline_event_t &event) override;
  void process_ip_set_change() override C3_FUNC_COLD;
  void process_port_change() override C3_FUNC_COLD;
  void process_port_and_ip_set_change() override C3_FUNC_COLD;
  void process_persistent_connections_change(bool persistent) override C3_FUNC_COLD;
  void process_local_capacity_change(c3_uint_t capacity) override C3_FUNC_COLD;
  void process_local_max_capacity_change(c3_uint_t max_capacity) override C3_FUNC_COLD;
  void reset_event_processor() override C3_FUNC_COLD;

public:
  C3_FUNC_COLD SocketOutputPipeline(const char* name, domain_t domain, host_object_t host,
    c3_uint_t input_capacity, c3_uint_t output_capacity, c3_byte_t base_id) noexcept:
    SocketPipeline(name, domain, host, input_capacity, output_capacity, base_id),
    sop_deferred_objects(domain, SOP_DEFAULT_QUEUE_CAPACITY, SOP_MAX_QUEUE_CAPACITY) {
  }

  c3_uint_t get_local_queue_capacity() const { return sop_deferred_objects.get_capacity(); }
  c3_uint_t get_local_queue_max_capacity() const { return sop_deferred_objects.get_max_capacity(); }
};

}

#endif // _PL_SOCKET_PIPELINES_H
