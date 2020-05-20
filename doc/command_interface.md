
CyberCache Cluster: Command Interfaces
======================================

CyberCache Cluster suite provides three ways of communicating with the server
(`cybercached` daemon):

1. The "raw" TCP/IP interface (see separate network protocol documentation).
2. Console interface (commands can be sent to it by the `cybercache` application 
   that supports interactive and non-interactive modes).
3. PHP interface, through the `cybercache.so` PHP extension.

This manual documents the latter two; please refer to `cybercache(5)` man page
(or `CyberCache_Network_Protocol.pdf`) for information on basic structure of 
CyberCache network packets, definitions of "chunks", and other low-level stuff 
(even though command descriptions in this file do include `Request sequence` and 
`Server response` sub-sections, it may be necessary to refer to protocol 
documentation for information on individual chunks).

By default, CyberCache server listens to port 8120; see [List of TCP and UDP
port numbers](https://en.wikipedia.org/wiki/List_of_TCP_and_UDP_port_numbers); a
different value can be specified using `listener_port` configuration file
option. Any option in the server configuration file **only affects server
(`cybercached`)** and its responses, but has no effect on the server console
(`cybercache`), which may (and on many occasions will) reside on a different
host.

#### Getting More Help

Even though this manual covers `cybercache` (console application used to
communicate with `CyberCache`) usage quite extensively, one can also get extra
help by starting `cybercache` and executing its `help` command. Additionally,
`help <command>` will print out instructions for that particular command.

As to configuring `cybercached` daemon itself, its default configuration file
`/etc/cybercache/cybercached.cfg` contains detailed descriptions of all
"user-servicable" options, including their possible values, effects, as well
as recommended settings.

#### Testing CyberCache Server

To test CyberCache installation, follow these steps:

1) Make sure that PHP 7.0 is (still) installed; if not:

    sudo apt install php
    
and check that it's PHP 7.0:

    php -v

2) Follow instructions in "/usr/lib/cybercache/tests/README.md"

### PHP Interface ###

CyberCache PHP resource is created with a call to one of the following methods
(the `$resource` argument to PHP methods, in the below documentation, refers to
a resource returned by one of these functions):

- `resource c3_session(array $options=null)`
- `resource c3_fpc(array $options=null)`

The methods take optional associative array and, if successful, return PHP
resource. In case of an error, these methods report it using standard PHP means
(error message will be printed to the console if PHP is run as a command-line
interpreter, or will be written to Apache error log if PHP is run as Apache
module), and then throw `Exception`. These methods differ from other CyberCache
extension methods in their error handling because they are meant to be used in
class constructors that do not have error return values, and would have to
throw an exception on their own anyway.

The following options (option array keys) can be specified during resource
creation; note that all values (even representing numbers, such as compression
threshold) **must** be PHP strings:

- `address` : address to connect to (default is `127.0.0.1`),
- `port` : port to connect to (default is `8120`),
- `hasher` : what hash method to use for passwords (default is `murmurhash2`),
- `admin` : administrator password to pass to admin-level commands (default it none),
- `user` : user password to pass to user-level commands (default it none),
- `compressor` : what compression method to use (default is `snappy`),
- `threshold` : minimum size of the buffer eligible for compression (default is `4096`),
- `marker` : whether to send integrity checks after each command (default is `true`).

All options can also be set in the configuration file; for sessions, the options
are `c3.session_<option>`, and for FPC they look like `c3.fpc_<option>`, where
`<option>` is one of those that can be passed to `c3_session()` or `c3_fpc()`;
these options can be set at any time using standard PHP `ini_set()` function.

> **IMPORTANT**: for hashing algorithm setting to take effect:
>
> 1) the `c3.session_hasher` must precede both `c3.session_user` and/or
>    `c3.session_admin` options in the PHP INI file; the same order must be
>    kept when setting these options using `ini_set()` function,
>
> 2) the `c3.fpc_hasher` must precede both `c3.fpc_user` and `c3.fpc_admin`
>    options in the PHP INI file; the same order must be kept when setting
>    these options using `ini_set()` function,
>
> 3) the `hasher` key must precede both `user` and `admin` keys in the array
>    passed to `c3_session()` and/or `c3_fpc()` extension functions,
>
> 4) the `password_hash_method` server configuration option must be specified
>    in the server cofiguration file before `user_password`, `admin_password`,
>    and/or `bulk_password` options.
>
> In other words, whenever a password is processed by the server or PHP
> extension, last encountered password hashing method is used (or, if none was
> specified, the default method). Password hashing method is an advanced
> setting; if in doubt -- just do not alter it, default value is good enough.

In addition, there are two global options:

- `c3.bot_regex` option that can be used to set regular expression pattern for
  web bot detection; contrary to all other options, this option can only be set
  in PHP ini file (or using '-d' PHP command line argument), and cannot be
  changed at run time, and

- `c3.info_password_type` option that controls authentication level for
  so-called information commands (`PING`, `CHECK`, `INFO`, and `STATS`); its 
  value *must* match that of the server `info_password_type` option (see server 
  configuration file for allowed values).

> *IMPORTANT*: not only keys, but also all values of the options passed to 
> `c3_session()` and `c3_fpc()` functions **must** be strings; moreover, options
> that are booleans are processed just like INI option values, which meands that
> `on`, `yes`, `true` and all strings thar represent nonzero numbers are treated
> as boolean values of `true`, and *all* other strings as boolean `false`.

PHP functions that do actual communication accept resource handle as their first
argument, and return a boolean, an integer number, a binary string, or an array;
error return is a `false` or a `null`. If a method's return value is
conceptually boolean, the method is implemented as returning an `int` (`0` or
`1`) to be able to use `false` as error return. In case of an error, respective
error message for last API call during current session can then be fetched with
`c3_get_last_error()`, which would return an empty string if called after a
successful communication/transaction.

> *IMPORTANT*: the `c3_get_last_error()` function only returns errors returned
> by the *server*; errors encountered elsewhere (e.g. during processing of some
> extension function's arguments) will *not* be returned.

Resources created using `c3_session()` and `c3_fpc()` functions do not need to 
be released explicitry; they will be disposed automatically when the last 
variable referencing them goes out of scope.

Apart from functions necessary to support FPC operations per se, there is one
extra PHP function, `get_capabilities()`, which returns capabilities of FPC
backend as associative array with the following keys and boolean values (the
values shown below are what current version of CyberCache PHP extension returns
upon the `get_capabilities()` call):

- 'automatic_cleaning' => `false` (does Magento have to do periodic cleaning of the cache?)
- 'tags' => `true` (does backend support FPC records' tags?),
- 'expired_read' => `true` (is it possible to read an expired record?),
- 'priority' => `false` (does backend support *explicitly* specified record "priorities"?),
- 'infinite_lifetime'  => `true` (is it possible to set "infinite" lifetime for a record?),
- 'get_list' => `true` (is it possible to get complete list of record IDs [and tags]?).

### Console Interface ###

Console is case-insensitive as far as commands and their arguments *that are
keywords* are concerned (so, for instance, one can enter `INFO FPC`, or `INFO
fpc`, or `info FPC`, or `Info fPC`; they will all work). Console accepts binary
data as quoted strings (using single or double quotes). Within any string, the
following escape sequences are recognized and processed:

- `\\` : backslash,
- `\xx` : character with hexadecimal code `xx` (in upper or lower case),
- `\r` : carriage return,
- `\n` : line feed (new line),
- `\t` : tabulation (tab character),
- `\'` : apostrophe (single quote),
- `\'` : backtick (back quote / grave accent),
- `\"` : double quote.

> *NOTE*: Other common escape sequences (such as `\a` for beep, `\b` for
> backspace, `\e` for Escape, or for `\f` form feed) are not supported as they
> would interfere with hexadecimal codes).

Whenever it is necessary to enter duration through console (say, as an argument
to the `SET` command), it is possible to use `s` (seconds, the default), `m`
(minutes), `h` (hours), `d` (days), or `w` (weeks) suffixes. Likewise, to enter
memory size, one can use suffixes `b` (bytes, the default/implied), `k`
(kilobytes), `m` (megabytes), `g` (gigabytes), or `t` (terabytes). When it is
necessary to enter an array (again, for a command like `SET`), its individual
elements should be separated by one or more spaces.

Just like in configuration file, in console boolean values can be specified
using reserved words `true`, `yes`, `on`, `false`, `no`, and `off`, in either
upper or lower (or even mixed) case.

If first non-space character of an entered line is a hash mark (`#`), the rest 
of the line will be ignored.

If command is entered without arguments *and* its name contains asterisk
character(s) (`*`), the command name is treated as a mask, and console would
print out list of all commands that match that mask.

Console interface conventions almost completely match those of the server 
configuration file.

Interface Commands
------------------

These are commands used to interact with console, and do not have counterparts 
in PHI API, or server protocol.

### `HELP` ###

Lists all supported commands, or describes a particular command. The question 
mark (`?`) is an alias for `help`, but can only be used without arguments (that 
is, `? <console-command` is *not* supported).

Syntax:

    HELP [ <console-command> ]

### `VERSION` ###

Prints out console version. To get server version, it is necessary to execute 
`GET version` command.

Syntax:

    VERSION

### `VERBOSITY` ###

Allows to set verbosity of the messages output by console. Setting verbosity
level to, say, `terse`, or any other less verbose level than `normal` (which is
the default) may benefit execution of console scripts, as it will reduce "noise"
in the output.

> This console command has no effect on the server; to control verbosity of
> server output, use `SET log_level <level>` command instead; similarly, use
> `GET log_level` to retrieve current value.

Syntax:

    VERBOSITY [ debug | verbose | normal | terse | warning | error | fatal | explicit ]

### `EXECUTE` ###

Loads file specified as argument and executes commands contained in it.

> **IMPORTANT**: if argument file contains `ADMIN` or `USER` commands, those 
> commands can have arguments -- password values (*not* recommended!). If 
> arguments to password-setting commands are not specified, console will prompt 
> the user to enter them.

Loaded files can also contain `EXECUTE` commands that load and process other 
files; recursion depth is limited by `2` in Community Edition, and 8 in 
Enterprise version of the CyberCache Cluster.

Syntax:

    EXECUTE <command-file-path>

### `EXIT` / `QUIT` / `BYE` ###

Closes console session and exits to the operating system. The server keeps 
running (to shut down the server, `SHUTDOWN` command should be used).

> Note that `EXIT` command has two aliases: `QUIT` and `BYE`, which do exactly
> the same.

Syntax:

    EXIT | QUIT | BYE

Auxiliary Commands
------------------

Commands in this section have immediate affect on `cybercache` console only, so
almost all regular subsections are omitted for clarity. Other console-only
commands (e.g. `HELP` or `EXIT`) are not in this section because they do not
affect subsequently executed commands.

Some of these commands have counterparts in PHP API, in that there are 
`CyberCache` configuration methods that can (or sometimes must) be called on the 
instance of `CyberCache` class before executing commands that actually connect 
to the server; whenever that is the case, respective commands' descriptions have 
`PHP extension method:` subsections.

> IMPORTANT: almost all settings controlled by these commands have sensible 
> defaults; it is *not necessary* to execute them before the commands they 
> affect. Even `CONNECT` command can be omitted, in which case the console will 
> send commands to `localhost` (`127.0.0.1`) using default port 8120.

When invoked without an argument, these commands return (print out) currently
stored value(s) that will be used when another command, the one that they
affect, will be executed -- except for 'ADMIN' and 'USER' commands, which would 
only tell if respective password is set or not.

### `CONNECT` ###

Sets network address and, optionally, port to connect to while executing server
commands. The address can be specified as either IP address, or a host name
without protocol prefix. In latter case, it will be resolved to IP address and
printed out. If port number is omitted, the default value of 8120 will be used.
Specified values will remain in effect till end of the session, or until another
`CONNECT` command is executed.

Syntax:

    CONNECT [ <network-address> [ <port-number> ]]

PHP INI options:

    c3.session_address
    c3.fpc_address

Option name passed in option array to `c3_session()` / `c3_fpc()`:

    address

### `PERSISTENT` ###

Specifies whether future connections to the server should be persistent, or if
the console should hang up after issuing each command. Default value (if
'persistent <boolean>' is never executed) is 'true'. If the command is executed
without an argument, current setting will be printed. It is important that the
same type of connections (per-command or persistent) are used by all components
of the CyberCache cluster.

Syntax:

    PERSISTENT [ <boolean> ]

PHP INI options:

    c3.session_persistent
    c3.fpc_persistent

Option name passed in option array to `c3_session()` / `c3_fpc()`:

    persistent

### `USER` ###

Reads password that will then be passed on withing all commands that require
user-level authentication. Does not accept any arguments; instead, it enters
password-input mode, and will accept input on a new line, *not* echoing
characters being typed. Entered value remains in effect until end of the
session, or until another `USER` command is executed. If this console command
had never been executed but `ADMIN` command had, user-level commmands will be
issued with admin password instead. If neither password had been set, user-level
commands will be issued without password.

Syntax:

    USER

PHP INI options:

    c3.session_user
    c3.fpc_user

Option name passed in option array to `c3_session()` / `c3_fpc()`:

    user

### `ADMIN` ###

Reads password that will then be passed on withing all commands that require
admin-level authentication (and, if `USER` password had not been entered, along
with all commands requiring user-level authentication). Does not accept any
arguments; instead, it enters password-input mode, and will accept input on a
new line, *not* echoing characters being typed. Entered value remains in effect
until end of the session, or until another `USER` command is executed. If this
console command had never been executed, admin-level commands will be issued
without password.

Syntax:

    ADMIN

PHP INI options:

    c3.session_admin
    c3.fpc_admin

Option name passed in option array to `c3_session()` / `c3_fpc()`:

    admin

### `USERAGENT` ###

Affects subsequent `READ`, `WRITE`, `LOAD`, `TEST`, and `SAVE` commands by
passing them type of user agent that stores data (which can affect record
lifetime). Entered value remains in effect until end of the session, or until
another `USERAGENT` command is executed; re-connecting to the server does not
change this setting. The following user agents are recognized:

- `unknown` : (0x00) an unknown bot (empty "user agent" in the request, or one that *starts* with `curl`),
- `bot` : (0x01) a known bot; PHP extension detects it with the following regexp: 
  '/^alexa|^blitz\.io|bot|^browsermob|crawl|^facebookexternalhit|feed|google web preview|^ia_archiver|indexer|^java|jakarta|^libwww-perl|^load impact|^magespeedtest|monitor|^Mozilla$|nagios |^\.net|^pinterest|postrank|slurp|spider|uptime|^wget|yandex/i',
- `warmer` : (0x02) CyberHULL cache warmer (has "CyberCache<version>.<build>" in the request),
- `user` : (0x03) probably a valid user, not a bot (the default).

> *NOTE*: order of constants is such that it arranges data marked with them in
> order from most likely to least likely to get kicked out during optimization /
> garbage collection processes.

Syntax:

    USERAGENT [ <agent-type> ]

PHP INI options:

    N/A

Option name passed in option array to `c3_session()` / `c3_fpc()`:

    N/A

### `TAGS` / `ADDTAGS` / `REMOVETAGS` ###

Affect subsequent `SAVE` commands by passing them list of tags to be associated
with the record. Entered list remains in effect until end of the session, or
until another `TAGS`, or `ADDTAGS`, or `REMOVETAGS` command is executed;
re-connecting to the server does not change this setting.

The `TAGS` command completely replaces the list that is in effect at the time of 
command execution, while `ADDTAGS` and `REMOVETAGS` commands add or remove tags 
to the current list, respectively.

> NOTE: only `TAGS` command can be specified without arguments (displaying 
> current list of tags), while `ADDTAGS` and `REMOVETAGS` commands must have at 
> least one argument.

Syntax:

    TAGS [ <tag> [ <tag> [[ ... ]]]]
    ADDTAGS <tag> [ <tag> [[ ... ]]]
    REMOVETAGS <tag> [ <tag> [[ ... ]]]

PHP INI options:

    N/A

Option name passed in option array to `c3_session()` / `c3_fpc()`:

    N/A

### `LIFETIME` ###

Affects subsequent `WRITE` and `SAVE` commands by passing them record lifetime
(number of seconds, or a special value; see below). Entered value remains in
effect until end of the session, or until another `LIFETIME` command is
executed; re-connecting to the server does not reset this setting.

Specifying lifetime of `0` means "infinite". Specifying `-1` (the default) means
use whatever default value had been set in the configuration file using the
`session_default_lifetimes` or `fpc_default_lifetimes` options for a specific
user agent (that will be in effect at the time of `LOAD`/`SAVE` execution).
Using value bigger than one set by `fpc_max_lifetime` does not generate an
error (because console neither uses configuration file, nor communicates with
the server while executing the `LIFITIME` command), but subsequent `SAVE`
command will clip expiration time to current time plus `fpc_max_lifetime`.

Syntax:

    LIFETIME [ <duration> ]

PHP INI options:

    N/A

Option name passed in option array to `c3_session()` / `c3_fpc()`:

    N/A

### `MARKER` ###

Affects all subsequent commands in that it controls whether integrity check
marker will be added after each command. Entered value remains in effect until
end of the session, or until another `MARKER` command is executed; re-connecting
to the server does not reset this setting.

Syntax:

    MARKER [ yes | no | on | off | true | false ]

PHP INI options:

    c3.session_marker
    c3.fpc_marker

Option name passed in option array to `c3_session()` / `c3_fpc()`:

    marker

### `THRESHOLD` ###

Sets minimal size for the data (being sent to the server) to be eligible for
compression attempt. This option does not affect the server in any way, the
server compression is controlled by the options in its configuration file. If
size of the data (such as a session or FPC record) is less than the threshold,
it will always be sent to the server uncompressed.

Setting this option to 4294967295 (that is, 2^32-1) essentially disable
compression of the outbound data.

Syntax:

    THRESHOLD [ <positive-number> ]

PHP INI options:

    c3.session_threshold
    c3.fpc_threshold

Option name passed in option array to `c3_session()` / `c3_fpc()`:

    threshold

### `COMPRESSOR` ###

Specifies which compressor will be used for commands that contain payloads, such
as `WRITE` and `SAVE`. Entered value remains in effect until end of the session,
or until another `COMPRESSOR` command is executed; re-connecting to the server
does not reset this setting. Argument can be one of the following (see 
configuration file for info on speed and strengths of these algoritms):

- `lzf` : LZF by Marc Alexander Lehmann
- `snappy` : Snappy by Google (the default)
- `lz4` : LZ4 by Yann Collet (cannot compress buffers bigger than 2 gigabytes)
- `lzss3` : LZSS by Haruhiko Okumura
- `brotli` : Brotli by Jyrki Alakuijala and Zoltan Szabadka
- `zstd` : Zstd by Yann Collet (Facebook, Inc.)
- `zlib` : Zlib (gzip) by Jean-loup Gailly and Mark Adler
- `lzham` : Lzham by Richard Geldreich, Jr.

Note that this command controls compression of payloads of issued *commands*; 
which compressor will be used to compress payloads of responses is controlled by 
respective options in server configuration files.

Syntax:

    COMPRESSOR [ lzf | snappy | lz4 | lzss3 | brotli | zstd | zlib | lzham ]

PHP INI options:

    c3.session_compressor
    c3.fpc_compressor

Option name passed in option array to `c3_session()` / `c3_fpc()`:

    compressor

### `HASHER` ###

Specifies which hashing algorithm will be used to for passwords entered via
`USER` and `ADMIN` commands. Entered value remains in effect until end of the
session, or until another `HASHER` command is executed; re-connecting to the
server does not reset this setting. Argument can be one of the following (see 
configuration file for more detailed descriptions):

- `xxhash` : extremely fast algorithm with good distribution, by Yann Collet,
- `farmhash` : very fast algorithm with extremely good distributionby Google;
  requires support of SSE 4.2 instructions by CPU,
- `spookyhash` : algorithm by Bob Jenkins, almost as fast as Google's; does not
  require any special hardware support,
- `murmurhash2` : the algorithm by Austin Appleby, used by Redis cache server,
- `murmurhash3` : next generation of the `murmurhash2` algorithm.

Default value is `murmurhash2`.

> **IMPORTANT**: if non-default password hash method is set in the configuration
> file using `password_hash_method` option, AND either user or administrative
> password is set (using `user_password` or `admin_password` option,
> respectively), then `HASHER` command **must** be executed in the console 
> before execution of any command that requires authentication. And, similarly, 
> PHP method `hasher()` **must** be called on the `CyberCache` instance before 
> calling any method that will carry out a command requiring authentication.

Syntax:

    HASHER [ xxhash | farmhash | spookyhash | murmurhash2 | murmurhash3 ]

PHP INI options:

    c3.session_hasher
    c3.fpc_hasher

Option name passed in option array to `c3_session()` / `c3_fpc()`:

    hasher

### `RESULT` ###

Unlike all other commands in the section (except `NEXT`), `RESULT` does not
affect any subsequent commands; instead, it prints out portion of the result of
last executed command that returned a list or a chunk of data.

Whenever a command returns a chunk of data, last line after printed out data is
`Received <N> bytes; showing <M> bytes starting at <offset>.`. Likewise, when a 
command returns a list (counted or now), last line after printed out data is 
`Received <N> items; showing <M> items starting at <item-index>.` If not all 
data chunk bytes or list items are displayed, the `RESULT` command can be used
to view them.

Negative offset (`<from>`) means *from the end of buffer or list*, negative 
length means *this many bytes before offset*; omitted offset means *from the 
very beginning of the buffer of list*, while omitted length means *to the very 
end of the buffer or list*. However, there is a limit to how many bytes or list 
items this command will display, and it is hardcoded.

The `<from>` and `<length>` parameters used with the last invocation of `RESULT`
are remembered by the console and affect what `NEXT` command would display.
Invocation of any command that returns a data chunk or a list resets them.

Syntax:

    RESULT [ <from> [ <length> ]]

PHP INI options:

    N/A

Option name passed in option array to `c3_session()` / `c3_fpc()`:

    N/A

### `NEXT` ###

Just like `RESULT`, `NEXT` does not affect any subsequent commands but just
prints out portion of the result of last executed command that returned a list
or a chunk of data. It prints out `<length>` data bytes or list items right
after the last portion printed by `RESULT` or `NEXT`, whichever was executed
last; if `RESULT` was not executed after a command that returned a data chunk or
a list (and thus reset the offset), it would start at zero offset. Omitting
`<length>` means *same as the last used one*. It *is* allowed to execute `NEXT` 
command without executing `RESULT` first.

Syntax:

    NEXT [ <length> ]

PHP INI options:

    N/A

Option name passed in option array to `c3_session()` / `c3_fpc()`:

    N/A

### `AUTORESULT` ###

Sets command execution result printing mode: what kind of results should be 
fully printed out automatically. If executed without argument, displays 
currently active mode. Available modes are:

- `simple` : print out all results except lists and byte buffers,
- `lists` : print out all results except byte buffers (the default),
- `all` : print out all types of results.

Here, "not printing out" a result means that console will only indicate that,
say, an buffer with N bytes had been received, and `RESULT` (or `NEXT`) command
should be used to display received strings.

Syntax:

    AUTORESULT [ simple | lists | all ]

PHP INI options:

    N/A

Option name passed in option array to `c3_session()` / `c3_fpc()`:

    N/A

### `CHECKRESULT` ###

Checks that last response received from the server has specified type and,
optionally, that it contains specified data. If a `<string>`s are specified
along with `error` or `string` type, checks that response string contains all
specified substrings. If `<element>`s are specified, checks that list response
contains all given elements; matches must be exact, in that if, say, returned
list has element `Cats`, then `checkresult list Cat` would fail.

If `<offset>` *and* `<bytes>` are specified along with the `data` type, then
this command will check that data response has `<bytes>` at offset `<offset>`.
If `<bytes>` is specified as a binary string containing zero bytes (e.g. as
"foo\00bar"), then only bytes before first zero byte will be compared. If there
is `@` prefix before `<bytes>`, then `<bytes>` will be treated as file name; the
file will be loaded, and its contents will be used for comparison. A sequence
similar to the following can be used to test if a big data record was
successfully stored and then retrieved from the server:

    save my-id @my-big-data-file.xml
    checkresult ok
    load my-id
    checkresult data 0 @my-big-data-file.xml

Syntax:

    CHECKRESULT { ok | error [ <string1> [ <string2> [...]]] | string [ <string1> [ <string2> [...]]] |
      list [ <element1> [ <element2> [...]]] | data [ <offset> [@]<bytes> ] }

PHP INI options:

    N/A

Option name passed in option array to `c3_session()` / `c3_fpc()`:

    N/A

### `DISPLAY` ###

Controls output of `RESULT` and `NEXT` commands. The `<lines-per-screen>`
argument only affects `RESULT`, only when it (the `RESULT` command) is used
without arguments for the first time after some server command execution, and
only if result contains a byte buffer or a list. The `<bytes-per-line>` affects
both `RESULT` and `NEXT`, but only if result returned from the server is a byte
array; note that all bytes are output twice: once as a two-character hexadecimal
value, and once as a pritable character, so large `<bytes-per-line>` values are
not practical. Finally, the `<non-printable-char>` specifies what should be
printed if a byte in the received byte array does not represent a printable
character (default is '.').

Syntax:

    DISPLAY [ <lines-per-screen> [ <bytes-per-line> [ <non-printable-char> ]]]

PHP INI options:

    N/A

Option name passed in option array to `c3_session()` / `c3_fpc()`:

    N/A

### `PRINT` ###

Prints specified message. Can be used in batch mode, when it's necessary to 
inditate what file is being loaded (i.e. what type of configuration changes are 
being made), or that all previous commands in the file had been executed 
successfully. The batch file can be loaded by `EXECUTE` command, or specified as 
a command line argument to the console application.

Syntax:

    PRINT <message-to-display>

PHP INI options:

    N/A

Option name passed in option array to `c3_session()` / `c3_fpc()`:

    N/A

### `EMULATE` ###

Compiles and sends server command irrespective of whether specified command ID
is a valid one, and whether format of the command for specified ID (if it
*happens* to be valid) is correct, thus allowing to test server responses to
various invalid input.

In the below syntax diagram, first line represents either a single command ID, 
or a command descriptor. In the descriptor, a password can be specified either 
as a (possibly empty) string, or `-` (meaning *use currently active password*).
Payload should specified as a string (again, possibly empty); if it is prefixed 
with `@`, then the string is treated as file path that will be loaded and used 
as payload.

Remaining lines of the syntax diagram represent optional arguments; an argument 
can be an integer number, a string, or a list. If a string "looks" like a number 
but respective argument should still be sent as a string, it should be prefixed 
with `%`. A list is a sequence of strings in square brackets separated with 
spaces; it is *not* necessary to prefix any strings comprising list with `%`, 
they will all be sent as strings even if they "look" like numbers.

It is important that `{`, `}`, `[`, and `]` descriptor and list separators have 
spaces around them. Say, entering `{36 }` or `{ 36}` instead of `{ 36 }` would 
result in parsing error, and the command will not be sent to the server.

Sintax:

    EMULATE <id> | `{` <id> [ user | admin [ `-` | <password> [ [`@`]<payload> [ <check> ]]]] `}`
      [ <number> | <string> | `%`<number> | `[` [ <string> [ <string> [...]]] `]`
        [ <number> | <string> | `%`<number> | `[` [ <string> [ <string> [...]]] `]`
        [...]]]

PHP INI options:

    N/A

Option name passed in option array to `c3_session()` / `c3_fpc()`:

    N/A

Information Commands
--------------------

In order to execure any command in this category, authorization may or may not
be necessary depending upon `use_info_password` option in server configuration
file, which can take one of the following values:

- `none` : password is not necessary (the default),
- `user` : user password is required,
- `admin` : admin password is required.

> *IMPORTANT*: console application does not "know" what value was given to the 
> `use_info_password` in the server configuration file, so whenever an 
> information command issued to the console, it authorizes it with the 
> "strongest" password it received so far (i.e. if both "user" and "admin" 
> passwords were specified using `USER` and `ADMIN` console commands [see
> below], respectively, console will be sending "admin" password with every
> command); this works because password type is sent with every command, and
> authorizing any command (any, not just informational) with "admin" password
> even if the command only requires "user" password is OK -- *provided that*
> correct "admin" password is indeed specified in server's configuration file.

### `PING` ###

Simple server availability check, always responds with `OK` to a well-formed 
command. PHP method returns `true` on success, or `false` on error.

  Console command:

    [ USER ]
    [ ADMIN ]
    [ MARKER <boolean> ]
    PING

  PHP extension method:

    bool c3_ping($resource)
  
  Request sequence:

    DESCRIPTOR 0x01 [ PASSWORD ] [ MARKER ]
  
  Binlog / replication:

    N/A

  Server response:

    - OK [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    user_password <password-string>
    admin_password <password-string>
    use_info_password [ none | user | admin ]

### `CHECK` ###

Report results of last health check of the server, along with current load.
Meant be used to monitor server availability in highly loaded setups where
`PING` is not enough. On success, returns three integers; the first integer is
percentage of *connection* threads that are currently busy processing requests
(`100*<busy-threads>/<total-threads>`, i.e. the number between 0 and 100), the
second and third are numbers of warnings and non-fatal errors that has occurred
since server was started, respectively (actual errors are not returned, and has
to be looked up in log file). On error, PHP method returns empty array.

  Console command:

    [ USER ]
    [ ADMIN ]
    [ MARKER <boolean> ]
    CHECK

  PHP extension method:

    array c3_check($resource)
  
  Request sequence:

    DESCRIPTOR 0x02 [ PASSWORD ] [ MARKER ]
  
  Binlog / replication:

    N/A

  Server response:

    - DATA HEADER { CHUNK(NUMBER) CHUNK(NUMBER) CHUNK(NUMBER) } [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    user_password <password-string>
    admin_password <password-string>
    use_info_password [ none | user | admin ]

### `INFO` ###

Prints out current status and usage information either for specified domains or,
if domains are omitted, for all server domains. This command does *not* print
out configuration options; there's `GET` command for that. Likewise, it does
*not* return any performance counters; if that is necessary, `STATS` command
should be used.

The list returned by the server contains one element per parameter, with
parameters and their values separated with ` : ` (space-colon-space); before
printing them out, console figures out rightmost position of the colon, and
outputs the list justified relative to the position of the colon. If an
parameter's value is actually an array, it is reported as a string like
`[<val1>,<val2>,...<valN>]` and printed out by the console as-is.

  Console command:

    [ USER ]
    [ ADMIN ]
    [ MARKER <boolean> ]
    INFO [ GLOBAL | SESSION | FPC | ALL [ GLOBAL | SESSION | FPC | ALL [...]]]

  PHP extension method (returns array on success, empty array on error):

    array c3_info($resource, int $domain = C3_DOMAIN_ALL)
  
  Request sequence:

    DESCRIPTOR HEADER { 0x10 [ PASSWORD ] CHUNK(NUMBER) } [ MARKER ]
  
  Binlog / replication:

    N/A

  Server response:

    - LIST HEADER { PAYLOAD_INFO CHUNK(NUMBER) } PAYLOAD [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    user_password <password-string>
    admin_password <password-string>
    use_info_password [ none | user | admin ]

### `STATS` ###

Prints out performance counters either for specified domains or, if domains are
omitted, for entire server and all its domains. If <mask> argument is
specified, only retrieves performance counters whose names match the mask (and,
of course, which belong to specified domains). The mask may contain arbirtary
number of asterisks (`*`), which match any text so, for instance, the following
requests are possible (matching is case-insensitive):

    stats                            (the first three examples are equivalent)
    stats *
    stats * all
    stats *queue* session fpc
    stats * global
    stats *_pipeline_*
    stats num_*
    stats *events fpc

The list returned by the server contains one element per performance counter,
with counter names and their values separated with `: ` (colon-space); before
printing them out, console figures out rightmost position of the colon, and
outputs the list justified relative to the position of the colon. For some
counters, separate values are reported for global, session, and FPC domains (if
a counter is store-specific, "global" value actually refers to tags). If there
are more than three values per counter, they denote numbers of times each
tracked value was encountered (e.g. "1 4 3 5 0 3" means that value of `0` was
encountered 1 time, value `1` 4 times, value `2` 3 times, etc.).

> This command *requires instrumented version* of the CyberCache server, and
> will return error message if console or PHP extension is connected to
> production version of the CyberCache server.

  Console command:

    [ USER ]
    [ ADMIN ]
    [ MARKER <boolean> ]
    STATS [ <mask> [ GLOBAL | SESSION | FPC | ALL [ GLOBAL | SESSION | FPC | ALL [...]]]]

  PHP extension method (returns array on success, empty array on error):

    array c3_stats($resource, int $domain = C3_DOMAIN_ALL, string $mask = "*")
  
  Request sequence:

    DESCRIPTOR HEADER { 0x11 [ PASSWORD ] CHUNK(NUMBER) CHUNK(STRING) } [ MARKER ]
  
  Binlog / replication:

    N/A

  Server response:

    - LIST HEADER { PAYLOAD_INFO CHUNK(NUMBER) } PAYLOAD [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    user_password <password-string>
    admin_password <password-string>
    use_info_password [ none | user | admin ]

Administrative Commands
-----------------------

In order to execure any command in this category, it is necessary to enter
administrator password first (using `ADMIN` auxiliary command) -- unless admin
password is empty in the configuration file.

### `SHUTDOWN` ###

Shuts down the server *to which the console is currently connected*.

  Console command(s):

    [ ADMIN ]
    [ MARKER <boolean> ]
    SHUTDOWN

  PHP extension method:

    bool c3_shutdown($resource)
  
  Request sequence:

    DESCRIPTOR 0xF0 [ PASSWORD ] [ MARKER ]
  
  Binlog / replication:

    N/A

  Server response:

    - OK [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    admin_password <password-string>
    perf_shutdown_wait <duration>
    perf_shutdown_final_wait <duration>

### `LOCALCONFIG` ###

Loads local (i.e. located on the computer where server console is running)
configuration file and passes *its contents* to the CyberCache server for
interpretation.

Options that are not specified in the file being loaded are not affected
(i.e. will retain their values, either set by previously loaded
configuration file [default, specified using command line, local, or
remote], or defaults if no configuration file has set them during current
session). Therefore, it is entirely possible to have "small" config files that 
only change a particular set of options.

> **IMPORTANT**: the following options 
>
> - `user_password`
> - `admin_password`
> - `bulk_password`
> - `table_hash_method`
> - `password_hash_method`
> - `session_tables_per_store`,
> - `fpc_tables_per_store`,
> - `tags_tables_per_store`, and
> - `perf_num_internal_tag_refs`
>
> in the configuration file being loaded will be ignored; this behavior is
> consistent with `SET` and `REMOTECONFIG` console commands and respective PHP
> API calls.

> **VERY IMPORTANT**: if configuration file sent using `LOCALCONFIG` contains 
> `include` statements, those statements will be executed *on the host that runs
> the server*, which is most probabably not what one would expect or want.

`LOCALCONFIG` is a console / PHP API command (i.e. not a protocol command), in
that it is the console (or PHP method) that loads configuration file into its
buffer and then sends `SET` command to the server; from the server's standpoint,
there is no difference in interpreting a single name/value line of text, or a
complete contents of a configuration file.

  Console command(s):

    [ ADMIN ]
    [ MARKER <boolean> ]
    LOCALCONFIG [ <path> ]

  PHP extension method:

    bool c3_local_config($resource, string $path)

  Request sequence (identical to that of `SET` command):

    DESCRIPTOR HEADER { 0xF6 [ PASSWORD ] CHUNK(STRING) } [ MARKER ]

  Binlog / replication:

    N/A

  Server response:

    - OK [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    admin_password <password-string>
    The option(s) specified in the configuration file being loaded

### `REMOTECONFIG` ###

Passes path to a remote (i.e. located on the host where CyberCache server itself
is running) configuration file to the CyberCache server for loading and
interpretation. If path is not specified, last configuration file loaded using
`REMOTECONFIG` is assumed; if `REMOTECONFIG` with a valid path has not been
executed yet *in this console*, omitting path would result in error.  In other
words, default path is a property of console, not server; so PHP API call does
not have a default value for its parameter.

To locate the file, the server will use the same algorithm it uses to locate 
configuration file specified as command line argument; specifically:

- if path starts with either `.` or `/`, it will be used "as is" (note that 
  backslash is not interpreted as path separator, even on Windows),

- otherwise, server searches `/etc/` path for configuration file,

- if the file is not found, server checks directory containing its executable,

- if the file is still not found, server checks current directory.

Options that are not specified in the file being loaded are not affected
(i.e. will retain their values, either set by previously loaded
configuration file [default, specified using command line, local, or
remote], or defaults if no configuration file has set them during current
session). Therefore, it is entirely possible to have "small" config files that 
only change a particular set of options.

> **IMPORTANT**: the following options 
>
> - `user_password`
> - `admin_password`
> - `bulk_password`
> - `table_hash_method`
> - `password_hash_method`
> - `perf_num_internal_tag_refs`
>
> in the configuration file being loaded will be ignored; this behavior is
> consistent with `SET` and `LOCALCONFIG` console commands and respective PHP
> API calls.

The CyberCache protocol command used for the implementation of `REMOTECONFIG` 
console command and `remoteConfig()` PHP API call is `LOADCONFIG`.

  Console command(s):

    [ ADMIN ]
    [ MARKER <boolean> ]
    REMOTECONFIG [ <path> ]

  PHP extension method:

    bool c3_remote_config($resource, string $path)

  Request sequence (`LOADCONFIG` command):

    DESCRIPTOR HEADER { 0xF1 [ PASSWORD ] CHUNK(STRING) } [ MARKER ]

  Binlog / replication:

    N/A

  Server response:

    - OK [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    admin_password <password-string>
    The option(s) specified in the configuration file being loaded

### `RESTORE` ###

Loads cache records stored in a session of FPC binlog file that is specified by 
name; current cache contents is not dropped, the server keeps working and 
servicing incoming requests.

Even though there are separate binlog (saving) services for session and FPC
domains, binlog files themselves do not differ. It is possible, for example, to
configure the server to write session binlog to some "file.bin" with no binlog
file for FPC domain, then re-configure the service to not have session binlog
file, but write FPC binlog to that same "file.bin". If we then `RESTORE` from
that file, binlog loader will first replay saved session records, and then FPC
records. So `RESTORE` command does not have any domain-specific flavours.

  Console command(s):

    [ ADMIN ]
    [ MARKER <boolean> ]
    RESTORE <path>

  PHP extension method:

    bool c3_restore($resource, string $path)

  Request sequence:

    DESCRIPTOR HEADER { 0xF2 [ PASSWORD ] CHUNK(STRING) } [ MARKER ]

  Binlog / replication:

    N/A

  Server response:

    - OK [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    admin_password <password-string>

### `STORE` ###

Stores entire contents of the specified cache domain (or domains) to the
database file. If `path` refers to an existing file, it will be overwritten.
If first arguments is `all` (meaning both session and FPC domains), then all
records will be saved to a single file. If `path` does *not* contain forward
slaches ('/'), then it will be prefixed with path to CyberCache's data
directory (usually `/var/lib/cybercache/`); otherwise, it will be used as-is.

The `<user-agent>` argument allows to filter out some records; CyberCache keeps
track of user types that created each record, so it might make sense to, say,
exclude session records created by bots, or FPC records created by unknown
users. The `<user-agent>` specifies "lowest" user agent type whose records has
to be preserved; for instance, specifying `warmer` will cause records created
by cache warmer and regular users to be stored, while records created by
unknown users and bots will be skipped. Default is `unknown` (which essentially
means "store all records").

The `<sync-mode>` argument controls synchronization during writing data to
disk; available options are (in order of fastest but least reliable, to slowest
but most reliable): `none` (the default), `data-only`, `full`.

> *IMPORTANT*: if you want to store session and FPC data using different sets
> of user agent and/or synchronization options, you'll have to use *two*
> `STORE` commands. And you'll also have to use **different** file names for
> each of them, so that second command does not overwrite output of the first.

Files created using `STORE` command can then be loaded using `RESTORE`. There
is also set of server configuration options that allow to configure the server
to save databases on shutdown, and the re-load them on startup; see
`session_db_file`, `fpc_db_file` and related options' description in the main
server configuration file.

  Console command(s):

    [ ADMIN ]
    [ MARKER <boolean> ]
    STORE { all | session | fpc } <path> [ { unknown | bot | warmer | user } [ { none | data-only | full ] ]

  PHP extension method:

    bool c3_store($resource, int $domain, string $path, int $ua = C3_UA_UNKNOWN, int $sync = C3_SYNC_NONE)

  Request sequence:

    DESCRIPTOR HEADER { 0xF3 [ PASSWORD ] CHUNK(NUMBER) CHUNK(STRING) CHUNK(NUMBER) CHUNK(NUMBER) } [ MARKER ]

  Binlog / replication:

    N/A

  Server response:

    - OK [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    admin_password <password-string>

### `GET` ###

Get current value of any configuration option other than `admin_password`,
`user_password`, or `bulk_password`. It is possible to request values of more
then one variable. All values are always returned as strings; it is up to the
caller to parse them into ints etc. as/if needed (though in PHP that is rarely
necessary).

> It is possible to use arbitrary number of asterisks (`*`) in the name of the
> option, thus retrieving values of all options that match the wildcard. For
> instance, it is possible to retrieve options values like this:
>
>     - get *
>     - get perf_*
>     - get *read*
>     - get perf_*replicator*
>     - get *_capacity
>
> It is also possible to specify more than one mask per request.

Configuration option values are reported in the same format as that of the 
`INFO` and `STATS` commands.

Not all options' values can be retrieved using `GET`; for instance, because of
the asynchronous nature of CyberCache subsystems, only [most] numeric values can
be retrieved, but not strings: it is not possible to request current log or
binlog file name, or rotation path, for instance -- that would require locking
respective subsystem, and CyberCache is architected in a way that avoids locking
if at all possible. Some numeric values *could* be retrieved, but it's not clear
how to represent them; as an example, `perf_fpc_init_table_capacity` sets
*initial* capacity of all hash tables of the FPC object store, but by the time
of `GET` request those tables would most likely have different capacities, and
there can be up to `256` hash tables in the FPC store (in *Enterprise* edition).
Passwords will not be retrieved for obvious reasons (in fact, they are not
stored even internally: only their hash codes are). Therefore:

> Options whose values cannot be retrieved will have **value could not be 
> retrieved** against their names; they won't be completely omitted so that not 
> to confuse the user (especially if options were queried using a mask).

In addition to options that can be set in the configuration file or command
line, `GET` can also fetch one option that can *not* be set; namely, `version`.

  Console command(s):

    [ ADMIN ]
    [ MARKER <boolean> ]
    GET <configuration-option> [ <configuration-option> [ ... ]]

  PHP extension method (returns empty array on errors):

    array c3_get($resource, string ... $option_names)

  Request sequence:

    DESCRIPTOR HEADER { 0xF5 [ PASSWORD ] CHUNK(LIST) } [ MARKER ]

  Binlog / replication:

    N/A

  Server response:

    - LIST HEADER { [ PAYLOAD_INFO ] CHUNK(NUMBER) } [ PAYLOAD ] [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    admin_password <password-string>
    The option(s) being fetched

### `SET` ###

Set new value of any configuration option. Both console and PHP method
concatenate option name and value with a space (" ") and send them to the server
as a single string; if option value is an empty string *or* starts with a space,
it will be quoted.

> **IMPORTANT**: an attempt to set any of the following options 
>
> - `user_password`
> - `admin_password`
> - `bulk_password`
> - `table_hash_method`
> - `password_hash_method`
> - `perf_num_internal_tag_refs`
>
> will be ignored; this behavior is consistent with `LOCALCONFIG` and
> `REMOTECONFIG` console commands and respective PHP API calls.

  Console command(s):

    [ ADMIN ]
    [ MARKER <boolean> ]
    SET <configuration-option> <new-value>

  PHP extension method:

    bool c3_set($resource, string $option_name, $option_value)
  
  Request sequence:

    DESCRIPTOR HEADER { 0xF6 [ PASSWORD ] CHUNK(STRING) } [ MARKER ]

  Binlog / replication:

    N/A

  Server response:

    - OK [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    admin_password <password-string>
    The option being changed

### `LOG` ###

Log provided message to the server log. The command is actually passed to (and
executed by) the server; its output is *not* echoed to the console. Current log
level of the server does *not* affect this command, it will always work (unless,
of course, logging is disabled in the configuration).

  Console command(s):

    [ ADMIN ]
    [ MARKER <boolean> ]
    LOG <message>

  PHP extension method:

    bool c3_log($resource, string $message)
  
  Request sequence:

    DESCRIPTOR HEADER { 0xFA [ PASSWORD ] CHUNK(STRING) ] } [ MARKER ]

  Binlog / replication:

    N/A

  Server response:

    - OK [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    admin_password <password-string>
    log_file_name <file-path>
    log_file_max_size <file_size>

### `ROTATE` ###

Rotate log file that corresponds to the service specified as argument; file
rotation will be forced even if log file size did not reach rotation threshold
specified in the configuration. Unlike with `INFO` or `STATS` commands, not
specifying domain argument causes regular log file (i.e. not s session or FPC
binlog) rotation. To force rotation of all log files, it is necessary to list
all domains/services (`LOG`, `SESSIONBINLOG`, `FPCBINLOG`).

  Console command(s):

    [ ADMIN ]
    [ MARKER <boolean> ]
    ROTATE [ LOG | SESSIONBINLOG | FPCBINLOG [ LOG | SESSIONBINLOG | FPCBINLOG [...]]]

  PHP extension method (here, `DOMAIN_GLOBAL` stands for regular log file):

    bool c3_log($resource, int $domain = C3_DOMAIN_GLOBAL)
  
  Request sequence:

    DESCRIPTOR HEADER { 0xFB [ PASSWORD ] CHUNK(NUMBER) ] } [ MARKER ]

  Binlog / replication:

    N/A

  Server response:

    - OK [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    admin_password <password-string>
    log_file_name <file-path>
    session_binlog_file_name <file-path>
    fpc_binlog_file_name <file-path>

Session Cache Commands
----------------------

In order to execure any command in this category, it is necessary to enter user
password (using `USER` auxiliary command) first -- unless user password is empty
in the server configuration file.

### `READ` ###

Fetches session cache entry. Console prints out *uncompressed* fetched data as
text, replacing non-printable characters with dots. When working with cache
through console, user agent type (the `CHUNK(NUMBER)` passed to to the server,
usually the result of analyzing `user_agent` field of the request passed to the
PHP method) is the one specified using `USERAGENT` console command.

> An attempt to fetch a non-existent session cache entry does *not* result in
> an error: in such a case, server returns `Ok` response, `read()` returns
> an empty string (which is consistent with `SessionHandlerInterface::read()`
> calling convention), and `getErrorMessage()` returns an empty string.

If specified session record exists and not marked as "deleted", then if it had
been expired, it will be "revived" by setting its expiration timestamp to
current time plus `session_read_extra_lifetime[<user-agent>]`, and returned by
the server. If `session_eviction_mode` is *not* `expiration-lru` or
`strict-expiration-lru`, then record's expiration time will be adjusted as if it
was expired (but only if `session_read_extra_lifetime` is bigger than remaining
lifetime).

  Console command(s):

    [ USER ]
    [ USERAGENT [ <agent-type> ]]
    [ MARKER <boolean> ]
    READ <entry-id>

  PHP extension method (user agent is taked from resource):

    string c3_read($resource, string $entry_id)

  Request sequence:

    DESCRIPTOR HEADER { 0x21 [ PASSWORD ] CHUNK(STRING) CHUNK(NUMBER)
      [ CHUNK(NUMBER) ] } [ MARKER ]

  Binlog / replication:

    N/A

  Server response:

    - OK [ MARKER ]
    - DATA HEADER { PAYLOAD_INFO } PAYLOAD [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    user_password <password-string>
    session_read_extra_lifetime <duration> [ <duration> [...]]
    session_eviction_mode { strict-expiration-lru | expiration-lru | lru | strict-lru }

### `WRITE` ###

Stores session cache entry. When working with cache through console, user
agent type (the `CHUNK(NUMBER)` passed to to the server, usually the result
of analyzing `user_agent` field of the request passed to the PHP method) is
the one specified using `USERAGENT` console command. Return value of the
PHP method is consistent with that of `SessionHandlerInterface::write()`.

Console takes `lefitime` field from the value set by `LIFETIME` auxiliary
command: `0` means "infinite", `-1` means "use default" (calculated using the
algorithm described below), and any other positive integer value is called
"specific lifetime" and is used as-is.

If lifetime of `-1` is send to the server as part of the command, then during
first `session_first_write_num` writes to a session record, the lifetime of the
record is set to `session_first_write_lifetime` plus the difference between
`session_default_lifetime` and `session_first_write_lifetime` divided by
`session_first_write_num`, times number number of previous writes; that is, on
first write it is set to `session_first_write_lifetime`, and then with each
subsequent write it increases until it reaches `session_default_lifetime`
during write number `session_first_write_num+1`, and then platoes.

> **IMPORTANT**: if very first character of <session-data> argument is `@`, then 
> the rest of it is treated as name of the file that will be loaded, compressed, 
> and sent to the server as payload. If very first character of literal session 
> data entered using console has to be `@`, it has to be put into a file. If
> session data specified as a string happens to have zero bytes, it would
> get truncated at first zero byte upon sending; neither data loaded from file,
> nor PHP API have this restriction, so if it is necessary to send data with
> zero bytes in it using console, the data should be put into a file first.

> If session data is specified as a zero-length string (''), or an empty file, 
> the command will remove respective session record.

When sending data to the server from console, session entry data should of
course be entered in uncompressed form; the console will compress it before 
sending to the server using compressor specified with `COMPRESSOR` command.

  Console command(s):

    [ USER ]
    [ USERAGENT [ <agent-type> ]]
    [ LIFETIME [ <duration> ]]
    [ COMPRESSOR <compressor> ]
    [ MARKER <boolean> ]
    WRITE <entry-id> [@]<session-data>

  PHP extension method (user agent is taked from resource):

    bool c3_write($resource, string $entry_id, int $lifetime, string $entry_data)
  
  Request sequence (first number is user agent, second is lifetime):

    DESCRIPTOR HEADER {
      0x22 [ PASSWORD ] [ PAYLOAD_INFO ] CHUNK(STRING) CHUNK(NUMBER)
        CHUNK(NUMBER) [ CHUNK(NUMBER) ] } [ PAYLOAD ] [ MARKER ]

  Binlog / replication:

    DESCRIPTOR HEADER {
      0x22 [ PASSWORD ] [ PAYLOAD_INFO ] CHUNK(STRING) CHUNK(NUMBER)
        CHUNK(NUMBER) [ CHUNK(NUMBER) ] } [ PAYLOAD ] [ MARKER ]

  Server response:

    - OK [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    user_password <password-string>
    session_first_write_lifetime <duration> [ <duration> [...]]
    session_first_write_num <duration> [ <duration> [...]]
    session_default_lifetime <duration> [ <duration> [...]]

### `DESTROY` ###

Deletes session cache entry. Return value of the PHP method is consistent
with that of `SessionHandlerInterface::destroy()`; `false` is returned only if 
there was an error; if the record is not found, return value is still `true`.

  Console command(s):

    [ USER ]
    [ MARKER <boolean> ]
    DESTROY <entry-id>

  PHP extension method:

    bool c3_destroy($resource, string $entry_id)
  
  Request sequence:

    DESCRIPTOR HEADER { 0x23 [ PASSWORD ] CHUNK(STRING) } [ MARKER ]

  Binlog / replication:

    DESCRIPTOR HEADER { 0x23 [ PASSWORD ] CHUNK(STRING) } [ MARKER ]

  Server response:

    - OK [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    user_password <password-string>

### `GC` ###

Delete session cache entries that had not been updated during specified number
of seconds. Return value of the PHP method is consistent with that of
`SessionHandlerInterface::gc()`. If `session_eviction_mode` is set to
`strict-lru`, this request will be ignored; the server will just return `Ok`.

  Console command(s):

    [ USER ]
    [ MARKER <boolean> ]
    GC <duration>

  PHP extension method:

    bool c3_gc($resource, int $number_of_seconds)

  Request sequence:

    DESCRIPTOR HEADER { 0x24 [ PASSWORD ] CHUNK(NUMBER) } [ MARKER ]

  Binlog / replication (these commands are written to binlog as garbage collection progresses):

    [ DESCRIPTOR HEADER { 0x23 [ PASSWORD ] CHUNK(STRING) } [ MARKER ]
      [ DESCRIPTOR HEADER { 0x23 [ PASSWORD ] CHUNK(STRING) } [ MARKER ] [ ... ]]]

  Server response:

    - OK [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    user_password <password-string>
    session_eviction_mode { strict-expiration-lru | expiration-lru | lru | strict-lru }

Full Page Cache Commands
------------------------

In order to execure any command in this category, it is necessary to enter user
password (using `USER` auxiliary command) first -- unless user password is empty
in the server configuration file.

CyberCache has to support methods found in

1. `Zend_Cache_Backend` class,
2. `Zend_Cache_Backend_Interface` interface and
3. `Zend_Cache_Backend_ExtendedInterface`.

The first of the three does not require any special support; in the
Redis-based implementation of the FPC, only few methods are overwritten,
and even they either do not require interactions with cache store, or do
something that is already implemented in the parent class; like, say,
`isAutomaticCleaningAvailable()` returning `true`. So the real job is to
support methods found in `Zend_Cache_Backend_Interface` and
`Zend_Cache_Backend_ExtendedInterface` interfaces.

### `LOAD` ###

Fetches FPC entry. Console prints out *uncompressed* fetched data as text,
replacing non-printable characters with dots.

When working with cache through console, bot type (the `CHUNK(NUMBER)` passed to
to the server, usually the result of analyzing `user_agent` field of the request
passed to the PHP method) is the one specified using `USERAGENT` console command
(`user` by default).

> An attempt to fetch a non-existent FPC entry does *not* result in an error:
> in such a case, server returns `Ok` response, `load()` returns `false`
> (which is consistent with `Zend_Cache_Backend_Interface::load()`
> calling convention), and `getErrorMessage()` returns an empty string.

If specified FPC record exists and not marked as "deleted", then if it had been
expired, it will be "revived" by setting its expiration timestamp to current
time plus `fpc_read_extra_lifetime[<user-agent>]`, and returned by the server.
If `fpc_eviction_mode` is *not* `expiration-lru` or `strict-expiration-lru`,
then record's expiration time will be adjusted as if it was expired (but only if
`fpc_read_extra_lifetime` is bigger than remaining lifetime).

  Console command(s):

    [ USER ]
    [ USERAGENT [ <agent-type> ]]
    [ MARKER <boolean> ]
    LOAD <entry-id>

  PHP extension method (user agent is taked from resource):

    string c3_load($resource, string $entry_id)
  
  Request sequence:

    DESCRIPTOR HEADER { 0x41 [ PASSWORD ] CHUNK(STRING) CHUNK(NUMBER) } [ MARKER ]

  Binlog / replication:

    N/A

  Server response:

    - OK [ MARKER ]
    - DATA HEADER { PAYLOAD_INFO } PAYLOAD [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    user_password <password-string>
    fpc_read_extra_lifetime <duration> [ <duration> [...]]
    fpc_eviction_mode { strict-expiration-lru | expiration-lru | lru | strict-lru }

### `TEST` ###

Checks if specified FPC entry exists, returns `Ok` (not error message) if it
doesn't. PHP method `test()` returns `false` is the record does not exist, or
last modification timestamp if it does (which is consistent with
`Zend_Cache_Backend_Interface::test()` calling convention).

When working with cache through console, bot type (the `CHUNK(NUMBER)` passed to
to the server, usually the result of analyzing `user_agent` field of the request
passed to the PHP method) is the one specified using `USERAGENT` console command
(`user`by default).

If specified FPC record exists and not marked as "deleted", then if it had been
expired, it will be "revived" by setting its expiration timestamp to current
time plus `fpc_read_extra_lifetime[<user-agent>]`, and returned by the server.
If `fpc_eviction_mode` is *not* `expiration-lru` or `strict-expiration-lru`,
then record's expiration time will be adjusted as if it was expired (but only if
`fpc_read_extra_lifetime` is bigger than remaining lifetime).

  Console command(s):

    [ USER ]
    [ USERAGENT [ <agent-type> ]]
    [ MARKER <boolean> ]
    TEST <entry-id>

  PHP extension method (user agent is taked from resource):

    int c3_test($resource, string $entry_id)

  Request sequence:

    DESCRIPTOR HEADER { 0x42 [ PASSWORD ] CHUNK(STRING) CHUNK(NUMBER) } [ MARKER ]
  
  Binlog / replication:

    N/A

  Server response:

    - OK [ MARKER ]
    - DATA HEADER { CHUNK(NUMBER) } [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    user_password <password-string>
    fpc_read_extra_lifetime <duration> [ <duration> [...]]
    fpc_eviction_mode { strict-expiration-lru | expiration-lru | lru | strict-lru }

### `SAVE` ###

Stores data chunk into specified FPC entry. When working with cache through
console:

- bot type (the first `CHUNK(NUMBER)` passed to to the server, usually the
result of analyzing `user_agent` field of the request passed to the PHP method)
is the one specified using `USERAGENT` console command (`user` by default),

- tags (the `CHUNK(LIST)` passed to the server) are the ones entered using
`TAGS` console command; empty list by default,

- lefitime (the last `CHUNK(NUMBER)` passed to the server) is the one previously
specified using `LIFETIME` console command; default is `-1`, meaning the
lifetime specified for `fpc_default_lifetime` in the configuration file; zero
lifetime means "infinite" and is implemented by setting expiration time for the 
record to the maximum possible timestamp.

PHP method `save()` returns `true` on success or `false` on error (which is
consistent with `Zend_Cache_Backend_Interface::save()` calling convention).

> **IMPORTANT**: if very first character of <entry-data> argument is `@`, then 
> the rest of it is treated as name of the file that will be loaded, compressed, 
> and sent to the server as payload. If very first character of literal FPC 
> entry data entered using console has to be `@`, it has to be put into a file.
> If session data specified as a string happens to have zero bytes, it would get
> truncated at first zero byte upon sending; neither data loaded from file, nor
> PHP API have this restriction, so if it is necessary to send data with zero
> bytes in it using console, the data should be put into a file first.

> If FPC data is specified as a zero-length string (''), or an empty file, the
> command will remove respective FPC record.

When sending data to the server from console, FPC entry data should of course be
entered in uncompressed form; the console will compress it before sending to the
server using compressor specified with `COMPRESSOR` command.

  Console command(s):

    [ USER ]
    [ USERAGENT [ <agent-type> ]]
    [ LIFETIME [ <duration> ]]
    [ TAGS [ <tag> [ <tag> [[ ... ]]]]]
    [ ADDTAGS  <tag> [ <tag> [[ ... ]]]]
    [ REMOVETAGS  <tag> [ <tag> [[ ... ]]]]
    [ COMPRESSOR <compressor> ]
    [ MARKER <boolean> ]
    SAVE <entry-id> [@]<entry-data>

  PHP extension method (user agent is taked from resource):

    bool c3_save($resource, string $entry_id, int $lifetime,
      array $tags, string $entry_data)

  Request sequence (first number is user agent, second is lifetime):

    DESCRIPTOR HEADER 0x43 {
      [ PASSWORD ] [ PAYLOAD_INFO ] CHUNK(STRING) CHUNK(NUMBER) CHUNK(NUMBER) CHUNK(LIST)
      } [ PAYLOAD ] [ MARKER ]
  
  Binlog / replication:

    DESCRIPTOR HEADER 0x43 {
      [ PASSWORD ] [ PAYLOAD_INFO ] CHUNK(STRING) CHUNK(NUMBER) CHUNK(NUMBER) CHUNK(LIST)
      } [ PAYLOAD ] [ MARKER ]

  Server response:

    - OK [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    user_password <password-string>
    fpc_default_lifetime <duration> [ <duration> [...]]
    fpc_max_lifetime <duration> [ <duration> [...]]
    perf_num_internal_tags_per_page <num-tags>

### `REMOVE` ###

Deletes specified FPC entry. PHP method `remove()` returns `true` on success or
`false` on error (which is consistent with
`Zend_Cache_Backend_Interface::remove()` calling convention).

  Console command(s):

    [ USER ]
    [ MARKER <boolean> ]
    REMOVE <entry-id>

  PHP extension method:

    bool c3_remove($resource, string $entry_id)

  Request sequence:

    DESCRIPTOR HEADER { 0x44 [ PASSWORD ] CHUNK(STRING) } [ MARKER ]
  
  Binlog / replication:

    DESCRIPTOR HEADER { 0x44 [ PASSWORD ] CHUNK(STRING) } [ MARKER ]

  Server response:

    - OK [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    user_password <password-string>

### `CLEAN` ###

Deletes specified FPC entries.

PHP method `clean()` receives one of the following strings as mode:

- `all` : remove all cache entries,
- `old` : remove old cache entries (do garbage collection),
- `matchingTag` : remove entries that match all the tags passed as second argument,
- `notMatchingTag` : remove entries that do not match any of the tags passed as second argument,
- `matchingAnyTag` : remove entries that match at least one of the tags passed as second argument.

If `fpc_eviction_mode` is set to `lru-strict`, `CLEAN` request with `old` mode
will do nothing, the server will simply return `true`.

In `all` and `old` modes, PHP method does not accept tags, even as an empty
array; the argument in these modes must be either `NULL`, or completely omitted. 
In other modes, a `NULL` or omitted list is treated as an empty one.

The `clean()` method returns `true` on success or `false` on error (which
is consistent with `Zend_Cache_Backend_Interface::clean()` calling
convention).

  Console command(s):

    [ USER ]
    [ MARKER <boolean> ]
    CLEAN { all | old | matchall | matchnot | matchany } [ <tag> [ <tag> [ ... ]]]

  PHP extension method:

    bool c3_clean($resource, string $mode, array $tags = null)

  Request sequence:

    DESCRIPTOR HEADER { 0x45 [ PASSWORD ] CHUNK(NUMBER) [ CHUNK(LIST) ] } [ MARKER ]
  
  Binlog / replication (individual commands are written to binlog as garbage collection progresses):

    [ DESCRIPTOR HEADER { 0x44 [ PASSWORD ] CHUNK(STRING) } [ MARKER ]
      [ DESCRIPTOR HEADER { 0x44 [ PASSWORD ] CHUNK(STRING) } [ MARKER ] [ ... ]]]

  Server response:

    - OK [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    user_password <password-string>
    fpc_eviction_mode { strict-expiration-lru | expiration-lru | lru | strict-lru }

### `GETIDS` ###

Returns list of IDs of all existing FPC entries.

  Console command(s):

    [ USER ]
    [ MARKER <boolean> ]
    GETIDS

  PHP extension method (returns empty array on errors):

    array c3_get_ids($resource)

  Request sequence:

    DESCRIPTOR 0x61 [ PASSWORD ] [ MARKER ]
  
  Binlog / replication:

    N/A

  Server response:

    - LIST HEADER { [ PAYLOAD_INFO ] CHUNK(NUMBER) } [ PAYLOAD ] [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    user_password <password-string>

### `GETTAGS` ###

Returns list of all tags that have at least one FPC record associated with them.

  Console command(s):

    [ USER ]
    [ MARKER <boolean> ]
    GETTAGS

  PHP extension method (returns empty array on errors):

    array c3_get_tags($resource)

  Request sequence:

    DESCRIPTOR 0x62 [ PASSWORD ] [ MARKER ]
  
  Binlog / replication:

    N/A

  Server response:

    - LIST HEADER { [ PAYLOAD_INFO ] CHUNK(NUMBER) } [ PAYLOAD ] [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    user_password <password-string>

### `GETIDSMATCHINGTAGS` ###

Returns list of record IDs that are tagged with *all* specified tags.
Callers can pass a string to the PHP method instead of an array with a
single element.

> *NOTE*: Redis-base implementation performs intersection of all tags passed to 
> this method when retrieving IDs; the `matchingTag` mode of the `CLEAN` method 
> in Redis-based implementation calls `GETIDSMATCHINGTAGS` and then deletes 
> cache entries with returned IDs.

If empty array is passed, PHP method immediately returns empty array without
calling the server.

  Console command(s):

    [ USER ]
    [ MARKER <boolean> ]
    GETIDSMATCHINGTAGS [ <tag> [ <tag> [ ... ]]]

  PHP extension method (returns empty array on errors):

    array c3_get_ids_matching_tags($resource, $tags)

  Request sequence:

    DESCRIPTOR HEADER { 0x63 [ PASSWORD ] CHUNK(LIST) } [ MARKER ]
  
  Binlog / replication:

    N/A

  Server response:

    - LIST HEADER { [ PAYLOAD_INFO ] CHUNK(NUMBER) } [ PAYLOAD ] [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    user_password <password-string>

### `GETIDSNOTMATCHINGTAGS` ###

Returns list of record IDs that are *not* tagged with *all* specified tags. That
is, if a record is not tagged at all, of if it is only tagged with some tags but
*not* with any of the specified ones, it *will* be returned. Callers can pass a
string to the PHP method instead of an array with a single element.

> *NOTE*: Redis-base implementation performs difference between the list of all
> object IDs and all tags passed to this method; the `notMatchingTag` mode of
> the `CLEAN` method in Redis-based implementation calls `GETIDSNOTMATCHINGTAGS`
> and then deletes cache entries with returned IDs.

If empty array is passed, PHP method calls server with an empty list, but server
processes it as if it was a `GETIDS` request (returning *all* active entries).

  Console command(s):

    [ USER ]
    [ MARKER <boolean> ]
    GETIDSNOTMATCHINGTAGS [ <tag> [ <tag> [ ... ]]]

  PHP extension method (returns empty array on errors):

    array c3_get_ids_not_matching_tags($resource, $tags)
  
  Request sequence:

    DESCRIPTOR HEADER { 0x64 [ PASSWORD ] CHUNK(LIST) } [ MARKER ]
  
  Binlog / replication:

    N/A

  Server response:

    - LIST HEADER { [ PAYLOAD_INFO ] CHUNK(NUMBER) } [ PAYLOAD ] [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    user_password <password-string>

### `GETIDSMATCHINGANYTAGS` ###

Returns list of record IDs that are tagged with *any* of the specified
tags. That is, if a record is tagged with at least one of the specified
tags, it *will* be returned. Callers can pass a string to the PHP method
instead of an array with a single element.

> *NOTE*: Redis-base implementation performs union between all tags passed to
> this method; the `matchingAnyTag` mode of the `CLEAN` method in Redis-based
> implementation calls `GETIDSMATCHINGANYTAGS` and then deletes cache entries
> with returned IDs.

If empty array is passed, PHP method immediately returns an empty list without
calling the server.

  Console command(s):

    [ USER ]
    [ MARKER <boolean> ]
    GETIDSMATCHINGANYTAGS [ <tag> [ <tag> [ ... ]]]

  PHP extension method (returns empty array on errors):

    array c3_get_ids_matching_any_tags($resource, $tags)
  
  Request sequence:

    DESCRIPTOR HEADER { 0x65 [ PASSWORD ] CHUNK(LIST) } [ MARKER ]

  Binlog / replication:

    N/A

  Server response:

    - LIST HEADER { [ PAYLOAD_INFO ] CHUNK(NUMBER) } [ PAYLOAD ] [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    user_password <password-string>

### `GETFILLINGPERCENTAGE` ###

Returns filling percentage of the FPC store, as an integer number between 0 and
100. If memory quota is not set for the FPC domain (or an error is returned),
reported percentage is 0.

  Console command(s):

    [ USER ]
    [ MARKER <boolean> ]
    GETFILLINGPERCENTAGE

  PHP extension method:

    int c3_get_filling_percentage($resource)

  Request sequence:

    DESCRIPTOR 0x67 [ PASSWORD ] [ MARKER ]

  Binlog / replication:

    N/A

  Server response:

    - DATA HEADER { CHUNK(NUMBER) } [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    user_password <password-string>

### `GETMETADATAS` ###

Returns metadata associated with an FPC entry. If the entry does not exist,
server returns `Ok` and PHP method returns `false`. If the entry does exist, and
is associated with `N` tags, then server response contains expiration time and
last modification time (numbers of seconds since January 1st 1970), and a list
with `N` strings in its header. The PHP method returns associative array with
`expire`, `mtime`, and `tags` entries, where `tags` element is a regular,
non-associative array of strings.

If an FPC record had been expired but had not been deleted yet, it will be
"revived" by setting its expiration timestamp to current time plus
`fpc_read_extra_lifetime[<user-agent>]`, and returned by the server.

  Console command(s):

    [ USER ]
    [ MARKER <boolean> ]
    GETMETADATAS <entry-id>

  PHP extension method:

    array c3_get_metadatas($resource, string $entry_id)

  Request sequence:

    DESCRIPTOR HEADER { 0x68 [ PASSWORD ] CHUNK(STRING) } [ MARKER ]

  Binlog / replication:

    N/A

  Server response:

    - OK [ MARKER ]
    - DATA HEADER { CHUNK(NUMBER) CHUNK(NUMBER) CHUNK(LIST) } [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    user_password <password-string>
    fpc_read_extra_lifetime <duration> [ <duration> [...]]
    fpc_eviction_mode { strict-expiration-lru | expiration-lru | lru | strict-lru }

### `TOUCH` ###

Adds extra lifetime to specified entry. If the entry does not exist, the server
will return error message, and PHP method will return `false`. If resulting
lifitime exceeds that set with the `fpc_max_lifetime` option, lifetime will be
set to current plus `fpc_max_lifetime`. If the entry does exist and setting new
expiration time was successful, then server returns `Ok`, and PHP method will 
return `true`.

> *NOTE*: if record's lifetime was set to `infinite` by `SAVE` command, it will 
> not be affected by `TOUCH`; the server will still return `Ok`.

The server first calculates remaining TTL by subtracting current time from the
expiration time that is in effect as of the time of the request; if the
remaining TTL is negative (i.e. the record has expired), then it is set to zero.
New expiration time is then set to current time plus remaining TTL plus
specified extra lifetime.

  Console command(s):

    [ USER ]
    [ MARKER <boolean> ]
    TOUCH <entry-id> <extra-lifetime>

  PHP extension method:

    bool c3_touch($resource, string $entry_id, int $extra_lifetime)

  Request sequence:

    DESCRIPTOR HEADER { 0x69 [ PASSWORD ] CHUNK(STRING) CHUNK(NUMBER) } [ MARKER ]
  
  Binlog / replication:

    DESCRIPTOR HEADER { 0x69 [ PASSWORD ] CHUNK(STRING) CHUNK(NUMBER) } [ MARKER ]

  Server response:

    - OK [ MARKER ]
    - ERROR HEADER { CHUNK(STRING) } [ MARKER ]

  Configuration options:

    user_password <password-string>
    fpc_max_lifetime <duration> [ <duration> [...]]
