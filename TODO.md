
TODO List
=========

Release Plan
------------

- `[ ]` 1.5 - Maintenance release
- `[ ]` 1.4 - Optimized version
- `[.]` 1.3 - Production version (public release)
- `[x]` 1.2 - Instrumented version
- `[x]` 1.1 - Linux version
- `[x]` 1.0 - PHP client
- `[x]` 0.9 - Server console
- `[x]` 0.8 - Server daemon, object pipelines, complete config file
- `[x]` 0.7 - Hash objects, hash tables, optimizer
- `[x]` 0.6 - Configuration manager, parsing module, hash library
- `[x]` 0.5 - Memory management overhaul
- `[x]` 0.4 - Major fixes, stability improvements, syslog
- `[x]` 0.3 - I/O pipelines

Legend
------

Below, planned items are prefixed with hyphens, while completed items are prefixed with asterisks.

**NOTE**: markdown viewers do not distinguish between hyphens and asterisks; they are all "list items" for
them. In order to see which items are completed, you'll have to view this file as plain text.

1.3 - Production version
------------------------

* promote version to `1.3.0-1`
* remove debug code from Magento extension; remove debugging description from the accompanying documentation
* create `config/cybercached.mdcfg` input file for documentation/configuration generation
* implement `c3mcp` Markdown/Configuration Preprocessor utility:
  * usage: `c3mcp <options>`
  * splits input file by `[<name>: <value>]` blocks and processes blocks separately; block types:
      * `[DOCHEADER: <title>]`: outputs level 1 title and writes what follows to markdown file only
      * `[SECTION: <title>]`: outputs level 2 title to markdown; what follows is written as comment to config, as-is to markdown
      * `[FORMAT]`: outputs level 3 "Option format" title to markdown, the rest as list to markdown and as comment to config
      * `[DEFAULTS]`: outputs level 3 "Default values" title to markdown, the rest as list to markdown and as comment to config
      * `[DOCSECTION: <title>]`: outputs level 2 title and writes what follows to markdown file only
      * `[CONFIG]`: writes what follows to config file only, as-is
      * `[SEPARATOR]`: outputs line of hyphens (commented out in config)
  * if no output file is specified, writes to standard output
  * if input file is specified as `-`, reads from standard input
  * strips trailing spaces off all input lines
  * recognizes block quotes (insets) by leading `>` within `[SECTION]` blocks; outputs them as indented comments to config file
  * lines that consist entirely of hyphens are ignored (`[SEPARATOR]` should be used if that's what needed)
  * suppress output of more than one empty line in a row
* make `generate_md_cfg` preprocess input files using `c3p` and produce Community/Enterprise versions of markdown/configuration files
* modify package assembling procedure:
  * generate configuration from `cybercached.mdcfg`, pipe `c3p` output into it
  * generate markdown from `cybercached.mdcfg`, pipe it into `pandoc` to produce `cybercached(1)`
* use different sets of compiler options and defines for different editions:
  * CE: `-DC3_FAST -O2 -s`, 
  * EE: `-DC3_FASTEST -O3 -s`, 
  * EE instrumented: `-DC3_SAFEST -g -rdynamic -funwind-tables`
* compile external libraries with `-DNDEBUG -O3` (use `PRIVATE` scope):
  * compression
  * hashes
* promote version to `1.3.1`
* convert `SocketEventProcessor::sp_socket_events` from `Vector` to the array of size `MAX_IPS_PER_SERVICE`
* make code more strict using new CLion 2017.2 inspections:
  * strip `const` off fields already marked as `constexpr`
  * replace `NULL` with `nullptr`
  * mark virtual methods in derived classes with `override` (and scrap `virtual`)
  * mark single-argument ctors with `explicit`
  * replace implicit casts to `bool` with comparison expressions in various conditions (except in `assert()`s)
  * mark move ctors with `noexcept`
  * mark move assignment operators with `noexcept`
  * mark `constexpr` functions and methods `noexcept`
  * use `auto` for intializations from casts or template invocations
  * change `<type>*` arguments to `const <type>*` whenever appropriate and not done yet
* optimize code:
  * review the use of assertions
      * replace guard `if`s (with `[c3_]assert_failure()` in `else` branches) with `[c3_]assert()`s
      * use `assert()` for parameter checks, `c3_assert()` for invariants' checks
  * turn virtual functions containing only `[c3_]assert_failure()` into pure virtual in base classes, e.g.:
      * `Optimizer::on_gc()`, `Optimizer::on_message()`, etc.
      * `Allocator::alloc()`, `Allocator::free()`
  * improve synchronization objects:
      * in `C3_FAST` mode, make thread guards' ctors and methods bodies empty or returning constants
      * review pointer checks in thread sync objects' guards
      * provide separate overloads for timed and untimed mutex/queue methods OR inline them
  * check usage of returns values; make often-used methods with unused return values `void`; e.g.:
      * `SharedBuffers::configure_header()`
      * `PayloadHashObject::set_buffer()`
      * `SharedObjectBuffers::transfer_payload()`
      * `SharedBuffers::attach_payload()`
      * `ReaderWriter::command_reader_transfer_payload()`
      * `ReaderWriter::command_writer_set_ready_state()`
      * `ReaderWriter::response_writer_attach_payload()`
      * `ReaderWriter::response_writer_set_ready_state()`
      * `ObjectStore::init_object_store()`
      * `PayloadObjectStore::init_payload_object_store()`
      * `TagStore::allocate_tag_store()`
      * `TagStore::allocate()`
      * `PageObjectStore::allocate()`
      * `SessionObjectStore::allocate()`
      * `Thread::start()`
      * `Thread::request_stop()`
      * `Thread::wait_stop()`
      * `Thread::get_state()`
      * `Thread::set_state()`
      * `CompressorLibrary::initialize()`
      * `CommandWriter::io_rewind()`
      * `HeaderListChunkBuilder::configure()`
      * `HeaderListChunkBuilder::check()`
      * `PayloadChunkBuilder::add(const c3_byte_t*, usize, comp_data_t)`
      * `PayloadChunkBuilder::add(const PayloadListChunkBuilder&)`
      * `PayloadChunkBuilder::add(Payload*)`
      * `PayloadChunkBuilder::add()`
      * `HeaderChunkBuilder::configure()`
      * `HeaderChunkBuilder::check()`
      * `HeaderChunkBuilder::add_number()`
      * `HeaderChunkBuilder::add_string()`
      * `HeaderChunkBuilder::add_cstring()`
      * `HeaderChunkBuilder::add_list()`
  * remove unused code:
      * remove unused classes (e.g. `AdminCommandHeaderChunkBuilder`, `UserCommandHeaderChunkBuilder`)
      * remove unused messages (e.g. `xxx_FORCE_QUIT`), their handlers, and helper methods
      * remove unused methods in the `c3lib` library
      * remove unused methods in the PHP extension
      * remove unused methods in the console application
      * remove extraneous range etc. checks of configuration values passed through messages (they are now being
        checked in configuration parser); retain some checks as `c3_assert()`s
      * optimize `LockableObjectGuard` ctor (set `qmg_locked` unconditionally in non-`C3LM_ENABLED` modes)
      * optimize mutex locks ctors (set `mlb_locked` unconditionally in non-`C3LM_ENABLED` modes)
      * remove unused methods in the server daemon
  * improve queue management:
      * use direct access to queue capacities for builds w/o lock tracking
      * eliminate use of mutexes in capacity getters
      * make methods returning queue capacities (of various objects) 'const'
  * make `Thread::t_id` accessible using inline method
  * make use of function attributes defined in `c3_build.h`:
      * `C3_FUNC_COLD`: is a "cold spot", unlikely to be called; optimized for size
      * `C3_FUNC_HOT`: is a "hot spot", very likely to be called; optimized [more] aggressively for speed
      * `C3_FUNC_PURE`: pure function whose result depends on arguments only; may reference global memory
      * `C3_FUNC_CONST`: pure function whose result depends on arguments only; can NOT reference global memory
* outstanding improvements:
  * attribute `c3_set_error_message()` with `C3_FUNC_PRINTF(1)`
  * remove `Hasher::hash(const char*)`
  * fix autodoc for `Parser::enumerate()`
  * optimize `read_bytes()`/`write_bytes()` methods of device handlers
  * optimize `errno` usage
  * make inclusion of `c3_send()` and `c3_receive()` conditional
  * fix console test for Community Edition
* compile instrumented version with `-Og`
* add number of recompressions during last run to the output of the `INFO` command (for each optimizer, separately)
* improve compression support (first-order optimizations):
  * make `snappy` default compressor for Magento and PHP extensions, and for the console
  * add `static_assert()`s to compression methods translating `c3lib` compression levels into those of compressors' implementations
  * make sure console and extension use `CL_FASTEST` compression level
* promote version to `1.3.2`
* miscellaneous improvements:
  * use `static_assert()` to make sure `SocketResponseWriter` and `SocketCommandReader` are of the same size
  * refactor `SocketEventProcessor` class:
      * make methods whose returns values are not used `void`
      * do all checks on old object before `replace_watched_object()` call and eliminate first argument
  * refactor `PipelineEvent::get_type()` methods
  * optimize processing of `PEF_ERROR` flags (do it only once) and add comment on `PEF_HUP` processing:
      * in `SocketInputPipeline::process_object_event()` method
      * in `SocketOutputPipeline::process_object_event()` method
  * fix `CyberCache::emulate()`: write repeatedly on `IO_RESULT_RETRY`, just like PHP extension does
  * refactor `c3_sockets` module (extract send/receive methods into separate module; adjust `epoll` test utility)
  * add 'Testing' section to the version upgrade / build instructions
* promote version to `1.3.3`
* modify socket I/O pipelines to optionally work on persistent connections
  * add `PET_CONNECTION` event type
  * implement `PipelineConnectionEvent` class; pad it to be of `SocketCommandReader` size (use `static_assert()` to check)
      * `Memory& pce_memory` field
      * `int pce_socket` field to keep accepted connection
      * `c3_ipv4_t pce_ipv4` field to keep accepted connection
      * accessor methods
      * `static SocketCommandReader* convert(PipelineConnectionEvent*)` method
      * `static PipelineConnectionEvent* convert(SocketResponseWriter*)` method
  * expand `PipelineEvent` class documentation to make its purpose and mechanics clearer
  * add `PipelineConnectionEvent* pe_connection` field to the union of the `pipeline_event_t` structure
      * modify `SocketEventProcessor::get_next_event()` to fill the new field for `PET_CONNECTION` events
  * implement `SocketEventProcessor::replace_watched_object(PipelineConnectionEvent*)` method
  * modify `SocketPipeline` class
      * add `bool sp_persistent` field
      * implement public `bool is_using_persistent_connections() const` method
      * add `SIC_PERSISTENT_CONNECTIONS_ON` and `SIC_PERSISTENT_CONNECTIONS_OFF` elements to the `socket_input_command_t` enum
      * modify `SocketPipeline::process_queue_event()` to handle connection type change events
      * implement public `send_set_persistent_connections_command(bool)` method
      * add `virtual void process_connection_event(const pipeline_event_t &event) = 0` method
      * add `virtual void process_persistent_connections_change(bool persistent) = 0` method
      * modify `thread_proc()` to handle `PET_CONNECTION` events
  * modify server input pipeline to handle persistent connections
      * implement `listener_persistent <boolean>` option
      * implement `process_connection_event()` method that handles read, hangup, and error events on connections
      * implement `process_persistent_connections_change()` method (just change respective flag)
      * make `SocketInputPipeline::process_input_queue_object()` continue watch objects (connections) on persistent connections
      * make `SocketInputPipeline::process_object_event()` continue watch objects (connections) on persistent connections
  * modify server output pipeline to handle persistent connections
      * implement `session_replicator_persistent <boolean>` option
      * implement `fpc_replicator_persistent <boolean>` option
      * cache descriptors of open replication sockets in `SocketOutputPipeline` if connections are persistent
      * re-use and don't close socket handles in `SocketOutputPipeline::process_input_queue_object()` on persistent connections
      * dispose objects but do not close sockets in `SocketOutputPipeline::process_object_event()` on persistent connections
      * process connections dropped right after reading in `SocketOutputPipeline::process_object_event()`
      * implement `process_persistent_connections_change()` method (reset cached connection handles)
      * complete implementation of IP/port change handlers in socket output pipelines
  * fix watching connections (use `replace_watched_object()` when it *replaces* the object)
  * fix issues with outbound connections (peer may hang up):
      * silently re-create socket on first failure in `SocketOutputPipeline::process_input_queue_object()` method
      * add `Replicator_Reconnections` performance counter
  * fix handling of "overlapping" outgoing commands in output pipelines:
      * implement `Queue` class (unsynchronized queue) in `mt_queue` module
      * add `Internal_Queue_Max_Capacity`, `Internal_Queue_Reallocations`, and `Internal_Queue_Put_Failures` performance counters
      * introduce queue of `ReaderWriter*` elements in `SocketOutputPipeline` class; track and limit its size
      * add `SIC_LOCAL_QUEUE_CAPACITY_CHANGE` and `SIC_LOCAL_QUEUE_MAX_CAPACITY_CHANGE` messages and their handlers
      * implement `perf_session_replicator_local_queue_capacity`, `perf_session_replicator_local_max_queue_capacity`,
        `perf_fpc_replicator_local_queue_capacity`, and `perf_fpc_replicator_local_max_queue_capacity` server options
      * if `SocketOutputPipeline::process_input_queue_object()` receives an object when `sp_num_connections` is nonzero, queue it
      * when `sp_num_connections` becomes zero in `SocketOutputPipeline::process_object_event()`, check object queue and, if not empty,
        get an object from queue and call `SocketOutputPipeline::process_input_queue_object()` with it
      * add `Replicator_Deferred_Commands` and `Replicator_Max_Deferred_Commands` performance counters
      * add new `perf_` options to configuration files
  * move `ntotal` into `else` branch in both `process_object_event()` methods' implementations
  * minor refactoring:
      * implement and use (for checks, instead of `get_fd()`) `PipelineConnectionEvent::is_valid()`
      * replace `InputSocketMessage::dispose(rw)` calls with `ReaderWriter::dispose(rw)`
  * mark all server configuration parser's methods as "cold"
  * implement `PersistentSocket` class
      * `int ps_fd` field
      * `c3_ipv4_t ps_address` field
      * `c3_ushort_t ps_port` field
      * `c3_byte_t ps_options` field to hold `C3_SOCK_xxx` flags
      * `bool ps_persistent` field, do not close the socket if it's `true`
      * ctor taking `blocking` and `binding` boolean arguments and calling `init()`
      * dtor calling `disconnect(true)`
      * accessor methods (getters)
      * `bool connect(c3_ipv4_t address, c3_ushort_t port, bool persistent)` method
      * `bool reconnect()` method
      * `void disconnect(bool always)` method
  * implement `SocketGuard` class:
      * ctor takes reference to `PersistentSocket`
      * dtor calls `disconnect(false)` on referenced object
  * modify console application
      * add `PersistentSocket cc_socket` field to the `CyberCache` class
      * add `bool cc_persistent` field and accessor methods to the `CyberCache` class
      * implement and document `PERSISTENT <bool>` auxiliary command
      * in `CyberCache::emulate()`: replace `Socket socket` with `cc_socket`; define `SocketGuard(cc_socket)` right after successful
        connection; in writing loop, if *first* iteration returns `IO_RESULT_EOF` or `IO_RESULT_ERROR`, call `cc_socket.reconnect()`
  * modify PHP extension
      * introduce `thread_local PersistentSocket c3_request_socket`
      * on request cleanup, call `disconnect(true)`
      * implement `c3.session_persistent` and `c3.fpc_persistent` options and their handlers
      * implement `persistent` key in the option array passed to `c3_session()` and `c3_fpc()` functions
      * in `call_c3()`: replace `Socket socket` with `c3_socket`; define `SocketGuard(c3_socket)` right after successful connection;
        in writing loop, if *first* iteration returns `IO_RESULT_EOF` or `IO_RESULT_ERROR`, call `cc_socket.reconnect()`
  * transfer fixes and improvements from the "temporary" version:
      * make PHP extension use synchronous I/O mode; make sure console uses it as well
      * `c3_socket()` should always set `TCP_NODELAY` mode
      * implement `c3_begin/end_data_block()` methods in `c3lib/c3_sockets_io` module
      * use `c3_begin/end_data_block()` methods in `io_response_handlers.cc` module
      * use `c3_begin/end_data_block()` methods in `io_command_handlers.cc` module
      * fix log messages in `pl_socket_pipelines.cc` and `pl_socket_events.cc` modules
      * modify `AbstractLogger::log()` to write down milliseconds elapsed since last call if `LOG_MILLISECONDS_SINCE_LAST_CALL` is set
  * fix `-w` option of `cybercachewarmer` (either make argument optional, or document it properly)
  * remove `Socket` class; rename `PersistentSocket` to `Socket`
  * add `C3_FUNC_COLD` to each server option handler
  * configure package components to use persistent connections by default:
      * server daemon (add `xxx_persistent` options to `config/cybercached.mdcfg`)
      * console application
      * PHP extension
      * Magento extension
  * add tests for `xxx_persistent` server options
  * update documentation (stress importance of keeping client/server settings in sync):
      * console manual (`doc/command_interface.md`)
      * Magento extension settings (`src/magento/readme.md`)
      * reference documentation (`config/cybercached.mdcfg`)
  * final fixes and improvements for the `1.3.3` version
      * create RPM installation package
      * update copyright notices (at least add "-2019")
      * make sure all scripts have copyright notices
      * add Magento 1 extension to the project, document it
      * add Magento 1 extension files to the DEB packaging script
      * update DEB changelogs and re-package CyberCache
* promote version to `1.3.4`
* update version promotion instructions
* fix life time handling in Magento 1 extension
* implement `session_auto_save_interval <duration>` and `fpc_auto_save_interval <duration>` config options
  * extract session/FPC saving code into `Server::save_session/fpc_store()` methods
  * implement `session/fpc_auto_save_interval` options setting respective `Server` class fields; implement getters
  * introduce `SC_SAVE_SESSION_STORE` and `SC_SAVE_FPC_STORE` members of the `server_command_t` enum (new ID commands)
  * add processing of the new commands to the `Server::process_id_command()` method
  * add `c3_timestamp_t o_last_session/fpc_save` fields to the `Optimizer` class
  * in `Optimizer::run()`, check time elapsed since saves, and send `SC_SAVE_xxx_STORE` commands if needed
  * document `xxx_auto_save_interval` options, along with alternative use of `console -c` with cron
* increase optimizer run intervals to 20 seconds
* improve logging:
  * do not log "peer hung up" when using persistent connections
  * create binlog and log files readable by everyone
* compile PHP extension for v.7.1, API `20160303` (check PHP 7.2+ API versions)
* promote version to `1.3.5-1`
* remove Gygwin vestiges (`.bat` and `.cmd` files etc.)
* make final documentation changes and additions:
  * make sure all directories have `readme.md` files in them
    * fix `tests/readme.md` (does not mention `dev-switch-edition` that has to be run at least once)
  * create `LICENSE.md` file in the project root; include links to compressors' licenses; put `GPL_v2.txt` there
  * include `LICENSE.md` and `GPL_v2.txt` files into binary distribution (`opt/cybercache/license` directory)
      * copy hashers/compressors/regex licenses into `opt/cybercache/license/<lib>` directory
  * replace "CyberHULL, Ltd." with "CyberHULL."; add GPL v2 banner after "All rights reserved" in all:
      * source code files,
      * application banners,
      * scripts,
      * DEB and RPM meta-data,
      * documentation files (where applicable)
  * create `README.md` file in the project root

1.4 - Optimized version
-----------------------

- implement `memory` libraries, compile-time selection of memory manager
  - add per-thread alloc/realloc/free performance counters (pool all connections threads into "rest")
  - use `jemalloc` by default; add it as dependency to `.deb`s
- improve compression support (second-order optimizations):
  - make sure compressors use optimum config (e.g. that `BUILDFIXED` is not defined in zlib)
  - implement per-compressor re-compression thresholds:
      - add `recompression_thresholds <compressor> <lower-threshold> [ <upper-threshold> ]` option
      - remove `session_recompression_threshold` and `fpc_recompression_threshold` options
      - make use of thresholds when selecting compressors during optimization runs
  - add `hesper` compression engine to the Enterprise edition of the server, console, and PHP extension
  - improve information methods of the compression library:
      - re-work `CompressorLibrary::is_supported()` so that it's based on compile-time, not run-time settings
      - `CompressorLibrary::get_name()` should return names for unsupported (excluded) codecs
  - revise the use of compressor data "hints"
  - implement specialized compressors for small ASCII texts (EE-only; see http://www.asciitable.com):
    - include `hesper` algorithm support (`src/utils/hesper`)
    - implement `asciient` algorithm ("ASCII ENTropy", pronounced "ascient one"): fixed-code Huffman trained on small [Magento] texts
        - `asciient <max-size> <file-name-regex> <directory>` utility building Huffman tables from `CMD_DUMP` output
        - enable it in Enterprize Edition
- optimize compilation and libraries:
  - enable `-ffunction-sections` and global optimization; use profile-guided optimization
      - see https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
  - reduce number of exported symbols (e.g. with `-Wl,--retain-symbols-file=file`)
  - configure `farmhash` hardware optimizations using `cmake` or GCC attributes:
      - use GCC `__attribute__((target_clones (<options>)))`
      - see https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
      - see https://gcc.gnu.org/onlinedocs/gcc-6.2.0/gcc/Common-Function-Attributes.html#Common-Function-Attributes
- improve compression support (third-order optimizations):
  - extract `c3_compressor` into a separate library and change library compilation modes:
      - compile compression library with `-fPIC`
      - compile `c3lib` without `-fPIC`
  - make use of the `CL_EXTREME` compression level in Enterprise edition:
      - turn `ho_xflags` field into `ho_type`; use it instead of `HOF_TYPE_BITS` bits
      - introduce `HOF_COMP_OBSOLETE` flag, set it upon set of optimization compressors or custom dictionary changes (create method)
      - introduce `HOF_COMP_EXTREME` flag, set it if "extreme" compression level was used
      - implement `extreme_compression_period <from-time> <till-time>` server option (if times match: disabled)
      - server optimizers should use `CL_EXTREME` compression level during that period, `CL_DEFAULT` otherwise
  - implement support for custom dictionaries for `zstd` and `brotli` compressors in Enterprise edition:
      - `CMD_DUMP` (`0xFE`) command
      - `CMD_DUMP` support in server (instrumented version, EE-only; saves sidecar files with compressor/size data)
      - `dump <domain> <directory-path>` console command
      - `c3_dump()` PHP extension function
      - `cybercache-dict <compressor> <directory-path> <file-path>` command-line utility
      - `virtual CompressorEngine::set_dictionary(const char* path)` method
      - `DICTIONARY <compressor> <file-path>` console command
      - `compressor_dictionary <compressor> <file-path>` server option
      - `c3.zstd_dict` and `c3.brotli_dict` PHP extension INI options (at `PHP_INI_SYSTEM` level)
  - make `lz4` compressor work with 4G buffers (?)
- re-use memory insted of freeing/allocating it:
  - when `SocketResponseWriter` is created while `SocketCommandReader` is disposed
  - when `SocketResponseReader` is created while `SocketCommandWriter` is disposed (in output pipeline)
- consider the use of `shared_ptr` for payloads in `SharedBuffers`
  - eliminate cloning of 'CommandReader' objects in `PageObjectStore::process_save_command()` method
  - remove `clone()` methods from I/O reader/writer classes

1.5 - Maintenance version
-------------------------

- update third-party libraries to newest versions
- re-work libraries:
  - put compression and hash libraries into separate `.so` module
  - use system `gzip` library instead of own in Linux builds; remove respective `lintian` check block
  - exclude "brotli" compressor from Community Edition builds
- fix DEB packages files/directories modes issues detected by Lintian
- improve installation package by putting stuff into more befitting directories:
  - move PHP extension API stubs into `/usr/share/cybercache`
  - combine scripts from `/opt/cybercache/bin` into `/usr/bin/cybercache-ctl` (with command arguments)
- create Magento extension installation script in `/opt/cybercache/bin`
  - call it from `/usr/bin/cybercache-ctl` on `install-magento-extension` switch
- create "light" uncompression-only shared libs for all codecs except `lzf` and `snappy`
  - link "light" versions console and PHP client against them
  - include "light" versions of console and PHP client into Enterprise edition packages
- split `cybercache` package into:
  - `cybercache-libs`: compression and hash libraries (depends on `libjemalloc1`)
  - `cybercache-server`: server, console, configuration and test files (depends on `cybercache-libs`)
  - `cybercache-php7.0`: extension for PHP 7.0, plus respective test files (depends on `cybercache-libs`)
  - `cybercache-php7.1`: extension for PHP 7.1, plus respective test files (depends on `cybercache-libs`)
  - `cybercache-magento`: Magento 2 extension
  - `cybercache-all`: meta-package that depends on all of the above
- outstanding fixes:
  - fix logger seemingly spending more than 60s in "active" mode (broken stats?)
  - fix memory allocation issues in console application:
      - clean up history entries
      - clean up tag array entries
- review record lifetime handling:
  - honor `doNotTestCacheValidity` argument to `save()` passed by Magento (?)
  - simplify handling of session lifetimes
- define `__attribute__ ((__packed__))` as `C3_PACKED` and make respective substitutions (e.g. in `TagObject` class)
- improve signal handling:
  - add `TSTP` to the list of signals masked in all threads but main
  - treat `HUP` as re-initialisation rather than termination request in the *production* environment
- improve current thread ID handling:
  - introduce `c3_thread_id_t` typedef
  - turn static `local_thread_id` into field in `Thread` class (resolve linking issues)
  - make `Thread::get_id()` method inline
- compile complete set of documentation using info already present in `doc/*.md` files:
  - `scripts/c3mcp.md`
  - white paper
  - server documentation (`cybercached(1)`, compiled from current `cybercached.cfg`)
  - console documentation (`cybercache(1)`, compiled from `doc/command_interface.md`)
  - PHP client documentation
  - document differences between Community and Enterprise editions
  - document version number decoding in `protocol.md`
  - document binlog/database format in `protocol.md`
  - protocol documentation (`cybercached(5)`, compiled from `doc/protocol.md`)
  - use `c3p` to separate community- and enterprise-specific documentation
  - put together `editions.md` documenting differences using feature list
  - make sure documentation produces valid `man` pages; see [this tutorial](http://jbl.web.cern.ch/jbl/doc/manpages/)
  - generate `html` versions of all installed `.md` files (installation script); re-use style file created for old PDFs
- create library API documentation for source distribution:
  - make same-line C++ comments doxygen-style (`//` ==> `//<`)
  - generate HTML documentation
- implement additional warmer/profiler switches:
  - `-g:<user>,<password>`/`--login=<user>,<password>`: authenticate user
- implement stand-alone binlog optimizer utility and documentation for it
- implement colored output in the console application
