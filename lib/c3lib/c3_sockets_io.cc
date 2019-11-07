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
#include "c3_build.h"

#include <sys/socket.h>
#include <netinet/tcp.h>
#include <cerrno>

#include "c3_types.h"
#include "c3_errors.h"
#include "c3_profiler_defs.h"
#include "c3_sockets_io.h"

namespace CyberCache {

static int cork_the_socket(int fd, int option) {
  if (setsockopt(fd, SOL_TCP, TCP_CORK, &option, sizeof(option)) != 0) {
    c3_assert_failure();
    return c3_set_stdlib_error_message();
  }
  return 0;
}

int c3_begin_data_block(int fd) {
  return cork_the_socket(fd, 1);
}

int c3_end_data_block(int fd) {
  return cork_the_socket(fd, 0);
}

ssize_t c3_send(int fd, const void* buff, size_t length, bool block) {
  if (fd > 0 && buff != nullptr && length > 0) {
    ssize_t n = send(fd, buff, length, (block? 0: MSG_DONTWAIT) | MSG_NOSIGNAL);
    if (n < 0) {
      // not using switch() here because EAGAIN and EWOULDBLOCK are the same on most systems
      int error_code = errno;
      if (error_code == EAGAIN || error_code == EWOULDBLOCK) {
        return C3_SOCK_RESULT_RETRY;
      } else if (error_code == ECONNRESET || error_code == EPIPE){
        return C3_SOCK_RESULT_CLOSED;
      } else {
        return c3_set_stdlib_error_message();
      }
    }
    // if data could not be sent, we should have received -1 and `EAGAIN`
    c3_assert(n!=0);
    PERF_UPDATE_RANGE(Sockets_Sent_Data_Range, (c3_uint_t) n)
    return n;
  } else {
    return c3_set_einval_error_message();
  }
}

ssize_t c3_receive(int fd, void* buff, size_t length, bool block) {
  if (fd > 0 && buff != nullptr && length > 0) {
    ssize_t n = recv(fd, buff, length, (block? 0: MSG_DONTWAIT) | MSG_NOSIGNAL);
    if (n < 0) {
      // not using switch() here because EAGAIN and EWOULDBLOCK are the same on *most* systems
      int error_code = errno;
      if (error_code == EAGAIN || error_code == EWOULDBLOCK) {
        return C3_SOCK_RESULT_RETRY;
      } else if (error_code == ECONNRESET || error_code == EPIPE) {
        return C3_SOCK_RESULT_CLOSED;
      } else {
        return c3_set_stdlib_error_message();
      }
    }
    if (n == 0) {
      // this is documented behavior of recv(); see http://man7.org/linux/man-pages/man2/recv.2.html
      return C3_SOCK_RESULT_CLOSED;
    }
    PERF_UPDATE_RANGE(Sockets_Received_Data_Range, (c3_uint_t) n)
    return n;
  } else {
    return c3_set_einval_error_message();
  }
}

}
