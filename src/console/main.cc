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
 * Console entry point.
 */

#include "line_editor.h"
#include "console_commands.h"

using namespace CyberCache;

///////////////////////////////////////////////////////////////////////////////
// MEMORY MANAGEMENT CALLBACKS
///////////////////////////////////////////////////////////////////////////////

class ConsoleMemoryInterface: public MemoryInterface {
  void begin_memory_deallocation(size_t size) override;
  void end_memory_deallocation() override;
};

void ConsoleMemoryInterface::begin_memory_deallocation(size_t size) {
  printf("FATAL ERROR: cannot allocate %lu bytes of memory", size);
  _exit(EXIT_FAILURE);
}

void ConsoleMemoryInterface::end_memory_deallocation() {
  c3_assert_failure();
}

///////////////////////////////////////////////////////////////////////////////
// UTILITIES
///////////////////////////////////////////////////////////////////////////////

static bool is_option(const char* option, char short_option, const char* long_option) {
  char buffer[64];
  std::sprintf(buffer, "-%c", short_option);
  if (std::strcmp(buffer, option) == 0) {
    return true;
  }
  std::sprintf(buffer, "--%s", long_option);
  return std::strcmp(buffer, option) == 0;
}

static void execute_script(ConsoleParser& parser, const char* path) {
  const bool result = parser.parse(path);
  cc_log.print_all();
  if (!result) {
    exit(EXIT_FAILURE);
  }
}

static bool execute_command(ConsoleParser& parser, const char* text) {
  const bool ok = parser.execute(text);
  if (cc_result.has_changed()) {
    cc_server.set_offset(0);
    cc_server.set_count(0); // force re-calculation
    // if puzzled: see https://en.wikipedia.org/wiki/Duff's_device
    switch (cc_server.get_auto_result_mode()) {
      case ARM_LISTS:
        if (cc_result.is_array()) {
          case ARM_SIMPLE:
            cc_result.print();
          break;
        }
        // fall through
      default:
        parser.execute("result");
    }
  } else if (cc_log.get_num_messages() > 0) {
    cc_log.print_all();
  } else if (!cc_result.was_printed()) {
    // this comes into play if logging was set to some [very] terse mode
    if (!ok || ConsoleLogger::get_log_level() >= LL_TERSE) {
      std::printf("[%s]\n", ok? "ok": "error");
    }
  }
  return ok;
}

static void print_version() {
  printf("CyberCache Cluster Console %s\n", c3lib_version_build_string);
}

static void print_help(const char* exe_path) {
  static constexpr char help_text[] = R"C3_CL_HELP(Written by Vadim Sytnikov.
Copyright (C) 2016-2019 CyberHULL. All rights reserved.
This program is free software distributed under GPL v2+ license.

Use: %s [ <option>|<script> [ <option>|<script> [...]]]

Supported options are:

  -h | --help
    Print out this help message and exit.

  -q | --quiet
    Do not print version information, set 'error' (low) verbosity level.

  -c | --command
    Execute next argument as a command; if command fails, console application
    will exit with status 1; otherwise, console will exit with status 0 after
    processing the rest of the command line arguments.

  -e | --exit
    Exit with status 0 after processing of all preceding arguments.

Arguments that are not options are treated as names of scripts (collections of
console commands). They will be loaded and executed and, if there are errors,
console application will exit with status 1. If it is necessary to quit after
execution of a script even if there were no errors, you should either add
'exit' (or 'quit', or 'bye') command to the end of the script, or use '-e' (or
'--exit') option right after it.

If no scripts were specified on the command line, console will try to load
'cybercache.cfg' (if it exists), and will exit if it generates an error.

After processing the scripts (either specified as arguments, or the default
configuration file), console will enter interactive mode; use '?' or 'help'
interactive commands to get full list of commands supported by the console, as
well as some usage tips.
)C3_CL_HELP";

  print_version();
  std::printf(help_text, exe_path);
}

///////////////////////////////////////////////////////////////////////////////
// CONSOLE ENTRY POINT
///////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {

  // 1) Initialize libraries and objects
  // -----------------------------------

  ConsoleMemoryInterface memory_handler;
  Memory::configure(&memory_handler);
  C3_DEBUG(syslog_open("C3Console", false));
  NetworkConfiguration::set_sync_io(true);
  LineEditor line_editor;
  ConsoleParser parser(0, true);

  // 2) See if we have to print help or go into quiet mode
  // -----------------------------------------------------

  bool quiet = false;
  for (int i = 1; i < argc; i++) {
    const char* option = argv[i];
    if (is_option(option, 'h', "help")) {
      print_help(argv[0]);
      std::exit(EXIT_FAILURE);
    } else if (is_option(option, 'q', "quiet")) {
      ConsoleLogger::set_log_level(LL_ERROR);
      quiet = true;
    }
  }

  if (!quiet) {
    print_version();
  }

  // 3) Process commands and scripts
  // -------------------------------

  bool command_executed = false;
  bool script_executed = false;
  bool next_is_command = false;
  for (int j = 1; j < argc; j++) {
    const char* arg = argv[j];
    if (is_option(arg, 'c', "command")) {
      if (j == argc - 1 || next_is_command) {
        std::printf("ERROR: command is expected after -c and --command options");
        std::exit(EXIT_FAILURE);
      }
      next_is_command = true;
    } else if (is_option(arg, 'e', "exit")) {
      std::exit(EXIT_SUCCESS);
    } else if (!is_option(arg, 'q', "quiet")) {
      cc_log.reset();
      cc_result.reset_changed_state();
      if (next_is_command) {
        if (!execute_command(parser, arg)) {
          std::exit(EXIT_FAILURE);
        }
        next_is_command = false;
        command_executed = true;
      } else {
        execute_script(parser, arg);
        script_executed = true;
      }
    }
  }

  if (command_executed) {
    std::exit(EXIT_SUCCESS);
  }

  // 4) Process default configuration file if no commands were executed
  // ------------------------------------------------------------------

  if (!script_executed) {
    const char path[] = "cybercache.cfg";
    if (c3_file_access(path, AM_READABLE)) {
      execute_script(parser, path);
    }
  }

  // 5) Main loop: read and execute user commands
  // --------------------------------------------

  parser.set_exit_on_errors(false);
  for (;;) {
    const char* text = line_editor.get_line("command>", 1);
    if (text != nullptr) {
      cc_log.reset();
      cc_result.reset_changed_state();
      std::printf("\n");
      execute_command(parser, text);
    } else {
      /*
       * Line editor returns `NULL` upon Ctrl-C/Ctrl-Break; currently, we do not intercept `SIGINT`, so
       * we're not going to ever see `NULL` return, but we handle it anyway just in case.
       */
      break;
    }
  }
  exit(EXIT_SUCCESS);
}
