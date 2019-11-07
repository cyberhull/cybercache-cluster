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
 * Multithreading support: fastest possible implementation of inter-thread notifications on Linux.
 */
#ifndef _MT_QUICK_EVENT_H
#define _MT_QUICK_EVENT_H

#include "c3lib/c3lib.h"

#ifdef C3_CYGWIN
#include <unistd.h>
#endif // C3_CYGWIN

namespace CyberCache {

/**
 * Fastest possible implementation of simple inter-thread notifications on Linux.
 *
 * For Cygwin, notifications are implemented using pipes. While the use of pipes in Cygwin version of `Event` is a
 * necessity (handles has to be watchable by `poll()`), for `QuickEvent` emulation it should be possible to use a more
 * efficient mechanism (TBD).
 */
class QuickEvent {
protected:
  #ifndef C3_CYGWIN
  static constexpr int QE_IDLE = 0;
  static constexpr int QE_TRIGGERED = 1;
  int qe_futex; // Linux futex, always `int`-sized
  #else
  int qe_fds[2]; // pipe handles
  #endif // C3_CYGWIN

public:
  QuickEvent() C3_FUNC_COLD {
    #ifndef C3_CYGWIN
    qe_futex = QE_IDLE;
    #else
    if (pipe(e_fds) != 0) {
      c3_set_stdlib_error_message();
      c3_assert_failure();
    }
    #endif // C3_CYGWIN
  }
  ~QuickEvent() C3_FUNC_COLD {
    #ifdef C3_CYGWIN
    assert(e_fds[0] > 0 && e_fds[1] > 0);
    close(e_fds[0]);
    close(e_fds[1]);
    #endif // C3_CYGWIN
  }

  QuickEvent(QuickEvent&) = delete;
  QuickEvent(QuickEvent&&) = delete;
  void operator=(QuickEvent&) = delete;
  void operator=(QuickEvent&&) = delete;

  /**
   * Triggers the event and wakes up at most one thread waiting on it.
   */
  void notify();
  /**
   * Waits for some other thread to trigger the event.
   */
  void wait();
};

/**
 * Implementation of the event that supports timed waits (see `QuickEvent` class description).
 */
class QuickTimedEvent: public QuickEvent {
public:
  /**
   * Waits for up to `timeout` milliseconds for some other thread to trigger the event.
   *
   * @param timeout Number of milliseconds to wait for an event.
   * @return `true` if an event has occurred, `false` if wait timed out.
   */
  bool wait(c3_uint_t timeout);
};

} // CyberCache

#endif // _MT_QUICK_EVENT_H
