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
#include "c3_types.h"
#include "c3_signals.h"
#include "c3_errors.h"

namespace CyberCache {

static bool modify_signal_mask(int action, c3_signals_t signals) {
  int result = pthread_sigmask(action, signals.get_ref(), nullptr);
  if (result != 0) {
    c3_set_stdlib_error_message();
    return false;
  }
  return true;
}

bool c3_signals_enable(c3_signals_t signals) {
  return modify_signal_mask(SIG_UNBLOCK, signals);
}

bool c3_signals_disable(c3_signals_t signals) {
  return modify_signal_mask(SIG_BLOCK, signals);
}

bool c3_signals_disable_set(c3_signals_t signals) {
  return modify_signal_mask(SIG_SETMASK, signals);
}

int c3_signals_wait(c3_signals_t signals) {
  int signal;
  int result = sigwait(signals.get_ref(), &signal);
  if (result != 0) {
    c3_set_stdlib_error_message();
    return 0;
  }
  return signal;
}

}
