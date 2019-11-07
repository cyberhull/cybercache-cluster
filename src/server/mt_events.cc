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
#include "mt_events.h"

#ifdef C3_CYGWIN
// see http://man7.org/linux/man-pages/man2/pipe.2.html
// and http://man7.org/linux/man-pages/man7/feature_test_macros.7.html
#define _GNU_SOURCE
#endif
#include <unistd.h>

#ifdef C3_CYGWIN
#include <errno.h>
#include <fcntl.h>
#else
#include <sys/eventfd.h>
#include <linux/version.h>
#endif // C3_CYGWIN

namespace CyberCache {

bool Event::initialize() {
  #ifdef C3_CYGWIN
  if (pipe2(e_fds, O_NONBLOCK) == 0) {
    return true;
  } else {
    c3_set_stdlib_error_message();
    c3_assert_failure();
    return false;
  }
  #else // !C3_CYGWIN
  bool result =
  #if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)
    (e_fd = eventfd(0, 0)) > 0 && c3_make_fd_nonblocking(e_fd) == 0;
  #else // LINUX_VERSION_CODE > KERNEL_VERSION(2,6,26)
    (e_fd = eventfd(0, EFD_NONBLOCK)) > 0;
  #endif
  if (result) {
    return true;
  } else {
    c3_set_stdlib_error_message();
    c3_assert_failure();
    return false;
  }
  #endif // C3_CYGWIN
}

void Event::trigger() const {
  #ifdef C3_CYGWIN
  c3_assert(e_fds[1] != -1);
  char byte;
  ssize_t result = write(e_fds[1], &byte, 1);
  c3_assert(result == 1 || (result == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)));
  #else // !C3_CYGWIN
  c3_assert(e_fd != -1);
  c3_ulong_t count = 1;
  ssize_t result = write(e_fd, &count, sizeof count);
  c3_assert(result == sizeof count);
  (void) result; // `write()` is declared with attribute `warn_unused_result` [-Wunused-result]
  #endif // C3_CYGWIN
}

void Event::consume() const {
  #ifdef C3_CYGWIN
  c3_assert(e_fds[0] != -1);
  char buffer[4096];
  bool first_read = true;
  for (;;) {
    ssize_t result = read(e_fds[0], buffer, sizeof buffer);
    if (first_read) {
      assert(result > 0);
      first_read = false;
    }
    if (result <= 0) {
      break;
    }
  }
  #else // !C3_CYGWIN
  c3_assert(e_fd != -1);
  c3_ulong_t count = 0;
  ssize_t result = read(e_fd, &count, sizeof count);
  c3_assert(result == sizeof count && count > 0);
  (void) result; // `read()` is declared with attribute `warn_unused_result` [-Wunused-result]
  #endif // C3_CYGWIN
}

void Event::dispose() {
  #ifdef C3_CYGWIN
  if (e_fds[0] != -1) {
    assert(e_fds[1] != -1);
    close(e_fds[0]);
    close(e_fds[1]);
    e_fds[0] = e_fds[1] = -1;
  } else {
    assert(e_fds[1] == -1);
  }
  #else // !C3_CYGWIN
  if (e_fd != -1) {
    close(e_fd);
    e_fd = -1;
  }
  #endif // C3_CYGWIN
}

}
