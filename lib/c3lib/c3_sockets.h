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
 */
#ifndef _C3_SOCKETS_H
#define _C3_SOCKETS_H

#include "c3_types.h"

namespace CyberCache {

constexpr int C3_SOCK_NON_BLOCKING = 0x01; // perform asynchronous I/O
constexpr int C3_SOCK_REUSE_ADDR   = 0x02; // reuse address; for sockets used for c3_bind()

// minimum size of a buffer for an IPv4 address: 3 digits * 4 groups + 3 dots + 1 terminating '\0'
constexpr size_t C3_SOCK_MIN_ADDR_LENGTH = 16;

/**
 * Parses IPv4 address into 32-bit unsigned integer; bytes are in network order (big-endian).
 *
 * @param address IP address string
 * @param ip Pointer to the return value
 * @return 0 on success, -1 on error
 */
int c3_address2ip(const char* address, c3_ipv4_t& ip) C3_FUNC_COLD;

/**
 * Returns string representation of an IPv4 address (that should have bytes in network order; that is,
 * big-endian).
 *
 * @param ip IP address to convert, a 32-bit unsigned integer.
 * @param address Buffer for return value; if NULL (the default), internal thread-local buffer is used; if
 *   not NULL, it must point to a buffer of at least C3_SOCK_MIN_ADDR_LENGTH characters
 * @return Pointer to the buffer with string representation; there is no error return.
 */
const char* c3_ip2address(c3_ipv4_t ip, char* address = nullptr) C3_FUNC_COLD;

/**
 * Uses DNS to resolve host name (such as "cyberhull.com" or "localhost") to an IP address.
 *
 * @param host String representing host name.
 * @return Host IP on success, `INVALID_IPV4_ADDRESS` on errors.
 */
c3_ipv4_t c3_resolve_host(const char* host) C3_FUNC_COLD;

/**
 * Creates TCP/IP socket.
 *
 * @param options A combination of C3_SOCK_xxx constants; default is (C3_SOCK_NON_BLOCKING |
 *   C3_SOCK_REUSE_ADDR).
 * @return 0 on success, -1 on failure.
 */
int c3_socket(int options = C3_SOCK_NON_BLOCKING | C3_SOCK_REUSE_ADDR);

/**
 * Makes socket descriptor non-blocking (configures it for asynchronous I/O).
 *
 * @param fd Socket handle
 * @return 0 on success, -1 on failure
 */
int c3_make_fd_nonblocking(int fd);

/**
 * Binds TCP/IP socket to an address/port combination for data reception.
 *
 * @param fd Socket handle
 * @param host Name of the host to bind to
 * @param port Name of the service (port number) to bind to
 * @return 0 on success, -1 on failure
 */
int c3_bind(int fd, const char* host, const char* port);

/**
 * Binds TCP/IP socket to an address/port combination for data reception.
 *
 * @param fd Socket handle
 * @param host IP address of the host to bind to (`c3_ipv4_t`, in network byte order)
 * @param port Port number) to bind to
 * @return 0 on success, -1 on failure
 */
int c3_bind(int fd, c3_ipv4_t host, c3_ushort_t port);

/**
 * Connects TCP/IP socket to an address/port combination for data transmission.
 *
 * @param fd Socket handle
 * @param host Name of the host to connect to
 * @param port Name of the service (port number) to connect to
 * @return 0 on success, -1 on failure
 */
int c3_connect(int fd, const char* host, const char* port);

/**
 * Connects TCP/IP socket to an address/port combination for data transmission.
 *
 * @param fd Socket handle
 * @param host IP address of the host to connect to (`c3_ipv4_t`, in network byte order)
 * @param port Port number to connect to
 * @return 0 on success, -1 on failure
 */
int c3_connect(int fd, c3_ipv4_t host, c3_ushort_t port);

/**
 * Marks the socket as passive, ready to accept incoming connections.
 *
 * @param fd Socket handle
 * @param backlog Maximum length of the queue of incoming connections, default is INT_MAX_VAL.
 * @return 0 on success, -1 on failure
 */
int c3_listen(int fd, int backlog = INT_MAX_VAL);

/**
 * Accepts incoming connection, creates new connection socket, and optionally returns connection address.
 *
 * @param fd Socket handle
 * @param address Address buffer, enther NULL or a buffer for at least 14 characters
 * @param address_len Size of the address buffer, either zero or 14 or more
 * @param options Options for the new connection socket, default is C3_SOCK_NON_BLOCKING
 * @return New connection socket handle on success, 0 if there are no connections to accept, -1 on error
 */
int c3_accept(int fd, char *address = nullptr, size_t address_len = 0, int options = C3_SOCK_NON_BLOCKING);

/**
 * Accepts incoming connection, creates new connection socket, and returns connection address.
 *
 * @param fd Socket handle
 * @param address Reference to the variable into which parsed IPv4 remote address peer will be stored
 * @param options Options for the new connection socket, default is C3_SOCK_NON_BLOCKING
 * @return New connection socket handle on success, 0 if there are no connections to accept, -1 on error
 */
int c3_accept(int fd, c3_ipv4_t& address, int options = C3_SOCK_NON_BLOCKING);

/**
 * Closes socket.
 *
 * @param fd Socket descriptor to close
 * @return 0 on success, -1 on failure
 */
bool c3_close_socket(int fd);

/// Class that implements sockets for outbound connections
class Socket {
  int         ps_fd;         // socket handle
  c3_ipv4_t   ps_address;    // IP address
  c3_ushort_t ps_port;       // port number
  c3_byte_t   ps_options;    // bit mask of `C3_SOCK_xxx` options
  bool        ps_persistent; // `true` if socket connection is persistent

  void close() {
    c3_close_socket(ps_fd);
    ps_fd = -1;
  }

  void dispose() {
    if (ps_fd > 0) {
      close();
    }
  }

public:
  Socket(bool blocking, bool binding) noexcept {
    ps_fd = -1;
    ps_address = INVALID_IPV4_ADDRESS;
    ps_port = 0;
    ps_options = 0;
    if (!blocking) {
      ps_options |= C3_SOCK_NON_BLOCKING;
    }
    if (binding) {
      ps_options |= C3_SOCK_REUSE_ADDR;
    }
    ps_persistent = false;
  }
  Socket(const Socket&) = delete;
  Socket(Socket&&) = delete;
  ~Socket() { dispose(); }

  Socket& operator=(const Socket&) = delete;
  Socket& operator=(Socket&&) = delete;

  int get_fd() const { return ps_fd; }
  c3_ipv4_t get_address() const { return ps_address; };
  c3_ushort_t get_port() const { return ps_port; }
  bool is_persistent() const { return ps_persistent; }

  bool connect(c3_ipv4_t address, c3_ushort_t port, bool persistent) {
    ps_persistent = persistent;
    if (ps_fd > 0) {
      if (ps_persistent && ps_address == address && ps_port == port) {
        return true;
      }
      c3_close_socket(ps_fd);
    }
    ps_fd = c3_socket(ps_options);
    if (ps_fd > 0) {
      if (c3_connect(ps_fd, address, port) == 0) {
        ps_address = address;
        ps_port = port;
        return true;
      }
      close();
    }
    return false;
  }

  bool reconnect() {
    if (ps_fd > 0) {
      // descriptor cannot be re-used after a successful `connect()`
      c3_close_socket(ps_fd);
    }
    ps_fd = c3_socket(ps_options);
    if (ps_fd > 0) {
      if (c3_connect(ps_fd, ps_address, ps_port) == 0) {
        return true;
      }
      close();
    }
    return false;
  }

  void disconnect(bool always) {
    if (always || !ps_persistent) {
      dispose();
    }
  }
};

/// Wrapper class for automatic socket management
class SocketGuard {
  Socket& sg_socket;

public:
  explicit SocketGuard(Socket& socket): sg_socket(socket) {}
  ~SocketGuard() { sg_socket.disconnect(false); }

  SocketGuard(const SocketGuard&) = delete;
  SocketGuard(SocketGuard&&) = delete;

  SocketGuard& operator=(const SocketGuard&) = delete;
  SocketGuard& operator=(SocketGuard&&) = delete;
};

}

#endif // _C3_SOCKETS_H
