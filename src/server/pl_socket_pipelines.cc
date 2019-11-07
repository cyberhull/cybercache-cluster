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
#include "pl_net_configuration.h"
#include "pl_socket_pipelines.h"
#include "ht_shared_buffers.h"

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// SOCKET PIPELINE
///////////////////////////////////////////////////////////////////////////////

SocketPipeline::SocketPipeline(const char* name, domain_t domain, host_object_t host,
  c3_uint_t input_capacity, c3_uint_t output_capacity, c3_byte_t base_id):
  sp_name(name), sp_input_queue(domain, host, input_capacity, 0, base_id),
  sp_event_processor(name, *this, C3_DEFAULT_PORT) {

  if (output_capacity > 0) {
    sp_output_queue = alloc<OutputSocketQueue>(domain);
    new (sp_output_queue) OutputSocketQueue(domain, host, output_capacity, 0, base_id + 1);
  } else {
    sp_output_queue = nullptr;
  }

  sp_num_connections = 0;
  sp_socket_change = nullptr;
  sp_port_change = 0;
  sp_persistent = true;
  sp_quitting = false;
}

SocketPipeline::~SocketPipeline() { cleanup_socket_pipeline(); }

void SocketPipeline::cleanup_socket_pipeline() {
  if (sp_output_queue != nullptr) {
    sp_output_queue->dispose();
    get_memory_object().free(sp_output_queue, sizeof(OutputSocketQueue));
    sp_output_queue = nullptr;
  }
}

bool SocketPipeline::send_output_command(socket_output_command_t cmd) {
  if (sp_output_queue != nullptr) {
    return sp_output_queue->put(OutputSocketMessage(cmd));
  }
  return false;
}

bool SocketPipeline::send_output_object(ReaderWriter* object) {
  if (sp_output_queue != nullptr) {
    return sp_output_queue->put(OutputSocketMessage(object));
  }
  return false;
}

bool SocketPipeline::send_input_command(socket_input_command_t cmd) {
  if (sp_input_queue.put(InputSocketMessage(cmd))) {
    sp_event_processor.trigger_queue_event();
    return true;
  }
  return false;
}

bool SocketPipeline::send_input_command(socket_input_command_t cmd, const void* data, size_t size) {
  if (sp_input_queue.put(InputSocketMessage(PipelineCommand::create(cmd, sp_input_queue.get_domain(),
    data, size)))) {
    sp_event_processor.trigger_queue_event();
    return true;
  }
  return false;
}

bool SocketPipeline::log_object(log_level_t level, ReaderWriter* rw, const char* msg) {
  assert(rw != nullptr && rw->is_valid() && rw->is_set(IO_FLAG_NETWORK));
  const char* object_type;
  if (rw->is_set(IO_FLAG_IS_READER)) {
    if (rw->is_set(IO_FLAG_IS_RESPONSE)) {
      object_type = "RR"; // response reader
    } else {
      object_type = "CR"; // command reader
    }
  } else {
    if (rw->is_set(IO_FLAG_IS_RESPONSE)) {
      object_type = "RW"; // response writer
    } else {
      object_type = "CW"; // command writer
    }
  }
  return log(level, "%s: %s (%s: ip=%s fd=%d connections=%d)",
    sp_name, msg, object_type, c3_ip2address(rw->get_ipv4()), rw->get_fd(), sp_num_connections);
}

bool SocketPipeline::log_connection(log_level_t level, PipelineConnectionEvent* pce, const char* msg) {
  return log(level, "%s: %s (persistent connection: ip=%s fd=%d)",
    sp_name, msg, pce->get_address(), pce->get_fd());
}

void SocketPipeline::enter_quit_state() {
  sp_quitting = true;
  Thread::set_state(TS_QUITTING);
}

bool SocketPipeline::send_input_object(ReaderWriter* rw) {
  c3_assert(rw && rw->is_valid() && rw->is_set(IO_FLAG_NETWORK));

  if (sp_input_queue.put(InputSocketMessage(rw))) {
    sp_event_processor.trigger_queue_event();
    return true;
  }
  return false;
}

OutputSocketMessage SocketPipeline::get_output_message() {
  if (sp_output_queue != nullptr) {
    return std::move(sp_output_queue->get());
  } else {
    return std::move(OutputSocketMessage());
  }
}

void SocketPipeline::process_queue_event() {
  sp_event_processor.consume_queue_event();
  for (;;) {
    InputSocketMessage msg = sp_input_queue.try_get();
    switch (msg.get_type()) {
      case CMT_INVALID:
        C3_DEBUG_LOG("SP queue: no more events");
        // no more messages in the input queue
        return;
      case CMT_ID_COMMAND:
        C3_DEBUG_LOG("SP queue: command event");
        switch (msg.get_id_command()) {
          case SIC_PERSISTENT_CONNECTIONS_ON:
            log(LL_VERBOSE, "%s: switching to persistent connections", sp_name);
            process_persistent_connections_change(true);
            break;
          case SIC_PERSISTENT_CONNECTIONS_OFF:
            log(LL_VERBOSE, "%s: switching to per-command connections", sp_name);
            process_persistent_connections_change(false);
            break;
          case SIC_QUIT:
            log(LL_VERBOSE, "%s: QUIT request received", sp_name);
            enter_quit_state();
            break;
          default:
            c3_assert_failure();
        }
        break;
      case CMT_DATA_COMMAND: {
        C3_DEBUG_LOG("SP queue: data command event");
        const PipelineCommand& cmd = msg.get_data_command();
        c3_uint_t requested_capacity, set_capacity;
        switch (cmd.get_id()) {
          case SIC_IP_SET_CHANGE:
            if (!sp_quitting) {
              // enter "socket set change" mode if not in "quit" mode already
              sp_socket_change = msg.fetch_data_command();
            }
            break;
          case SIC_PORT_CHANGE:
            if (!sp_quitting) {
              // enter "port change" mode if not in "quit" mode already
              sp_port_change = msg.get_data_command().get_ushort_data();
            }
            break;
          case SIC_INPUT_QUEUE_CAPACITY_CHANGE:
            requested_capacity = cmd.get_uint_data();
            set_capacity = sp_input_queue.set_capacity(requested_capacity);
            log(LL_VERBOSE, "%s: input queue capacity set to %u (requested %u)",
              sp_name, set_capacity, requested_capacity);
            break;
          case SIC_INPUT_QUEUE_MAX_CAPACITY_CHANGE:
            requested_capacity = cmd.get_uint_data();
            set_capacity = sp_input_queue.set_max_capacity(requested_capacity);
            log(LL_VERBOSE, "%s: input queue max capacity set to %u (requested %u)",
              sp_name, set_capacity, requested_capacity);
            break;
          case SIC_OUTPUT_QUEUE_CAPACITY_CHANGE:
            if (sp_output_queue != nullptr) {
              requested_capacity = cmd.get_uint_data();
              set_capacity = sp_output_queue->set_capacity(requested_capacity);
              log(LL_VERBOSE, "%s: output queue capacity set to %u (requested %u)",
                sp_name, set_capacity, requested_capacity);
            }
            break;
          case SIC_OUTPUT_QUEUE_MAX_CAPACITY_CHANGE:
            if (sp_output_queue != nullptr) {
              requested_capacity = cmd.get_uint_data();
              set_capacity = sp_output_queue->set_max_capacity(requested_capacity);
              log(LL_VERBOSE, "%s: output queue max capacity set to %u (requested %u)",
                sp_name, set_capacity, requested_capacity);
            }
            break;
          case SIC_LOCAL_QUEUE_CAPACITY_CHANGE:
            process_local_capacity_change(cmd.get_uint_data());
            break;
          case SIC_LOCAL_QUEUE_MAX_CAPACITY_CHANGE:
            process_local_max_capacity_change(cmd.get_uint_data());
            break;
          default:
            c3_assert_failure();
        }
        break;
      }
      case CMT_OBJECT:
        C3_DEBUG_LOG("SP queue: object event");
        process_input_queue_object(msg.fetch_object());
        break;
    }
  }
}

void SocketPipeline::thread_proc(c3_uint_t id, ThreadArgument arg) {
  Thread::set_state(TS_ACTIVE);
  auto sp = (SocketPipeline*) arg.get_pointer();
  assert(sp && sp->sp_num_connections == 0 && sp->is_active());
  if (sp->is_initialized()) {
    for (;;) { // main (send-receive & configuration) loop
      do { // send-receive loop
        if (!sp->sp_quitting && Thread::received_stop_request()) {
          C3_DEBUG(sp->log(LL_DEBUG, "SP: entering quit state"));
          sp->enter_quit_state();
        } else {
          pipeline_event_t event;
          switch (sp->sp_event_processor.get_next_event(event)) {
            case PET_NONE:
              // all events had been consumed; grab new ones
              C3_DEBUG(sp->log(LL_DEBUG, "SP: entering idle state"));
              Thread::set_state(TS_IDLE);
              sp->sp_event_processor.wait_for_events();
              Thread::set_state(TS_ACTIVE);
              C3_DEBUG(sp->log(LL_DEBUG,
                "SP: entering active state (%u events)", sp->sp_event_processor.get_num_events()));
              break;
            case PET_QUEUE:
              PERF_INCREMENT_VAR_DOMAIN_COUNTER(sp->get_domain(), Pipeline_Queue_Events);
              C3_DEBUG(sp->log(LL_DEBUG, "SP: queue event"));
              sp->process_queue_event();
              break;
            case PET_SOCKET:
              PERF_INCREMENT_VAR_DOMAIN_COUNTER(sp->get_domain(), Pipeline_Socket_Events);
              C3_DEBUG(sp->log(LL_DEBUG, "SP: socket event"));
              sp->process_socket_event(event);
              break;
            case PET_OBJECT:
              PERF_INCREMENT_VAR_DOMAIN_COUNTER(sp->get_domain(), Pipeline_Object_Events);
              C3_DEBUG(sp->log(LL_DEBUG, "SP: object event"));
              sp->process_object_event(event);
              break;
            case PET_CONNECTION:
              PERF_INCREMENT_VAR_DOMAIN_COUNTER(sp->get_domain(), Pipeline_Connection_Events);
              C3_DEBUG(sp->log(LL_DEBUG, "SP: connection event"));
              sp->process_connection_event(event);
          }
        }
      } while (sp->is_active() || sp->sp_num_connections > 0);

      sp->reset_event_processor();

      if (sp->sp_quitting) {
        break; // "quit" or "force-quit" request received
      }

      C3_DEBUG(sp->log(LL_DEBUG, "SP: configuration request"));
      /*
       * A configuration request; it's not "either IP set change OR port change", these two requests
       * could both have come while there were still some live connections.
       */
      if (sp->sp_socket_change != nullptr && sp->sp_port_change != 0) {
        /*
         * Upon each change (IP set *or* port), input pipeline closes sockets and creates new ones;
         * if both IP set and port change, we only want to re-create sockets once.
         */
        sp->process_port_and_ip_set_change();
      } else if (sp->sp_socket_change != nullptr) {
        // configuration change: new IP set
        sp->process_ip_set_change();
      } else if (sp->sp_port_change != 0) {
        // configuration change: new port
        sp->process_port_change();
      }
    }
    sp->sp_event_processor.shutdown_processor();
    sp->cleanup();
  } else {
    sp->log(LL_FATAL, "%s: could not initialize event processor", sp->sp_name);
  }
}

///////////////////////////////////////////////////////////////////////////////
// RESPONSE OBJECT CONSUMER
///////////////////////////////////////////////////////////////////////////////

bool ResponseObjectConsumer::write_post_response_writer(SocketResponseWriter* srw) {
  c3_assert(srw && srw->is_active() && !srw->io_completed());
  // do first attempt at writing to ensure minimum response delay
  c3_ulong_t ntotal;
  srw->write(ntotal);
  // even if *all* of response data had been sent by the above call, we still need to return the object to
  // socket pipeline to:
  // - close connection,
  // - maintain count of open connections.
  return post_response_writer(srw);
}

bool ResponseObjectConsumer::post_error_response_impl(SocketResponseWriter* srw,
  const char* format, va_list args) {
  c3_assert(srw && srw->is_active() && format);
  char buffer[1024];
  int result = std::vsnprintf(buffer, sizeof buffer, format, args);
  if (result > 0) {
    ErrorResponseHeaderChunkBuilder header(*srw, server_net_config);
    c3_uint_t length = (c3_uint_t) result;
    if (header.estimate_string(length) > 0) {
      header.configure();
      header.add_string(buffer, length);
      header.check();
      return write_post_response_writer(srw);
    }
  }
  ReaderWriter::dispose(srw);
  c3_assert_failure();
  return false;
}

bool ResponseObjectConsumer::post_data_response_impl(SocketResponseWriter* srw, PayloadHashObject* pho,
  const char* format, va_list args) {
  c3_assert(srw && srw->is_active() && format);

  // create copy of the argument list because we will have to traverse it twice
  va_list args2;
  va_copy(args2, args);

  // initialize header and estimate sizes of data chunks that it will contain
  DataResponseHeaderChunkBuilder header(*srw, server_net_config);
  bool ok = true;
  const char* type = format;
  while (*type != '\0') {
    c3_uint_t size;
    switch (*type) {
      case 'N': // signed number
        size = header.estimate_number(va_arg(args, c3_int_t));
        c3_assert(size);
        break;
      case 'U': // unsigned number
        size = header.estimate_number(va_arg(args, c3_uint_t));
        c3_assert(size);
        break;
      case 'S': // string
        va_arg(args, char*);
        size = header.estimate_string(va_arg(args, c3_uint_t));
        c3_assert(size);
        break;
      case 'C': // C string
        size = header.estimate_cstring(va_arg(args, char*));
        c3_assert(size);
        break;
      case 'L':
        size = header.estimate_list(*va_arg(args, const HeaderListChunkBuilder*));
        c3_assert(size);
        break;
      default:
        c3_assert_failure();
        size = 0;
    }
    if (size > 0) {
      type++;
    } else {
      ok = false;
      break;
    }
  }

  // optionally configure payload
  if (ok) {
    if (pho != nullptr) {
      PayloadChunkBuilder payload(*srw, server_net_config);
      payload.add(pho);
      header.configure(&payload);
    } else {
      header.configure(nullptr);
    }

    // add data chunks to the header
    type = format;
    while (*type != '\0') {
      switch (*type) {
        case 'N': // signed number
          header.add_number(va_arg(args2, c3_int_t));
          break;
        case 'U': // unsigned number
          header.add_number(va_arg(args2, c3_uint_t));
          break;
        case 'S': { // string
          char* str = va_arg(args2, char*);
          c3_uint_t length = va_arg(args2, c3_uint_t);
          header.add_string(str, length);
          break;
        }
        case 'C': // C string
          header.add_cstring(va_arg(args2, char*));
          break;
        case 'L':
          header.add_list(*va_arg(args2, const HeaderListChunkBuilder*));
          break;
        default:
          c3_assert_failure();
      }
      type++;
    }

    // complete header configuration and sent response object back to the socket pipeline
    header.check();
    ok = write_post_response_writer(srw);
  }

  // if we did not succeed in configuring/sending the response object, delete it
  if (!ok) {
    ReaderWriter::dispose(srw);
  }

  // clean up stack
  va_end(args2);
  return ok;
}

SocketResponseWriter* ResponseObjectConsumer::create_response(const CommandReader& cr) {
  Memory& memory = cr.get_memory_object();
  SharedBuffers* sb = SharedBuffers::create(memory);
  auto srw = alloc<SocketResponseWriter>(memory);
  return new (srw) SocketResponseWriter(memory, cr.get_fd(), cr.get_ipv4(), sb);
}

SocketResponseWriter* ResponseObjectConsumer::create_object_response(const CommandReader& cr) {
  Memory& memory = cr.get_memory_object();
  SharedObjectBuffers* sob = SharedObjectBuffers::create_object(memory);
  auto srw = alloc<SocketResponseWriter>(memory);
  return new (srw) SocketResponseWriter(memory, cr.get_fd(), cr.get_ipv4(), sob);
}

/*
 * Certain commands can be loaded from binlogs and then executed, and for such commands responses should
 * not be sent. These commands, and response-sending methods used by them, are:
 *
 * CMD_WRITE (in `ht_session_store.cc`):
 *   post_ok_response(const CommandReader& cr);
 *   post_format_error_response(const CommandReader& cr);
 *   post_internal_error_response(const CommandReader& cr);
 * CMD_DESTROY (in `ht_session_store.cc`):
 *   post_ok_response(const CommandReader& cr);
 *   post_format_error_response(const CommandReader& cr);
 * CMD_GC (in `ht_session_store.cc`):
 *   post_ok_response(const CommandReader& cr);
 *   post_format_error_response(const CommandReader& cr);
 * CMD_SAVE (in `ht_page_store.cc`):
 *   post_ok_response(const CommandReader& cr);
 *   post_format_error_response(const CommandReader& cr);
 *   post_internal_error_response(const CommandReader& cr);
 * CMD_REMOVE (in `ht_page_store.cc`):
 *   post_ok_response(const CommandReader& cr);
 *   post_format_error_response(const CommandReader& cr);
 * CMD_CLEAN (in `ht_tag_manager.cc`):
 *   post_ok_response(const CommandReader& cr);
 *   post_format_error_response(const CommandReader& cr);
 * CMD_TOUCH (in `ht_page_store.cc`):
 *   post_ok_response(const CommandReader& cr);
 *   post_format_error_response(const CommandReader& cr);
 *   post_error_response(const CommandReader& cr, const char* format, ...);
 *
 * Since both `post_format_error_response()` and `post_internal_error_response()` call
 * `post_error_response()` internally, the only two methods that need to check command origin and bail
 * out if it's not "network" are:
 *
 *   post_ok_response(const CommandReader& cr); and
 *   post_error_response(const CommandReader& cr, const char* format, ...);
 */

bool ResponseObjectConsumer::post_ok_response(const CommandReader& cr) {
  if (cr.is_set(IO_FLAG_NETWORK)) {
    return post_ok_response(create_response(cr));
  }
  // must have been loaded from the binlog
  return true;
}

bool ResponseObjectConsumer::post_ok_response(SocketResponseWriter* srw) {
  c3_assert(srw && srw->is_active());
  OkResponseHeaderChunkBuilder ok_header(*srw, server_net_config);
  ok_header.configure();
  ok_header.check();
  return write_post_response_writer(srw);
}

bool ResponseObjectConsumer::post_error_response(const CommandReader& cr, const char* format, ...) {
  va_list args;
  va_start(args, format);
  bool result = false;
  if (cr.is_set(IO_FLAG_NETWORK)) {
    result = post_error_response_impl(create_response(cr), format, args);
  } else {
    // must have been loaded from the binlog
    char buffer[1024];
    int length = std::vsnprintf(buffer, sizeof buffer, format, args);
    if (length > 0) {
      result = log_error_response(buffer, length);
    }
  }
  va_end(args);
  return result;
}

bool ResponseObjectConsumer::post_format_error_response(const CommandReader& cr) {
  return post_error_response(cr, "Command [%02X] has invalid format", cr.get_command_id());
}

bool ResponseObjectConsumer::post_internal_error_response(const CommandReader& cr) {
  return post_error_response(cr, "Internal server error while processing command [%02X]",
    cr.get_command_id());
}

bool ResponseObjectConsumer::post_data_response(const CommandReader& cr, const char* format, ...) {
  va_list args;
  va_start(args, format);
  bool result = post_data_response_impl(create_response(cr), nullptr, format, args);
  va_end(args);
  return result;
}

bool ResponseObjectConsumer::post_data_response(SocketResponseWriter* srw, const char* format, ...) {
  va_list args;
  va_start(args, format);
  bool result = post_data_response_impl(srw, nullptr, format, args);
  va_end(args);
  return result;
}

bool ResponseObjectConsumer::post_data_response(const CommandReader& cr, PayloadHashObject* pho,
  const char* format, ...) {
  va_list args;
  va_start(args, format);
  bool result = post_data_response_impl(create_object_response(cr), pho, format, args);
  va_end(args);
  return result;
}

bool ResponseObjectConsumer::post_list_response(SocketResponseWriter* srw,
  const PayloadListChunkBuilder& list) {
  c3_assert(srw && srw->is_active());
  PayloadChunkBuilder payload(*srw, server_net_config);
  payload.add(list);
  ListResponseHeaderChunkBuilder header(*srw, server_net_config);
  if (header.estimate_number(list.get_count()) > 0) {
    header.configure(&payload);
    header.add_number(list.get_count());
    header.check();
    // send response object back to socket pipeline
    return write_post_response_writer(srw);
  }
  ReaderWriter::dispose(srw);
  c3_assert_failure();
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// SOCKET INPUT PIPELINE
///////////////////////////////////////////////////////////////////////////////

void SocketInputPipeline::process_input_queue_object(ReaderWriter* rw) {
  c3_assert(rw && rw->is_active() && rw->is_set(IO_FLAG_NETWORK) &&
    rw->is_clear(IO_FLAG_IS_READER) && rw->is_set(IO_FLAG_IS_RESPONSE));

  if (!rw->io_completed()) {
    /*
     * First attempt at writing the response object should have been done in the connection thread; if that
     * attempt did not write all the data, we will try again here.
     *
     * If `write()` fails to send all the data, we submit the object to the event processor; all further events
     * will be received in main thread loop as "object events" and will be processed by the
     * `SocketInputPipeline::process_object_event()` method until `write()` returns `OK` result.
     *
     * The object might as well be in error state now, but if that's so, we will simply get immediate error
     * return from write(), and will then handle the error in one place.
     */
    c3_ulong_t ntotal;
    switch (rw->write(ntotal)) {
      case IO_RESULT_OK:
        // completed this time; continue with releasing the object etc.
        break;
      case IO_RESULT_RETRY:
        // still could not write all the data; adding the object to the `epoll` watch list
        sp_event_processor.watch_object(rw);
        return;
      default:
        // got an error! report it and proceed with releasing the object
        log_object(LL_ERROR, rw, "could not send data [Q]");
        goto DISPOSE_THE_OBJECT;
    }
  }

  C3_DEBUG(log(LL_DEBUG, "< SENT response '%s' TO [%d] (queue)",
    c3_get_response_name(((ResponseWriter*)rw)->get_raw_response_type()), rw->get_fd()));

  if (sp_persistent) {
    auto srr = (SocketResponseWriter*) rw;
    PipelineConnectionEvent* pce = PipelineConnectionEvent::convert(srr);
    sp_event_processor.watch_connection(pce);
  } else {
  DISPOSE_THE_OBJECT:
    // the object came from input queue, it hasn't been watched, so no need to "unwatch" it here
    assert(sp_num_connections > 0);
    sp_num_connections--;
    C3_DEBUG(log_object(LL_DEBUG, rw, "closed connection [Q]"));
    c3_close_socket(rw->get_fd());
    ReaderWriter::dispose(rw);
  }
}

SocketInputPipeline::~SocketInputPipeline() {
  cleanup_socket_input_pipeline();
}

void SocketInputPipeline::process_socket_event(const pipeline_event_t& event) {
  const c3_byte_t flags = event.pe_flags;
  const socket_t& socket = event.pe_socket;
  c3_assert(socket.s_fd > 0 && socket.s_ipv4 != INVALID_IPV4_ADDRESS);

  if ((flags & (PEF_ERROR | PEF_HUP)) != 0) {
    /*
     * This is a listening, not a connection socket, so in case of error we report is
     * and continue with the socket.
     */
    if ((flags & PEF_ERROR) != 0) {
      log(LL_ERROR, "%s: socket %d error (remote IP: %s)",
        sp_name, socket.s_fd, c3_ip2address(socket.s_ipv4));
    } else {
      log(LL_WARNING, "%s: HUP event on socket %d (remote IP: %s)",
        sp_name, socket.s_fd, c3_ip2address(socket.s_ipv4));
    }
  } else {
    // we only accept new connections if not quitting or processing configuration change
    if (is_active()) {
      // new connection(s) to the server
      for (;;) {
        c3_ipv4_t ipv4;
        int fd = c3_accept(socket.s_fd, ipv4, C3_SOCK_NON_BLOCKING);
        if (fd > 0) {
          PERF_INCREMENT_COUNTER(Incoming_Connections)

          Memory& memory = get_memory_object();
          auto scr = alloc<SocketCommandReader>(memory);
          auto sob = SharedObjectBuffers::create_object(memory);
          new (scr) SocketCommandReader(memory, fd, ipv4, sob);
          c3_assert(scr->is_active());

          sp_event_processor.watch_object(scr);
          sp_num_connections++;
          C3_DEBUG(log_object(LL_DEBUG, scr, "new connection [S]"));

        } else if (fd < 0) {
          log(LL_ERROR, "%s: could not accept connection on socket %d (remote IP: %s): %s",
            sp_name, socket.s_fd, c3_ip2address(socket.s_ipv4), c3_get_error_message());
          break;
        } else {
          // zero return from accept(): accepted all pending connections
          break;
        }
      }
    }
  }
}

void SocketInputPipeline::process_object_event(const pipeline_event_t& event) {
  const c3_byte_t flags = event.pe_flags;
  ReaderWriter* rw = event.pe_object;
  c3_assert(rw && rw->is_active() && rw->is_set(IO_FLAG_NETWORK));

  if ((flags & PEF_ERROR) != 0) {
    // report the error and proceed with disposing the object
    log_object(LL_ERROR, rw, "connection error");
  } else if ((flags & PEF_HUP) != 0 && (flags & PEF_READ) == 0) {
    /*
     * log warning and proceed with disposing the object; it is important that not only `PEF_HUP` flag
     * was set, but also there are no data to read; otherwise, even if remote peer hung up, there would be
     * some data to read, and we'd process it as any other "read" event; this is generic "correct" behaviour
     * though: we're reading *commands* here, and existing CyberCache clients (PHP extension and console) would
     * never drop connection before receiving *response* from the server.
     */
    log_object(LL_WARNING, rw, "connection dropped");
  } else {
    c3_ulong_t ntotal;
    if ((flags & PEF_READ) != 0) { // "connection ready for reading"
      c3_assert(rw->is_set(IO_FLAG_IS_READER) && rw->is_clear(IO_FLAG_IS_RESPONSE));
      switch (rw->read(ntotal)) {
        case IO_RESULT_OK:
          // completed reading a command; stop watching the object, send it to the output queue and quit
          sp_event_processor.unwatch_object(rw);
          send_output_object(rw);
          return;
        case IO_RESULT_RETRY:
          // could not read all the data; keep the object on `epoll` watch list
          return;
        default:
          // got an error! report it and proceed with disposing the object
          log_object(LL_ERROR, rw, "could not receive data [E]");
      }
    } else { // "connection ready for writing"
      c3_assert(rw->is_clear(IO_FLAG_IS_READER) && rw->is_set(IO_FLAG_IS_RESPONSE));
      switch (rw->write(ntotal)) {
        case IO_RESULT_OK:
          // completed writing response; proceed with disposing the object
          C3_DEBUG(log(LL_DEBUG, "< SENT response '%s' TO [%d] (object)",
            c3_get_response_name(((ResponseWriter*)rw)->get_raw_response_type()), rw->get_fd()));
          if (sp_persistent) {
            auto srw = (SocketResponseWriter*) rw;
            PipelineConnectionEvent* pce = PipelineConnectionEvent::convert(srw);
            c3_assert(pce && pce->is_valid());
            sp_event_processor.replace_watched_object(pce);
            return;
          }
          break;
        case IO_RESULT_RETRY:
          // could not write all the data; keep the object on `epoll` watch list
          return;
        default:
          // got an error! report it and proceed with disposing the object
          log_object(LL_ERROR, rw, "could not send data [E]");
      }
    }
  }

  assert(sp_num_connections > 0);
  sp_num_connections--;
  C3_DEBUG(log_object(LL_DEBUG, rw, "closed connection [E]"));
  sp_event_processor.unwatch_object(rw);
  c3_close_socket(rw->get_fd());
  ReaderWriter::dispose(rw);
}

void SocketInputPipeline::process_connection_event(const pipeline_event_t& event) {
  const c3_byte_t flags = event.pe_flags;
  PipelineConnectionEvent* pce = event.pe_connection;
  c3_assert(pce && pce->is_valid());

  if ((flags & PEF_ERROR) != 0) {
    // report the error and proceed with disposing the connection object
    log_connection(LL_ERROR, pce, "connection error [E]");
  } else if ((flags & PEF_HUP) != 0 && (flags & PEF_READ) == 0) {
    /*
     * not only the peer hung up, but there is also no data to read; otherwise, we'd process
     * this event as a "read" event; here, we have nothing to do, so we just proceed with disposing
     * the connection object
     */
  } else {
    if ((flags & PEF_READ) != 0) { // "connection ready for reading"
      c3_ulong_t ntotal;
      SocketCommandReader* scr = PipelineConnectionEvent::convert(pce);
      c3_assert(scr && scr->is_active() && scr->is_set(IO_FLAG_IS_READER) && scr->is_clear(IO_FLAG_IS_RESPONSE));
      switch (scr->read(ntotal)) {
        case IO_RESULT_OK:
          /*
           * completed reading a command; stop watching the connection (event processor tracks objects by
           * connection handles, so since connection object and socket reader share the same handle, "unwatching"
           * the object even though we were watching connection is OK), send it to the output queue and quit
           */
          sp_event_processor.unwatch_object(scr);
          send_output_object(scr);
          return;
        case IO_RESULT_RETRY:
          // could not read all the data; replace connection object with socket reader on `epoll` watch list
          sp_event_processor.replace_watched_object(scr);
          return;
        default:
          // got an error! report it and dispose the object
          if ((flags & PEF_HUP) != 0 && ntotal == 0) {
            if (!is_using_persistent_connections()) {
              /*
               * if connections *are* persistent, peer will *always* hang up at some point: after receiving
               * server's response, and having no more commands to send; so in such a case we shouldn't report
               * it as a warning
               */
              log_object(LL_WARNING, scr, "peer hung up [CE]");
            }
          } else {
            log_object(LL_ERROR, scr, "could not receive data [CE]");
          }
          c3_assert(sp_num_connections > 0);
          sp_num_connections--;
          C3_DEBUG(log_object(LL_DEBUG, scr, "closed connection [CE]"));
          sp_event_processor.unwatch_object(scr);
          c3_close_socket(scr->get_fd());
          ReaderWriter::dispose(scr);
          return;
      }
    } else { // "connection ready for writing"
      /*
       * this branch is "impossible" for a persistent connection: after socket writer object is converted
       * to a connection object, next event can be `PEF_READ` or `PEF_HUP` (or, if something goes wrong,
       * `PEF_ERROR`), so we just report error and proceed with closing the connection
       */
      log_connection(LL_ERROR, pce, "cannot write [E]");
    }
  }

  c3_assert(sp_num_connections > 0);
  sp_num_connections--;
  C3_DEBUG(log_connection(LL_DEBUG, pce, "closed connection [E]"));
  sp_event_processor.unwatch_connection(pce);
  c3_close_socket(pce->get_fd());
  PipelineConnectionEvent::dispose(pce);
}

void SocketInputPipeline::process_ip_set_change() {
  c3_assert(sp_socket_change && sp_event_processor.get_num_sockets() == 0 && sp_port_change == 0);
  sp_event_processor.create_listening_sockets(get_ip_array(sp_socket_change), get_num_ips(sp_socket_change));
  if (sip_last_ipv4_set != nullptr) {
    InputSocketMessage::dispose(sip_last_ipv4_set);
  }
  sip_last_ipv4_set = sp_socket_change;
  sp_socket_change = nullptr;
}

void SocketInputPipeline::process_port_change() {
  c3_assert(sp_port_change && sp_socket_change == nullptr);
  sp_event_processor.set_port(sp_port_change);
  sp_port_change = 0;
  if (sip_last_ipv4_set != nullptr) {
    sp_event_processor.create_listening_sockets(get_ip_array(sip_last_ipv4_set),
      get_num_ips(sip_last_ipv4_set));
  }
}

void SocketInputPipeline::process_port_and_ip_set_change() {
  c3_assert(sp_port_change && sp_socket_change);
  sp_event_processor.set_port(sp_port_change);
  sp_port_change = 0;
  process_ip_set_change();
}

void SocketInputPipeline::process_persistent_connections_change(bool persistent) {
  /*
   * if we're switching from persistent connections to per-command, there could be connection
   * objects created for persistent connections, but that's OK: they will be processed and
   * disposed in due course
   */
  sp_persistent = persistent;
}

void SocketInputPipeline::process_local_capacity_change(c3_uint_t capacity) {
  c3_assert_failure();
}

void SocketInputPipeline::process_local_max_capacity_change(c3_uint_t max_capacity) {
  c3_assert_failure();
}

void SocketInputPipeline::reset_event_processor() {
  sp_event_processor.dispose_listening_sockets();
}

void SocketInputPipeline::cleanup() {
  cleanup_socket_input_pipeline();
  // cleanup_socket_pipeline();
}

void SocketInputPipeline::cleanup_socket_input_pipeline() {
  if (sip_last_ipv4_set != nullptr) {
    InputSocketMessage::dispose(sip_last_ipv4_set);
    sip_last_ipv4_set = nullptr;
  }
}

bool SocketInputPipeline::post_processors_quit_command() {
  return send_output_command(SOC_QUIT);
}

bool SocketInputPipeline::post_command_reader(CommandReader* cr) {
  return send_output_object(cr);
}

bool SocketInputPipeline::post_response_writer(ResponseWriter* rw) {
  return send_input_object(rw);
}

bool SocketInputPipeline::log_error_response(const char* message, int length) {
  return log_message(LL_ERROR, message, length);
}

///////////////////////////////////////////////////////////////////////////////
// SOCKET OUTPUT PIPELINE
///////////////////////////////////////////////////////////////////////////////

void SocketOutputPipeline::process_input_queue_object(ReaderWriter* rw) {
  c3_assert(rw && rw->is_valid() && rw->is_set(IO_FLAG_NETWORK) &&
    rw->is_clear(IO_FLAG_IS_READER) && rw->is_clear(IO_FLAG_IS_RESPONSE));

  if (sp_persistent && sp_num_connections > 0) {
    /*
     * In socket output pipelines, number of active connections has different meaning: not that
     * a connection had been established, but that a command or a response to a command is still
     * being transferred to or from a replication server.
     *
     * If we are in "persistent connections" mode, we cannot start transferring a new command until
     * sending previous command (*and* receiving response to it) is complete -- because connection
     * sockets are re-used. So we queue the command, and wait till transfer is done.
     */
    if (!sop_deferred_objects.put(ReaderWriterPointer(rw))) {
      log_object(LL_ERROR, rw, "could not defer writing [Q]");
    }
    PERF_INCREMENT_VAR_DOMAIN_COUNTER(get_domain(), Replicator_Deferred_Commands);
    PERF_UPDATE_VAR_DOMAIN_MAXIMUM(get_domain(), Replicator_Max_Deferred_Commands, sop_deferred_objects.get_count());
    return;
  }

  Memory& memory = get_memory_object();
  for (c3_uint_t i = 0; i < sp_event_processor.get_num_sockets(); i++) {
    for (c3_uint_t j = 0; j < 2; j++) {
      c3_ipv4_t ipv4;
      int fd = sp_event_processor.create_connection_socket(i, ipv4, sp_persistent);
      if (fd > 0) {
        c3_assert(rw);
        rw->io_rewind(fd, ipv4);
        c3_ulong_t ntotal;
        switch (rw->write(ntotal)) {
          case IO_RESULT_OK: { // completed writing in one go
            auto new_reader = alloc<SocketResponseReader>(memory);
            auto sob = SharedObjectBuffers::create_object(memory);
            new (new_reader) SocketResponseReader(memory, fd, ipv4, sob);
            C3_DEBUG(log_object(LL_DEBUG, new_reader, "new connection [Q]"));
            sp_event_processor.watch_object(new_reader);
            sp_num_connections++;
            break;
          }
          case IO_RESULT_RETRY: { // could not complete, must retry later
            SocketCommandWriter* new_writer;
            if (i + 1 < sp_event_processor.get_num_sockets()) {
              // there will be more iterations with different sockets, so clone current object
              new_writer = alloc<SocketCommandWriter>(memory);
              new (new_writer) SocketCommandWriter(memory, *rw, 0);
            } else {
              new_writer = nullptr;
            }
            C3_DEBUG(log_object(LL_DEBUG, rw, "new connection [Q]"));
            sp_event_processor.watch_object(rw);
            sp_num_connections++;
            rw = new_writer;
            break;
          }
          default:
            if (sp_persistent && j == 0) {
              /*
               * this was the first try; writing object to the socket did not succeed because
               * either a) remote peer (replication server) does not use persistent connections,
               * or b) connection was not kept alive for too long, and got dropped; after the
               * first failure, we just silently try to re-connect
               */
              sp_event_processor.close_connection_socket(i);
              PERF_INCREMENT_VAR_DOMAIN_COUNTER(get_domain(), Replicator_Reconnections);
              continue;
            } else {
              log_object(LL_ERROR, rw, "could not send data [Q]");
              sp_event_processor.close_connection_socket(i);
            }
        }
      } else {
        // failed call must have set IP address, it's part of its contract
        log(LL_ERROR, "%s: could not connect to %s to send a command",
            sp_name, c3_ip2address(ipv4));
      }
    }
    break;
  }
  if (rw != nullptr) {
    ReaderWriter::dispose(rw);
  }
}

void SocketOutputPipeline::process_socket_event(const pipeline_event_t& event) {
  // output pipelines do not listen to/watch sockets, so this type of event is impossible
  c3_assert_failure();
}

void SocketOutputPipeline::process_object_event(const pipeline_event_t& event) {
  const c3_byte_t flags = event.pe_flags;
  ReaderWriter* rw = event.pe_object;
  c3_assert(rw && rw->is_active() && rw->is_set(IO_FLAG_NETWORK));

  bool close_connection = true;
  if ((flags & PEF_ERROR) != 0) {
    // log the error, and proceed with disposing the object
    log_object(LL_ERROR, rw, "connection error");
  } else if ((flags & PEF_HUP) != 0 && (flags & PEF_READ) == 0) {
    /*
     * log the warning, and proceed with disposing the object; here, we make sure that not only remote peer
     * hung up, but that there are also no data to read; otherwise (in case there was dropper connection *and*
     * still some data to read), we'd process it as a regular "read" event; we're reading responses here, and
     * it would be normal for a replication server (peer) to disconnect right after sending the response
     */
    log_object(LL_WARNING, rw, "connection dropped");
  } else {
    c3_ulong_t ntotal;
    if ((flags & PEF_READ) != 0) { // "connection ready for reading"
      c3_assert(rw->is_set(IO_FLAG_IS_READER) && rw->is_set(IO_FLAG_IS_RESPONSE));
      switch (rw->read(ntotal)) {
        case IO_RESULT_OK: {
          // only close connection if we're not in persistent connections mode, or remote peer hung up
          if (sp_persistent && (flags & PEF_HUP) == 0) {
            close_connection = false;
          }
          // completed reading the response; if there was an error, log it
          response_type_t response_type = ((SocketResponseReader*) rw)->get_type();
          switch (response_type) {
            case RESPONSE_OK:
              break;
            case RESPONSE_ERROR:
              log_object(LL_ERROR, rw, "received ERROR response");
              break;
            default: // "can't happen" (we don't replicate commands that would send other responses)
              c3_assert_failure();
          }
          break;
        }
        case IO_RESULT_RETRY:
          // could not read all the data; keep the object on `epoll` watch list
          return;
        default:
          // got an error! report it and proceed with disposing the object
          log_object(LL_ERROR, rw, "could not receive data [E]");
      }
    } else { // "connection ready for writing"
      c3_assert(rw->is_clear(IO_FLAG_IS_READER) && rw->is_clear(IO_FLAG_IS_RESPONSE));
      switch (rw->write(ntotal)) {
        case IO_RESULT_OK: {
          // completed writing command; create response object, dispose command object
          Memory& memory = get_memory_object();
          auto rr = alloc<SocketResponseReader>(memory);
          auto sob = SharedObjectBuffers::create_object(memory);
          new (rr) SocketResponseReader(memory, rw->get_fd(), rw->get_ipv4(), sob);
          c3_assert(rw->is_active());
          sp_event_processor.replace_watched_object(rr);
          c3_assert(rr->get_fd() == rw->get_fd());
          ReaderWriter::dispose(rw);
          return;
        }
        case IO_RESULT_RETRY:
          // could not write all the data; keep the object on `epoll` watch list
          return;
        default:
          // got an error! report it and proceed with disposing the object
          log_object(LL_ERROR, rw, "could not send data [E]");
      }
    }
  }

  assert(sp_num_connections > 0);
  sp_num_connections--;
  C3_DEBUG(log_object(LL_DEBUG, rw, "closed connection [E]"));
  sp_event_processor.unwatch_object(rw);
  if (close_connection) {
    sp_event_processor.close_connection_socket_by_fd(rw->get_fd());
  }
  ReaderWriter::dispose(rw);

  /*
   * if there are deferred objects, process one of them; we do not check whether we're in "persistent
   * connections" mode because the mode could have been changed to 'OFF' after an object was queued; we
   * do not check if connection had just been closed either because if could have been closed because
   * of socket error, or because remote peer is not using persistent connections and just hung up
   */
  ReaderWriterPointer rwp = sop_deferred_objects.get();
  if (rwp.is_valid()) {
    process_input_queue_object(rwp.get());
  }
}

void SocketOutputPipeline::process_connection_event(const pipeline_event_t& event) {
  /*
   * output pipelines only watch `ReaderWriter` objects (for persistent connections, handles are cached
   * in separate arrays, not in `PipelineConnectionEvent` objects), so this type of event is impossible
   */
  c3_assert_failure();
}

void SocketOutputPipeline::process_ip_set_change() {
  c3_assert(sp_socket_change && sp_event_processor.get_num_sockets() == 0);
  sp_event_processor.set_connection_sockets_info(get_ip_array(sp_socket_change), get_num_ips(sp_socket_change));
  InputSocketMessage::dispose(sp_socket_change);
  sp_socket_change = nullptr;
}

void SocketOutputPipeline::process_port_change() {
  c3_assert(sp_port_change && sp_event_processor.get_num_sockets() == 0);
  sp_event_processor.set_port(sp_port_change);
  sp_port_change = 0;
}

void SocketOutputPipeline::process_port_and_ip_set_change() {
  process_port_change();
  process_ip_set_change();
}

void SocketOutputPipeline::process_persistent_connections_change(bool persistent) {
  if (!(sp_persistent = persistent)) {
    sp_event_processor.close_connection_sockets();
  }
}

void SocketOutputPipeline::process_local_capacity_change(c3_uint_t capacity) {
  assert(capacity);
  c3_uint_t set_capacity = sop_deferred_objects.set_capacity(capacity);
  log(LL_VERBOSE, "%s: replication queue capacity set to %u (requested %u)",
      sp_name, set_capacity, capacity);
}

void SocketOutputPipeline::process_local_max_capacity_change(c3_uint_t max_capacity) {
  assert(max_capacity);
  c3_uint_t set_capacity = sop_deferred_objects.set_max_capacity(max_capacity);
  log(LL_VERBOSE, "%s: replication queue max capacity set to %u (requested %u)",
      sp_name, set_capacity, max_capacity);
}

void SocketOutputPipeline::reset_event_processor() {
  sp_event_processor.dispose_connection_sockets();
}

}
