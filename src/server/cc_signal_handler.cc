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
#include "cc_signal_handler.h"
#include "cc_subsystems.h"
#include "cc_server.h"

namespace CyberCache {

SignalHandler signal_handler;

#include <unistd.h>

///////////////////////////////////////////////////////////////////////////////
// SignalHandler
///////////////////////////////////////////////////////////////////////////////

SignalHandler::SignalHandler() noexcept {
  // see http://man7.org/linux/man-pages/man7/signal.7.html
  sh_mask.empty().add(SIGHUP).add(SIGINT).add(SIGQUIT).add(SIGTERM) // quit requests
    .add(SIGABRT).add(SIGILL).add(SIGFPE).add(SIGSEGV).add(SIGBUS) // application errors
    // attempts to wait for `SIGKILL` and `SIGSTOP` would be silently ignored, so we don't add them
    .add(SIGUSR1).add(SIGUSR2);
}

void SignalHandler::log_and_exit(const char* message) const {
  // we do not know in what state server logger is, so we use system one
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wformat-security"
  syslog_message(LL_FATAL, message);
  #pragma GCC diagnostic pop

  server.on_abort();
  _exit(EXIT_FAILURE);
}

void SignalHandler::block_signals() const {
  c3_signals_disable(sh_mask);
}

void SignalHandler::send_quit_message() {
  c3_assert(Thread::get_id() == TI_MAIN);
  kill(getpid(), SIGUSR1);
}

void SignalHandler::thread_proc(c3_uint_t id, ThreadArgument arg) {
  Thread::set_state(TS_ACTIVE);
  auto processor = (SignalHandler*) arg.get_pointer();
  assert(processor);
  for (;;) {
    Thread::set_state(TS_IDLE);
    int signal = c3_signals_wait(processor->sh_mask);
    Thread::set_state(TS_ACTIVE);
    switch (signal) {
      case 0:
        /*
         * This "signal" could have been caused by an internal `c3_signals_wait()` failure; if this
         * happens during startup then, since logging an error increments error count (which will
         * then be checked by server startup code), server startup will be aborted; "quitting" state
         * will be set by proc wrapper.
         */
        syslog_message(LL_ERROR, "Error waiting for signals");
        return;
      case SIGHUP:
      case SIGINT:
      case SIGQUIT:
      case SIGTERM:
        server_logger.log(LL_NORMAL, "Quit request received (%d)", signal);
        server.post_quit_message();
        continue;
      case SIGABRT:
        processor->log_and_exit("ABORT request received, exiting");
      case SIGILL:
        processor->log_and_exit("Illegal instruction encountered, exiting");
      case SIGFPE:
        processor->log_and_exit("Floating point exception occurred, exiting");
      case SIGSEGV:
        processor->log_and_exit("Invalid memory reference encountered, exiting");
      case SIGBUS:
        processor->log_and_exit("Bad memory access encountered, exiting");
      case SIGUSR1:
      case SIGUSR2:
        syslog_message(LL_VERBOSE, "USER signal (%d) received, signal processor will now quit", signal);
        // "quitting" state will be set by proc wrapper
        return;
      default:
        c3_assert_failure();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// SignalHandler
///////////////////////////////////////////////////////////////////////////////

} // CyberCache
