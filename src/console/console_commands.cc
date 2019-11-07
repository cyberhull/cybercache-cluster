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
#include "net_configuration.h"
#include "console_commands.h"
#include "command_help.h"
#include "line_input.h"

namespace CyberCache {

LogList    cc_log;
Result     cc_result;
CyberCache cc_server;

///////////////////////////////////////////////////////////////////////////////
// String File
///////////////////////////////////////////////////////////////////////////////

/**
 * Helper class for commands that send local files as strings; the strings are "binary" in that they may
 * contain zero bytes.
 */
class StringFile {
  char*  sf_text;   // file contents as a '\0'-terminated string
  size_t sf_length; // size of the contents not counting terminatind '\0' byte

  void init() {
    sf_text = nullptr;
    sf_length = 0;
  }
  void dispose();

public:
  StringFile() { init(); }
  explicit StringFile(const char* path) {
    init();
    load(path);
  }
  StringFile(const StringFile&) = delete;
  StringFile(StringFile&&) = delete;
  ~StringFile() { dispose(); }

  StringFile& operator=(const StringFile&) = delete;
  StringFile& operator=(StringFile&&) = delete;

  bool load(const char* path);
  void set_contents(const char* str);
  size_t get_length() const { return sf_length; };
  const char* get_string() const { return sf_text; }
};

void StringFile::dispose() {
  if (sf_text != nullptr) {
    free_memory(sf_text, sf_length + 1);
    init();
  }
}

bool StringFile::load(const char* path) {
  dispose();
  sf_text = (char*) c3_load_file(path, sf_length, global_memory);
  return sf_text != nullptr;
}

void StringFile::set_contents(const char* str) {
  dispose();
  if (str != nullptr) {
    sf_length = std::strlen(str);
    sf_text = (char*) allocate_memory(sf_length + 1);
    std::strcpy(sf_text, str);
  }
}

///////////////////////////////////////////////////////////////////////////////
// LogList
///////////////////////////////////////////////////////////////////////////////

void LogList::print_all() {
  for (c3_uint_t i = 0; i < get_count(); i++) {
    std::printf("%s\n", get(i));
  }
}

void LogList::reset() {
  ll_num_errors = 0;
  remove_all();
}

///////////////////////////////////////////////////////////////////////////////
// ConsoleLogger
///////////////////////////////////////////////////////////////////////////////

log_level_t ConsoleLogger::cl_log_level = LL_NORMAL;

bool ConsoleLogger::log_message(log_level_t level, const char* message, int length) {
  c3_assert(message && length > 0);
  const char* prefix;
  switch (level) {
    case LL_FATAL:
      prefix = "[fatal error] ";
      break;
    case LL_ERROR:
      cc_log.increment_error_number();
      prefix = "[error] ";
      break;
    case LL_WARNING:
      prefix = "[warning] ";
      break;
    default:
      c3_assert(level > LL_INVALID && level < LL_NUMBER_OF_ELEMENTS);
      prefix = "";
  }
  if (level <= cl_log_level) {
    const size_t prefix_length = std::strlen(prefix);
    const size_t total_length = prefix_length + length + 1;
    char buffer[total_length];
    std::memcpy(buffer, prefix, prefix_length);
    std::memcpy(buffer + prefix_length, message, (size_t) length);
    buffer[total_length - 1] = 0;
    cc_log.add_message(buffer);
    return true;
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// HELPERS / UTILITIES
///////////////////////////////////////////////////////////////////////////////

static bool has_no_args(Parser& parser, c3_uint_t num) {
  if (num > 0) {
    parser.log_error("Command '%s' does not accept any arguments.", parser.get_command_name());
    return false;
  }
  return true;
}

static bool has_args(Parser& parser, c3_uint_t num) {
  if (num == 0) {
    parser.log_error("Command '%s' requires at least one argument.", parser.get_command_name());
    return false;
  }
  return true;
}

static bool has_required_args(Parser& parser, c3_uint_t num, c3_uint_t required) {
  if (num != required) {
    parser.log_error("Command '%s' requires exactly %u argument%s.",
      parser.get_command_name(), required, plural(required));
    return false;
  }
  return true;
}

static bool has_one_arg(Parser& parser, c3_uint_t num) {
  return has_required_args(parser, num, 1);
}

static bool more_than_one_arg(Parser& parser) {
  parser.log_error("Command '%s' expects at most one argument.", parser.get_command_name());
  return false;
}

static bool exit_app(Parser& parser, c3_uint_t num) {
  if (has_no_args(parser, num)) {
    parser.log(LL_TERSE, "Good bye.");
    cc_log.print_all();
    /*
     * At this point, script buffer is still allocated if parse(path) was used to actually
     * parse the script. If we exit here, that buffer (and, potentially, other buffers as well) would
     * be reported as "still reachable". To test memory allocation integrity, scripts without
     * "exit" statements should be used.
     */
    exit(EXIT_SUCCESS);
  }
  return false;
}

static void set_password(Parser& parser, const char* password, bool admin) {
  if (password != nullptr && password[0] == 0) {
    password = nullptr;
  }
  const char* type;
  if (admin) {
    console_net_config.set_admin_password(password);
    type = "Administrative";
  } else {
    console_net_config.set_user_password(password);
    type = "User";
  }
  parser.log(LL_NORMAL, "%s password had been %s.", type, password != nullptr? "set": "RESET");
}

static bool process_password(Parser& parser, parser_token_t* args, c3_uint_t num, bool admin) {
  switch (num) {
    case 0:
      if (parser.is_interactive()) {
        LineInput line_input;
        const char* password = line_input.get_password("password>", 1);
        LineInput::line_feed();
        if (password != nullptr) {
          set_password(parser, password, admin);
        } else {
          parser.log(LL_NORMAL, "Password entry cancelled (previous, if any, remains in effect).");
        }
        return true;
      } else {
        parser.log_command_error("Cannot interactively enter password in batch mode.");
        return false;
      }
    case 1: {
      parser_token_t& arg = args[0];
      if (arg.is("-")) {
        set_password(parser, nullptr, admin);
        return true;
      } else if (arg.is("?")) {
        c3_hash_t hash = admin? console_net_config.get_admin_password():
          console_net_config.get_user_password();
        parser.log(LL_EXPLICIT, "Current %s password is %s.",
          admin? "administrative" : "user",
          hash == INVALID_HASH_VALUE? "EMPTY": "valid");
        return true;
      } else if (parser.is_interactive()) {
        parser.log_error("Password should not be specified as argument in interactive mode.");
        return false;
      } else {
        set_password(parser, arg.get_string(), admin);
        return true;
      }
    }
    default:
      return more_than_one_arg(parser);
  }
}

static void print_lifetime(char* buffer, size_t length) {
  c3_int_t lifetime = cc_server.get_lifetime();
  switch (lifetime) {
    case -1:
      std::snprintf(buffer, length, "-1 (use default)");
      break;
    case 0:
      std::snprintf(buffer, length, "0 (infinite)");
      break;
    default:
      Parser::print_duration(buffer, length, (c3_uint_t) lifetime);
  }
}

static bool get_positive_number(Parser& parser, parser_token_t& arg, c3_uint_t& num, const char* name) {
  c3_uint_t number;
  if (arg.get_uint(number) && number > 0) {
    num = number;
    return true;
  } else {
    parser.log_error("Invalid <%s> argument (positive integer expected): '%s'",
      name, arg.get_string());
    return false;
  }
}

static c3_uint_t get_default_display_count() {
  c3_uint_t count = cc_server.get_count();
  if (count == 0) {
    // another server command had been executed since last RESULT/NEXT
    count = 1;
    if (cc_result.is_list()) {
      count = Result::get_lines_per_screen();
    } else if (cc_result.is_array()) {
      count = Result::get_lines_per_screen() * Result::get_bytes_per_line();
    }
  }
  return count;
}

static bool get_display_count(Parser& parser, parser_token_t& arg, c3_uint_t& count) {
  return get_positive_number(parser, arg, count, "number");
}

static bool get_domain_mode(Parser& parser, parser_token_t* args, c3_uint_t num, c3_uint_t& mode) {
  if (num > 0) {
    mode = 0;
    c3_uint_t i = 0;
    do {
      parser_token_t& arg = args[i];
      if (arg.is("global")) {
        mode |= DM_GLOBAL;
      } else if (arg.is("session")) {
        mode |= DM_SESSION;
      } else if (arg.is("fpc")) {
        mode |= DM_FPC;
      } else if (arg.is("all")) {
        mode |= DM_ALL;
      } else {
        parser.log_error("Invalid domain ID: '%s'", arg.get_string());
        return false;
      }
    } while (++i < num);
  } else {
    mode = DM_ALL;
  }
  return true;
}

static bool always_empty_set(Parser& parser, const char* command) {
  parser.log(LL_WARNING, "Command not sent: '%s' w/o tags always returns empty set.", command);
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// COMMAND HANDLERS
///////////////////////////////////////////////////////////////////////////////

static bool PARSER_SET_PROC(help)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  switch (num) {
    case 0:
      get_help(parser);
      return true;
    case 1:
      return get_help(parser, args[0].get_string());
    default:
      return more_than_one_arg(parser);
  }
}

static bool PARSER_SET_PROC(version)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_no_args(parser, num)) {
    parser.log(LL_EXPLICIT,
      "CyberCache Cluster Console %s\nWritten by Vadim Sytnikov\nCopyright (C) 2016-2019 CyberHULL.",
      c3lib_version_build_string);
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(verbosity)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  static const char* levels[] = {
    "invalid", // never matched against input
    "explicit",
    "fatal",
    "error",
    "warning",
    "terse",
    "normal",
    "verbose",
    "debug"
  };
  static_assert(LL_NUMBER_OF_ELEMENTS == 9, "Number of log levels has changed");
  switch (num) {
    case 0:
      parser.log(LL_EXPLICIT, "Current verbosity level '%s'.", levels[ConsoleLogger::get_log_level()]);
      return true;
    case 1:
      for (c3_uint_t i = 1 /* skip 'invalid' */; i < LL_NUMBER_OF_ELEMENTS; i++) {
        const char* level = levels[i];
        if (args[0].is(level)) {
          ConsoleLogger::set_log_level((log_level_t) i);
          parser.log(LL_NORMAL, "Verbosity level set to '%s'.", level);
          return true;
        }
      }
      parser.log_error("Unknown verbosity level: '%s'", args[0].get_string());
      return false;
    default:
      return more_than_one_arg(parser);
  }
}

static bool PARSER_SET_PROC(execute)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_one_arg(parser, num)) {
    c3_uint_t level = parser.get_nesting_level() + 1;
    if (level < MAX_CONFIG_INCLUDE_LEVEL) {
      auto parent_parser = (ConsoleParser&) parser;
      ConsoleParser exec_parser(level, parent_parser.get_exit_on_errors());
      const char* path = args[0].get_string();
      // the following call puts parser into "non-interactive" mode internally
      return exec_parser.parse(path, global_memory);
    } else {
      parser.log_error("Too many nested 'EXECUTE' statements (%u is maximum nesting level).",
        MAX_CONFIG_INCLUDE_LEVEL);
    }
  }
  return false;
}

static bool PARSER_SET_PROC(exit)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return exit_app(parser, num);
}

static bool PARSER_SET_PROC(quit)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return exit_app(parser, num);
}

static bool PARSER_SET_PROC(bye)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return exit_app(parser, num);
}

static bool PARSER_SET_PROC(connect)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t port_num;
  c3_ushort_t port = C3_DEFAULT_PORT;
  switch (num) {
    case 2:
      if (!args[1].get_uint(port_num) || port_num < 1000 || port_num > USHORT_MAX_VAL) {
        parser.log_error("Port number not in 1000..%u: '%s'",
          USHORT_MAX_VAL, args[1].get_string());
        return false;
      }
      port = (c3_ushort_t) port_num;
      // fall through
    case 1:
      cc_server.set_port(port);
      if (!cc_server.set_address(args[0].get_string())) {
        parser.log_error("Invalid address [%s].", c3_get_error_message());
        return false;
      }
      // fall through
    case 0:
      parser.log(LL_EXPLICIT, "Will connect to '%s', port %hu",
        cc_server.get_address(), cc_server.get_port());
      return true;
    default:
      parser.log_error("Command 'connect' expects zero to two arguments.");
      return false;
  }
}

static bool PARSER_SET_PROC(persistent)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  switch (num) {
    case 0:
      parser.log(LL_EXPLICIT, "Persistent connections are currently %s",
         cc_server.is_persistent()? "ON": "OFF");
      return true;
    case 1: {
      bool persistent;
      parser_token_t& arg = args[0];
      if (arg.get_boolean(persistent)) {
        cc_server.set_persistent(persistent);
        parser.log(LL_NORMAL, "Persistent connections set to %s", persistent? "ON": "OFF");
        return true;
      } else {
        parser.log_error("Ill-formed boolean argument: %s", arg.get_string());
        return false;
      }
    }
    default:
      return more_than_one_arg(parser);
  }
}

static bool PARSER_SET_PROC(user)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return process_password(parser, args, num, false);
}

static bool PARSER_SET_PROC(admin)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  return process_password(parser, args, num, true);
}

static bool PARSER_SET_PROC(useragent)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  static const char* agents[] = {
    "unknown",
    "bot",
    "warmer",
    "user"
  };
  switch (num) {
    case 0:
      parser.log(LL_EXPLICIT, "Currently active user agent is '%s'.", agents[cc_server.get_user_agent()]);
      return true;
    case 1:
      for (c3_uint_t ua = UA_UNKNOWN; ua < UA_NUMBER_OF_ELEMENTS; ua++) {
        const char* agent = agents[ua];
        if (args[0].is(agent)) {
          cc_server.set_user_agent((user_agent_t) ua);
          parser.log(LL_NORMAL, "User agent set to '%s'.", agent);
          return true;
        }
      }
      parser.log_error("Unknown user agent: '%s'", args[0].get_string());
      return false;
    default:
      return more_than_one_arg(parser);
  }
}

static bool PARSER_SET_PROC(tags)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (num > 0) {
    cc_server.remove_all_tags();
    parser.log(LL_NORMAL, "Removed all tags from current set.");
    c3_uint_t i = 0;
    do {
      const char* tag = args[i].get_string();
      if (cc_server.add_tag(tag)) {
        parser.log(LL_NORMAL, "Added tag '%s' to the set.", tag);
      } else {
        parser.log(LL_WARNING, "Tag '%s' has already been added to the set.", tag);
      }
    } while (++i < num);
  } else {
    c3_uint_t num_tags = cc_server.get_num_tags();
    if (num_tags > 0) {
      parser.log(LL_EXPLICIT, "Current set of tags:");
      c3_uint_t j = 0;
      do {
        parser.log(LL_EXPLICIT, "  '%s'", cc_server.get_tag(j));
      } while (++j < num_tags);
    } else {
      parser.log(LL_NORMAL, "Current set of tags is empty.");
    }
  }
  return true;
}

static bool PARSER_SET_PROC(addtags)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (num > 0) {
    c3_uint_t i = 0;
    do {
      const char* tag = args[i].get_string();
      if (cc_server.add_tag(tag)) {
        parser.log(LL_NORMAL, "Added tag '%s' to the set.", tag);
      } else {
        parser.log(LL_WARNING, "Tag '%s' was already in the set.", tag);
      }
    } while (++i < num);
    return true;
  } else {
    parser.log_error("Command 'addtags' requires at least one argument.");
    return false;
  }
}

static bool PARSER_SET_PROC(removetags)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (num > 0) {
    c3_uint_t i = 0;
    do {
      const char* tag = args[i].get_string();
      if (cc_server.remove_tag(tag)) {
        parser.log(LL_NORMAL, "Removed tag '%s' from the set.", tag);
      } else {
        parser.log(LL_WARNING, "Tag '%s' was NOT in the set.", tag);
      }
    } while (++i < num);
  } else {
    if (cc_server.get_num_tags() > 0) {
      cc_server.remove_all_tags();
      parser.log(LL_NORMAL, "Removed all tags from the set.");
    } else {
      parser.log(LL_NORMAL, "Tag set is already empty.");
    }
  }
  return true;
}

static bool PARSER_SET_PROC(lifetime)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  char buffer[32];
  switch (num) {
    case 0:
      print_lifetime(buffer, sizeof buffer);
      parser.log(LL_EXPLICIT, "Currently set lifetime is %s", buffer);
      return true;
    case 1: {
      c3_int_t lifetime;
      parser_token_t& arg = args[0];
      if (arg.is("-1")) {
        lifetime = -1;
      } else {
        c3_uint_t seconds;
        if (arg.get_duration(seconds)) {
          static const c3_uint_t MAX_DURATION = days2seconds(365);
          if (seconds > MAX_DURATION) {
            parser.log_error("Lifetime too big (cannot exceed a year): %s", arg.get_string());
            return false;
          }
          lifetime = (c3_int_t) seconds;
        } else {
          parser.log_error("Ill-formed lifetime: %s", arg.get_string());
          return false;
        }
      }
      cc_server.set_lifetime(lifetime);
      print_lifetime(buffer, sizeof buffer);
      parser.log(LL_NORMAL, "Lifetime set to %s", buffer);
      return true;
    }
    default:
      return more_than_one_arg(parser);
  }
}

static bool PARSER_SET_PROC(marker)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  switch (num) {
    case 0:
      parser.log(LL_EXPLICIT, "Integrity check marker is currently %s",
        console_net_config.get_command_integrity_check()? "ON": "OFF");
      return true;
    case 1: {
      bool marker;
      parser_token_t& arg = args[0];
      if (arg.get_boolean(marker)) {
        console_net_config.set_command_integrity_check(marker);
        parser.log(LL_NORMAL, "Integrity check marker set to %s", marker? "ON": "OFF");
        return true;
      } else {
        parser.log_error("Ill-formed boolean argument: %s", arg.get_string());
        return false;
      }
    }
    default:
      return more_than_one_arg(parser);
  }
}

static bool PARSER_SET_PROC(compressor)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  static_assert(CT_NUMBER_OF_ELEMENTS == 9, "Number of compressors is not nine");
  static const char* compressors[CT_NUMBER_OF_ELEMENTS] = {
    "none", // never matched against user input
    "lzf",
    "snappy",
    "lz4",
    "lzss3",
    "brotli",
    "zstd",
    "zlib",
    "lzham"
  };
  static_assert(CT_NUMBER_OF_ELEMENTS == 9, "Number of compressors has changed");
  switch (num) {
    case 0:
      parser.log(LL_EXPLICIT, "Currently active compressor is '%s'.",
        compressors[console_net_config.get_compressor(DOMAIN_GLOBAL)]);
      return true;
    case 1:
      for (c3_uint_t comp = CT_NONE + 1; comp < CT_NUMBER_OF_ELEMENTS; comp++) {
        const char* compressor = compressors[comp];
        if (args[0].is(compressor)) {
          #if !C3_ENTERPRISE
          if (comp == CT_BROTLI) {
            parser.log(LL_ERROR, "The 'brotli' compressor is only supported in Enterprise Edition");
            return false;
          }
          #endif
          console_net_config.set_compressor(DOMAIN_GLOBAL, (c3_compressor_t) comp);
          parser.log(LL_NORMAL, "Compressor set to '%s'.", compressor);
          return true;
        }
      }
      parser.log_error("Unknown compressor: '%s'", args[0].get_string());
      return false;
    default:
      return more_than_one_arg(parser);
  }
}

static bool PARSER_SET_PROC(threshold)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  switch (num) {
    case 0:
      parser.log(LL_EXPLICIT, "Currently active compression threshold is %u bytes.",
        console_net_config.get_compression_threshold());
      return true;
    case 1: {
      c3_uint_t threshold;
      if (get_positive_number(parser, args[0], threshold, "compression-threshold")) {
        console_net_config.set_compression_threshold(threshold);
        return true;
      }
      return false;
    }
    default:
      return more_than_one_arg(parser);
  }
}

static bool PARSER_SET_PROC(hasher)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  static_assert(HM_NUMBER_OF_ELEMENTS == 6, "Number of hashers is not six");
  static const char* hashers[HM_NUMBER_OF_ELEMENTS] = {
    "invalid", // never matched against user input
    "xxhash",
    "farmhash",
    "spookyhash",
    "murmurhash2",
    "murmurhash3"
  };
  static_assert(HM_NUMBER_OF_ELEMENTS == 6, "Number of hash methods has changed");
  switch (num) {
    case 0:
      parser.log(LL_EXPLICIT, "Currently active hash method is '%s'.",
        hashers[password_hasher.get_method()]);
      return true;
    case 1:
      for (c3_uint_t hm = HM_INVALID + 1; hm < HM_NUMBER_OF_ELEMENTS; hm++) {
        const char* hasher = hashers[hm];
        if (args[0].is(hasher)) {
          password_hasher.set_method((c3_hash_method_t) hm);
          parser.log(LL_NORMAL, "Hash method set to '%s'.", hasher);
          return true;
        }
      }
      parser.log_error("Unknown hash method: '%s'", args[0].get_string());
      return false;
    default:
      return more_than_one_arg(parser);
  }
}

static bool PARSER_SET_PROC(result)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_int_t from = 0;
  c3_uint_t count = get_default_display_count();
  switch (num) {
    case 2:
      if (!get_display_count(parser, args[1], count)) {
        return false;
      }
      // fall through
    case 1: {
      parser_token_t& arg = args[0];
      if (!arg.get_int(from)) {
        parser.log_error("Invalid <from> argument (an integer expected): '%s'", arg.get_string());
        return false;
      }
      // fall through
    }
    case 0:
      if (cc_result.is_array() || cc_result.is_list()) {
        c3_uint_t max_count = cc_result.get_num_elements();
        if (count > max_count) {
          count = max_count;
        }
        // figure out where to continue with 'next' by default
        c3_long_t max_offset = max_count;
        c3_long_t next_offset = from >= 0? (c3_long_t) from + count: (max_offset + from) + count;
        if (next_offset < 0) {
          next_offset = 0;
        } else if (next_offset > max_offset) {
          next_offset = max_offset;
        }
        cc_server.set_offset((c3_uint_t) next_offset);
        cc_server.set_count(count);
      }
      // print out requested result segment
      cc_result.print(from, count);
      return true;
    default:
      parser.log_error("Zero, one, or two arguments expected.");
      return false;
  }
}

static bool PARSER_SET_PROC(next)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t from = cc_server.get_offset();
  c3_uint_t count = get_default_display_count();
  switch (num) {
    case 1:
      if (!get_display_count(parser, args[0], count)) {
        return false;
      }
      // fall through
    case 0:
      if (cc_result.is_array() || cc_result.is_list()) {
        c3_uint_t max_count = cc_result.get_num_elements();
        if (count > max_count) {
          count = max_count;
        }
        // figure out where to continue with 'next' by default
        c3_long_t max_offset = max_count;
        if (from >= max_offset) {
          parser.log(LL_EXPLICIT, "No more elements to display.");
          return true; // not an error
        }
        c3_long_t next_offset = from + count;
        if (next_offset > max_offset) {
          next_offset = max_offset;
        }
        cc_server.set_offset((c3_uint_t) next_offset);
        cc_server.set_count(count);
      }
      // print out requested result segment
      cc_result.print(from, count);
      return true;
    default:
      return more_than_one_arg(parser);
  }
}

static bool PARSER_SET_PROC(autoresult)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  log_level_t level = LL_EXPLICIT;
  switch (num) {
    case 1: {
      parser_token_t& mode = args[0];
      if (mode.is("simple")) {
        cc_server.set_auto_result_mode(ARM_SIMPLE);
      } else if (mode.is("lists")) {
        cc_server.set_auto_result_mode(ARM_LISTS);
      } else if (mode.is("all")) {
        cc_server.set_auto_result_mode(ARM_ALL);
      } else {
        parser.log_error("Invalid AUTORESULT mode: '%s'", mode.get_string());
        return false;
      }
      level = LL_NORMAL;
      // fall through
    }
    case 0: {
      const char* modes[] = { "simple", "lists", "all" };
      parser.log(level, "Current AUTORESULT mode is '%s'",
        modes[cc_server.get_auto_result_mode()]);
      return true;
    }
    default:
      return more_than_one_arg(parser);
  }
}

static bool PARSER_SET_PROC(checkresult)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_args(parser, num)) {
    bool result = false;
    parser_token_t& mode = args[0];
    if (mode.is("ok")) {
      if (num != 1) {
        parser.log_error("OK mode does not accept extra arguments.");
        return false;
      }
      result = cc_result.is_ok();
    } else if (mode.is("error")) {
      if (cc_result.is_error()) {
        result = true;
        for (c3_uint_t i = 1; i < num; i ++) {
          if (!cc_result.contains(args[i].get_string())) {
            result = false;
            break;
          }
        }
      }
    } else if (mode.is("string")) {
      if (cc_result.is_string()) {
        result = true;
        for (c3_uint_t i = 1; i < num; i ++) {
          if (!cc_result.contains(args[i].get_string())) {
            result = false;
            break;
          }
        }
      }
    } else if (mode.is("list")) {
      if (cc_result.is_list()) {
        result = true;
        for (c3_uint_t i = 1; i < num; i ++) {
          if (!cc_result.contains(args[i].get_string())) {
            result = false;
            break;
          }
        }
      }
    } else if (mode.is("data")) {
      StringFile file;
      const char* data = nullptr;
      c3_uint_t length = 0;
      c3_uint_t offset = 0;
      if (num == 3) {
        if (!args[1].get_uint(offset)) {
          parser.log_error("Ill-formed offset argument: '%s'", args[1].get_string());
          return false;
        }
        data = args[2].get_string();
        if (data[0] == '@') {
          const char* path = data + 1;
          if (file.load(path)) {
            data = file.get_string();
            size_t size = file.get_length();
            if (size > UINT_MAX_VAL) {
              parser.log_error("Comparison DATA file too big: '%s'", path);
              return false;
            }
            length = (c3_uint_t) size;
          } else {
            parser.log_error("Could not load comparison DATA from '%s'.", path);
            return false;
          }
        } else {
          length = (c3_uint_t) std::strlen(data);
        }
      } else if (num != 1) {
        parser.log_error("DATA mode takes no, or <offset> and <bytes> arguments.");
        return false;
      }
      result = cc_result.is_array() && (data == nullptr || cc_result.contains(offset, data, length));
    } else {
      parser.log_error("Invalid check mode: '%s'", mode.get_string());
      return false;
    }
    if (result) {
      return true;
    } else {
      parser.log_error("Result check has FAILED.");
      char error_file_path[MAX_FILE_PATH_LENGTH];
      c3_get_home_path(error_file_path, sizeof error_file_path, "cybercache.console-errors");
      FILE* file = fopen(error_file_path, "w");
      if (file != nullptr) {
        std::fprintf(file, "Result check failed in '%s', line %u.\n",
          parser.get_file_path(), parser.get_line_number());
        std::fprintf(file, "Expected result (`checkresult` arguments):");
        for (c3_uint_t i = 0; i < num; i++) {
          std::fprintf(file, " '%s'", args[i].get_string());
        }
        std::fprintf(file, "\nActual result (produced by console or received from the server):\n");
        cc_result.print(file, 0, 0);
        fclose(file);
        parser.log_status(LL_NORMAL, "Error data saved to '%s'", error_file_path);
      } else {
        parser.log_error("Could not create '%s'", error_file_path);
      }
    }
  }
  return false;
}

static bool PARSER_SET_PROC(display)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t lines_per_screen = Result::get_lines_per_screen();
  c3_uint_t bytes_per_line = Result::get_bytes_per_line();
  char substitution_char = Result::get_substitution_char();
  log_level_t level = LL_EXPLICIT;
  switch (num) {
    case 3: {
      const char* subst = args[2].get_string();
      if (std::strlen(subst) != 1) {
        parser.log_error("Single substitution character expected: '%s'", subst);
        return false;
      }
      substitution_char = *subst;
      if (std::isprint(substitution_char) == 0) {
        parser.log_error("Specified substitution character is not printable.");
        return false;
      }
      // fall through
    }
    case 2:
      if (!get_positive_number(parser, args[1], bytes_per_line, "bytes-per-line")) {
        return false;
      }
      // fall through
    case 1:
      if (!get_positive_number(parser, args[0], lines_per_screen, "lines-per-screen")) {
        return false;
      }
      // now that ALL the tests were passed, we can actually change settings
      Result::set_lines_per_screen(lines_per_screen);
      Result::set_bytes_per_line(bytes_per_line);
      Result::set_substitution_char(substitution_char);
      level = LL_NORMAL;
      // fall through
    case 0:
      parser.log(level, "Current 'display' settings:");
      parser.log(level, "      Lines per screen: %u", lines_per_screen);
      parser.log(level, "        Bytes per line: %u", bytes_per_line);
      parser.log(level, "Substitution character: '%c'", substitution_char);
      return true;
    default:
      parser.log_error("The 'display' command expects not more than three arguments.");
      return false;
  }
}

static bool PARSER_SET_PROC(print)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_one_arg(parser, num)) {
    // using formatted output to process '%' characters properly (i.e. not treat them as format specifiers)
    parser.log(LL_EXPLICIT, "%s", args[0].get_string());
    return true;
  }
  return false;
}

static bool parse_command_id(Parser& parser, parser_token_t& arg, c3_byte_t& id) {
  c3_uint_t num;
  if (arg.get_uint(num) && num <= 0xFF) {
    id = (c3_byte_t) num;
    return true;
  } else {
    parser.log_error("Invalid command ID: '%s'", arg.get_string());
    return false;
  }
}

static CommandInfo parse_command_descriptor(Parser& parser, parser_token_t* args, c3_uint_t num,
  StringFile& payload, c3_uint_t& index) {
  c3_byte_t id;
  bool check = console_net_config.get_command_integrity_check();
  parser_token_t& arg = args[0];
  if (arg.is("{")) {
    // 1) command ID
    // -------------
    if (num > 1 && parse_command_id(parser, args[1], id)) {
      // 2) '}' | `admin` | `user`
      // -------------------------
      arg = args[2];
      if (num < 3 || !(arg.is("}") || arg.is("admin") || arg.is("user"))) {
        parser.log_error("'admin', 'user', or '}' expected after ID");
        return CommandInfo(CMD_INVALID);
      }
      if (arg.is("}")) {
        index = 3;
        return CommandInfo(id, false, nullptr, check);
      }
      bool admin = false;
      if (arg.is("admin")) {
        admin = true;
      }
      // 3) '}' | '-' | <password>
      // -------------------------
      if (num < 4) {
        parser.log_error("'-', password, or '}' expected after '%s'", arg.get_string());
        return CommandInfo(CMD_INVALID);
      }
      arg = args[3];
      if (arg.is("}")) {
        index = 4;
        return CommandInfo(id, admin, nullptr, check);
      }
      const char* password = nullptr;
      if (!arg.is("-")) {
        password = arg.get_string();
      }
      // 4) '}' | ['@']<payload>
      // -----------------------
      if (num < 5) {
        parser.log_error("Payload or '}' expected after '%s'", arg.get_string());
        return CommandInfo(CMD_INVALID);
      }
      arg = args[4];
      if (arg.is("}")) {
        index = 5;
        return CommandInfo(id, admin, password, check);
      }
      const char* data = arg.get_string();
      if (data[0] == '@') {
        const char* path = data + 1;
        if (!payload.load(path)) {
          parser.log_error("Could not load data from '%s'", path);
          return CommandInfo(CMD_INVALID);
        }
        if (payload.get_length() > UINT_MAX_VAL) {
          parser.log_error("Payload file '%s' is too big", path);
          return CommandInfo(CMD_INVALID);
        }
      } else {
        payload.set_contents(data);
      }
      // 5) '}' | <boolean>
      // -----------------------
      if (num < 6) {
        parser.log_error("Boolean or '}' expected after '%s'", arg.get_string());
        return CommandInfo(CMD_INVALID);
      }
      arg = args[5];
      if (arg.is("}")) {
        index = 6;
        return CommandInfo(id, admin, password, check);
      }
      if (!arg.get_boolean(check)) {
        parser.log_error("Ill-formed integrity check flag (boolean): '%s'", arg.get_string());
        return CommandInfo(CMD_INVALID);
      }
      // 6) '}'
      // ------
      if (num < 7 || !args[6].is("}")) {
        parser.log_error("Unterminated descriptor block");
      } else {
        index = 7;
        return CommandInfo(id, admin, password, check);
      }
    }
  } else {
    if (parse_command_id(parser, arg, id)) {
      index = 1;
      return CommandInfo(id, false, nullptr, check);
    }
  }
  return CommandInfo(CMD_INVALID);
}

static const StringList* parse_command_list(Parser& parser, parser_token_t* args, c3_uint_t num,
  c3_uint_t& index) {
  StringList tmp(num - index + 1);
  while (index < num) {
    parser_token_t& arg = args[index++];
    if (arg.is("]")) {
      auto list = alloc<StringList>();
      new (list) StringList(tmp.get_count() + 1);
      for (c3_uint_t i = 0; i < tmp.get_count(); i++) {
        list->add(tmp.get(i));
      }
      return list;
    } else {
      tmp.add(arg.get_string());
    }
  }
  return nullptr;
}

static bool PARSER_SET_PROC(emulate)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_args(parser, num)) {

    // 1) Get command descriptor
    // -------------------------

    c3_uint_t index = 0;
    StringFile payload;
    CommandInfo info = parse_command_descriptor(parser, args, num, payload, index);
    if (info.is_auto()) {
      // error should have been logged already
      c3_assert(cc_log.get_num_errors() && cc_log.get_num_messages());
      return false;
    }

    // 2) Collect arguments
    // --------------------

    CommandArgument arguments[num];
    char format[num];
    c3_uint_t num_arguments = 0;
    bool ok = true;
    while (index < num) {
      parser_token_t& arg = args[index++];
      const char* data = arg.get_string();
      c3_int_t signed_num;
      c3_uint_t unsigned_num;
      if (arg.is("[")) {
        const StringList* list = parse_command_list(parser, args, num, index);
        if (list != nullptr) {
          arguments[num_arguments] = CommandArgument(list);
          format[num_arguments] = 'L';
        } else {
          ok = false;
          break;
        }
      } else if (data[0] == '-' && arg.get_int(signed_num)) {
        arguments[num_arguments] = CommandArgument(signed_num);
        format[num_arguments] = 'N';
      } else if (data[0] == '%') {
        arguments[num_arguments] = CommandArgument(data + 1);
        format[num_arguments] = 'S';
      } else if (arg.get_uint(unsigned_num)) {
        arguments[num_arguments] = CommandArgument(unsigned_num);
        format[num_arguments] = 'U';
      } else {
        arguments[num_arguments] = CommandArgument(data);
        format[num_arguments] = 'S';
      }
      num_arguments++;
    }

    // 3) Execute command
    // ------------------

    if (ok) {
      format[num_arguments] = 0;
      cc_result = cc_server.emulate(info, payload.get_string(), (c3_uint_t) payload.get_length(),
        format, arguments);
    }

    // 4) Clean up lists, if any
    // -------------------------

    for (c3_uint_t i = 0; i < num_arguments; i++) {
      if (format[i] == 'L') {
        const StringList* list = arguments[i].get_list();
        list->~StringList();
        free_memory((void*) list, sizeof(StringList));
      }
    }
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(wait)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_one_arg(parser, num)) {
    c3_uint_t msecs;
    if (get_positive_number(parser, args[0], msecs, "milliseconds")) {
      if (msecs <= 60 * 1000) {
        usleep(msecs * 1000);
        return true;
      } else {
        parser.log_error("Cannot `wait` more than a minute");
      }
    }
  }
  return false;
}

static bool PARSER_SET_PROC(walk)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  parser.log(LL_EXPLICIT, "One does not simply walk into CyberCache Cluster.");
  return true;
}

static bool PARSER_SET_PROC(ping)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_no_args(parser, num)) {
    cc_result = cc_server.execute(CMD_PING);
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(check)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_no_args(parser, num)) {
    cc_result = cc_server.execute(CMD_CHECK);
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(info)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t mode;
  if (get_domain_mode(parser, args, num, mode)) {
    cc_result = cc_server.execute(CMD_INFO, "U", mode);
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(stats)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  #if C3_ENTERPRISE
  const char* mask = "*";
  if (num > 0) {
    mask = args[0].get_string();
    args++;
    num--;
  }
  c3_uint_t mode;
  if (get_domain_mode(parser, args, num, mode)) {
    cc_result = cc_server.execute(CMD_STATS, "US", mode, mask);
    return true;
  }
  #else
  parser.log_error("The 'STATS' command is only available in Enterprise edition");
  #endif // C3_ENTERPRISE
  return true; // make it possible to run automated tests (`false` would interrupt them)
}

static bool PARSER_SET_PROC(shutdown)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_no_args(parser, num)) {
    cc_result = cc_server.execute(CMD_SHUTDOWN);
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(localconfig)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_one_arg(parser, num)) {
    StringFile file;
    const char* path = args[0].get_string();
    if (file.load(path)) {
      cc_result = cc_server.execute(CMD_SET, "S", file.get_string());
      return true;
    } else {
      parser.log_error("Could not load local configuration file: '%s'", path);
    }
  }
  return false;
}

static bool PARSER_SET_PROC(remoteconfig)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_one_arg(parser, num)) {
    cc_result = cc_server.execute(CMD_LOADCONFIG, "S", args[0].get_string());
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(restore)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_one_arg(parser, num)) {
    cc_result = cc_server.execute(CMD_RESTORE, "S", args[0].get_string());
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(store)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t ua = UA_UNKNOWN;
  c3_uint_t sync = SM_NONE;
  switch (num) {
    case 4: {
      parser_token_t& arg = args[3];
      if (arg.is("none")) {
        sync = SM_NONE;
      } else if (arg.is("data-only")) {
        sync = SM_DATA_ONLY;
      } else if (arg.is("full")) {
        sync = SM_FULL;
      } else {
        parser.log_error("STORE: unrecognized synchronization mode: '%s'.", arg.get_string());
        return false;
      }
    }
    case 3: {
      parser_token_t& arg = args[2];
      if (arg.is("unknown")) {
        ua = UA_UNKNOWN;
      } else if (arg.is("bot")) {
        ua = UA_BOT;
      } else if (arg.is("warmer")) {
        ua = UA_WARMER;
      } else if (arg.is("user")) {
        ua = UA_USER;
      } else {
        parser.log_error("STORE: unrecognized user agent: '%s'.", arg.get_string());
        return false;
      }
    }
    case 2: {
      c3_uint_t domain;
      parser_token_t& arg = args[0];
      if (arg.is("all")) {
        domain = DM_SESSION | DM_FPC;
      } else if (arg.is("session")) {
        domain = DM_SESSION;
      } else if (arg.is("fpc")) {
        domain = DM_FPC;
      } else {
        parser.log_error("STORE: unrecognized domain: '%s'.", arg.get_string());
        return false;
      }
      cc_result = cc_server.execute(CMD_STORE, "USUU", domain, args[1].get_string(), ua, sync);
      return true;
    }
    default:
      parser.log_error("Command STORE expects 2..4 arguments.");
      return false;
  }
}

static bool PARSER_SET_PROC(get)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_args(parser, num)) {
    StringList list(num);
    c3_uint_t i = 0;
    do {
      list.add_unique(args[i].get_string());
    } while (++i < num);
    cc_result = cc_server.execute(CMD_GET, "L", &list);
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(set)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_required_args(parser, num, 2)) {
    const char* name = args[0].get_string();
    const char* value = args[1].get_string();
    // if value is an empty string OR starts with a space, quote it
    const char* quote = (*value != '\0' && *value != ' ')? "": "\'";
    const size_t length = std::strlen(name) + std::strlen(value) + 8;
    char buffer[length];
    int printed = snprintf(buffer, length, "%s %s%s%s", name, quote, value, quote);
    if (printed > 0) {
      cc_result = cc_server.execute(CMD_SET, "S", buffer);
      return true;
    }
    parser.log_error("Ill-formed option name and/or value.");
  }
  return false;
}

static bool PARSER_SET_PROC(log)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_one_arg(parser, num)) {
    cc_result = cc_server.execute(CMD_LOG, "S", args[0].get_string());
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(rotate)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  c3_uint_t mode;
  if (num > 0) {
    mode = 0;
    c3_uint_t i = 0;
    do {
      parser_token_t& arg = args[i];
      if (arg.is("log")) {
        mode |= DM_GLOBAL;
      } else if (arg.is("sessionbinlog")) {
        mode |= DM_SESSION;
      } else if (arg.is("fpcbinlog")) {
        mode |= DM_FPC;
      } else {
        parser.log_error("Invalid log rotation mode: '%s'", arg.get_string());
        return false;
      }
    } while (++i < num);
  } else {
    mode = DM_GLOBAL;
  }
  cc_result = cc_server.execute(CMD_ROTATE, "U", mode);
  return true;
}

static bool PARSER_SET_PROC(read)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_one_arg(parser, num)) {
    cc_result = cc_server.execute(CMD_READ, "SU", args[0].get_string(), cc_server.get_user_agent());
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(write)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_required_args(parser, num, 2)) {
    StringFile file;
    const char* data = args[1].get_string();
    c3_uint_t length;
    if (data[0] == '@') {
      const char* path = data + 1;
      if (file.load(path)) {
        data = file.get_string();
        size_t size = file.get_length();
        if (size > UINT_MAX_VAL) {
          parser.log_error("Session data file too big: '%s'", path);
          return false;
        }
        length = (c3_uint_t) size;
      } else {
        parser.log_error("Could not load session data from '%s'.", path);
        return false;
      }
    } else {
      length = (c3_uint_t) std::strlen(data);
    }
    cc_result = cc_server.execute(CMD_WRITE, length, data, "SUN",
      args[0].get_string(), cc_server.get_user_agent(), cc_server.get_lifetime());
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(destroy)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_one_arg(parser, num)) {
    cc_result = cc_server.execute(CMD_DESTROY, "S", args[0].get_string());
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(gc)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_one_arg(parser, num)) {
    c3_uint_t seconds;
    if (args[0].get_duration(seconds)) {
      cc_result = cc_server.execute(CMD_GC, "U", seconds);
      return true;
    } else {
      parser.log_error("Ill-formed <duration> (inactivity period): '%s'", args[0].get_string());
    }
  }
  return false;
}

static bool PARSER_SET_PROC(load)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_one_arg(parser, num)) {
    cc_result = cc_server.execute(CMD_LOAD, "SU", args[0].get_string(), cc_server.get_user_agent());
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(test)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_one_arg(parser, num)) {
    cc_result = cc_server.execute(CMD_TEST, "SU", args[0].get_string(), cc_server.get_user_agent());
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(save)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_required_args(parser, num, 2)) {
    StringFile file;
    const char* data = args[1].get_string();
    c3_uint_t length;
    if (data[0] == '@') {
      const char* path = data + 1;
      if (file.load(path)) {
        data = file.get_string();
        size_t size = file.get_length();
        if (size > UINT_MAX_VAL) {
          parser.log_error("FPC data file too big: '%s'", path);
          return false;
        }
        length = (c3_uint_t) size;
      } else {
        parser.log_error("Could not load FPC data from '%s'.", path);
        return false;
      }
    } else {
      length = (c3_uint_t) std::strlen(data);
    }
    cc_result = cc_server.execute(CMD_SAVE, length, data, "SUNL",
      args[0].get_string(), cc_server.get_user_agent(), cc_server.get_lifetime(), cc_server.get_tags());
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(remove)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_one_arg(parser, num)) {
    cc_result = cc_server.execute(CMD_REMOVE, "S", args[0].get_string());
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(clean)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_args(parser, num)) {
    clean_mode_t mode;
    parser_token_t& arg = args[0];
    if (arg.is("all")) {
      mode = CM_ALL;
    } else if (arg.is("old")) {
      mode = CM_OLD;
    } else {
      if (arg.is("matchall")) {
        if (num == 1) {
          return always_empty_set(parser, "matchall");
        }
        mode = CM_MATCHING_ALL_TAGS;
      } else if (arg.is("matchnot")) {
        mode = CM_NOT_MATCHING_ANY_TAG;
      } else if (arg.is("matchany")) {
        if (num == 1) {
          return always_empty_set(parser, "matchany");
        }
        mode = CM_MATCHING_ANY_TAG;
      } else {
        parser.log_error("Invalid cleaning mode: '%s'", arg.get_string());
        return false;
      }
      StringList list(num);
      for (c3_uint_t i = 1; i < num; ++i) {
        list.add_unique(args[i].get_string());
      }
      cc_result = cc_server.execute(CMD_CLEAN, "UL", mode, &list);
      return true;
    }
    if (num > 1) {
      parser.log_error("Cleaning mode '%s' does not accept tags.", arg.get_string());
      return false;
    }
    cc_result = cc_server.execute(CMD_CLEAN, "U", mode);
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(getids)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_no_args(parser, num)) {
    cc_result = cc_server.execute(CMD_GETIDS);
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(gettags)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_no_args(parser, num)) {
    cc_result = cc_server.execute(CMD_GETTAGS);
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(getidsmatchingtags)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (num > 0) {
    StringList list(num);
    c3_uint_t i = 0; do {
      list.add_unique(args[i].get_string());
    } while (++i < num);
    cc_result = cc_server.execute(CMD_GETIDSMATCHINGTAGS, "L", &list);
    return true;
  } else {
    return always_empty_set(parser, "getidsmatchingtags");
  }
}

static bool PARSER_SET_PROC(getidsnotmatchingtags)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  StringList list(num + 1);
  for (c3_uint_t i = 0; i < num; ++i) {
    list.add_unique(args[i].get_string());
  }
  cc_result = cc_server.execute(CMD_GETIDSNOTMATCHINGTAGS, "L", &list);
  return true;
}

static bool PARSER_SET_PROC(getidsmatchinganytags)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (num > 0) {
    StringList list(num);
    c3_uint_t i = 0; do {
      list.add_unique(args[i].get_string());
    } while (++i < num);
    cc_result = cc_server.execute(CMD_GETIDSMATCHINGANYTAGS, "L", &list);
    return true;
  } else {
    return always_empty_set(parser, "getidsmatchinganytags");
  }
}

static bool PARSER_SET_PROC(getfillingpercentage)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_no_args(parser, num)) {
    cc_result = cc_server.execute(CMD_GETFILLINGPERCENTAGE);
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(getmetadatas)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_one_arg(parser, num)) {
    cc_result = cc_server.execute(CMD_GETMETADATAS, "S", args[0].get_string());
    return true;
  }
  return false;
}

static bool PARSER_SET_PROC(touch)(Parser& parser, parser_token_t* args, c3_uint_t num) {
  if (has_required_args(parser, num, 2)) {
    c3_uint_t seconds;
    if (args[1].get_duration(seconds)) {
      cc_result = cc_server.execute(CMD_TOUCH, "SU", args[0].get_string(), seconds);
      return true;
    } else {
      parser.log_error("Ill-formed <duration> (lifetime extension): '%s'", args[1].get_string());
    }
  }
  return false;
}

static parser_command_t console_commands[] = {
  PARSER_SET_ENTRY(help),
  PARSER_SET_ENTRY(version),
  PARSER_SET_ENTRY(verbosity),
  PARSER_SET_ENTRY(execute),
  PARSER_SET_ENTRY(exit),
  PARSER_SET_ENTRY(quit),
  PARSER_SET_ENTRY(bye),
  PARSER_SET_ENTRY(connect),
  PARSER_SET_ENTRY(persistent),
  PARSER_SET_ENTRY(user),
  PARSER_SET_ENTRY(admin),
  PARSER_SET_ENTRY(useragent),
  PARSER_SET_ENTRY(tags),
  PARSER_SET_ENTRY(addtags),
  PARSER_SET_ENTRY(removetags),
  PARSER_SET_ENTRY(lifetime),
  PARSER_SET_ENTRY(marker),
  PARSER_SET_ENTRY(compressor),
  PARSER_SET_ENTRY(threshold),
  PARSER_SET_ENTRY(hasher),
  PARSER_SET_ENTRY(result),
  PARSER_SET_ENTRY(next),
  PARSER_SET_ENTRY(autoresult),
  PARSER_SET_ENTRY(checkresult),
  PARSER_SET_ENTRY(display),
  PARSER_SET_ENTRY(print),
  PARSER_SET_ENTRY(emulate),
  PARSER_SET_ENTRY(wait),
  PARSER_SET_ENTRY(walk),
  PARSER_SET_ENTRY(ping),
  PARSER_SET_ENTRY(check),
  PARSER_SET_ENTRY(info),
  PARSER_SET_ENTRY(stats),
  PARSER_SET_ENTRY(shutdown),
  PARSER_SET_ENTRY(localconfig),
  PARSER_SET_ENTRY(remoteconfig),
  PARSER_SET_ENTRY(restore),
  PARSER_SET_ENTRY(store),
  PARSER_SET_ENTRY(get),
  PARSER_SET_ENTRY(set),
  PARSER_SET_ENTRY(log),
  PARSER_SET_ENTRY(rotate),
  PARSER_SET_ENTRY(read),
  PARSER_SET_ENTRY(write),
  PARSER_SET_ENTRY(destroy),
  PARSER_SET_ENTRY(gc),
  PARSER_SET_ENTRY(load),
  PARSER_SET_ENTRY(test),
  PARSER_SET_ENTRY(save),
  PARSER_SET_ENTRY(remove),
  PARSER_SET_ENTRY(clean),
  PARSER_SET_ENTRY(getids),
  PARSER_SET_ENTRY(gettags),
  PARSER_SET_ENTRY(getidsmatchingtags),
  PARSER_SET_ENTRY(getidsnotmatchingtags),
  PARSER_SET_ENTRY(getidsmatchinganytags),
  PARSER_SET_ENTRY(getfillingpercentage),
  PARSER_SET_ENTRY(getmetadatas),
  PARSER_SET_ENTRY(touch)
};

constexpr size_t NUM_CONSOLE_COMMANDS = sizeof(console_commands) / sizeof(parser_command_t);

///////////////////////////////////////////////////////////////////////////////
// ConsoleParser
///////////////////////////////////////////////////////////////////////////////

ConsoleParser::ConsoleParser(c3_uint_t level, bool exit_on_errors):
  Parser(level, console_commands, NUM_CONSOLE_COMMANDS) {
  static bool commands_are_initialized = false;
  if (!commands_are_initialized) {
    initialize_commands(console_commands, NUM_CONSOLE_COMMANDS);
    commands_are_initialized = true;
  }
  cp_exit_on_errors = exit_on_errors;
}

void ConsoleParser::on_error() {
  if (!is_interactive() && cp_exit_on_errors) {
    cc_log.print_all();
    exit(EXIT_FAILURE);
  }
}

void ConsoleParser::on_unknown_set(const char* name) {
  log_error("Unknown command: '%s'", name);
  on_error();
}

void ConsoleParser::on_set_error(const char* name) {
  log(LL_VERBOSE, "Command '%s' FAILED.", name);
  on_error();
}

void ConsoleParser::on_unknown_get(const char* name) {
  log_error("Unknown command: '%s'", name);
}

void ConsoleParser::on_get_error(const char* name) {
  log(LL_WARNING, "Could not retrieve value of `%s`", name);
}

bool ConsoleParser::enumerator(void* context, const char* command) {
  c3_assert(context && command);
  auto list = (StringList*) context;
  list->add_unique(command); // keep sorted while adding
  return true;
}

bool ConsoleParser::execute(const char* statement) {
  c3_assert(statement && statement[0]);

  // see if help was requested with a shortcut
  if (statement[0] == '?' && statement[1] == 0) {
    statement = "help";
  }

  // convert command to lower case, but leave arguments (if any) intact
  const size_t command_length = std::strlen(statement);
  const size_t total_length = command_length + 1;
  char command[total_length];
  const char* space = std::strchr(std::strcpy(command, statement), ' ');
  const char* command_end = space != nullptr? space: command + command_length;
  for (char* c = command; c < command_end; c++) {
    *c = (char) std::tolower(*c);
  }

  // execute the command, or list all commands matching the mask
  if (space != nullptr || std::strchr(command, '*') == nullptr) {
    // 1) command with arguments
    return parse("command", command, command_length, true);
  } else {
    // 2) command search mask
    StringList list(16);
    c3_uint_t num = enumerate(command, enumerator, &list);
    if (num == 0) {
      log(LL_NORMAL, "There are no commands matching mask '%s'", statement);
    } else {
      c3_assert(list.get_count() == num);
      log(LL_EXPLICIT, "Commands matching mask '%s':", statement);
      for (c3_uint_t i = 0; i < num; i++) {
        log(LL_EXPLICIT, "  %s", list.get(i));
      }
    }
    return true;
  }
}

} // CyberCache
