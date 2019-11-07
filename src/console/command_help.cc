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
#include "command_help.h"

#include <cctype>

namespace CyberCache {

void get_help(Parser& parser) {
  static constexpr char text[] = R"C3HELP(List of commands supported by console v%s, grouped by categories:
  INTERFACE commands:
    ?, help, version, verbosity, execute, exit, quit, bye.
  AUXILIARY commands:
    connect, persistent, user, admin, useragent, tags, addtags, removetags,
    lifetime, marker, compressor, threshold, hasher, result, next,
    autoresult, checkresult, display, print, emulate, wait.
  INFORMATION commands (sent to server):
    ping, check, info, stats.
  ADMINISTRATIVE commands (sent to server):
    shutdown, localconfig, remoteconfig, restore, store,
    get, set, log, rotate.
  SESSION store commands (sent to server):
    read, write, destroy, gc.
  FPC commands (sent to server):
    load, test, save, remove, clean, getids, gettags,
    getidsmatchingtags, getidsnotmatchingtags, getidsmatchinganytags,
    getfillingpercentage, getmetadatas, touch.
Use 'HELP <command>' to print out that command's format and description (note
that command names are case-INsensitive). Enter <mask-containing-asterisks>
(as a command, not as an argument to 'HELP') to get list of commands matching
the mask. Additionally, 'HELP syntax' will print out some general tips on how
to specify arguments etc. Please see documentation for full information on
console commands and server protocol.)C3HELP";
  parser.log(LL_EXPLICIT, text, c3lib_version_build_string);
}

bool get_help(Parser& parser, const char* command) {
  assert(command && command[0]);

  // help texts for each command
  static constexpr char text[] = R"C3COMMANDS(
SYNTAX
Basics:
  Console command syntax closely matches that of a configuration option. For
  instance, all arguments, even those representing numbers, can be quoted. One
  notable exception is that there is no line continuation (but hash character,
  '#', does start comment if used outside of a quoted string).
Quoted strings:
  Arguments containing spaces, escape sequences, or hash characters should be
  quoted using single or double quotes, or backtick ('`'). The following
  escape sequences are recognized: '\\', '\r', '\n', '\t', '\'', '\`', '\"',
  '\@', and '\xx' (where 'xx' is hexadecimal code of a character). Some common
  escape sequences are not supported: '\a', '\b', '\e', and '\f' (they would
  interfere with the simplified hex code escapes). It is *not* necessary to
  escape a quote within a string delimited with quotes of a different kind
  (e.g. "'", `"`, and '"' are all perfectly valid).
Suffixes:
  Arguments that are durations and sizes can have suffixes: 's' (seconds, the
  default/implied), 'm' (minutes), 'h' (hours), 'd' (days), 'w' (weeks); 'b'
  (bytes, the default/implied), 'k' (kilobytes), 'm' (megabytes), 'g'
  (gigabytes), and 't' (terabytes).
Booleans:
  A boolean value can be specified using 'yes', 'no', 'on', 'off', 'true', or
  'false'. Numbers (e.g. 0 and 1) cannot be used as booleans.
Arrays:
  An array argument (to some 'SET' commands) is specified as a sequence of
  numbers or keywords separated with spaces.
Case sensitivity:
  All reserved words (including command names, boolean constants, hasher or
  compressor names etc.) and suffixes can be specified in either lower, or
  upper (or even mixed) case. Option names (first arguments to 'GET' and 'SET'
  commands) are case-sensitive and should always be specified in lower case.$
HELP
Format:
  help [ <command-name> ]
Description:
  If executed without arguments, prints out general console usage information;
  otherwise (if an argument is given) displays usage information for the
  specified command.
Alias:
  Question mark ('?') is an alias for 'help'; however, only for its use
  without arguments ('? <command-name>' is not supported), and only in
  interactive mode.$
VERSION
Format:
  version
Description:
  Prints out version number of the console application. Takes no arguments. To
  get server version number, 'GET version' command should be used instead.$
VERBOSITY
Format:
  verbosity [ <verbosity-level> ]
Description:
  Sets verbosity of the messages output by console. The <verbosity-level>
  argument can be one of the following (listed in order from most to lest
  verbose): 'debug', 'verbose', 'normal' (the default), 'terse', 'warning',
  'error', 'fatal', 'explicit'. Setting verbosity level to, say, 'terse' (or
  to any other level less verbose than 'normal') may be beneficial for
  execution of console scripts, as it will reduce "noise" in the output. If
  executed without argument, prints out current verbosity level.
  This console command has no effect on the server; to control verbosity of
  server output, use 'SET log_level <level>' command.$
EXECUTE
Format:
  execute <command-file-path>
Description:
  Loads file specified as argument and executes commands contained in it. The
  file may contain any console commands, including 'EXECUTE'; however, some
  commands (e.g. 'USER' and 'ADMIN') behave differently when executed from a
  file, please see documentation for details.$
EXIT
Format:
  exit
Description:
  Closes console and exits to the operating system. Does not affect the server
  in any way (to shut down the server, 'SHUTDOWN' command should be used).
Aliases:
  quit, bye$
QUIT
Format:
  quit
Description:
  Closes console and exits to the operating system. Does not affect the server
  in any way (to shut down the server, 'SHUTDOWN' command should be used).
Aliases:
  exit, bye$
BYE
Format:
  bye
Description:
  Closes console and exits to the operating system. Does not affect the server
  in any way (to shut down the server, 'SHUTDOWN' command should be used).
Aliases:
  exit, quit$
CONNECT
Format:
  connect [ <network-address> [ <port-number> ]]
Description:
  Sets network address and, optionally, port number for future connections to
  the server. If port is omitted, default port 8120 is used. If both arguments
  are omitted, prints out current settings. It is possible to specify either
  internet address (e.g. 'cybercache.cyberhull.com') or IP address as
  <network-address>; default is 127.0.0.1 ('localhost').$
PERSISTENT
Format:
  persistent [ <boolean> ]
Description:
  Specifies whether future connections to the server should be persistent, or
  if the console should hang up after issuing each command. Default value (if
  'persistent <boolean>' is never executed) is 'true'. If the command is
  executed without an argument, current setting will be printed. It is
  important that the same type of connections (per-command or persistent) are
  used by all components of the CyberCache cluster.
Booleans:
  A boolean value can be specified using reserved words 'yes', 'no', 'on',
  'off', 'true', or 'false' (in upper, lower, or mixed case).$
USER
Format:
  user [ - | ? | <password> ]
Description:
  Allows to enter user password (which should match the one specified using
  'user_password' in server configuration file), or check if user password is
  currently set IN CONSOLE. Characters being entered are not echoed. Default
  user password is an empty string.
Arguments:
  In interactive mode (i.e. not when 'USER' command gets loaded from a file
  using 'EXECUTE' command), entering empty password actually leaves previously
  entered password intact. To set password to an empty string (effectively
  disabling it) in interactive mode, it is necessary to enter 'USER -'.
  Entering 'USER ?' will check if a valid user password had been entered
  before. The <password> argument (a valid password, or an empty string) can
  only be specified to the 'USER' command in a file that is loaded with
  'EXECUTE' (i.e. in so-called batch mode).$
ADMIN
Format:
  admin [ - | <password> ]
Description:
  Allows to enter administrative password (which should match the one
  specified using 'admin_password' in server configuration file), or check if
  administrative password is currently set IN CONSOLE. Characters being
  entered are not echoed. Default administrative password is an empty string.
Arguments:
  In interactive mode (i.e. not when 'ADMIN' command gets loaded from a file
  using 'EXECUTE' command), entering empty password actually leaves previously
  entered password intact. To set password to an empty string (effectively
  disabling it) in interactive mode, it is necessary to enter 'ADMIN -'.
  Entering 'ADMIN ?' will check if a valid administrative password had been
  entered before. The <password> argument (a valid password, or an empty
  string) can only be specified to the 'ADMIN' command in a file that is
  loaded with 'EXECUTE' (i.e. in so-called batch mode).$
USERAGENT
Format:
  useragent [ unknown | bot | warmer | user ]
Description:
  Selects which user agent will be emulated during subsequent execution of
  'READ', 'WRITE', 'LOAD', 'TEST', and 'SAVE' commands. Default is 'user'. Not
  specifying any arguments prints out currently active user agent.$
TAGS
Format:
  tags [ <tag> [ <tag> [[ ... ]]]]
Description:
  Defines set of tags that will be submitted along with 'SAVE' command.
  Contrary to 'ADDTAGS' and 'REMOVETAGS' commands, completely replaces current
  set of tags rather than alters it. If executed without arguments, prints out
  currently active set of tags (empty by default)$
ADDTAGS
Format:
  addtags <tag> [ <tag> [[ ... ]]]
Description:
  Adds tag(s) to the current set to be submitted along with 'SAVE' command;
  specifying at least one tag name to add is mandatory.$
REMOVETAGS
Format:
  removetags [<tag> [ <tag> [[ ... ]]]]
Description:
  Removes tag(s) from the current set to be submitted along with 'SAVE'
  command; specifying at least one tag name to remove is mandatory. If
  executed without arguments, removes all tags from current set.$
LIFETIME
Format:
  lifetime [ <duration> ]
Description:
  Specifies life time of session and FPC records that will be created or
  modified by future 'WRITE' and 'SAVE' commands; 0 means "infinite", -1 (the
  default) stands for "use whatever life time is specified for given
  'USERAGENT' in server configuration file via 'session_default_lifetimes' and
  'fpc_default_lifetimes' option". If executed without an argument, prints out
  current setting.
Durations:
  If <duration> is not set to a special values 0 or -1 and does not have a
  suffix, it is treated as number of seconds. The suffix can be one of the
  following: 's' (seconds), 'm' (minutes), 'h' (hours), 'd' (days), or
  'w' (weeks). Maximum value that can be specified using any notation is
  equivalent to about one year. Effective lifetime of the FPC record will be
  set to the minimum of two values: one specified by this command, and one
  set using 'fpc_max_lifetime' option in server configuration file (if passed
  value exceeds 'fpc_max_lifetime', it will be silently clipped); session
  record lifetime will not be clipped.$
MARKER
Format:
  marker [ <boolean> ]
Description:
  Specifies whether integrity check marker should be added to the commands
  sent to the server; default is 'on'. If executed without an argument, prints
  out current setting.
Booleans:
  A boolean value can be specified using reserved words 'yes', 'no', 'on',
  'off', 'true', or 'false' (in upper, lower, or mixed case).$
COMPRESSOR
Format:
  compressor [ <compressor-type> ]
Description:
  Specifies which compressor to use to compress payloads of commands sent to
  the server, such as 'WRITE' and 'SAVE'; default is 'snappy'. If executed
  without arguments, prints out current setting.
Compressor types:
  Compressor can be specified using one of the following reserved words:
  'lzf', 'snappy', 'lz4', 'lzss3', 'brotli', 'zstd', 'zlib' (gzip), 'lzham';
  default is 'snappy'. The 'brotli' compressor is only available in Enterprise
  Edition of the suite. See documentation for details.$
THRESHOLD
Format:
  threshold [ <positive-number> ]
Description:
  Specifies minimal size of the data that console will try to compress before
  sending to server; data chunks (such as session of FPC records) that are
  smaller then the threshold will always be sent uncompressed. Setting this
  option to 4294967295 essentially disables compression of outbound data.$
HASHER
Format:
  hasher [ <hash-method> ]
Description:
  Specifies hash method that will be used to process passwords submitted to
  the server as part of the authentication process; default is 'murmurhash2'.
  If executed without arguments, prints out current setting.
Hash methods:
  Hash method should be one of the following: 'xxhash', 'farmhash',
  'spookyhash', 'murmurhash2' (the default), or 'murmurhash3'.
Important:
  If either user or admin password is not empty, then hash method set using
  'HASHER' console command MUST match the one set in server configuration file
  using 'password_hash_method' option. They both have the same default values,
  so if you change one, you MUST change the other, or else authentications
  will fail, and commands sent to the server will not be executed there. Also,
  it is a must to use console of the same version with the server: different
  versions might use different seeds even for the same hash methods.$
RESULT
Format:
  result [ <from> [ <number> ]]
Description:
  Prints result of the last command executed BY THE SERVER. If server did not
  return a data buffer or a string list, then both <from> and <number>
  arguments will be ignored even if specified. Otherwise, if server returned
  byte buffer then <from> specifies offset, and <number> specifies number of
  bytes to print out. If server returned a list, then <from> specifies first
  element, and <number> specifies number of elements to display. Default for
  <from> is 0, and default for <number> is either calculated based on values
  set by 'DISPLAY' command and result type, or (if no server command was
  executed since last result display) re-used from last 'RESULT' or 'NEXT'
  invocation. The <from> argument is zero-based, and can be negative, in which
  case it is interpreted as "from the end of the buffer or list".$
NEXT
Format:
  next [ <number> ]
Description:
  Prints out next <number> of bytes or list elements from the result of last
  command executed BY THE SERVER. If server command returned data that is not
  a byte buffer or string list, then <number> is ignored, and executing 'next'
  command is equivalent to executing 'result' command without arguments. If,
  say, after executing server command that returned a byte buffer or a string
  list, 'result 12 5' was executed, then executing 'next 10' would be
  equivalent to executing 'result 17 10'; executing 'next 8' after that would
  be equivalent to 'result 27 8', and so on. In other words, the 'next'
  command is a convenience shortcut. Default value for <number> is calculated
  similarly to that of the 'RESULT' command.$
AUTORESULT
Format:
  autoresult [ simple | lists | all ]
Description:
  Specifies what types of results should be printed out automatically upon
  command execution; available modes are:
  'simple': print out all results except lists and byte buffers,
  'lists': print out all results except byte buffers (the default),
  'all': print out all results.
  "Not printing out" a result means that console will only indicate that, say,
  a list of N strings had been received, and 'RESULT' command should be used
  to display received strings. Executing this command without argument will
  print current mode.$
CHECKRESULT
Format:
  checkresult <type> [ <arguments> ]
Description:
  Checks that result of last executed SERVER command is <type>, and optionally
  checks if data returned from the server contains particular data; the <type>
  argument and optional check parameters can be one of the following:
  'ok': no extra arguments/checks are allowed (OK response contains no data),
  'error [ <string1> [...]]': checks that error response from the server
    contains all specified strings,
  'string [ <string1> [...]]': checks that server response contains all
    specified strings,
  'list [ <element1> [...]]': checks that list response from the server
    contains all specified elements (matches must be exact); if <elementN>
    starts with '%', then 'CHECKRESULT' will check if any of the returned
    lists' elements contains <elementN> as a substring.
  'data [ <offset> [@]<bytes> ]': checks that data received from server
    contains <bytes> at offset <offset>; if <bytes> is specified as a binary
    string with zero bytes, it will be truncated before comparison; if <bytes>
    argument has `@` prefix, it is treated as a name of file that will be
    loaded, and its contents will be used for comparison.
  This command is meant to be used in batch mode (in files comprising
  comprehensive server tests).$
DISPLAY
Format:
  display [ <lines-per-screen> [ <bytes-per-line> [ <non-printable-char> ]]]
Description:
  Controls output of 'RESULT' and/or 'NEXT' commands. The <lines-per-screen>
  sets how many lines will be displayed by 'RESULT' if it is invoked without
  arguments for the first time after some server command successfully returned
  a byte buffer or a list. The <bytes-per-line> sets how many bytes will be
  displayed by both 'RESULT' and 'NEXT' if those are used to print out byte
  buffer returned by the server; each byte is then printed twice: as a hex
  value, and as a character; if byte does not represent a printable character,
  the <non-printable-char> argument will be used instead. Default for
  <lines-per-screen> is 24, default <bytes-per-line> is 16, and default
  <non-printable-char> is '.' (dot). Executing 'DISPLAY' without arguments
  prints out current settings; if some (but not all) arguments are specified,
  omitted parameters retain their previously set values.$
PRINT
Format:
  print <text-message>
Description:
  Prints specified message. Meant to be used in batch mode (i.e. in a file
  processed by 'EXECUTE' command, or given as a command line argument).$
EMULATE
Format:
  emulate <descriptor> [ <argument> [ <argument> [...]]]
where
  descriptor: either command ID (number in range 0..255), or a sequence
    { <command-id> [ user|admin [ -|<password> [ <payload> [ <check> ]]]] }
  where '{' and '}' characters are mandatory and:
    <password>: a (possibly empty) string or '-' (meaning use current),
    <payload>: a (possibly empty) string; if has '@' prefix, treated as file,
    <check>: a boolean that specifies whether to send integrity check marker.
An <argument> can be any of the following:
  a number: if it can be parsed as a number,
  a string: if a number should be passed as a string, prefix it with '%%'; if
    a string should start with '%%', prefix the string with another '%%' that
    will get stripped before processing,
  a list: sequence of strings between '[' and ']', all delimited by spaces (if
    the list contains strings that are actually numbers, they will be passed
    as strings anyway, they should not be prefixed with '%%').
This console command compiles and sends server command irrespective of whether
command ID is a valid one, and whether format of the command for specified ID
(if it happens to be a valid one) is correct or not, thus allowing to test
server responses to various invalid input. Currently, only supports up to 15
arguments (curly and square braces count as arguments).$
WAIT
Format:
  wait <milliseconds>
Waits specified number of milliseconds; meant to be used in batch tests
whenever it is necessary to let previously issued command have some time to
complete its actions on the server side. For example, if a binlog has just been
loaded, the server may need some time to "play back" all its actions, so if
results of those actions need to be checked, `wait` might be needed. Maximum
allowed wait time is one minute (60,000 milliseconds).$
WALK
Can't help with that! I'm a mere console application... You may want to
contact my creators at CyberHULL:
  businessdevelopment@cyberhull.com$
PING
Format:
  ping
Description:
  Checks server availability, does not take any arguments. May require either
  user or admin authentication (depending upon value of 'use_info_password'
  server configuration option).
Server response:
  Always 'OK' if server is available.$
CHECK
Format:
  check
Description:
  Reports result of last server health check, as well its current load. Does
  not take any arguments. May require either user or admin authentication
  (depending upon value of 'use_info_password' server configuration option).
Server response:
  Three integers: 1) server load as percentage of connection (worker) threads
  that are currently busy processing requests (number between 0 and 100),
  2) number of warnings since server was started, and 3) number of non-fatal
  errors since server was started.$
INFO
Format:
  info [ <domain> [ <domain> [...]]]
where <domain> is one of the following:
  global, session, fpc, all.
Description:
  Requests status/health/usage information from the server, such as server
  load, used/available memory for stores, disk space for logger and binlogs,
  and the likes. May require either user or admin authentication (depending
  upon value of 'use_info_password' server configuration option). Optional
  arguments are "domains" for which the information is requested; not
  specifying any arguments means "all domains". Does not return values of any
  configuration options; for that, 'GET' command should be used instead.
Server response:
  List of strings with status/health/etc. information.$
STATS
Format:
  stats [ <mask> [ <domain> [ <domain> [...]]]]
where <mask> is a pattern against performance counters will be matched (may
contain arbitrary number of wildcard characters, '*'), and <domain> is one of
the following:
  global, session, fpc, all.
Description:
  Requests values of various performance counters, requires instrumented
  version of the server (it is possible to issue this command from console
  even to a non-instrumented server, but server's response will be an error
  message). May require either user or admin authentication (depending upon
  value of 'use_info_password' server configuration option). Optional
  arguments are name mask and "domains" for which performance counters are
  requested; omitting arguments stands for "all names and all domains" (i.e.
  is equivalent to '* all').
Server response:
  List of strings, where each string contains name and value of a performance
  counter or, if server is not instrumented, an error message.
Availability:
  This command is only available in Enterprise edition. For the command to
  return server stats, Instrumented version of the server must be running.$
SHUTDOWN
Format:
  shutdown
Description:
  Initiates server shutdown procedure. May require administrative
  authentication (depends upon 'admin_password' server configuration option).
  Does not take any arguments.
Server response:
  'OK', or an error message.$
LOCALCONFIG
Format:
  localconfig <configuration-file-path>
Description:
  Loads configuration file located on the same computer where CONSOLE (and not
  necessary server) is running. Loaded file is sent to the server and
  processed there; as a consequence, if loaded file contains 'include'
  statements, those statements must refer to files on computer on which the
  SERVER is running. Options not set in the configuration file will not be
  affected. May require administrative authentication (depends upon
  'admin_password' server configuration option).
Important:
  Options 'user_password', 'admin_password', 'bulk_password',
  'table_hash_method', 'password_hash_method', and
  'perf_num_internal_tag_refs' in the loaded file will be ignored; this
  behavior is consistent with 'SET' and 'REMOTECONFIG' console commands.
Server response:
  'OK', or an error message.$
REMOTECONFIG
Format:
  remoteconfig <configuration-file-path>
Description:
  Loads configuration file located on the same computer where SERVER is
  running; to locate the file, server uses the same algorithm as for config
  files specified on its command line. Options not set in the configuration
  file will not be affected. May require administrative authentication
  (depends upon 'admin_password' server configuration option).
Important:
  Options 'user_password', 'admin_password', 'bulk_password',
  'table_hash_method', 'password_hash_method', and
  'perf_num_internal_tag_refs' in the loaded file will be ignored; this
  behavior is consistent with 'SET' and 'LOCALCONFIG' console commands.
Server response:
  'OK', or an error message.$
RESTORE
Format:
  restore <binlog-file-path>
Description:
  Loads session or FPC records from binlog file located on the same computer
  where SERVER is running. The server loads binlogs concurrently while
  processing all other requests. May require administrative authentication
  (depends upon 'admin_password' server configuration option).
Server response:
  'OK', or an error message.$
STORE
Format:
  store <domain> <database-file-path> [ <user-agent> [ <sync-mode> ]]
Description:
  Stores cache records of specified domain(s) to a database file, filtering
  them according to specified user agent: only records created by user agents
  that belong to specified or "higher" types will be stored. Allowed domains
  are 'session', 'fpc', or 'all' (stands for 'session' and 'fpc'). Recognized
  user agents are 'unknown' (the default), 'bot', 'warmer', and 'user'.
  Supported synchronization modes are (in order from fastest by least reliable
  to slowest but most reliable): 'none' (the default), 'data-only', and
  'full'. Note that existing files will be overwritten.
Server response:
  'OK', or an error message.$
GET
Format:
  get <option-name-or-mask> [ <option-name-or-mask> [...]]
Description:
  Requests values of options from the server. Any options can be queried,
  although some requests may return "value could not be retrieved" (this is
  the case with password-related options, as well as some initial values; see
  documentation for details). If arguments contain asterisks ('*', arbitrary
  number is supported), they are treated as name masks: server will return
  values of all options with matching names. May require administrative
  authentication (depends upon 'admin_password' server configuration option).
Server response:
  List of strings, each string containing name and value of an option.$
SET
Format:
  set <option-name> <option-value>
Description:
  Sets new value of a configurable server option. Values that denote durations
  or file/memory sizes may have suffixes; size suffixes are 'b' (bytes;
  implied if none is specified), 'k' (kilobytes), 'm' (megabytes),
  'g' (gigabytes), or 't' (terabytes); for duration suffixes, see 'LIFETIME'
  command description. May require administrative authentication (depends upon
 'admin_password' server configuration option).
Important:
  Attempts to set options 'user_password', 'admin_password', 'bulk_password',
  'table_hash_method', 'password_hash_method', and
  'perf_num_internal_tag_refs' will be ignored; this behavior is consistent
  with 'LOCALCONFIG' and 'REMOTECONFIG' console commands.
Server response:
  'OK', or an error message.$
LOG
Format:
  log <message>
Description:
  Logs specified message to the SERVER log file. Current log level of the
  server does not affect this command: if logging is not completely disabled,
  the message will always be logged. May require administrative authentication
  (depends upon 'admin_password' server configuration option).
Server response:
  'OK', or an error message.$
ROTATE
Format:
  rotate [ log | sessionbinlog | fpcbinlog [ log | sessionbinlog | fpcbinlog [...]]]
Description:
  Rotates log file(s) that correspond to the services specified as
  argument(s). Rotation will be enforced even if respective [bin]log file did
  not reach its rotation size threshold yet. If argument is omitted, this
  command will rotate's only server's "regular" log file. May require
  administrative authentication (depends upon 'admin_password' server
  configuration option).
Server response:
  'OK', or an error message.$
READ
Format:
  read <session-entry-id>
Description:
  Fetches session record (from the server) by its ID. Passes currently active
  'USERAGENT' to the server. May require user-level authentication (depends
  upon 'user_password' server configuration option).
Server response:
  'OK' if record did not exist, or binary data buffer otherwise.$
WRITE
Format:
  write <session-entry-id> [@]<session-data>
Description:
  Compresses (using method set with 'COMPRESSOR' command) and sends session
  record to the server, along with currently active 'USERAGENT' and 'LIFETIME'
  values. May require user-level authentication (depends upon 'user_password'
  server configuration option). If very first character of the <session-data>
  is '@', then the rest of the argument is treated not as a binary string, but
  as a local file name to load, compress, and send to the server. If it is
  necessary to send a string that starts with '@', it has to be put into a
  file. If session data specified as a *string* happens to have zero bytes, it
  would get truncated at first zero byte upon sending; neither data loaded
  from file, nor server API have this restriction, so if it's necessary to
  send data with zero bytes in it using console, the data should be put into a
  file first. Sending zero-length data removes respective session record.
Server response:
  'OK', or an error message.$
DESTROY
Format:
  destroy <session-entry-id>
Description:
  Deletes session record with specified ID from the server). May require
  user-level authentication (depends upon 'user_password' server configuration
  option).
Server response:
  'OK', or an error message.$
GC
Format:
  gc <duration>
Description:
  Delete from server all session records that were not updated during
  specified <duration>; see 'LIFETIME' command description for ways to specify
  <duration> using suffixes. May require user-level authentication (depends
  upon 'user_password' server configuration option).
Important:
  If 'session_eviction_mode' server option is set to 'strict-lru', this
  request will be silently ignored by the server.
Server response:
  'OK', or an error message.$
LOAD
Format:
  load <fpc-entry-id>
Description:
  Fetches FPC record from the server by record ID. Passes currently active
  'USERAGENT' to the server. May require user-level authentication (depends
  upon 'user_password' server configuration option).
Server response:
  'OK' if record did not exist, or binary data buffer otherwise.$
TEST
Format:
  test <fpc-entry-id>
Description:
  Checks if FPC record with gived ID exists in the store. Passes currently
  active 'USERAGENT' to the server. May require user-level authentication
  (depends upon 'user_password' server configuration option).
Server response:
  'OK' if record did not exist, or last modification timestamp otherwise.$
SAVE
Format:
  save <fpc-entry-id> [@]<fpc-entry-data>
Description:
  Compresses (using method set with 'COMPRESSOR' command) and sends FPC record
  to the server, along with currently active 'USERAGENT', 'LIFETIME', and set
  of tags (specified using 'TAGS', 'ADDTAGS', and/or 'REMOVETAGS' commands).
  May require user-level authentication (if 'user_password' server
  configuration option is non-empty). If very first character of the
  <fpc-entry-data> is '@', then the rest of the argument is treated not as a
  binary string, but as a local file name to load, compress, and send to the
  server. If it is necessary to send a string that starts with '@', then it
  has to be put into a file. If FPC data specified as a string happens to have
  zero bytes, it would get truncated at first zero byte upon sending; neither
  data loaded from file, nor server API have this restriction, so if it is
  necessary to send data with zero bytes in it using console, the data should
  be put into a file first. Sending zero-length data removes respective FPC
  record.
Server response:
  'OK', or an error message.$
REMOVE
Format:
  remove <fpc-entry-id>
Description:
  Deletes FPC record with specified ID from the server. May require user-level
  authentication (depends upon 'user_password' server configuration option).
Server response:
  'OK', or an error message.$
CLEAN
Format:
  clean { all | old | matchall | matchnot | matchany } [ <tag> [ <tag> [...]]]
Description:
  Deletes all FPC records that match specified criteria (requires user-level
  authentication if 'user_password' server configuration option is not empty):
  'all': entire FPC store (list of tags argument should be empty),
  'old': records with expired lifetime (list of tags should be empty),
  'matchall': records that are marked with all specified tags (if list of tags
    is empty, console does not send the command to the server),
  'matchnot': record is not marked with any of the specified tags (if list of
    tags is empty, the command *will* be sent to the server, but its result
    will be equivalent to that of 'matchall'),
  'matchany': record is marked with at least one of the specified tags (if
    list of tags is empty, console does not send the command to the server).
Important:
  If command argument is 'old', while 'fpc_eviction_mode' server option is set
  to 'strict-lru', this request will be silently ignored by the server.
Server response:
  'OK', or an error message.$
GETIDS
Format:
  getids
Description:
  Fetches list of IDs of all records in FPC store. Does not take any
  arguments; requires user-level authentication if 'user_password' server
  configuration option is not empty.
Server response:
  List of all FPC record IDs, or an error message.$
GETTAGS
Format:
  gettags
Description:
  Fetches list of all tags that have at least one FPC entry marked with them.
  Does not take any arguments; requires user-level authentication if
  'user_password' server configuration option is not empty.
Server response:
  List of all tags, or an error message.$
GETIDSMATCHINGTAGS
Format:
  getidsmatchingtags [ <tag> [ <tag> [ ... ]]]
Description:
  Fetches list of IDs of all records in FPC store that are marked with all
  specified tags (NOTE: if no tags are specified, console does not send the
  command to the server and returns empty list immediately). Requires
  user-level authentication if 'user_password' server configuration option is
  not empty.
Server response:
  List of IDs of selected FPC records, or an error message.$
GETIDSNOTMATCHINGTAGS
Format:
  getidsnotmatchingtags [ <tag> [ <tag> [ ... ]]]
Description:
  Fetches list of IDs of all records in FPC store that are not marked with any
  of the specified tags (NOTE: not specifying any tags to this command is
  equivalent to executing 'GETIDS'). Requires user-level authentication if
  'user_password' server configuration option is not empty.
Server response:
  List of IDs of selected FPC records, or an error message.$
GETIDSMATCHINGANYTAGS
Format:
  getidsmatchinganytags [ <tag> [ <tag> [ ... ]]]
Description:
  Fetches list of IDs of all records in FPC store that are marked with at
  least one of the specified tags (NOTE: if no tags are specified as
  arguments, the command is not sent to the server, the console just returns
  empty list immediately). Requires user-level authentication if
  'user_password' server configuration option is not empty.
Server response:
  List of IDs of selected FPC records, or an error message.$
GETFILLINGPERCENTAGE
Format:
  getfillingpercentage
Description:
  Queries filling percentage of the FPC store; if memory quota is not set for
  the FPC store, this command always returns 0. Does not take any arguments,
  requires user-level authentication if 'user_password' server configuration
  option is not empty.
Server response:
  An integer (in 0..100 range), or an error message.$
GETMETADATAS
Format:
  getmetadatas <fpc-entry-id>
Description:
  Returns metadata associated with an FPC entry that is specified by its ID.
  Requires user-level authentication if 'user_password' server configuration
  option is not empty.
Server response:
  If specified entry does not exist, returns 'OK'; otherwise, returns
  expiration timestamp, last modification timestamp, and list of tags
  associated with the specified entry.$
TOUCH
Format:
  touch <fpc-entry-id> <lifetime>
Description:
  Adds extra lifetime to the FPC entry specified by its ID (see 'LIFETIME'
  command for the description of how to specify duration/lifetime using
  suffixes). If resulting lifetime exceeds maximum set via 'fpc_max_lifetime'
  server configuration option for the 'USERAGENT' OF THE RECORD (i.e. set
  during last record access using 'LOAD', 'SAVE', or other command that could
  alter 'USERAGENT'), then expiration timestamp will be set to current time
  plus 'fpc_max_lifetime' (for the 'USERAGENT').
Server response:
  If specified FPC entry does not exist, returns error message; otherwise, if
  lifetime was added successfully, returns 'OK'.$
)C3COMMANDS";

  // compile the string to search for
  const size_t command_length = std::strlen(command);
  const size_t total_command_length = command_length + 3;
  char uc_command[total_command_length];
  uc_command[0] = '\n';
  for (size_t i = 0; i < command_length; i++) {
    uc_command[i + 1] = (char) std::toupper(command[i]);
  }
  uc_command[command_length + 1] = '\n';
  uc_command[command_length + 2] = 0;

  // find command description
  const char* description = std::strstr(text, uc_command);
  if (description != nullptr) {
    description += command_length + 2; // skip command name and newline characters
    const char* end = std::strchr(description, '$');
    c3_assert(end);

    // extract and log command description
    const size_t description_length = end - description;
    const size_t total_description_length = description_length + 1;
    char buffer[total_description_length];
    std::memcpy(buffer, description, description_length);
    buffer[description_length] = 0;
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wformat-security"
    parser.log(LL_EXPLICIT, buffer);
    #pragma GCC diagnostic pop
    return true;
  } else {
    parser.log(LL_ERROR, "HELP '%s': can't help with an unknown command", command);
    return false;
  }
}

}
