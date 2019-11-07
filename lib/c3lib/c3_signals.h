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
 * Enabling, disabling, and processing signals at thread level.
 */
#ifndef _C3_SIGNALS_H
#define _C3_SIGNALS_H

#include "c3_build.h"

#include <csignal>

namespace CyberCache {

/**
 * Composer of signal masks.
 *
 * Even though <signal.h> does contain `sigemptyset()`, `sigaddset()` and other such functions, they are
 * essentially unavailable in Cygwin except in some compilation modes, so we have to provide our own.
 *
 * A good reference of what signal mask is and how it can be composed is /usr/include/bash/sig.h header
 * from Cygwin GCC distribution.
 *
 * Luckily, <signal.h> always defines `sigset_t`, signal constants such as `SIGINT`, and the `NSIG` macro
 * that represents total number of signals supported by the system.
 *
 * For Linux builds, we do have to use standard means of signal mask manipulation, because in Linux there are many more
 * signals, and `sigset_t` is no longer a scalar.
 */
class c3_signals_t {
  sigset_t ss_mask; // bit mask of signals to enable/disable/watch

  #ifdef C3_CYGWIN
  static sigset_t sig_mask(int signal) { return 1u << (signal - 1); }
  #endif

public:
  c3_signals_t() { empty(); }
  explicit c3_signals_t(sigset_t mask) { ss_mask = mask; }

  sigset_t get() const { return ss_mask; }
  const sigset_t* get_ref() const { return &ss_mask; }
  bool contains(int signal) const {
    #ifdef C3_CYGWIN
    return (ss_mask & sig_mask(signal)) != 0;
    #else
    return sigismember(&ss_mask, signal) != 0;
    #endif
  }

  c3_signals_t& add(int signal) {
    #ifdef C3_CYGWIN
    ss_mask |= sig_mask(signal);
    #else
    sigaddset(&ss_mask, signal);
    #endif
    return *this;
  }
  c3_signals_t& add_all() {
    #ifdef C3_CYGWIN
    ss_mask = sig_mask(NSIG) - 1;
    #else
    sigfillset(&ss_mask);
    #endif
    return *this;
  }
  c3_signals_t& remove(int signal) {
    #ifdef C3_CYGWIN
    ss_mask &= ~sig_mask(signal);
    #else
    sigdelset(&ss_mask, signal);
    #endif
    return *this;
  }
  c3_signals_t& empty() {
    #ifdef C3_CYGWIN
    ss_mask = 0;
    #else
    sigemptyset(&ss_mask);
    #endif
    return *this;
  }

  c3_signals_t& operator+(int signal) { return add(signal); }
  c3_signals_t& operator-(int signal) { return remove(signal); }
};

/**
 * Enables signals specified by a mask in *current* thread; if current thread then spawns new thread(s),
 * signal mask will be inherited.
 *
 * @param signals Mask of signals to be enabled; signals not in the mask will *not* be affected.
 * @return On success, returns `true`; on errors, returns `false` and sets error message.
 */
bool c3_signals_enable(c3_signals_t signals) C3_FUNC_COLD;
/**
 * Disables signals specified by a mask in *current* thread; if current thread then spawns new thread(s),
 * signal mask will be inherited.
 *
 * @param signals Mask of signals to be blocked; signals not in the mask will *not* be affected
 * @return On success, returns `true`; on errors, returns `false` and sets error message.
 */
bool c3_signals_disable(c3_signals_t signals) C3_FUNC_COLD;
/**
 * Disables signals specified by a mask in *current* thread, while enabling all other signals; if current
 * thread then spawns new thread(s), signal mask will be inherited.
 *
 * @param signals Mask of signals to be blocked; signals not in the mask will be enabled
 * @return On success, returns `true`; on errors, returns `false` and sets error message.
 */
bool c3_signals_disable_set(c3_signals_t signals) C3_FUNC_COLD;
/**
 * Waits for one of the signals specified by the mask; can be used to wait for signals previously
 * explicitly blocked for the thread.
 *
 * @param signals Mask of signals to wait for.
 * @return On success, returns ID of the signal; on failure, returns `0` and sets error message.
 */
int c3_signals_wait(c3_signals_t signals);

} // CyberCache

#endif // _C3_SIGNALS_H
