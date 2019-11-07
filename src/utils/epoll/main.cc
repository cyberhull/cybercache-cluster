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
#include "c3lib/c3lib.h"

#include <cstdarg>
#include <cstdio>
#include <cerrno>
#include <sys/socket.h>

using namespace CyberCache;

static void fail(const char* format, ...) {
  char buffer[2048];
  va_list args;
  va_start(args, format);
  c3_assert_def(int length) std::vsnprintf(buffer, sizeof buffer, format, args);
  va_end(args);
  assert(length > 0);
  std::fprintf(stderr, "%s [%s].\n", buffer, c3_get_error_message());
  exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
  std::printf("Epoll emulation layer testing utility %s.\n"
    "Copyright (C) 2016-2019 CyberHULL.\n"
    "Written by Vadim Sytnikov.\n", c3lib_version_build_string);
  if (argc != 2) {
    std::printf("\nUsage: %s <port-number>\n", argv[0]);
    return EXIT_FAILURE;
  }

  // initialize sockets and other network stuff
  int socket_fd = c3_socket();
  if (socket_fd == -1) {
    fail("Could not create socket");
  }
  const char* host = "127.0.0.1";
  const char* port = argv[1];
  if (c3_bind(socket_fd, host, port) == -1) {
    fail("Could not bind socket");
  }
  if (c3_listen(socket_fd) != 0) {
    fail("Could not mark socket as passive");
  }
  int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    fail("Could not create 'epoll' instance");
  }
  struct epoll_event event;
  event.data.fd = socket_fd;
  event.events = (uint32_t)(EPOLLIN | EPOLLET);
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event) == -1) {
    fail("Could not add listening socket (%d) to epoll instance (%d)", socket_fd, epoll_fd);
  }
  const c3_uint_t MAX_NUM_EVENTS = 64;
  struct epoll_event* events = (epoll_event*) global_memory.calloc(MAX_NUM_EVENTS, sizeof event);
  std::printf("Connected to %s:%s (desc = %d; will quit if 'exit' is received).\n", host, port, socket_fd);

  // enter main event loop
  for (;;) {
    int num = epoll_wait(epoll_fd, events, MAX_NUM_EVENTS, -1);
    if (num < 0) {
      fail("Error waiting for events on epoll instance (%d)", epoll_fd);
    }
    for (int i = 0; i < num; i++) {
      const epoll_event& e = events[i];
      if ((e.events & EPOLLERR) != 0 || (e.events & EPOLLHUP) != 0 || (e.events & EPOLLIN) == 0) {
        // an error has occurred, or the socket is not ready for reading
        fail("Event error (desc=%d, flags=%08X)", e.data.fd, e.events);
      } else if (socket_fd == e.data.fd) {
        // must be one or more incoming connection(s)
        for (;;) {
          char address[C3_SOCK_MIN_ADDR_LENGTH];
          int conn_fd = c3_accept(socket_fd, address, sizeof address, C3_SOCK_NON_BLOCKING);
          if (conn_fd > 0) {
            std::printf("Accepted new connection (desc=%d, address=%s).\n", conn_fd, address);
          } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
              // we've processed all incoming connections
              break;
            } else {
              fail("Could not accept connection");
            }
          }
          // start watching incoming connection's socket
          event.data.fd = conn_fd;
          event.events = (uint32_t)(EPOLLIN | EPOLLET);
          if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &event) == -1) {
            fail("Could not add connection (%d) to epoll instance (%d)", conn_fd, epoll_fd);
          }
        }
      } else {
        /*
         * Not another incoming connection: it's actual data to be read. Since we're in edge-triggered
         * mode (EPOLLET), we have to read all available data, because we won't be notified about it again.
         */
        for (;;) {
          char buffer[1024];
          ssize_t count = c3_receive(e.data.fd, buffer, sizeof buffer);
          if (count == C3_SOCK_RESULT_ERROR) {
            fail("Could not read from incoming connection socket (%d)", e.data.fd);
          } else if (count == C3_SOCK_RESULT_RETRY) {
            break;
          } else if (count == C3_SOCK_RESULT_CLOSED) {
            std::printf("Closed connection (desc=%d).\n", e.data.fd);
            // emulation layer can NOT remove descriptors automatically on closing
            if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, e.data.fd, &event) == -1) {
              fail("Could not remove descriptor from epoll instance");
            }
            if (close(e.data.fd) != 0) {
              fail("Could not close incoming connection socket (%d)", e.data.fd);
            }
            break;
          }
          assert(count > 0);
          // echo received data
          std::fwrite(buffer, (size_t) count, 1, stdout);
          // see if we should quit
          if (std::memcmp(buffer, "exit", 4) == 0 &&
            (buffer[4] == 0 || buffer[4] == '\n' || buffer[4] == '\r')) {
            free(events);
            close(socket_fd);
            std::printf("Bye.\n");
            return EXIT_SUCCESS;
          }
        }
      }
    }
  }
  free(events);
  close(socket_fd);
  return EXIT_SUCCESS;
}
