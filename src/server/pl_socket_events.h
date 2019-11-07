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
 * I/O pipelines: classes implementing epoll-based messaging and notifications.
 */
#ifndef _PL_SOCKET_EVENTS_H
#define _PL_SOCKET_EVENTS_H

#include "c3lib/c3lib.h"
#include "mt_events.h"

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// PIPELINE EVENTS
///////////////////////////////////////////////////////////////////////////////

/// What kind of pointer a triggered event contains (what `epoll_data.ptr` points to)
enum pipeline_event_type_t {
  PET_NONE = 0,  // an invalid event / no event
  PET_QUEUE,     // a queue event: a new message had been posted to the message queue
  PET_SOCKET,    // a socket event: a new incoming connection to accept
  PET_OBJECT,    // an object's [inbound] connection / [outbound] socket became ready for reading/writing
  PET_CONNECTION // an already established inbound connection for which there's no "reader" object yet
};

/**
 * Base class for all events that can be pointed to by `epoll_data.ptr`.
 *
 * In order to maximize performance, we poll all events using single `epoll` instance: listening sockets (for
 * new connections), objects (for multi-part reading and writing), even message queues (for configuration change
 * commands and quit requests). `epoll` events that we use keep only generic pointers, so when an event occurs, we
 * need to a) figure out type of the event, and b) get extra data associated with the event; this class and its
 * descendants serve these two purposes.
 */
class PipelineEvent {
  const void* const           pe_null; // always `NULL`; this is where `ReaderWriter`s vtable resides
  const pipeline_event_type_t pe_type; // type of the event, a `PET_xxx` constant

protected:
  pipeline_event_type_t get_internal_type() const { return pe_type; }
  explicit PipelineEvent(pipeline_event_type_t type): pe_null(nullptr), pe_type(type) {}

public:
  pipeline_event_type_t get_type() const {
    if (pe_null != nullptr) {
      return PET_OBJECT;
    } else {
      return pe_type;
    }
  }

  static pipeline_event_type_t get_type(const void* p) {
    if (p != nullptr) {
      return ((const PipelineEvent*) p)->get_type();
    } else {
      return PET_NONE;
    }
  }
};

/// Event object implementing notifications on queue put()s; can be pointed to by `epoll_data.ptr`
class PipelineQueueEvent: public PipelineEvent {
  Event pqe_event; // queue event

public:
  PipelineQueueEvent(): PipelineEvent(PET_QUEUE) {}

  bool initialize() { return pqe_event.initialize(); }
  int get_event_fd() const { return pqe_event.get_event_fd(); }
  void trigger() const { pqe_event.trigger(); }
  void consume() const { pqe_event.consume(); }
  // no need to call this in dtor: contained object's dtor will be called anyway
  void dispose() { pqe_event.dispose(); }
};

/// Internal representation of sockets
struct socket_t {
  c3_ipv4_t s_ipv4; // IP address of the socket
  int       s_fd;   // socket handle, or -1 if the socket is closed
};

/**
 * Event object implementing notifications on incoming connections; can be put into a `Vector`, and can
 * be pointed to by `epoll_data.ptr`.
 *
 * It should always be created in "valid" state (i.e. with positive socket handle); even though there is
 * default ctor initializing to invalid state, as well as copy ctor and operator that do not check
 * validity of the handle, these are here only to meet `Vector` requirements.
 */
class PipelineSocketEvent: public PipelineEvent {
  socket_t pse_socket; // socket IP address and descriptor

public:
  PipelineSocketEvent(): PipelineEvent(PET_SOCKET) {
    pse_socket.s_ipv4 = INVALID_IPV4_ADDRESS;
    pse_socket.s_fd = -1;
  }
  PipelineSocketEvent(c3_ipv4_t ipv4, int fd): PipelineEvent(PET_SOCKET) {
    assert(fd > 0);
    pse_socket.s_ipv4 = ipv4;
    pse_socket.s_fd = fd;
  }
  explicit PipelineSocketEvent(const socket_t socket): PipelineEvent(PET_SOCKET) {
    c3_assert(socket.s_fd > 0);
    pse_socket.s_ipv4 = socket.s_ipv4;
    pse_socket.s_fd = socket.s_fd;
  }
  PipelineSocketEvent(const PipelineSocketEvent&) = delete;
  PipelineSocketEvent(PipelineSocketEvent&& se) noexcept: PipelineEvent(PET_SOCKET) {
    pse_socket = se.pse_socket;
    se.pse_socket.s_ipv4 = INVALID_IPV4_ADDRESS;
    se.pse_socket.s_fd = -1;
  }
  ~PipelineSocketEvent() { dispose(); }

  PipelineSocketEvent& operator=(const PipelineSocketEvent&) = delete;
  PipelineSocketEvent& operator=(PipelineSocketEvent&& se) noexcept {
    std::swap(pse_socket, se.pse_socket);
    return *this;
  }

  c3_ipv4_t get_ip() const { return pse_socket.s_ipv4; }
  int get_fd() const { return pse_socket.s_fd; }
  void set_fd(int fd) { pse_socket.s_fd = fd; }
  const socket_t& get_socket() const { return pse_socket; }
  void set_socket(int fd, c3_ipv4_t ipv4) {
    pse_socket.s_fd = fd;
    pse_socket.s_ipv4 = ipv4;
  }

  void close() {
    if (pse_socket.s_fd > 0) {
      c3_close_socket(pse_socket.s_fd);
      pse_socket.s_fd = -1;
    }
  }
  void dispose() {
    close();
    pse_socket.s_ipv4 = INVALID_IPV4_ADDRESS;
  }
};

/**
 * Event object that holds all the information that is necessary to create `SocketCommandReader`
 * instance in case remote peer continues sending commands.
 *
 * The purpose of this object is to support persistent connections: after response is sent, we do not know
 * whether remote peer will send another command, or hang up. So we convert last used `SocketResponseWriter`
 * to an instance of this class, and start watching for incoming read or hangup events on its connection socket.
 */
class PipelineConnectionEvent: public PipelineEvent {
  Memory&         pce_memory;      // `Memory` object used for allocations
  const int       pce_fd;          // socket handle of the open inbound connection
  const c3_ipv4_t pce_ipv4;        // IP address of the open inbound connection
  char            pce_padding[16]; // padding to the size of `SocketCommandReader`/`SocketResponseWriter`

  PipelineConnectionEvent(Memory& memory, int fd, c3_ipv4_t ipv4): PipelineEvent(PET_CONNECTION),
    pce_memory(memory), pce_fd(fd), pce_ipv4(ipv4) {
  }

public:
  bool is_valid() const {
    return pce_fd > 0 && pce_ipv4 != INVALID_IPV4_ADDRESS;
  }
  int get_fd() const { return pce_fd; }
  const char* get_address() const { return c3_ip2address(pce_ipv4); }

  static PipelineConnectionEvent* convert(SocketResponseWriter* srw);
  static SocketCommandReader* convert(PipelineConnectionEvent* pce);
  static void dispose(PipelineConnectionEvent* pce) {
    pce->pce_memory.free(pce, sizeof(PipelineConnectionEvent));
  }
};

///////////////////////////////////////////////////////////////////////////////
// SOCKET EVENT PROCESSOR
///////////////////////////////////////////////////////////////////////////////

// Event properties that can be reported by event processor
constexpr c3_byte_t PEF_NONE  = 0x00; // no/invalid event
constexpr c3_byte_t PEF_READ  = 0x01; // input event occurred
constexpr c3_byte_t PEF_WRITE = 0x02; // output event occurred
constexpr c3_byte_t PEF_HUP   = 0x04; // peer hung up; I/O may still be possible, but no more events of the same type
constexpr c3_byte_t PEF_ERROR = 0x08; // an error occurred, connection should be closed

/// Data associated with an event
struct pipeline_event_t {
  union {
    ReaderWriter*            pe_object;     // pointer to `Socket[Command/Response][Reader/Writer]`
    socket_t                 pe_socket;     // socket IP address and handle
    PipelineConnectionEvent* pe_connection; // persistent connection data convertible to `SocketCommandReader`
  };
  c3_byte_t       pe_flags;  // a combination of `PEF_xxx` constants
};

/**
 * Class that implements asynchronous socket I/O.
 *
 * *IMPORTANT*: even though IP set this class listens or connects to, as well as port number, can be
 * changed at any time, doing so while there are active connection may lead to severe errors, because its
 * methods do not do any checks to ensure such changes are "safe" at the time respective calls are made.
 * Therefore, users of this class are responsible for making such checks themselves, and requesting IP set
 * and/or port changes when there are no active connections *only*.
 */
class SocketEventProcessor {
  static constexpr c3_uint_t MAX_EPOLL_EVENTS = 256; // num of events pulled in one go (matches 'poll()' limit)

  epoll_event        sp_epoll_events[MAX_EPOLL_EVENTS]; // events filled by epoll_wait()
  PipelineQueueEvent sp_queue_event;   // event watching for queue updates
  FixedVector<PipelineSocketEvent, MAX_IPS_PER_SERVICE> sp_socket_events; // array of currently active sockets
  const char* const  sp_service_name;  // name of the service using this processor
  AbstractLogger&    sp_logger;        // logger interface
  int                sp_epoll;         // `epoll` handle
  c3_uint_t          sp_num_events;    // number of events in `epoll` event buffer
  c3_uint_t          sp_next_event;    // index of the next event to be retrieved from event buffer
  c3_ushort_t        sp_port;          // port number to listen or connect to

public:
  C3_FUNC_COLD SocketEventProcessor(const char* service_name, AbstractLogger& logger, c3_ushort_t port):
    sp_service_name(service_name), sp_logger(logger) {
    sp_epoll = -1;
    sp_num_events = 0;
    sp_next_event = 0;
    sp_port = port;
  }
  SocketEventProcessor(const SocketEventProcessor&) = delete;
  SocketEventProcessor(SocketEventProcessor&&) = delete;
  ~SocketEventProcessor() C3_FUNC_COLD { shutdown_processor(); }

  SocketEventProcessor& operator=(const SocketEventProcessor&) = delete;
  SocketEventProcessor& operator=(SocketEventProcessor&&) = delete;

  bool initialize_processor() C3_FUNC_COLD;
  bool is_initialized() const { return sp_epoll > 0 && sp_queue_event.get_event_fd() > 0; }
  void trigger_queue_event() const { sp_queue_event.trigger(); }
  void consume_queue_event() const { sp_queue_event.consume(); }

  c3_uint_t get_num_events() const { return sp_num_events; }
  size_t get_num_sockets() const { return sp_socket_events.get_count(); }

  void create_listening_sockets(const c3_ipv4_t* ips, c3_uint_t num) C3_FUNC_COLD;
  void dispose_listening_sockets() C3_FUNC_COLD;

  void set_port(c3_ushort_t port) C3_FUNC_COLD;
  bool set_connection_sockets_info(const c3_ipv4_t* ips, c3_uint_t num) C3_FUNC_COLD;
  int create_connection_socket(c3_uint_t i, c3_ipv4_t &ipv4, bool persistent);
  void close_connection_socket(c3_uint_t i) { sp_socket_events.get(i).close(); }
  void close_connection_socket_by_fd(int fd);
  void close_connection_sockets() C3_FUNC_COLD;
  void dispose_connection_sockets() C3_FUNC_COLD;

  void watch_object(ReaderWriter* rw) const;
  void watch_connection(PipelineConnectionEvent* pce) const;
  void replace_watched_object(ReaderWriter* rw) const;
  void replace_watched_object(PipelineConnectionEvent* pce) const;
  void unwatch_object(const ReaderWriter* rw) const;
  void unwatch_connection(PipelineConnectionEvent* pce) const;

  void wait_for_events();
  pipeline_event_type_t get_next_event(pipeline_event_t& pe);
  void shutdown_processor() C3_FUNC_COLD;
};

}

#endif // _PL_SOCKET_EVENTS_H
