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
 * Network communications support.
 *
 * This module contains socket I/O methods that are only used by some components (but not by the server).
 */
#ifndef _C3_SOCKETS_IO_H
#define _C3_SOCKETS_IO_H

#include "c3_sockets.h"

namespace CyberCache {

// return values for c3_send() and c3_receive()
constexpr ssize_t C3_SOCK_RESULT_ERROR  = -1; // an error occurred
constexpr ssize_t C3_SOCK_RESULT_RETRY  = -2; // operation would block, caller should try again
constexpr ssize_t C3_SOCK_RESULT_CLOSED = -3; // connection had been closed or reset by remote

/**
 * Instructs TCP socket to start buffering data being written (e.g. with `c3_send()`), and do not send them out until
 * `c3_end_data_block()` is called, or until the socket is closed.
 *
 * @param fd Socket handle
 * @return 0 on success, -1 on error
 */
int c3_begin_data_block(int fd);

/**
 * Instructs TCP socket to send out data buffered since last `c3_begin_data_block()` call on the socket. For this
 * function to actually trigger sending out buffered data, the socket must have `TCP_NODELAY` option set; the
 * `c3_socket()` function does set that option automatically.
 *
 * @param fd Socket handle
 * @return 0 on success, -1 on error
 */
int c3_end_data_block(int fd);

/**
 * Sends data to a socket.
 *
 * @param fd Socket handle
 * @param buff Data buffer, must not be NULL
 * @param length Number of bytes to send, must be greater than zero
 * @param block 'true' if should wait for completion, 'false' (the default) otherwise; if the socket is
 *   non-blocking, this argument will have no effect, and the call will never wait.
 * @return May return the following values:
 *   - positive number of bytes sent,
 *   - C3_SOCK_RESULT_ERROR: an error occured, error message had been set,
 *   - C3_SOCK_RESULT_RETRY: sending would block, should retry later,
 *   - C3_SOCK_RESULT_CLOSED: remote peer reset the connection
 */
ssize_t c3_send(int fd, const void* buff, size_t length, bool block = false);

/**
 * Receives data from a socket.
 *
 * @param fd Socket handle
 * @param buff Data buffer, must not be NULL
 * @param length Number of bytes to receive, must be greater than zero
 * @param block 'true' if should wait for completion, 'false' (the default) otherwise; if the socket is
 *   non-blocking, this argument will have no effect, and the call will never wait.
 * @return May return the following values:
 *   - positive number of bytes received,
 *   - 0 ("end-of-file", no more data available),
 *   - C3_SOCK_RESULT_ERROR: an error occured, error message had been set,
 *   - C3_SOCK_RESULT_RETRY: receiving would block, should retry later,
 *   - C3_SOCK_RESULT_CLOSED: remote peer reset the connection
 */
ssize_t c3_receive(int fd, void* buff, size_t length, bool block = false);

}

#endif // _C3_SOCKETS_IO_H
