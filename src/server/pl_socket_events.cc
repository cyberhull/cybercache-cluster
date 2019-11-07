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
#include "ht_shared_buffers.h"
#include "pl_socket_events.h"

#include <cerrno>
#include <new>

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// PipelineConnectionEvent
///////////////////////////////////////////////////////////////////////////////

static_assert(sizeof(PipelineConnectionEvent) == sizeof(SocketCommandReader),
  "Sizes of 'PipelineConnectionEvent' and 'SocketCommandReader' do not match");
static_assert(sizeof(PipelineConnectionEvent) == sizeof(SocketResponseWriter),
  "Sizes of 'PipelineConnectionEvent' and 'SocketResponseWriter' do not match");

PipelineConnectionEvent* PipelineConnectionEvent::convert(SocketResponseWriter* srw) {
  assert(srw && srw->is_active() && srw->is_set(IO_FLAG_IS_RESPONSE) && srw->is_clear(IO_FLAG_IS_READER));
  Memory& memory = srw->get_memory_object();
  int fd = srw->get_fd();
  c3_ipv4_t ipv4 = srw->get_ipv4();
  srw->~SocketResponseWriter(); // this removes reference to shared buffers, etc.
  return new (srw) PipelineConnectionEvent(memory, fd, ipv4);
}

SocketCommandReader* PipelineConnectionEvent::convert(PipelineConnectionEvent* pce) {
  assert(pce && pce->get_internal_type() == PET_CONNECTION);
  Memory& memory = pce->pce_memory;
  int fd = pce->pce_fd;
  c3_ipv4_t ipv4 = pce->pce_ipv4;
  auto sob = SharedObjectBuffers::create_object(memory);
  return new (pce) SocketCommandReader(memory, fd, ipv4, sob);
}

///////////////////////////////////////////////////////////////////////////////
// SocketEventProcessor
///////////////////////////////////////////////////////////////////////////////

bool SocketEventProcessor::initialize_processor() {
  c3_assert(sp_epoll == -1 && sp_queue_event.get_event_fd() == -1);

  sp_epoll = epoll_create(1);
  if (sp_epoll > 0 && sp_queue_event.initialize()) {
    epoll_event event;
    event.data.ptr = &sp_queue_event;
    event.events = uint32_t(EPOLLIN | EPOLLET);
    if (epoll_ctl(sp_epoll, EPOLL_CTL_ADD, sp_queue_event.get_event_fd(), &event) == 0) {
      C3_DEBUG(sp_logger.log(LL_DEBUG, "%s: initialized event processor", sp_service_name));
      return true;
    }
  }
  return false;
}

void SocketEventProcessor::create_listening_sockets(const c3_ipv4_t* ips, c3_uint_t num) {
  assert(ips && num);
  c3_assert(sp_socket_events.get_count() == 0);
  for (c3_uint_t i = 0; i < num; i++) {
    sp_socket_events.push(PipelineSocketEvent());
    PipelineSocketEvent& pse = sp_socket_events.get(i);
    c3_ipv4_t ipv4 = ips[i];
    int fd = c3_socket(C3_SOCK_NON_BLOCKING | C3_SOCK_REUSE_ADDR);
    if (fd > 0) {
      bool result = false;
      if (c3_bind(fd, ipv4, sp_port) == 0) {
        if (c3_listen(fd) == 0) {
          epoll_event event;
          event.data.ptr = &pse;
          event.events = uint32_t(EPOLLIN | EPOLLET | EPOLLRDHUP);
          if (epoll_ctl(sp_epoll, EPOLL_CTL_ADD, fd, &event) == 0) {
            pse.set_socket(fd, ipv4);
            sp_logger.log(LL_NORMAL, "%s: listening to %s [%u]",
              sp_service_name, c3_ip2address(ipv4), sp_port);
            result = true;
          } else {
            sp_logger.log(LL_ERROR, "%s: could not create listening event for %s [%u]",
              sp_service_name, c3_ip2address(ipv4), sp_port);
          }
        } else {
          sp_logger.log(LL_ERROR, "%s: could not start listing to %s [%u]",
            sp_service_name, c3_ip2address(ipv4), sp_port);
        }
      } else {
        sp_logger.log(LL_ERROR, "%s: could not bind to %s [%u]",
          sp_service_name, c3_ip2address(ipv4), sp_port);
      }
      if (!result) {
        c3_close_socket(fd);
      }
    } else {
      sp_logger.log(LL_ERROR, "%s: could not create socket for %s [%u]",
        sp_service_name, c3_ip2address(ipv4), sp_port);
    }
  }
}

void SocketEventProcessor::dispose_listening_sockets() {
  for (size_t i = 0; i < sp_socket_events.get_count(); i++) {
    const PipelineSocketEvent& socket_event = sp_socket_events[i];
    int fd = socket_event.get_fd();
    if (fd > 0) { // could have been closed individually (say, upon HUP event)
      epoll_event event; // just to satisfy Linux kernels older than 2.6.9
      epoll_ctl(sp_epoll, EPOLL_CTL_DEL, fd, &event);
      c3_close_socket(fd);
      sp_logger.log(LL_VERBOSE, "%s: closed socket for %s",
        sp_service_name, c3_ip2address(socket_event.get_socket().s_ipv4));
    }
  }
  sp_socket_events.clear();
}

void SocketEventProcessor::set_port(c3_ushort_t port) {
  c3_assert(port && port >= 1024);
  sp_port = port;
  sp_logger.log(LL_VERBOSE, "%s: will connect to port %u", sp_service_name, port);
}

bool SocketEventProcessor::set_connection_sockets_info(const c3_ipv4_t* ips, c3_uint_t num) {
  assert(ips && num);
  c3_assert(sp_socket_events.get_count() == 0);
  for (c3_uint_t i = 0; i < num; i++) {
    sp_logger.log(LL_VERBOSE, "%s: will connect to IP %s",
      sp_service_name, c3_ip2address(ips[i]));
    sp_socket_events.push(PipelineSocketEvent(ips[i], -1));
  }
  return true;
}

int SocketEventProcessor::create_connection_socket(c3_uint_t i, c3_ipv4_t &ipv4, bool persistent) {
  PipelineSocketEvent& event = sp_socket_events.get(i);
  // this method *must* set ipv4 even in case of error
  ipv4 = event.get_ip();
  int fd = event.get_fd();
  if (fd > 0) {
    if (persistent) {
      return fd;
    } else {
      // must have switched off persistent connections recently
      event.close();
    }
  }
  fd = c3_socket(C3_SOCK_NON_BLOCKING);
  if (fd > 0) {
    if (c3_connect(fd, ipv4, sp_port) != 0) {
      c3_close_socket(fd);
    } else {
      if (persistent) {
        event.set_fd(fd);
      }
      C3_DEBUG(sp_logger.log(LL_DEBUG, "%s: connected to %s [%u]", sp_service_name, c3_ip2address(ipv4), sp_port));
      return fd;
    }
  }
  return -1;
}

void SocketEventProcessor::close_connection_socket_by_fd(int fd) {
  for (c3_uint_t i = 0; i < sp_socket_events.get_count(); i++) {
    PipelineSocketEvent& event = sp_socket_events.get(i);
    if (event.get_fd() == fd) {
      event.close();
      break;
    }
  }
}

void SocketEventProcessor::close_connection_sockets() {
  for (c3_uint_t i = 0; i < sp_socket_events.get_count(); i++) {
    sp_socket_events.get(i).close();
  }
}

void SocketEventProcessor::dispose_connection_sockets() {
  close_connection_sockets();
  sp_socket_events.clear();
}

void SocketEventProcessor::watch_object(ReaderWriter* rw) const {
  assert(rw && rw->is_active());
  c3_assert(sp_epoll > 0);
  epoll_event event;
  event.data.ptr = rw;
  event.events = uint32_t(rw->is_set(IO_FLAG_IS_READER)? EPOLLIN | EPOLLET | EPOLLRDHUP: EPOLLOUT | EPOLLET);
  c3_assert_def(int result) epoll_ctl(sp_epoll, EPOLL_CTL_ADD, rw->get_fd(), &event);
  c3_assert(result == 0);
}

void SocketEventProcessor::watch_connection(PipelineConnectionEvent* pce) const {
  assert(pce && pce->is_valid());
  c3_assert(sp_epoll > 0);
  epoll_event event;
  event.data.ptr = pce;
  event.events = EPOLLIN | EPOLLET | EPOLLRDHUP; // EPOLLHUP is always watched even if not specified
  c3_assert_def(int result) epoll_ctl(sp_epoll, EPOLL_CTL_ADD, pce->get_fd(), &event);
  c3_assert(result == 0);
}

void SocketEventProcessor::replace_watched_object(ReaderWriter* rw) const {
  assert(rw && rw->is_active());
  c3_assert(sp_epoll > 0);
  epoll_event event;
  event.data.ptr = rw;
  event.events = uint32_t(rw->is_set(IO_FLAG_IS_READER)? EPOLLIN | EPOLLET | EPOLLRDHUP: EPOLLOUT | EPOLLET);
  // epoll does not allow to add same descriptor to the watch list twice, so we have to "replace" it
  c3_assert_def(int result) epoll_ctl(sp_epoll, EPOLL_CTL_MOD, rw->get_fd(), &event);
  c3_assert(result == 0);
}

void SocketEventProcessor::replace_watched_object(PipelineConnectionEvent* pce) const {
  assert(pce && pce->is_valid());
  c3_assert(sp_epoll > 0);
  epoll_event event;
  event.data.ptr = pce;
  event.events = EPOLLIN | EPOLLET | EPOLLRDHUP; // EPOLLHUP is always watched even if not specified
  // epoll does not allow to add same descriptor to the watch list twice, so we have to "replace" it
  c3_assert_def(int result) epoll_ctl(sp_epoll, EPOLL_CTL_MOD, pce->get_fd(), &event);
  c3_assert(result == 0);
}

void SocketEventProcessor::unwatch_object(const ReaderWriter* rw) const {
  assert(rw != nullptr && rw->is_active());
  c3_assert(sp_epoll > 0);
  epoll_event event; // need this to satisfy Linux kernels older than 2.6.9
  c3_assert_def(int result) epoll_ctl(sp_epoll, EPOLL_CTL_DEL, rw->get_fd(), &event);
  c3_assert(result == 0);
}

void SocketEventProcessor::unwatch_connection(PipelineConnectionEvent* pce) const {
  assert(pce && pce->is_valid());
  c3_assert(sp_epoll > 0);
  epoll_event event; // need this to satisfy Linux kernels older than 2.6.9
  c3_assert_def(int result) epoll_ctl(sp_epoll, EPOLL_CTL_DEL, pce->get_fd(), &event);
  c3_assert(result == 0);
}

void SocketEventProcessor::wait_for_events() {
  c3_assert(sp_epoll > 0);
  sp_next_event = 0;
  int result;
  do {
    // if `epoll_wait()` fails upon, say, attaching `gdbserver`, just try again...
    result = epoll_wait(sp_epoll, sp_epoll_events, MAX_EPOLL_EVENTS, -1 /* wait indefinitely */);
  } while (result == -1 && errno == EINTR);

  if (result >= 0) {
    sp_num_events = (c3_uint_t) result;
  } else {
    c3_set_stdlib_error_message();
    c3_assert_failure();
    sp_num_events = 0;
  }
}

pipeline_event_type_t SocketEventProcessor::get_next_event(pipeline_event_t& pe) {
  if (sp_next_event < sp_num_events) {

    // 1) figure out event type
    const epoll_event& event = sp_epoll_events[sp_next_event++];
    const void* data = event.data.ptr;
    pipeline_event_type_t type = PipelineEvent::get_type(data);

    // 2) get associated event data
    switch (type) {
      case PET_SOCKET:
        pe.pe_socket = ((PipelineSocketEvent*) data)->get_socket();
        break;
      case PET_OBJECT:
        pe.pe_object = (ReaderWriter*) data;
        break;
      case PET_CONNECTION:
        pe.pe_connection = (PipelineConnectionEvent*) data;
        break;
      default: // must be `PET_QUEUE`
        c3_assert(type == PET_QUEUE);
        #ifdef C3_SAFE
        pe.pe_object = nullptr
        #endif
        ;
    }

    // 3) get event flags
    pe.pe_flags = PEF_NONE;
    if ((event.events & EPOLLIN) != 0) {
      pe.pe_flags |= PEF_READ;
    }
    if ((event.events & EPOLLOUT) != 0) {
      pe.pe_flags |= PEF_WRITE;
    }
    if ((event.events & (EPOLLHUP | EPOLLRDHUP)) != 0) {
      pe.pe_flags |= PEF_HUP;
    }
    if ((event.events & EPOLLERR) != 0) {
      pe.pe_flags |= PEF_ERROR;
    }
    return type;
  }
  return PET_NONE;
}

void SocketEventProcessor::shutdown_processor() {
  // this has no effect if socket fds are -1, which is the case for connection sockets
  dispose_listening_sockets();

  int queue_event_fd = sp_queue_event.get_event_fd();
  if (queue_event_fd > 0) {
    epoll_event event; // just to satisfy Linux kernels older than 2.6.9
    epoll_ctl(sp_epoll, EPOLL_CTL_DEL, queue_event_fd, &event);
    sp_queue_event.dispose();
  }
  if (sp_epoll > 0) {
    epoll_close(sp_epoll);
    sp_epoll = -1;
  }
}

}
