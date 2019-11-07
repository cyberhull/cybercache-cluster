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
#include "mt_quick_event.h"

namespace CyberCache {

#ifndef C3_CYGWIN
#include <syscall.h>
#include <linux/futex.h>

static int futex_op(int op, int *address, int value)
{
  return (int) syscall(SYS_futex, address, op, value, nullptr, nullptr, 0);
}
static int futex_op(int op, int *address, int value, const struct timespec* timeout)
{
  return (int) syscall(SYS_futex, address, op, value, timeout, nullptr, 0);
}
#endif

///////////////////////////////////////////////////////////////////////////////
// QuickEvent
///////////////////////////////////////////////////////////////////////////////

void QuickEvent::notify() {
  #ifndef C3_CYGWIN
  qe_futex = QE_TRIGGERED;
  futex_op(FUTEX_WAKE_PRIVATE, &qe_futex, 1 /* just one thread */);
  #else
  c3_assert(e_fds[1] > 0);
  char byte;
  ssize_t result = write(e_fds[1], &byte, 1);
  c3_assert(result == 1);
  #endif // C3_CYGWIN
}

void QuickEvent::wait() {
  #ifndef C3_CYGWIN
  futex_op(FUTEX_WAIT_PRIVATE, &qe_futex, QE_IDLE);
  qe_futex = QE_IDLE;
  #else
  c3_assert(e_fds[0] > 0);
  char byte;
  ssize_t result = read(e_fds[0], byte, 1);
  c3_assert(result == 1);
  #endif // C3_CYGWIN
}

///////////////////////////////////////////////////////////////////////////////
// QuickTimedEvent
///////////////////////////////////////////////////////////////////////////////

bool QuickTimedEvent::wait(c3_uint_t timeout) {
  #ifndef C3_CYGWIN
  struct timespec timeout_spec = {
    timeout / 1000, // seconds
    (timeout % 1000) * 1000000 // nanoseconds
  };
  int result = futex_op(FUTEX_WAIT_PRIVATE, &qe_futex, QE_IDLE, &timeout_spec);
  qe_futex = QE_IDLE;
  return result == 0;
  #else // C3_CYGWIN
  c3_assert(e_fds[0] > 0);
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(e_fds[0], &fds);
  struct timeval timeout_val = {
    timeout / 1000, // seconds
    (timeout % 1000) * 1000 // microseconds
  };
  if (select(e_fds[0] + 1, &fds, nullptr, nullptr, &timeout_val) == 1) {
    char byte;
    ssize_t result = read(e_fds[0], byte, 1);
    c3_assert(result == 1);
    return true;
  }
  return false;
  #endif // C3_CYGWIN
}

} // CyberCache
