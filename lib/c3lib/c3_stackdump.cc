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
#include "c3_stackdump.h"

#if C3_STACKDUMP_ENABLED

#include "c3_types.h"
#include "c3_errors.h"

#include <mutex>
#include <cstdio>

#ifdef C3_CYGWIN

#include <unwind.h>

#else // !CYGWIN

#include <regex>
#include <execinfo.h>
#include <cxxabi.h>

#endif // CYGWIN

namespace CyberCache {

static std::mutex stacktrace_mutex; // only one stack trace at a time is allowed

#ifdef C3_CYGWIN

struct trace_arg_t {
  int   ta_depth; // current stack depth
  FILE* ta_file;  // where to output stack trace result
};

static _Unwind_Reason_Code trace_function(_Unwind_Context* ctx, void* trace_arg) {
  trace_arg_t *arg = (trace_arg_t *) trace_arg;
  _Unwind_Ptr ptr = _Unwind_GetIP(ctx);
  fprintf(arg->ta_file, "#%02d: %p\n", arg->ta_depth++, (void*) ptr);
  return _URC_NO_REASON;
}

static bool dump_stack_frame(FILE* file, bool include_caller) {
  fprintf(file, "\nStarting stack frame dump:\n");
  trace_arg_t arg { 0, file };
  _Unwind_Backtrace(&trace_function, &arg);
  fprintf(file, "End of stack frame dump.\n");
  fflush(file);
  return true;
}

#else // !C3_CYGWIN

static void dump_function(FILE* file, const char* symbol, int depth) {
  //
  // The below regular expression corresponds to the following:
  //
  //  ^
  //  (
  //    [^\(\[\s]+
  //  )
  //  \s*
  //  (
  //  \(
  //    ([^\+\)]+)
  //    (\+([^\)]+))?
  //  \)
  //  )?
  //  \s*
  //  \[([^\]]+)\]
  //  \s*
  //  $
  //
  std::regex re("^([^\\(\\[\\s]+)\\s*(\\(([^\\+\\)]*)(\\+([^\\)]+))?\\))?\\s*\\[([^\\]]+)\\]\\s*$");
  std::cmatch match;
  if (std::regex_match(symbol, match, re)) {
    if (match.size() == 7) {
      // extract symbol information
      std::csub_match module_part = match[1];
      const char* module_name = module_part.first;
      int module_len = (int) module_part.length();
      std::csub_match func_part = match[3];
      const char* func_name = func_part.first;
      int func_len = (int) func_part.length();
      std::csub_match offset_part = match[5];
      const char* offset_str = offset_part.first;
      int offset_len = (int) offset_part.length();
      std::csub_match address_part = match[6];
      const char* address_str = address_part.first;
      int address_len = (int) address_part.length();
      char* unmangled_name = nullptr;
      const char* uname = "<static/unnamed function>";
      const char* status_text = "nothing to unmangle";
      char func_name_z[func_len + 1];
      if (func_len > 0) {
        // unmangle the name
        std::memcpy(func_name_z, func_name, (size_t) func_len);
        func_name_z[func_len] = 0;
        int status;
        unmangled_name = abi::__cxa_demangle(func_name_z, 0, 0, &status);
        uname = unmangled_name? unmangled_name: func_name_z;
        switch (status) {
          case 0:
            status_text = "successfully unmangled";
            break;
          case -1:
            status_text = "memory allocation error";
            break;
          case -2:
            status_text = "not a mangled name";
            break;
          case -3:
            status_text = "invalid argument";
            break;
          default:
            status_text = "unknown error";
            break;
        }
      }
      fprintf(file, "%d) Module %.*s at %.*s [disp %.*s] (%s):\n  %s\n",
        depth, module_len, module_name, address_len, address_str, offset_len, offset_str, status_text, uname);
      free(unmangled_name);
    } else {
      fprintf(file, "ERROR: Unrecognized symbol format (matches = %lu):\n  '%s'\n", match.size(), symbol);
    }
  } else {
    fprintf(file, "ERROR: Unrecognized symbol format:\n  '%s'\n", symbol);
  }
}

/*
 * Even though this method is very unlikely to be inlined, we need to *guarantee* it,
 * hence 'noinline' attrubite; otherwise, we'd risk starting stack trace dump at wrong
 * depth, and would skip the method where problem has occurred.
 */
static bool dump_stack_frame(FILE* file, bool include_caller) __attribute__((noinline));

static bool dump_stack_frame(FILE* file, bool include_caller) {
  bool result = false;
  fprintf(file, "\nStarting stack frame dump (caller is %scluded):\n", include_caller? "in": "ex");
  const int MAX_DEPTH = 64;
  void* pointers[MAX_DEPTH];
  int depth = backtrace(pointers, MAX_DEPTH);
  char** names = backtrace_symbols(pointers, depth);
  if (names) {
    int from = 2; // skip `c3_xxx_stackdump()` and `dump_stack_frame()`
    if (!include_caller) {
      from++;
    }
    for (int i = from; i < depth; i++) {
      dump_function(file, names[i], i - from);
    }
    free(names);
    result = true;
  } else {
    fprintf(file, "ERROR: Could not fetch symbols. Was executable compiled with correct flags? (-g -rdynamic).\n");
  }
  fprintf(file, "End of stack frame dump.\n");
  return result;
}

#endif // C3_CYGWIN

void c3_show_stackdump(bool include_caller) {
  std::lock_guard<std::mutex> lock(stacktrace_mutex);
  dump_stack_frame(stderr, include_caller);
}

bool c3_save_stackdump(const char* path, bool include_caller) {
  std::lock_guard<std::mutex> lock(stacktrace_mutex);
  FILE* file = fopen(path, "a");
  if (file) {
    bool result = dump_stack_frame(file, include_caller);
    fclose(file);
    return result;
  } else {
    c3_set_stdlib_error_message();
    return false;
  }
}

} // CyberCache

#endif // C3_STACKDUMP_ENABLED
