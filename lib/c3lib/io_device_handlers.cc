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
#include "io_device_handlers.h"
#include "io_net_config.h"
#include "c3_sockets.h"
#include "c3_profiler_defs.h"

#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// SocketReader
///////////////////////////////////////////////////////////////////////////////

io_result_t SocketReader::read_bytes(int fd, c3_byte_t* buff, c3_uint_t nbytes, c3_uint_t &nread) const {
  c3_assert(fd > 0 && buff && nbytes);
  ssize_t n = recv(fd, buff, nbytes, (NetworkConfiguration::get_sync_io()? 0: MSG_DONTWAIT) | MSG_NOSIGNAL);
  if (n < 0) {
    nread = 0;
    int error_code = errno;
    // not using switch() here because EAGAIN and EWOULDBLOCK are the same on *most* systems
    if (error_code == EAGAIN || error_code == EWOULDBLOCK) {
      return IO_RESULT_RETRY;
    } else if (error_code == ECONNRESET || error_code == EPIPE) {
      return IO_RESULT_EOF;
    } else {
      return IO_RESULT_ERROR;
    }
  }
  nread = (c3_uint_t) n;
  if (n == 0) {
    // this is documented behavior of recv(); see http://man7.org/linux/man-pages/man2/recv.2.html
    return IO_RESULT_EOF;
  }
  PERF_UPDATE_RANGE(Sockets_Received_Data_Range, (c3_uint_t) n)
  return IO_RESULT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// SocketWriter
///////////////////////////////////////////////////////////////////////////////

io_result_t SocketWriter::write_bytes(int fd, const c3_byte_t* buff, c3_uint_t nbytes, c3_uint_t &nwritten) const {
  c3_assert(fd > 0 && buff && nbytes);
  ssize_t n = send(fd, buff, nbytes, (NetworkConfiguration::get_sync_io()? 0: MSG_DONTWAIT) | MSG_NOSIGNAL);
  if (n < 0) {
    nwritten = 0;
    int error_code = errno;
    // not using switch() here because EAGAIN and EWOULDBLOCK are the same on most systems
    if (error_code == EAGAIN || error_code == EWOULDBLOCK) {
      return IO_RESULT_RETRY;
    } else if (error_code == ECONNRESET || error_code == EPIPE){
      return IO_RESULT_EOF;
    } else {
      return IO_RESULT_ERROR;
    }
  }
  // if data could not be sent, we should have received -1 and `EAGAIN`
  c3_assert(n > 0 && n <= nbytes);
  nwritten = (c3_uint_t) n;
  PERF_UPDATE_RANGE(Sockets_Sent_Data_Range, (c3_uint_t) n)
  return IO_RESULT_OK;
}

///////////////////////////////////////////////////////////////////////////////
// FileReader
///////////////////////////////////////////////////////////////////////////////

io_result_t FileReader::read_bytes(int fd, c3_byte_t* buff, c3_uint_t nbytes, c3_uint_t &nread) const {
  c3_assert(fd > 0 && buff && nbytes);
  ssize_t result = read(fd, buff, nbytes);
  switch (result) {
    case -1:
      nread = 0;
      return IO_RESULT_ERROR;
    case 0:
      nread = 0;
      return IO_RESULT_EOF;
    default:
      c3_assert(result > 0 && result <= nbytes);
      nread = (c3_uint_t) result;
      return IO_RESULT_OK;
  }
}

///////////////////////////////////////////////////////////////////////////////
// FileWriter
///////////////////////////////////////////////////////////////////////////////

io_result_t FileWriter::write_bytes(int fd, const c3_byte_t* buff, c3_uint_t nbytes, c3_uint_t &nwritten) const {
  c3_assert(fd > 0 && buff && nbytes);
  ssize_t result = write(fd, buff, nbytes);
  switch (result) {
    case -1:
    case 0: // write() is not supposed to return this for non-zero `nbytes`, but just in case...
      nwritten = 0;
      return IO_RESULT_ERROR;
    default:
      c3_assert(result > 0 && result <= nbytes);
      nwritten = (c3_uint_t) result;
      return IO_RESULT_OK;
  }
}

}
