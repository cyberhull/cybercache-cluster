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

// `epoll` emulation layer is not needed in Linux
#ifdef C3_CYGWIN

#include "c3_descriptor_vector.h"
#include "c3_epoll.h"

#include <new>
#include <cerrno>

namespace CyberCache {

// this is what `getrlimit(RLIMIT_NOFILE)` returns under Cygwin
const int MAX_POLL_EVENTS = 256;
const c3_uint_t POLL_EVENT_MASK = POLLIN | POLLPRI | POLLOUT | POLLERR | POLLHUP;

// type to be used for descriptor indexes
typedef int epoll_index_t;

/**
 * Three kinds of epoll events can be added to watch list:
 *
 * - listening to an input socket for an accept(),
 * - listening to a connection handle (waiting for data transmission),
 * - watching an `eventfd` (Linux) or `pipe2` (Cygwin) handle for "input ready" event.
 *
 * In any of these cases, `epoll_event::data.ptr` points to an object (a `ReaderWriter`-, or a
 * `PipelineCommand`-derived one), so we have no place to store file descriptor, and have to store it
 * separately along with `epoll_event`.
 *
 * A more efficient implementation would have used [int -> epoll_event] map here.
 */
struct EpollDescriptor {
  struct epoll_event ee_event; // event mask and user data
  int ee_fd;    // socket handle returned by socket() or accept()

  // this ctor is needed for vectors to be able to create "holes" in their buffers, as well as dummy
  // objects for error returns; the "holes" are defined as descriptors having `ee_fd` <= 0
  EpollDescriptor() {
    ee_event.events = 0;
    ee_event.data.u64 = 0;
    ee_fd = -1;
  }

  EpollDescriptor(const epoll_event& event, int fd) {
    ee_event = event;
    ee_fd = fd;
  }
} __attribute__ ((__packed__));

class EpollInstance {
  // set of events added to this instance
  DescriptorVector<EpollDescriptor, epoll_index_t> ei_descriptors;

public:
  EpollInstance() :
    ei_descriptors(64, 32, 8, 4) {
  }

  ~EpollInstance() {
    assert(ei_descriptors.get_count() == 0);
    ei_descriptors.deallocate();
  }

  // get total number of event slots, both used and empty
  epoll_index_t get_size() const {
    return (epoll_index_t) ei_descriptors.get_size();
  }

  // get number of used ("occupied") slots
  epoll_index_t get_count() const {
    return (epoll_index_t) ei_descriptors.get_count();
  }

  EpollDescriptor& operator[](epoll_index_t i) const {
    return ei_descriptors[i];
  }

  epoll_index_t add(EpollDescriptor&& desc) {
    assert(desc.ee_fd > 0);
    return ei_descriptors.add(std::move(desc));
  }

  epoll_index_t find(int fd) {
    for (epoll_index_t i = 0; i < (epoll_index_t) ei_descriptors.get_size(); i++) {
      if (ei_descriptors[i].ee_fd == fd) {
        return i;
      }
    }
    // this method is used for checks, so should not trigger assert()s itself
    return -1;
  }

  bool remove(int fd) {
    epoll_index_t i = find(fd);
    if (i != -1) {
      ei_descriptors.remove(i);
      return true;
    }
    // result of this method is checked, and assert() is triggered at call site
    return false;
  }
};

static DescriptorVector<EpollInstance*, epoll_index_t> epoll_instances(64, 8, 16, 8);

///////////////////////////////////////////////////////////////////////////////
// EPOLL API IMPLEMENTATION
///////////////////////////////////////////////////////////////////////////////

int epoll_create(int size) {
  if (size > 0) {
    auto epi = alloc<EpollInstance>();
    return epoll_instances.add(new (epi) EpollInstance()) + 1;
  } else {
    errno = EINVAL;
    return -1;
  }
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event* event) {
  if (epfd > 0 && epoll_instances[epfd - 1] && fd > 0 && event) {
    EpollInstance &ep_inst = *epoll_instances[epfd - 1];
    switch (op) {
      case EPOLL_CTL_ADD: // add a descriptor to `epoll` instance
        if (ep_inst.find(fd) == -1) {
          ep_inst.add(EpollDescriptor(*event, fd));
          return 0;
        } else {
          errno = EEXIST;
          return -1;
        }
      case EPOLL_CTL_DEL: // remove a descriptor from `epoll` instance
        if (ep_inst.remove(fd)) {
          return 0;
        } else {
          errno = ENOENT;
          return -1;
        }
      case EPOLL_CTL_MOD: { // change type of watched events for a descriptor in `epoll` instance
        epoll_index_t event_index = ep_inst.find(fd);
        if (event_index >= 0) {
          EpollDescriptor &ep_desc = ep_inst[event_index];
          assert(ep_desc.ee_fd == fd);
          ep_desc.ee_event = *event;
          return 0;
        } else {
          errno = ENOENT;
          return -1;
        }
      }
      default:
        errno = EINVAL;
        return -1;
    }
  } else {
    errno = EINVAL;
    return -1;
  }
}

int epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout) {
  if (events) {
    if (epfd > 0 && epoll_instances[epfd - 1] && maxevents > 0) {
      EpollInstance &ep_inst = *epoll_instances[epfd - 1];
      epoll_index_t max_num_poll_events = ep_inst.get_count();
      if (max_num_poll_events > MAX_POLL_EVENTS) {
        max_num_poll_events = MAX_POLL_EVENTS;
      }
      struct pollfd poll_events[max_num_poll_events];
      epoll_index_t num_poll_events = 0;
      for (epoll_index_t i = 0; i < ep_inst.get_size(); i++) {
        EpollDescriptor &desc = ep_inst[i];
        if (desc.ee_fd > 0) {
          struct pollfd &p = poll_events[num_poll_events];
          p.fd = desc.ee_fd;
          p.events = (c3_ushort_t) (desc.ee_event.events & POLL_EVENT_MASK);
          assert(p.events);
          p.revents = 0;
          if (++num_poll_events == max_num_poll_events) {
            break;
          }
        }
      }
      if (num_poll_events) {
        int n_ready = poll(poll_events, (nfds_t) num_poll_events, timeout);
        if (n_ready == -1) {
          // `errno` should have been set by `poll()` call
          return -1;
        } else if (n_ready > 0) {
          int k = 0; // number of events returned in events[]
          for (int j = 0; j < num_poll_events; j++) {
            struct pollfd& poll_event = poll_events[j];
            assert(!(poll_event.revents & POLLNVAL));
            uint32_t registered_events = (uint32_t) (poll_event.revents & POLL_EVENT_MASK);
            if (registered_events) {
              epoll_index_t triggered_event = ep_inst.find(poll_event.fd);
              if (triggered_event >= 0) {
                events[k].events = registered_events;
                events[k++].data = ep_inst[triggered_event].ee_event.data;
                if (k >= maxevents || k >= n_ready) {
                  // TODO: handle triggered events that do not fit `events[]`
                  break;
                }
              } else {
                // this is an "internal error" actually, but `EBADF` is the closest code we can use here
                errno = EBADF;
                return -1;
              }
            }
          }
          return k;
        }
      }
      /*
       * Zero return means "no file descriptor became ready during the requested timeout milliseconds"
       * (not an error). Theoretically, it can also mean that there were no descriptors to watch, but in
       * practice this should never happen: input socket pipeline adds (at the very least) a queue event
       * handle during its startup sequence, and the same applies to the output socket pipeline.
       */
      return 0;
    } else {
      errno = EINVAL;
      return -1;
    }
  } else {
    errno = EFAULT;
    return -1;
  }
}

int epoll_close(int epfd) {
  if (epfd > 0) {
    int ep_index = epfd - 1;
    EpollInstance* epi = epoll_instances[ep_index];
    if (epi) {
      /*
       * At this point, we can still have descriptors registered with the instance and, more importantly,
       * user data such as `ReaderWriter` pointers associated with them. Even though we could examine the
       * contents here and clean everything up, it would be a hack... Besides, the caller could have
       * received a command like "force-quit", which mandates as quick exit as possible, w/o any attempts
       * at cleanup (at least, no cleanup other than what's necessary to not crash the system; what
       * would be "memory leakage" might be perfectly fine, in that it might not matter as the server is
       * winding down, and all the memory would be reclaimed in a very short while anyway). So we leave
       * it up to the callers to make sure proper cleanup had been performed, or skip said cleanup if
       * told so by the server.
       */
      epoll_instances.remove(ep_index);
      epi->~EpollInstance();
      free_memory(epi, sizeof(EpollInstance));
      return 0;
    } else {
      errno = EBADF;
      return -1;
    }
  } else {
    errno = EBADF;
    return -1;
  }
}

} // CyberCache

#endif // C3_CYGWIN
