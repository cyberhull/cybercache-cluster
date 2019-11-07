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
 * Managing signals received by the application.
 */
#ifndef _CC_SIGNAL_HANDLER_H
#define _CC_SIGNAL_HANDLER_H

#include "c3lib/c3lib.h"
#include "mt_threads.h"

namespace CyberCache {

/**
 * Signal processor.
 *
 * Used to 1) disable signals in all threads except its own one, 2) process the signals.
 */
class SignalHandler {
  c3_signals_t sh_mask; // bit mask of signals

  void log_and_exit(const char* message) const C3_FUNC_COLD C3_FUNC_NORETURN;

public:
  SignalHandler() noexcept C3_FUNC_COLD;

  void block_signals() const C3_FUNC_COLD;
  static void send_quit_message() C3_FUNC_COLD;

  static void thread_proc(c3_uint_t id, ThreadArgument arg);
};

extern SignalHandler signal_handler;

} // CyberCache

#endif // _CC_SIGNAL_HANDLER_H
