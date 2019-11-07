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
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstddef>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "c3_types.h"
#include "c3_errors.h"
#include "c3_profiler_defs.h"
#include "c3_sockets.h"

namespace CyberCache {

constexpr size_t PORT_STRING_LENGTH = 8;

static char* port2str(char* buffer, c3_ushort_t port) {
  c3_assert_def(int result) snprintf(buffer, PORT_STRING_LENGTH, "%hd", port);
  c3_assert(result > 0);
  return buffer;
}

int c3_address2ip(const char* address, c3_ipv4_t& ip) {
  if (address != nullptr) {
    struct in_addr net_address;
    if (inet_aton(address, &net_address) != 0) {
      ip = net_address.s_addr;
      return 0;
    } else {
      // `inet_aton()` does not set `errno`
      return c3_set_error_message("Invalid IPv4 address: '%s'", address);
    }
  } else {
    return c3_set_einval_error_message();
  }
}

const char* c3_ip2address(c3_ipv4_t ip, char* address) {
  struct in_addr net_address;
  net_address.s_addr = ip;
  // inet_ntoa() *is* thread-safe; see
  // https://www.gnu.org/software/libc/manual/html_node/Host-Address-Functions.html
  const char* ip_address = inet_ntoa(net_address);
  if (address != nullptr) {
    std::strncpy(address, ip_address, C3_SOCK_MIN_ADDR_LENGTH);
    address[C3_SOCK_MIN_ADDR_LENGTH - 1] = 0;
    return address;
  }
  return ip_address;
}

c3_ipv4_t c3_resolve_host(const char* host) {
  if (host != nullptr) {
    hostent* record = gethostbyname(host);
    if(record != nullptr) {
      PERF_INCREMENT_COUNTER(Socket_Hosts_Resolved)
      auto address = (in_addr*) record->h_addr;
      return address->s_addr;
    } else {
      // `gethostbyname()` sets `h_errno`, not `errno`
      c3_set_error_message("Could not resolve host: '%s'", host);
    }
  } else {
    c3_set_einval_error_message();
  }
  return INVALID_IPV4_ADDRESS;
}

int c3_socket(int options) {
  int fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd > 0) {
    if ((options & C3_SOCK_NON_BLOCKING) != 0) {
      if (c3_make_fd_nonblocking(fd) != 0) {
        close(fd);
        return -1; // error message had already been set
      }
    }
    if ((options & C3_SOCK_REUSE_ADDR) != 0) {
      int do_reuse = 1;
      if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &do_reuse, sizeof(do_reuse)) < 0) {
        close(fd);
        return c3_set_stdlib_error_message();
      }
    }
    /*
     * The only time we do not need this is creation of a listening socket (for epoll); there,
     * TCP_NODELAY does not help, but it doesn't do any harm either.
     */
    int no_delay = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay)) < 0) {
      close(fd);
      return c3_set_stdlib_error_message();
    }
    PERF_INCREMENT_COUNTER(Sockets_Created)
    return fd;
  } else {
    return c3_set_stdlib_error_message();
  }
}

int c3_make_fd_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    return c3_set_stdlib_error_message();
  }
  return 0;
}

int c3_bind(int fd, const char* host, const char* port) {
  if (fd > 0 && host != nullptr && port != nullptr) {
    struct addrinfo  hint;
    struct addrinfo* info;

    std::memset(&hint, 0, sizeof(hint));
    hint.ai_family   = AF_INET;
    hint.ai_socktype = SOCK_STREAM;

    int result = getaddrinfo(host, port, &hint, &info);
    if (result == 0) {
      result = bind(fd, info->ai_addr, info->ai_addrlen);
      if (result != 0) {
        c3_set_stdlib_error_message();
      }
    } else {
      // sets 'result' to -1
      result = c3_set_gai_error_message(result);
    }
    freeaddrinfo(info);
    PERF_INCREMENT_COUNTER(Sockets_Bound)
    return result;
  }
  return c3_set_einval_error_message();
}

int c3_bind(int fd, c3_ipv4_t host, c3_ushort_t port) {
  if (port > 0) {
    char port_str[PORT_STRING_LENGTH];
    char host_str[C3_SOCK_MIN_ADDR_LENGTH];
    return c3_bind(fd, c3_ip2address(host, host_str), port2str(port_str, port));
  }
  return c3_set_einval_error_message();
}

int c3_connect(int fd, const char* host, const char* port) {
  if (fd > 0 && host != nullptr && port != nullptr) {
    struct addrinfo  hint;
    struct addrinfo* info;

    std::memset(&hint, 0, sizeof(hint));
    hint.ai_family   = AF_INET;
    hint.ai_socktype = SOCK_STREAM;

    int result = getaddrinfo(host, port, &hint, &info);
    if (result == 0) {
      result = connect(fd, info->ai_addr, info->ai_addrlen);
      if (result != 0) {
        c3_set_stdlib_error_message();
      } else {
        PERF_INCREMENT_COUNTER(Socket_Outbound_Connections)
      }
    } else {
      // sets 'result' to -1
      result = c3_set_gai_error_message(result);
    }
    freeaddrinfo(info);
    return result;
  }
  return c3_set_einval_error_message();
}

int c3_connect(int fd, c3_ipv4_t host, c3_ushort_t port) {
  if (port > 0) {
    char port_str[PORT_STRING_LENGTH];
    char host_str[C3_SOCK_MIN_ADDR_LENGTH];
    return c3_connect(fd, c3_ip2address(host, host_str), port2str(port_str, port));
  }
  return c3_set_einval_error_message();
}

int c3_listen(int fd, int backlog) {
  if (fd > 0 && backlog > 0) {
    if (listen(fd, backlog) != 0) {
      return c3_set_stdlib_error_message();
    }
    return 0;
  }
  return c3_set_einval_error_message();
}

int c3_accept(int fd, char *address, size_t address_len, int options) {
  struct sockaddr addr;
  // using parentheses to satisfy GCC's -Wall
  if (fd > 0 && ((address == nullptr && address_len == 0) ||
    (address != nullptr && address_len >= sizeof addr.sa_data))
    && (options == 0 || options == C3_SOCK_NON_BLOCKING)) {
    addr.sa_family = AF_INET;
    socklen_t addr_len = sizeof addr;
    int sock = accept4(fd, &addr, &addr_len, (options & C3_SOCK_NON_BLOCKING) != 0? SOCK_NONBLOCK: 0);
    if (sock > 0) {
      if (address != nullptr) {
        if ((size_t) addr_len <= sizeof addr) {
          struct sockaddr_in* socket_address = (struct sockaddr_in*) &addr;
          const char* ip_address = inet_ntoa(socket_address->sin_addr);
          std::strncpy(address, ip_address, address_len);
          // it's "better" to get broken address than non-terminated string
          address[address_len - 1] = 0;
          PERF_INCREMENT_COUNTER(Socket_Inbound_Connections)
        } else {
          PERF_INCREMENT_COUNTER(Sockets_Accept_Error_Address)
          close(sock);
          return c3_set_error_message("Accepted address too long: %ld chars",
            addr_len - offsetof(struct sockaddr, sa_data));
        }
      }
      /*
       * The following may only be needed for an ancient Linux kernel < 2.6.28 (w/o accept4() support)
       *
      if (options & C3_SOCK_NON_BLOCKING) {
        if (c3_make_fd_nonblocking(sock)) {
          close(sock);
          return -1; // error message had already been set
        }
      }
      */
      return sock;
    }
    int error_code = errno;
    if (error_code == EAGAIN || error_code == EWOULDBLOCK /* same as EAGAIN on most but not all systems*/) {
      PERF_INCREMENT_COUNTER(Sockets_Accept_Try_NoConn)
      return 0;
    } else {
      PERF_INCREMENT_COUNTER(Sockets_Accept_Error_Other)
      return c3_set_stdlib_error_message();
    }
  }
  return c3_set_einval_error_message();
}

int c3_accept(int fd, c3_ipv4_t& address, int options) {
  assert(fd > 0);
  char str_addr[C3_SOCK_MIN_ADDR_LENGTH];
  int sock = c3_accept(fd, str_addr, sizeof str_addr, options);
  if (sock > 0) {
    if (c3_address2ip(str_addr, address) != 0) {
      PERF_INCREMENT_COUNTER(Sockets_Accept_Error_IP)
      close(sock);
      return -1;
    }
  }
  return sock;
}

bool c3_close_socket(int fd) {
  if (fd > 0) {
    if (close(fd) != 0) {
      c3_set_stdlib_error_message();
      return false;
    }
    PERF_INCREMENT_COUNTER(Sockets_Closed)
    return true;
  } else {
    c3_set_einval_error_message();
    return false;
  }
}

}
