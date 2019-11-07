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
 * Linux `epoll` emulation layer; uses `poll` avaulable in Cygwin. Users of this
 * file must define `C3_CYGWIN` macro either before inclusion of this file, or
 * with a -D compiler switch.
 *
 * Can be used as a drop-in replacement of `epoll` with the following caveats:
 *
 * - it requires C++ compilation because its implementation uses C++ classes,
 *
 * - `epoll_close()` must be used instead of `close()` on the handle returned by
 *   `epoll_create()` because the handle returned by the latter is not a system
 *   one, it is internal to `epoll` emulation layer,
 *
 * - `epoll_create1()` function (available in Linux kernel 2.6.27+ and glibc
 *   2.9+) is not supported, because there is currently no way of providing the
 *   `EPOLL_CLOEXEC` functionality anyway.
 */
#ifndef _C3_EPOLL_H
#define _C3_EPOLL_H

#ifdef C3_CYGWIN

#include <sys/poll.h>

namespace CyberCache {

#define EPOLLIN      POLLIN
#define EPOLLPRI     POLLPRI
#define EPOLLOUT     POLLOUT
#define EPOLLRDNORM  0x040     // no `poll` counterpart
#define EPOLLRDBAND  0x080     // no `poll` counterpart
#define EPOLLWRNORM  0x100     // no `poll` counterpart
#define EPOLLWRBAND  0x200     // no `poll` counterpart
#define EPOLLMSG     0x400     // unused in the kernel
#define EPOLLERR     POLLERR
#define EPOLLHUP     POLLHUP
#define EPOLLRDHUP   0x2000    // no `poll` counterpart
#define EPOLLONESHOT (1 << 30) // no `poll` counterpart
#define EPOLLET      (1 << 31) // no `poll` counterpart

#define EPOLL_CTL_ADD 1	// add a descriptor to `epoll` instance
#define EPOLL_CTL_DEL 2	// remove a descriptor from `epoll` instance
#define EPOLL_CTL_MOD 3	// change type of watched events for a descriptor in `epoll` instance

typedef union epoll_data {
  void *ptr;    // user data pointer
  int fd;       // connection descriptor
  uint32_t u32; // 32-bit user data
  uint64_t u64; // 64-bit user data
} epoll_data_t;

struct epoll_event {
  uint32_t events;	 // epoll events
  epoll_data_t data; // user data associated with events
} __attribute__ ((__packed__));

/**
 * Creates an epoll instance; returns descriptor for the new instance.
 * The `size` parameter is ignored, but must be greater than zero.
 * The returned descriptor should be closed with `epoll_close()`.
 */
int epoll_create(int size) C3_FUNC_COLD;

/**
 * Essentially an alias for epoll_create(); flags are ignored.
 */
inline int epoll_create1(int flags) { return epoll_create(1); }

/**
 * Manipulates an epoll instance `epfd` returned from the call to
 * `epoll_create()`. Returns 0 in case of success, -1 in case of error (the
 * `errno` variable will contain the specific error code). The `op` parameter is
 * one of the EPOLL_CTL_* constants defined above. The `fd` descriptor is the
 * target of the operation. The `event` parameter describes which events the
 * caller is interested in, as well as any associated user data.
 */
int epoll_ctl(int epfd, int op, int fd, struct epoll_event* event);

/**
 * Waits for events on an epoll instance `epfd`. Returns the number of triggered
 * events returned in `events` buffer, or -1 in case of an error, with the
 * `errno` variable set to the specific error code. The `events` parameter is a
 * buffer that will contain triggered events. The `maxevents` is the maximum
 * number of events to be returned (usually size of the "events" array). The
 * `timeout` parameter specifies the maximum wait time, in milliseconds (-1
 * means infinite).
 */
int epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout);

/**
 * Closes `epoll` descriptor created with `epoll_create()`; returns zero on
 * success. On error, -1 is returned, and errno is set appropriately.
 */
int epoll_close(int epfd) C3_FUNC_COLD;

} // namespace CyberCache

#else // !C3_CYGWIN

// for close()
#include <unistd.h>
// for `epoll` implementation itself
#include <sys/epoll.h>

// not available in kernels before 2.6.17
#ifndef EPOLLRDHUP
#define EPOLLRDHUP 0x2000
#endif

// native `epoll` descriptor must be closed as a file handle
inline int epoll_close(int fd) { return close(fd); }

#endif // C3_CYGWIN
#endif // _C3_EPOLL_H
