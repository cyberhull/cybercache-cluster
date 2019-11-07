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
 * Multithreading support: events that can be watched with `epoll`.
 */
#ifndef _MT_EVENTS_H
#define _MT_EVENTS_H

#include "c3lib/c3lib.h"

namespace CyberCache {

/**
 * An event object that can be watched using `epoll`.
 *
 * Native (Linux) implementation uses `eventfd()` that is specifically designed for such cases, while
 * emulation layer uses pipe; we could have used socketpair() for emulation just as well, but it doesn't
 * matter: we do not need bi-directional communication, and pipes have very slightly less overhead (not
 * that it's important, but still; see
 * https://wiki.openlighting.org/index.php/Pipe_vs_Unix_Socket_Performance).
 */
class Event {
  #ifdef C3_CYGWIN
  int e_fds[2];
  #else // !C3_CYGWIN
  int e_fd;
  #endif // C3_CYGWIN

public:
  /// Default ctor; sets invalid state; actual initialization has to be done using initialize()
  Event() {
    #ifdef C3_CYGWIN
    e_fds[0] = e_fds[1] = -1;
    #else // !C3_CYGWIN
    e_fd = -1;
    #endif // C3_CYGWIN
  }
  Event(const Event&) = delete;
  Event(Event&&) = delete;
  /// Deinitializes the object if it hasn't been done explicitly using dispose()
  ~Event() { dispose(); }

  Event& operator=(const Event&) = delete;
  Event& operator=(Event&&) = delete;

  /// Returns a descriptor to be watched using `epoll`
  int get_event_fd() const {
    #ifdef C3_CYGWIN
    return e_fds[0];
    #else // !C3_CYGWIN
    return e_fd;
    #endif // C3_CYGWIN
  }

  /// Actually creates the object and makes its handle available (default ctor only sets invalid state)
  bool initialize();
  /// Triggers the event; can be called multiple times in a row
  void trigger() const;
  /// Consumes an event; this can *only* be called once after a trigger() call
  void consume() const;
  /// Deinitializes the object; the handle should not be watched at this point
  void dispose();
};

}

#endif // _MT_EVENTS_H
