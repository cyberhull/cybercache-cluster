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
#include "c3_parser.h"
#include "c3_timer.h"
#include "c3_string.h"
#include "c3_errors.h"
#include "c3_files.h"

#include <cstdarg>
#include <cstdlib>
#include <cstdio>

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// TOKENS
///////////////////////////////////////////////////////////////////////////////

bool parser_token_t::is(const char* str) {
  return c3_matches(pt_data, str);
}

bool parser_token_t::get_long(c3_long_t& num) {
  c3_assert(pt_data && pt_length);
  char* actual_end;
  c3_long_t n = std::strtoll(pt_data, &actual_end, 0);
  if (actual_end == pt_data + pt_length) {
    num = n;
    return true;
  }
  return false;
}

bool parser_token_t::get_ulong(c3_ulong_t& num, char* suffux) {
  c3_assert(pt_data && pt_length);
  const char* data_end = pt_data + pt_length;
  const char* suffix_pos = data_end;
  if (suffux != nullptr) {
    suffix_pos--;
  }
  char* actual_end;
  c3_ulong_t n = std::strtoull(pt_data, &actual_end, 0);
  if (actual_end >= suffix_pos) {
    if (suffux != nullptr) {
      *suffux = actual_end < data_end? *suffix_pos: (char) 0;
    }
    num = n;
    return true;
  }
  return false;
}

bool parser_token_t::get_int(c3_int_t& num) {
  c3_long_t n;
  if (get_long(n) && n >= INT_MIN_VAL && n <= INT_MAX_VAL) {
    num = (c3_int_t) n;
    return true;
  }
  return false;
}

bool parser_token_t::get_uint(c3_uint_t& num, char* suffux) {
  c3_ulong_t n;
  if (get_ulong(n, suffux) && n <= UINT_MAX_VAL) {
    num = (c3_uint_t) n;
    return true;
  }
  return false;
}

bool parser_token_t::get_float(float& num) {
  c3_assert(pt_data && pt_length);
  char* actual_end;
  float n = std::strtof(pt_data, &actual_end);
  if (actual_end == pt_data + pt_length) {
    num = n;
    return true;
  }
  return false;
}

bool parser_token_t::get_double(double& num) {
  c3_assert(pt_data && pt_length);
  char* actual_end;
  double n = std::strtod(pt_data, &actual_end);
  if (actual_end == pt_data + pt_length) {
    num = n;
    return true;
  }
  return false;
}

bool parser_token_t::get_size(c3_ulong_t& bytes) {
  c3_ulong_t size;
  char suffix;
  if (get_ulong(size, &suffix)) {
    switch (suffix) {
      case 0:
      case 'b':
      case 'B':
        bytes = size;
        return true;
      case 'k':
      case 'K':
        bytes = kilobytes2bytes(size);
        return true;
      case 'm':
      case 'M':
        bytes = megabytes2bytes(size);
        return true;
      case 'g':
      case 'G':
        bytes = gigabytes2bytes(size);
        return true;
      case 't':
      case 'T':
        bytes = terabytes2bytes(size);
        return true;
      default:
        break;
    }
  }
  return false;
}

bool parser_token_t::get_duration(c3_uint_t& seconds) {
  c3_uint_t duration;
  char suffix;
  if (get_uint(duration, &suffix)) {
    switch (suffix) {
      case 0:
      case 's':
      case 'S':
        seconds = duration;
        return true;
      case 'm':
      case 'M':
        seconds = minutes2seconds(duration);
        return true;
      case 'h':
      case 'H':
        seconds = hours2seconds(duration);
        return true;
      case 'd':
      case 'D':
        seconds = days2seconds(duration);
        return true;
      case 'w':
      case 'W':
        seconds = weeks2seconds(duration);
        return true;
      default:
        break;
    }
  }
  return false;
}

bool parser_token_t::get_boolean(bool& value) {
  c3_assert(pt_data && pt_length);
  if (is("true") || is("yes") || is("on")) {
    value = true;
    return true;
  }
  if (is("false") || is("no") || is("off")) {
    value = false;
    return true;
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// DEFAULT HANDLERS
///////////////////////////////////////////////////////////////////////////////

ssize_t PARSER_GET_PROC(default_proc)(Parser& parser, char* buff, size_t size) {
  /*
   * We do not trigger assertion failure here because calling a non-existent "get" handler might be valid
   * under certain circumstances: for instance, name of a command w/o proper "get" handler could be
   * returned by enumeration method (say, during `GET` server command execution), and instead of checking
   * whether each command has a get handler, we call all of them, and ignore `-1` results.
   *
   * Additionally, some options may not have getters because it is impossible to fetch a meaningful value
   * at run time (e.g. hash table or queue capacity from a multi-table store).
   */
  return -1;
}

bool PARSER_SET_PROC(default_proc)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  /*
   * It is perfectly legal for a "command" not to have a setter: if the command is, for instance, some
   * sort of a counter, or a version string etc.
   */
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// PARSER
///////////////////////////////////////////////////////////////////////////////

Parser::Parser(c3_uint_t level, const parser_command_t* commands, c3_uint_t ncommands):
  pr_commands(commands), pr_ncommands(ncommands), pr_nesting_level(level) {
  pr_file_path = nullptr;
  pr_buffer = nullptr;
  pr_last_command = LAST_EXECUTED_COMMAND_PLACEHOLDER;
  pr_interactive = false;
}

ssize_t Parser::print_duration(char* buff, size_t length, c3_uint_t duration) {
  char suffix[2];
  suffix[0] = 's';
  suffix[1] = 0;
  if (duration >= 60 && duration % 60 == 0) {
    duration /= 60;
    suffix[0] = 'm';
    if (duration >= 60 && duration % 60 == 0) {
      duration /= 60;
      suffix[0] = 'h';
      if (duration >= 24 && duration % 24 == 0) {
        duration /= 24;
        suffix[0] = 'd';
        if (duration >= 7 && duration % 7 == 0) {
          duration /= 7;
          suffix[0] = 'w';
        }
      }
    }
  }
  return std::snprintf(buff, length, "%u%s", duration, suffix);
}

ssize_t Parser::print_size(char* buff, size_t length, c3_ulong_t size) {
  char suffix[2];
  suffix[0] = 'b';
  suffix[1] = 0;
  if (size >= 1024 && size % 1024 == 0) {
    size /= 1024;
    suffix[0] = 'k';
    if (size >= 1024 && size % 1024 == 0) {
      size /= 1024;
      suffix[0] = 'm';
      if (size >= 1024 && size % 1024 == 0) {
        size /= 1024;
        suffix[0] = 'g';
        if (size >= 1024 && size % 1024 == 0) {
          size /= 1024;
          suffix[0] = 't';
        }
      }
    }
  }
  return std::snprintf(buff, length, "%llu%s", size, suffix);
}

void Parser::on_unknown_set(const char* name) {
  log_status(LL_ERROR, "unknown statement: '%s'", name);
}

void Parser::on_set_error(const char* name) {
  log_command_status(LL_WARNING, "could not set option value");
}

void Parser::on_unknown_get(const char* name) {
  // do nothing by default
}

void Parser::on_get_error(const char* name) {
  // do nothing by default
}

void Parser::initialize_commands(parser_command_t* commands, size_t num) {
  std::qsort(commands, num, sizeof(parser_command_t), [](const void *p1, const void *p2) -> int {
    auto c1 = (parser_command_t*) p1;
    auto c2 = (parser_command_t*) p2;
    return std::strcmp(c1->pc_name, c2->pc_name);
  });
}

void Parser::log_status(log_level_t level, const char* format, ...) {
  char message[2048];
  va_list args;
  va_start(args, format);
  std::vsnprintf(message, sizeof message, format, args);
  va_end(args);
  log(level, "%s:%u : %s", pr_what, pr_line, message);
}

void Parser::log_command_status(log_level_t level, const char* format, ...) {
  char message[2048];
  va_list args;
  va_start(args, format);
  std::vsnprintf(message, sizeof message, format, args);
  va_end(args);
  assert(pr_nargs && pr_args[0].pt_data);
  log(level, "%s:%u [%s] %s", pr_what, pr_line, pr_args[0].pt_data, message);
}

void Parser::log_command_error(const char* format, ...) {
  char message[2048];
  va_list args;
  va_start(args, format);
  std::vsnprintf(message, sizeof message, format, args);
  va_end(args);
  assert(pr_nargs && pr_args[0].pt_data);
  log(LL_ERROR, "%s:%u [%s] %s", pr_what, pr_line, pr_args[0].pt_data, message);
}

void Parser::log_error(const char* format, ...) {
  char message[2048];
  va_list args;
  va_start(args, format);
  int length = std::vsnprintf(message, sizeof message, format, args);
  va_end(args);
  if (pr_interactive) {
    log_message(LL_ERROR, message, length);
  } else {
    assert(pr_nargs && pr_args[0].pt_data);
    log(LL_ERROR, "%s:%u [%s] %s", pr_what, pr_line, pr_args[0].pt_data, message);
  }
}

bool Parser::skip_ws() {
  while (pr_pos < pr_size) {
    switch (pr_buffer[pr_pos]) {
      case '\r': {
        size_t next_pos = pr_pos + 1;
        if (next_pos < pr_size && pr_buffer[next_pos] == '\n') {
          pr_pos = next_pos;
        }
        // fall through
      }
      case '\n':
        pr_line++;
        // fall through
      case ' ':
      case '\t':
      case '\v': // we have no good way to account for "form feed", so just count it as a space
        pr_pos++;
        continue;
      case '#':
        do { // skip comment
          pr_pos++;
        } while (pr_pos < pr_size && pr_buffer[pr_pos] != '\n' && pr_buffer[pr_pos] != '\r');
        continue;
      default:
        return true; // found a non-WS character at `pr_pos`
    }
  }
  return false; // end-of-buffer reached, and we did not find any more non-WS characters
}

bool Parser::skip_line_ws() {
  while (pr_pos < pr_size) {
    switch (pr_buffer[pr_pos]) {
      case '\\': {
        bool new_line = false;
        size_t next_pos = pr_pos + 1;
        if (next_pos < pr_size && pr_buffer[next_pos] == '\r') {
          new_line = true;
          pr_pos = next_pos;
          next_pos++;
        }
        if (next_pos < pr_size && pr_buffer[next_pos] == '\n') {
          new_line = true;
          pr_pos = next_pos;
        }
        if (new_line) {
          pr_line++;
        } else {
          log_status(LL_ERROR, "backslash outside of quoted string can only be followed by new line");
        }
        // fall through
      }
      case ' ':
      case '\t':
      case '\v': // we have no good way to account for "form feed", so just count it as a space
        pr_pos++;
        continue;
      case '\n':
      case '\r':
      case '#': // no [more] arguments
        return false;
      default: // found [another] argument
        return true;
    }
  }
  return false; // end-of-buffer; no [more] arguments
}

const char* Parser::get_token(c3_uint_t& length) {
  c3_uint_t start_pos = pr_tokpos;
  length = 0;
  // this method is only called when we know there is at least one character available
  char delimiter = pr_buffer[pr_pos];
  switch (delimiter) {
    case '\'':
    case '\"':
    case '`':
      pr_pos++; // yep, we do have a delimiter
      break;
    default:
      delimiter = 0;
  }
  bool done = false;
  char c = 0;
  while (!done && pr_pos < pr_size) {
    if (pr_tokpos >= PARSER_TOTAL_ARGS_SIZE) {
      log_status(LL_ERROR, "statement too long");
      return nullptr;
    }
    c = pr_buffer[pr_pos];
    switch (c) {
      case ' ':
      case '\t':
      case '#':
        if (delimiter != '\0') {
          pr_tokens[pr_tokpos++] = c;
          pr_pos++;
          break;
        }
        // fall through
      case '\v':
      case '\r':
      case '\n':
        done = true;
        break;
      case '\\': // en escape sequence
        if (++pr_pos < pr_size) {
          char esc = pr_buffer[pr_pos];
          switch (esc) {
            case '\r':
              if (delimiter != '\0') {
                size_t next_pos = pr_pos + 1;
                if (next_pos < pr_size && pr_buffer[next_pos] == '\n') {
                  pr_pos = next_pos;
                }
              }
              // fall through
            case '\n':
              if (delimiter != '\0') {
                pr_pos++;
                pr_line++;
              } else {
                // next `skip_line_ws()` call will eat up CR/LF
                done = true;
              }
              continue;
            case 'r':
              esc = '\r';
              break;
            case 'n':
              esc = '\n';
              break;
            case 't':
              esc = '\t';
              // fall through
            case '\\':
            case '\'':
            case '\"':
            case '`':
              break;
            default:
              if (is_valid_xdigit(esc)) {
                size_t second_xdigit = pr_pos + 1;
                if (second_xdigit < pr_size) {
                  char esc2 = pr_buffer[second_xdigit];
                  if (is_valid_xdigit(esc2)) {
                    pr_pos = second_xdigit;
                    esc = (char)((xdigit_value(esc) << 4) + xdigit_value(esc2));
                  } else {
                    log_status(LL_ERROR, "ill-formed escape sequence: '\\%c%c'", esc, esc2);
                    return nullptr;
                  }
                } else {
                  log_status(LL_ERROR, "incomplete escape sequence: '\\%c'", esc);
                  return nullptr;
                }
              } else {
                log_status(LL_ERROR, "invalid escape sequence: '\\%c'", esc);
                return nullptr;
              }
          }
          pr_tokens[pr_tokpos++] = esc;
          pr_pos++;
        } else {
          log_status(LL_ERROR, "trailing backslash");
          return nullptr;
        }
        break;
      default:
        if (c && c == delimiter) {
          pr_pos++;
          done = true;
        } else {
          pr_tokens[pr_tokpos++] = c;
          pr_pos++;
        }
    }
  }
  if (delimiter != '\0' && c != delimiter) {
    log_status(LL_ERROR, "missing closing quote (%c)", delimiter);
    return nullptr;
  }
  if (pr_tokpos > start_pos || delimiter != '\0') {
    length = pr_tokpos - start_pos;
    pr_tokens[pr_tokpos++] = 0;
    return pr_tokens + start_pos;
  }
  // if we're here, it means the token was something like a single ill-formed escape sequence; the
  // problem should have been reported (logged) already
  return nullptr;
}

bool Parser::add_argument(const char* token, c3_uint_t length) {
  if (pr_nargs < PARSER_MAX_ARGS) {
    pr_args[pr_nargs].pt_data = token;
    pr_args[pr_nargs].pt_length = length;
    pr_nargs++;
    return true;
  }
  log_command_status(LL_ERROR, "too many arguments");
  return false;
}

parser_command_t* Parser::find_command(const char* name) {
  auto command = (parser_command_t*) std::bsearch(name, pr_commands, pr_ncommands,
    sizeof(parser_command_t), [](const void *p1, const void *p2) -> int {
      auto n1 = (const char*) p1;
      auto c1 = (parser_command_t*) p2;
      return std::strcmp(n1, c1->pc_name);
    });
  return command;
}

bool Parser::parse(const char* what, const char* buffer, size_t size, bool interactive) {
  c3_assert(what && buffer);
  // initialize internal state
  pr_what = what;
  pr_buffer = buffer;
  pr_size = size;
  pr_pos = 0;
  pr_line = 1;
  pr_interactive = interactive;

  // main loop
  bool result = true;
  while (skip_ws()) { // skip whitespace up to the next command
    // prepare to receive another command and its arguments
    pr_tokpos = 0;
    pr_nargs = 0;
    // fetch command
    bool statement_is_fully_processed = false;
    c3_uint_t length;
    const char* command = get_token(length);
    if (command != nullptr && add_argument(command, length)) {
      parser_command_t* cmd = find_command(command);
      if (cmd != nullptr) {
        statement_is_fully_processed = true;
        // collect command arguments
        while (skip_line_ws()) {
          const char* arg = get_token(length);
          if (!add_argument(arg, length)) {
            statement_is_fully_processed = false;
            result = false;
            break;
          }
        }
        // invoke command
        if (statement_is_fully_processed) {
          pr_last_command = cmd->pc_name;
          if (!cmd->pc_set_proc(*this, pr_args + 1, pr_nargs - 1)) {
            on_set_error(command);
            result = false;
          }
        }
      } else {
        on_unknown_set(command);
        result = false;
      }
    }
    // consume unprocessed arguments of an unknown command, if any
    if (!statement_is_fully_processed) {
      while (skip_line_ws()) {
        get_token(length);
      }
    }
  }
  pr_buffer = nullptr;
  return result;
}

bool Parser::parse(const char* path, Memory& memory) {
  c3_assert(path);
  size_t size;
  auto buffer = (char*) c3_load_file(path, size, memory);
  if (buffer != nullptr) {
    log(LL_VERBOSE, "Parsing '%s'...", path);
    bool result;
    if (size > 0) {
      const char* old_path = pr_file_path;
      pr_file_path = path;
      result = parse(path, buffer, size, false);
      pr_file_path = old_path;
    } else {
      result = true;
    }
    memory.free(buffer, size + 1);
    return result;
  }
  log(LL_ERROR, "Could not load '%s' (%s)", path, c3_get_error_message());
  return false;
}

ssize_t Parser::query(const char* command, char* buff, size_t size) {
  c3_assert(command && buff && size);
  parser_command_t* cmd = find_command(command);
  if (cmd != nullptr) {
    pr_last_command = cmd->pc_name;
    ssize_t nchars = cmd->pc_get_proc(*this, buff, size);
    if (nchars <= 0) {
      on_get_error(command);
    }
    return nchars;
  }
  on_unknown_get(command);
  return -1;
}

c3_uint_t Parser::enumerate(const char* mask, parser_enum_proc_t callback, void* context) {
  c3_assert(mask && callback);
  StringMatcher matcher(mask);
  for (c3_uint_t i = 0; i < pr_ncommands; i++) {
    const char* name = pr_commands[i].pc_name;
    if (matcher.matches(name)) {
      if (!callback(context, name)) {
        break;
      }
    }
  }
  return matcher.get_num_matches();
}

}
