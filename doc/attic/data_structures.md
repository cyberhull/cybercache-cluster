
Internal Data Structures And Their Use
======================================

All data structures and algorithms for manipulating them are disigned with the
following goals in mind:

1. Keep the *number* of memory allocations to the minimum.

2. Allow highly concurrent access, including locking just particular data 
   elements; clearly distinguish read and write (exclusive) access; use
   asynchronous I/O and queues instead of locking whenever possible (a-la Google
   Go channels); use memory fences instead of locking for data shared between 
   threads whenever appropriate.

3. No data duplication, always favour interlinking to reference sets.

4. Structures must be carefully designed to always have 64-bit values 
   (including pointers) aligned on 64-bit boundaries, 32-bit values on 32-bit 
   boundaries (including pointers in 32-bit builds), and so on; the goal here is
   *not* to ensure maximum portability (target platform is almost guaranteed to
   have an Intel x86-family CPU-based), but to ensure maximum speed (e.g. hash
   libraries have compile-time constants enabling certain speed-ups if the data
   is lnown to be aligned).

5. Speed is a priority; whenever speed can be gained at the expense of a [small]
   extra memory allocation, it should be done.

6. Maximum reliability is a must, the system must be fully recoverable after 
   failure of any particular operation (only the result of the failed operation
   can be lost, but it must not affect other data and/or operations). Whenever
   appropriate, threads must track their operations and especially the objects
   that they lock -- both to make it possible to do extra checks, and for other
   threads to be able to find out and fix deadlock should that ever occur.

Primary namespace is `CyberCache`.

Auxiliary Classes
-----------------

These classes do not represent server data structures, but rather serve as
helpers, having a set of useful (usually static) methods. They "know" nothing
about `Config` class, or any other part of the server infrastructure.

### `Timer` ###

Convenience class with static methods allowing to get current UNIX-epoch
timestamp (number of seconds since Jan 1, 1970), current time in microseconds or
milliseconds, parse or pretty-print timestamps, calculate time intervals between
timestamps, etc. in a portable manner.

Uses chrono::high_resolution_clock internally.

### `Parser` ###
  
Parses text comprised by various "statements", with those statemet's values
being potentially quoted or having suffixes denoting durations or sizes.

Have methods for parsing text buffers and files, as well as utility methods 
extracting generic integer and floating point values, dirations, sizes etc.

### `Hasher` ###

Calculates 64-bit hash code of a string using specified algorithm. Supports the
following hashing methods:

1. `murmurhash2` : the one used by Redis,
2. `murmurhash3` : successor to `murmurhash2` (has better distribution),
3. `cityhash` : [currently excluded due to `farmhash`] one of the fastest, by Google; requires new-gen CPUs,
4. `spookyhash` : almost as fast as Google's, and without `cityhash`'s limitations,
5. `farmhash` : successor to `cityhash` from Google, [see
   source](https://github.com/google/farmhash); compiled *with* all hardware-specific optimizations
   turned on, so should only be turned on if the administrator is sure that hardware allows that (it
   cant's be default for this reason),
6. `xxhash` : very fast hash from the creator of `lz4` and `zstd` compressors; see [source
   code](https://github.com/Cyan4973/xxHash); this is the default.

All hashers are built as libraries against which the application links.

### `Compressor` ###

Compressed or decompresses a buffer using specified algorithm. The following
compression methods are supported(see [this article for the summary of
compression methods](https://en.wikibooks.org/wiki/Data_Compression/Dictionary_compression),
as well as [this page for the performance analysis of big-data
compressors](http://mattmahoney.net/dc/text.html)):

1. `snappy` : Google compressor used by them in `BigTable`, `MapReduce`; the default; also
   used by Redis; compresses 3+x faster than `lz4`; see [source code](https://github.com/google/snappy),
2. `lzf` : another compressor used by Redis; compresses 2+x faster than `lz4`; see
   [distributive](http://dist.schmorp.de/liblzf/),
3. `lz4` : very fast compressor, used by many, see [source code](https://github.com/Cyan4973/lz4),
4. `gzip`: zlib library, see [library web site](http://www.zlib.net/),
5. `lzham`: based on LZMA (compressor used by 7Zip), by a Valve engineer, made to be used in games; has
   appr. 3x decompression speed with only slightly worse compression ratio compared to `LZMA`, and
   almost 2x decompression speed and more than 1.5x compression ratio compared to `gzip`; see [source
   code](https://github.com/richgel999/lzham_codec),
6. `brotli`: Google compressor with primary target being web traffic; has built-in preset
   dictionary; see [source code](https://github.com/google/brotli),
7. `zstd`: compressor from the creator of `lz4` compressor and `xxhash`; compresses better than `gzip`,
   decompression speed it 3x that of `gzip`, it is one of the fastest decompressors; has "learning mode"
   for building custom dictionaries greatly facilitating compressing small files; see [source
   code](https://github.com/Cyan4973/zstd).

All compressors are built as libraries against which the application links.

### `Memory` ###

Memory allocator with the ability to track the amount of used memory; has the 
following methods:

- `malloc(size_t size)` : Wrapper around memory allocator's equivalent of static
  `malloc()` function; adds `size` to the total used memory size for the object.

- `calloc(size_t size)` : Wrapper around memory allocator's equivalent of static
  `calloc()` function; adds `size` to the total used memory size for the object.

- `realloc(void* p, size_t old_size, size_t new_size)` : Wrapper around memory
  allocator's equivalent of static `realloc()` function; adds difference between
  `new_size` and `old_size` to the total used memory size for the object.

- `free(void* p, size_t size)` : Wrapper around memory allocator's equivalent of
  static `free()` function; subtracts `size` from the total used memory size for
  the object.

- `static get_block_size(void* p)` : Uses `malloc_usable_size()` (that would be
  `malloc_size()` for OS X, or `_msize()` for Windows) to fetch actual memory
  size of the block; actual size is bigger than or equal to requested size; 
  presence of this method in the interface, as well as actual implementation, 
  is controlled by macros, which call sites should check and, if it's not 
  present, make do without it.

- `inplace_realloc(void* p, size_t old_size, size_t new_size)` : Tries to fit 
  the object into already allocated memory block; if `new_size` is smaller than 
  `old_size`, it checks if the difference is less than compile-time constant of 
  memory size that can be "wasted" during such reallocations: if it is, it 
  subtracts the difference from total memory size for the object and returns 
  `p`, otherwise it returns `null`; if `new_size` is bigger than `old_size`, it 
  checks compile-time constant of whether the use of `get_block_size()` is 
  allowed and, if so, calls that method to see it its returned value bigger or 
  equal to the `new_size`; if it is, the difference is added to the total 
  memory size for the object, and `p` is returned.

- `get_used_memory_size()` : Fetches total amount of memory currently being 
  used by this `Memory` object.

- `static heap_check()` : If used memory library provides global heap check 
  function, then this method runs it, logs found errors if any, and returns a 
  boolean indicating whether the check succeeded.

`Memory` object can be configured at compile time to use one of the following allocators ([see
details here](http://blog.reverberate.org/2009/02/one-malloc-to-rule-them-all.html),
[here (Wikipedia)](https://en.wikipedia.org/wiki/C_dynamic_memory_allocation), and
[here (PerconaDB)](https://www.percona.com/blog/2012/07/05/impact-of-memory-allocators-on-mysql-performance/):

1. `default` : default allocator of the host operating system,
2. `jemalloc` : one of the allocators used by Redis, FreeBSD, and Facebook ([see
   description](http://www.canonware.com/jemalloc/)); the default for CyberCache,
3. `tcmalloc` : Google's TCMalloc, another allocator used by Redis ([see 
   description](http://goog-perftools.sourceforge.net/doc/tcmalloc.html)),
4. `nedmalloc` : fast, scallable multithreaded allocator; its previous version
   (PTMalloc) is used by GNU `libc` ([see
   description](http://www.nedprod.com/programs/portable/nedmalloc/index.html)).

`tcmalloc-2.2` and `jemalloc-3.0` seem to work the best for PerconaDB under 
high loads (>= 1.5k connections).

### `PerfCounter` ###

Performance counter is a simple structure that consists of the following fields:

- `int64       pc_value` : value of the counter,
- `const char* pc_name` : name of the counter (as output by `STATS` console command).

Performance counters are kept in arrays; both individual counters and their
arrays are manipulated using macros (only defined if the server is built in
*instrumented* mode) that can:

- define a counter (`pc_name` is set to a stringized name of the counter, *after* that name is prefixed),
- define a counter with initial `pc_value`,
- define a counter with initial `pc_value` equal to max possible (for counters tracking mimimum values),
- get name of the counter variable (by adding prefix to specified name),
- increment a counter by one,
- add a value (such a memory pool size, or string length) to a counter,
- set counter to `min()` of its current value and specified value,
- set counter to `max()` of its current value and specified value,
- define a local variable and set it to current time in microseconds,
- add difference between current time and value of a variable to a counter,
- divide value of one counter by value of another counter and store result in third counter,
- and so on.

Synchronization Classes
-----------------------

These do not hold "persistent" cache data themselves, but rather control access 
to such data, as well as serve as temporary containers (synchronization objects 
are sometimes built into container primitives, or even used as base classes).

### `SyncObject` ###

Base class for all synchronization objects *except* `SpinLock`; used for
pointers/references that can reference derived synchronization objects.

Has the following fields (its constructor accepts constants to fill them):

- `uint8 so_domain` : scope (global / session / fpc),
- `uint8 so_object` : host object (server / domain / config / log / binlog / replicator / optimizer),
- `uint8 so_type` : synchronization object type (message queue / shared mutex / shared downgradable mutex),
- `uint8 so_id` : unique ID (in case there are several synchronization objects of the same type per host).

### `Mutex` ###

Base class for all mutexes; used for pointers/references that can reference
derived mutexes; has no methods except constructor.

### `SharedMutex` ###

A thin wrapper around C++ 11 `shared_timed_mutex`, allowing many readers *or* 
one writer at any time.

### `DynamicMutex` ###

Works very much like `SharedMutex`, but also allows to downgrade write lock to
read lock in atomic manner. Used when a thread has done all potentially
destructive work on a data structure and needs to proceed with futher
modifications that can be done concurrently with other threads' read operations.

### `SpinLock` ###

This object has static methods only, and is used to get exclusive access to a
`PayloadHashObject` using that object's `ho_lock` field. Has `lock()`,
`unlock()`, and `try_lock()` methods.

### `MessageQueue` ###

Template class with synchronized access. Has methods to push a new element to
the back of the queue (this would also `notify()` internal semaphore), pop an
element from the head (would `wait()` on said semaphore until there's a new
message to process), or try popping an element from the head without locking;
all with `move` samantics. The internal semaphore is not a "complete" one:
instead, `push()` and `pop()` methods of the queue combine data manipulations
with locking, reusing the same mutex that would have been used by a semaphore
solely to access its counter and the `condition_variable`.

Works very much like `chan` in Go language, but somewhat less efficiently.

### Helper Classes ###

For mutexes, there are convenience classes allowing to automatically lock/unlock
them, as well as downgrade the lock (if applicable).

Server Classes
--------------

Core data structures are of fixed size, and are statically allocated (their 
objects are thus created at application startup). Most dynamic elements are of 
variable size, with their own `new` operators.

Statically allocated objects do not rely on the execution order of their ctors,
but instead have `init()` methods called by server startup code.

### `Config` ###

Loads configuration files, holds various cache parameters; it is the very first
object to initialize, as even `Logger` object depends on it; statically allocated.

Uses `Parser` to interpret configuration options; the `Parsers` does *not* use 
any extra memory allocations, and employs binary search to quickly find options' 
handlers (this is more efficient than using standard C++ ordered map container).

Implemented as a base class in the library; derived classes (`ServerConfig`, 
`ConsoleConfig`, `ClientConfig`) have bery different implementations, and only 
expose very limited set of settings through virtual methods.

Maintains `initialized` flag (set to `true` after the very first configuration
file had been read during startup phase, or if no configuration file was given,
but startup phase has nonetheless ended, in which case it would show/log a
warning) and own set of logging methods. If `initialized` is `true`, then
`user_password`, `admin_password`, `bulk_password`, and `hash_method`
configuration variables can no longer be changed.

Configuration options with names starting with `perf_` are clearly marked in
configuration file comments as *you should really, really know what you are
doing* ones.

### `LogMessage` ###

A representation of constant-length string stored in `MessageQueue` of the
`Logger` object; has the following fields:

- `uint16 lm_length` : Length of the message,
- `char   lm_message` : The message itself.

### `Logger` ###

Handles distributed access to server log using `MessageQueue` object; statically
allocated. Supported logging levels are (from terse to verbose):

- `INVALID` : logger is in shutdown mode; this level can only be set internally by the logger itself,
- `EXPLICIT` : the server is explicitly told to log something (e.g. with a `LOG` console command),
- `FATAL` : a message about a fatal error (e.g. memory corruption),
- `ERROR` : a message about an error (e.g. dropped connection),
- `WARNING` : a warning message (e.g. non-enforceable setting in a config file being loaded),
- `TERSE` : a status change message (e.g. a component had been initialized successfully),
- `NORMAL` : a regular message (e.g. a config option changed),
- `VERBOSE` : a system information message (e.g. a queue or hash table capacity increased),
- `DEBUG` : debugging message (e.g. another connection established).

An `EXPLICIT` message is guaranteed to be logged (unless the logger is in
shutdown mode already).

`Logger` class has separate methods for sending commands (quit request, log
level change, log file path change) and logging as such; the method logging
messages accepts log level, format string, and variable list of arguments; it
checks current log level and, if logging of requested level is allowed, uses
`vsnprintf()` to do actual printing to on-stack buffer, then calls private
method to create and add to the queue a `LogMessage` object, passing it the
buffer and its used length returned by `vsnprintf()`.

### `Thread` ###

A wrapper around a C++ 11 thread. Upon creation, each thread is given a unique
ID/index into global array of references to `Thread` objects (static field holds
total number of threads), which (the ID) it stores into a `thread_local`-storage
variable, so

1. Any thread can get its object data at any point,
2. Any other thread can check other threads' data by scanning the array.

The array of `Thread` objects is of the size `MAX_CONNECTION_THREADS+9`, where
`10` is the number of non-connection threads (which go first in the array, with
very first slot occupied by the `main()` thread). The `Thread` object has the
following fields:

- `uint64       t_start_time` : Start of a job (receiving a connection, a buffer, etc.; usecs),
- `std::thread  t_thread` : C++ 11 object representing the thread (for `join()`s),
- `Mutex*       t_mutex_ref` : Pointer to a mutex object the thread has locked, or is waiting on,
- `HashObject*  t_object_ref` : Pointer to a data object the thread has locked, or is waiting on,
- `SyncObject*  t_queue_ref` : Pointer to a queue object the thread has locked, or is waiting on,
- `uint8        t_state` : Thread status (unused / active / requested to quit / quitting).
- `uint8        t_mutex_state` : How mutex object is being used (unused /
                read wait / write wait / read lock / write lock),
- `uint8        t_object_state` : How data object is being used (unused / waiting / locked),
- `uint8        t_queue_state` : How queue object is being used (unused /
                waiting to push / pushing / waiting to pop / trying to pop / popping),

The `t_state` field is used by the `ConnectionThread::set_num_threads()` method
controlling the number of connection theads; if new value is smaller than
current number of connection threads, for each "extra" thread the `set()` method
puts quit request into the field and `join()`s the thread on `*t_thread` (then
setting capacity of its `TagVector` object to zero to free up some memory); each
connection thead checks that field when it is about to get new message from the
queue and, if it sees the request to quit (or if "quit" message is received from
the queue), it quits; when `join()` returns in `set()` method, it sets `t_state`
field of that thread to "unused".

Synchronization objects "know" about `Thread` class, and manipulate its fields
in order to do their checks (e.g. is the thread that is trying to unlock a mutex
the one that locked it?), as well as provide data for advanced recovery (e.g. if
the thread crashes with an exception, the `catch` block in that thread's main
loop would examine synchronization objects locked by the thread and release
them; or if a `SpinLock` cannot get hold of a `PayloadHashObject` for many
seconds, it can request a check to see who's doing what with the object, and
whether respective thread hung somewhere).

A `Thread` is only allowed to lock (or wait on) *up to* one mutex (shared *or*
downgradable), one spinlock, and one queue -- **only** in this order! Some
lock(s) in that sequence can be "skipped" though; therfore:

- if a mutex is being locked, there must be no active spin lock or a message 
  queue lock,
- if a spin lock is being acquired, there must be no active message queue lock,
- if a mutex, a spin lock, or a message queue is being locked, there must not 
  be another active lock on a synchronization object of the *same* type.

This is reflected in the structure of the tracking fields of the `Thread`
object, and instrumented version of the server does these checks in
synchronization objects' locking methods (along with checks in unlock /
downgrade methods: that the thread indeed had acquired respective lock). The
deepest possible stack of synchronization objects uses/locks by a thread might
look like this:

- read lock on `Domain`s `DynamicMutex` object,
- exclusive `SpinLock` on `PayloadHashObject` in that `Domain` (needed even if
  `Domain` was locked for writing: to play nice with optimization thread),
- push lock on `Logger`s, *or* `Optimizer`'s, *or* `Binlog`s message queue, or a 
  queue of deleted objects of a domain.

The following `Thread` subclasses exist:

- `ConnectionThread` : waits for jobs on `MessageQueue` of the `Server` object; the number of
   these objects is configurable, and can be changed at run time,

- `LogThread` : waits on `MessageQueue` of `Logger` object for data to log (write to log file),

- `ListenerThread` : waits on `epoll` object waiting for incoming connections, creates and reads command
  objects, and posts them to its output queue for connection threads to process,

- `BinlogThread` : there are two of these, one per domain; each waits on `MessageQueue` of `Binlog` object
  of its domain for data to log; after the `BinlogBuffer` object received from the queue (and, possibly,
  the buffer of the `PayloadHashObject` referred to by `BinlogBuffer`) is dumped to the log file, the
  `BinlogBuffer` object is put into `MessageQueue` of the `Replicator` object of respective domain,

- `ReplicationThread` : there are two of these as well, one per domain; each waits on `MessageQueue` of
  `Replicator` object of its domain for data to send to the replication server; after the buffer of the
  `PayloadHashObject` referred to by `BinlogBuffer` is sent to the replication server, the `pho_nreaders`
  field is decremented in `PayloadHashObject`; when it becomes zero, it allows buffer manipilations like
  freeing upon expiration, recomression etc.; the reason for the introduction of a separate thread for
  replication is to allow `BinlogThread` of the domain to do its job as quickly as possible, not wasting
  time on pumping data to replication servers: it is important to maintain binlog as current as possible,

- `BinlogLoaderThread` : a thread that is used for restoration from binlogs; its itput queue only accepts
  commands; this thread does not have own output queue, but instead accepts reference to the output queue
  as an argument; when it receives "load-binlog-file" command, it opens that file and starts loading
  command objects and pumping them to the output queue (while doing that, it does not accept any commands,
  but it does check thread termination requests, so there is no need to synchronize it with others: it
  won't open other binlog files until it full processes its current one, or until thread termination
  request arrives),
  
- `BinlogSaverThread` : a thread that is used for saving cache databases; its input queue accepts both
  commands and objects (just like queues of replication threads), while its output queue is rudimentary
  and is only used by binlog saver to notify about completion of database save operation,

- `TagManagerThread` : a thread that processes FPC commands that have to do with tags (connection threads
  forward their commands to this tag manager's thread as soon as they "understand" that the command is
  *mostly* about managing tags; also, when an FPC `SAVE` command is processed by a connection thread, that
  thread only creates (or finds existing) payload hash object, and then forwards both found/created object
  and command object to the tag manager; this happens even if FPC record is not tagged: the tag manager
  still has to link the payload hash object into the chain of "untagged" objects,

- `OptimizationThread` : is a template base class; it is instantiated and extended separately for
  session and FPC objects; it waits on `pop()` from its message queue, does the job, then checks the
  time to next optimization run *or* if it is already within an optimization run; the thread has three
  configuration options controlling the runs:

      - `perf_session_optrun_wait` and `perf_fpc_optrun_wait` : how often do them (time, in seconds,
      between the runs; used as timeout for message queue `get()`),

      - `perf_session_optrun_num_checks` and `perf_fpc_optrun_num_checks` : how many objects it can
      check for expiration and/or unoptimal compression; each of this options receives five values in the
      configuration file: for when number of busy connection threads is zero, less than 33% of CPU cores,
      less than 66% of CPU cores, less than 100% of CPU cores, or matches or exceeds number of CPU cores,

      - `perf_session_optrun_num_recomps` and `perf_fpc_optrun_num_recomps` : how many objects it can
      re-compress (using compressors from the `session_compression_methods` and `fpc_compression_methods`
      lists) in one go; just like with the options setting numbers of checks, these options take five
      values, for different CPU loads,

  when it breaks the run due to exceeding any of those configurable parameters, it remembers the next
  object it was about to process, so that after next `pop()` it would know where to restart; immediately
  before processing the next `pop()`ed object, it checks if that's the object it paused its previous
  run on, and makes adjustments if necessary; it owns `pho_opt_prev`, `pho_opt_next`, and
  `pho_opt_useragent` fields in the `PayloadHashObject`. This thread always checks if the record is
  locked, but only tries to lock it itself with `SpinLock::try_lock()`, it never waits on already locked
  records; likewise, it always skips records that are being read (having non-zero `pho_nreaders`, without
  even trying to lock them. If it successfully locks the record and finds out the record has to be
  re-compressed, it fetches buffer and its sizes, sets "being-optimized" flag, unlocks the record and
  tries to re-compress it; if recompression succeeds (i.e. gain is bigger than specified by
  `perf_session_optrun_min_recomp_gain` or `perf_fpc_optrun_min_recomp_gain` options), the thread tries
  to lock the record again; if locks does not succeed, or if the thread finds that that the record is
  "deleted", or being read (having non-zero `pho_nreaders`), or that "being-optimized" flag had been
  cleared, it discards compression buffers and continues its run.

- `SignalHandler` thread: waits for signals from the system, and handles them; a signal can be, say,
  an `SIGINT` if the application is told to quit, or a `SIGILL` if the application has performed an
  illegal instruction (which might be the case if server compiled for, say, `AVX` instruction set to
  speed up its hash functions was run on hardware that does not support it). The handler takes necessary
  actions and, if necessary, posts "quit" message to the main thread and quits itself. The advantages of
  blocking signals in all threads except specific, separate thread handling signals are numerous,
  including the fact that I/O operations won't be interrupted and signal-related recovery code is not
  needed in their implementations.

Additionally, there's also main application thread that waits on configuration
`MessageQueue` for configuration requests, or a shutdown request.

### `Connection` ###

An object that is created by the main application thread (if the server is not 
in shutdown mode) and inserted into `MessageQueue` of the `Server` object. 
`BinlogBuffer` is *not* created at this point; it is only created by the thread 
that has received a `Connection` object and is processing the connection request.

### `Server` ###

Maintains all TCP/IP communications, uses `MessageQueue` of `Connection` objects
to schedule jobs to process connection requests; statically allocated.

### `BinlogBuffer` ###

Dinamically-resizable buffer acting very much like a `StringBuffer` in many 
implementations; used to receive data from TCP/IP port and pass them over to 
the binlog. Has the following fields:

- `PayloadHashObject* bb_object` : Object with `pho_buffer` containing last chunk of data,
- `uint8*              bb_buff` : Data buffer; very first byte is server command,
- `uint32              bb_capacity` : Data buffer capacity (max size),
- `uint32              bb_size` : Current size of data in `bb_buff`.

The `bb_object` is only set if the command being processed (first byte in
`bb_buff`) is `WRITE` or `SAVE`.

### `Binlog` ###

Object that maintains `MessageQueue` of `BinlogBuffer` objects to be dumped to
the binlog of its domain.

Whenever new data is received from TCP/IP port, a `ConnectionThread` receives
`Connection` object through `Server`s mesage queue; the thread then creates a
new `BinlogBuffer` object and fills it with contents of the request (read using
`Connection` object) up to, but not including the final `COMPRESSED_CHUNK` if
there is one (see Command Interface description). Then `PayloadHashObject` is
created, its `pho_buffer` is filled with compressed data from
`COMPRESSED_CHUNK`, and pointer to that object is put into `bb_object`.
`BinlogBuffer` is then put into the `MessageQueue` of the `Binlog` of respective
domain to be dumped to the binlog file (and, possible, to be sent to replication
server), and the queue reader thread is automatically notified.

Upon triggered the `MessageQueue`'s semaphore, `BinlogThread` wakes up, pulls
`BinlogBuffer` from the queue, dumps its contents to the binlog file, and (if
`bb_object` is referenced) than dumps `COMPRESSED_CHUNK` using data from
`pho_buffer` (but building VLQs and such on the fly). Synchronization mode of 
the binlog file is controlled by configuration variable; see MySQL/InnoDB 
documentation for [similar
modes](https://dev.mysql.com/doc/refman/5.5/en/innodb-parameters.html#sysvar_innodb_flush_method).

The `BinlogBuffer` object is then put into the message queue of the 
`Replicator` object of the same domain.

### `Replicator` ###

Object that maintains `MessageQueue` of `BinlogBuffer` objects to be sent to a 
replication server; there is one such object per domain.

If replication is active, the contents of `BinlogBuffer` received from
`Replicator`s message queue is sent to the replication server.

Finally, if `BinlogBuffer` references `PayloadHashObject`, then `pho_nreaders`
field of that object is decremented, potentially making it posible to physically
delete that object, recompress its data in `pho_buffer`, or otherwise change it.

### `OptimizerMessage` ###

The object that is posted to the message queue of `Optimizer` objects. Has the 
following fields:

- `PayloadHashObject*  om_object` : (union field) Pointer to the object to be modified,
- `OptimizationConfig* om_config` : (union field) Pointer to the object containing optimizer configuration,
- `uint8               om_request` : Request type (add / update / delete an object / GC / config change),
- `uint8               om_type` : Type of the passed object (none/PHO/config),
- `uint8               om_useragent` : User agent that was in effect at the time of the request,
- `uint8               om_comp` : Compression type for "add" requests,
- `uint32              om_time` : a timestamp or time period (duration).

Session and FPC optimizers process garbage collection requests differently:

- They both reset optimization runs' saved positions if they were inside 
  optimization runs at the time of receiving GC request; contrary to regular 
  optimization runs, GC run is always done in one go *and* no re-compression 
  attempts are made during it.

- Session optimization thread treats `om_time` as the number of seconds of
  inactivity: if session data was not *modified* during that many seconds, it
  should be deleted. During gagbage collection runs, optimization thread does
  *not* lock objects (using `SpinLock`) before checking: instead, it checks the
  thread then checks (again, without locking) if the object had already been
  deleted; if it hadn't, only then the thread locks the object, checks (with
  memory barrier) that "deleted" flag is still clear, and proceeds with
  deletion. During GC runs on session domain, object expiration timestamp is
  *not* checked.

- FPC optimization request does not have arguments; the thread just compares
  `pho_exp_time` field of every object against current time to see which ones
  expired; if an object did expire, the thread checks if the object had already
  been deleted (all without locking the object); if if hadn't been deleted yet,
  only then the thread locks it, re-checks "deleted" flag (with memory barrier)
  and, if it's still clear, proceeds with deletion.

### `Optimizer` ###

Has static methods for recompressing buffers of `PayloadHashObject`s using 
compression algorithms specified in the configuration file, and `Compressor` 
class methods; there is one `Optimizer` object per domain.

It is very likely that the final set of algorythms would be `snappy` as initial
compressor, and `zstd` as default re-compressor, with two custom-built internal
dictionaries (one for sessions, one for FPC) -- `PHP` extension will have to
have `snappy` compressor and decompressor, and `zstd` decompressor with two
built-in dictionaries. "Advanced" version of `CyberCache` should include the 
ability of "training" `zstd` with actual session and FPC data.

The `Optimizer` object maintains internal `<number-of-user-agents>` lists of
objects, each list being an object containing number of entries in the list
(lists are double-linked ones, not vectors: removal should have `O(1)`, not
`O(n/2)` complexity); for each list there are separate configuration options
(`perf_session_optrun_retain` and `perf_fpc_optrun_retain`) disabling removal of
last `N` entries (set *on per-user-agent basis*), which cannot be less than `1`
(so that, for instance, a session of currently running bot won't be removed);
the higher the user agent index, the bigger that `N` for it should be.

When "add" `OptimizerMessage` arrives, user agent in the object pointed to by
`om_object` should be zero (uninitialized), and is simply set by the
optimization thread to the `om_useragent`. When "update" `OptimizerMessage`
arrives, if `om_useragent` is *bigger* than what's in the object pointed to by
`om_object`, then the object has to be migrated from its current optimization
list to one specified by `om_useragent`: this might happen *only* in FPC domain,
if say a cache entry was created by a bot or cache warmer, but is now being
accessed by a real user; updates also always move affected records to the top of
their lists.

Optimizer breaks its scheduled *optimization* run when:

- it processed all the objects it manages (this might be the case when 
  `perf_xxx_optrun_num_checks` is actually bigger than total number of object in 
  the domain), OR

- it checked `perf_xxx_optrun_num_checks` objects during optimization pass (the 
  number of object checked during memory recollection pass does not count: the 
  optimizer will only break memory recollection pass when amount of used memory 
  will become smaller than memory quota for that domain; *NOTE*: this applies to 
  scheduled optimization runs only; during explicitly requested garbage 
  collection run, optimizer will check *all* objects), OR

- it performs `perf_xxx_optrun_num_recomps` re-compression attempts (if `N` 
  compressors are specified in `xxx_compression_methods`, then actual number of 
  compression attempts will be `perf_xxx_optrun_num_recomps` times `N`), OR

- it detects that a message had been posted to its queue (this is necessary to
  prevent queue stalls in case maximum queue capacity was set to a small value
  *and* to maintain better responsiveness); however, memory recollection pass
  will always be completed: the optimizer won't stop until used memory falls
  below quota), OR

- more than `perf_xxx_optrun_wait` seconds has passed since the run had started.

#### Deleting an Object ####

Deleting an object is implemented as an *almost* non-blocking operation; it is
*not* necessary to obtain write lock on the `DynamicMutex` of respective domain:
the object will simply be marked as "deleted", and the rest of removal will be
done by next connection thread(s) that obtains write lock on said mutex to *add
or update* a new record. Specifically:

- When FPC optimizer GCs an object, it sends `TM_UNLINK_OBJECT` message to tag manager, which then posts
  the object to `PageObjectStore`s queue of deleted objects.

- When tag manager deletes an object during processing a "delete objects having these tags" request, it
  posts `OPT_UNLINK_OBJECT` message (for each selected object) to FPC optimizer, which then posts the
  object to `PageObjectStore`s queue of deleted objects.

- The "delete FPC object with this ID" requests are first processed by `PageObjectStore`, which *only*
  finds `PageObject`; then, if it's not yet marked as "deleted", marks it and forwards the command along
  with found object straight to the tag manager.

- In other words: when tag manager or FPC optimizer receives a `xxx_UNLINK_OBJECT` message, it "knows" it
  came from its "peer" (they both do some forms of GC), so, after unlinking the object, it knows its
  "peer" had already done its part of the job, so the object gets posted to the deleted objects queue of
  the `PageObjectStore` object; when session optimizer receives `OPT_UNLINK_OBJECT` request, there's even
  no tag manager at all, so optimizer's actions remain the same (just post the object to
  `SessionObjectStore`s queue of deleted objects).

- Whenever a connection thread locks store table for writing, it does `try_pop()` on that table's
  queue of deleted objects, and if there are objects in it, deletes them *from the table* and deallocates
  them; if the object still has readers, connection thread puts it back to the message queue for someone
  else to try again later.

- Deletion of objects by connection threads having write lock on a store table is controlled by two
  options: how many objects they are allowed to delete in one go if they have to remap a hash table (which
  might be the case if they are inserting new objects), and how many objects they may delete if remapping
  is not necessary; the former can be set to `0`, the latter cannot.

#### Eviction Strategies ####

Contrary to LRU eviction algorithms used by Redis, which [are based on
statistical approximation](http://redis.io/topics/lru-cache), CyberCache employs
true LRU eviction, with data most recently *accessed* (i.e. checked, read, or
written) *guaranteed* to be evicted later than "stale" data, and it does so
using algorithms with O(1) complexity. All modes of the `session_eviction_mode`
and `fpc_eviction_mode` configuration options (`strict-expiration-lru`,
`expiration-lru`, `lru`, and `strict-lru`) use LRU eviction algorithm when it is
necessary to free up some memory; the differences between them are that:

- `strict-expiration-lru` mode will not delete a record that has *not* expired
  unless total memory used by domain exceeds its quota
  (`session_max_memory_size` for session domain, or `fpc_max_memory_size` for
  FPC domain); if memory quota is set to `0` (meaning "unlimited"), then in this
  mode the optimizer will never delete a record that is not expired in
  respective domain,

- `expiration-lru` mode (the default for session domain) *does* take into
  account expiration timestamps and may purge expired records even when the
  server still has enough free memory in a domain (session or FPC); however,
  even in this mode the server:

      - will remove a record with bigger TTL instead of a record that has
        smaller TTL but that was accessed more recently if it runs out of
        memory,

      - will "revive" expired records that are read, checked, or otherwise
        accessed (if they are have not been deleted yet); to reflect this,
        `Zend_Cache_Backend_ExtendedInterface::getCapabilities()` reports the
        `expired_read` option as `true`.

- `lru` eviction is "pure" LRU and never removes expired records just because
  they have expired, although it still honors explicit garbage collection
  requests from Magento application or server console (`GC` and `CLEAN old`
  commands, which delete expired records); this is the default for FPC domain,

- `strict-lru` mode works just like `lru`, except that it ignores even explicit 
  garbage collection requests.

Eviction mode for any domain can be changed at run time. For this to work, even
in `strict-lru` mode the server always sets (and modifies as needed) expiration 
timestamps; it just never checks them if `strict-lru` is in effect.

Data Classes
------------

These classes hold and manipulate cache data that the server stores and servers
to its users.

### `HashObject` ###

Base class for all dynamically-allocated objects that can be put into a
`HashTable` and can be individually locked; has the following fields:

- `HashObject* ho_ht_prev` : Previous object in the bucket chain, or `null`,
- `HashObject* ho_ht_next`: Next object in the bucket chain, or `null`,
- `HashObject* ho_prev` : Previous object in the global chain of objects of its type, or `null`,
- `HashObject* ho_next` : Next object in the global chain of objects of its type, or `null`,
- `uint64      ho_hash` : Hash code for the object,
- `uint32      ho_length` : Total record size, bytes,
- `uint16      ho_nlength` : Record name length, including terminating `\0`, bytes,
- `uint8       ho_lock` : Flag for use by `SpinLock`,
- `uint8       ho_flags` : Various flags (session/FPC? lockable?).

Name length never changes (Magento does not require this), but length of the 
object may: `PageObject` derived from this class may (theoretically, anyway) 
get a different set of tags during execution of `SAVE` (see command interface 
description). We always store name *at the very end* of the object; its offset 
*must* always be obtained as `(uint8*) p + ho_length - ho_nlength` (because,
even if a record "knows" where its data end, there may be a free/empty space
between the data and the name).

Hash codes used by CyberCache are 64-bit; since there are `N` `HashTable`s per
`ObjectTable` (where `N` is a power of `2`, always greater than `1`), the
`HashTable` uses bucket number that is the remainder of the division of the
original 64-bit hash code by 'N', or else `(N-1)/N` of all `HashTable` buckets
would be empty.

However, we always pass to `HashTable` methods, and subsequently store in
`ho_hash` of `HashObject`, the full 64-bits of the original hash code without
chopping off lowest bits that were used to figure out which table to access; we
also store hash table ordinal (within `Store` into `ho_ht_ordinal`) so that each
`HashObject` would know which `HashTable` it belongs to (which is necessary for
the removal of an object from its table).

The `ho_ht_xxx` fields are owned by the `HashTable` the object is in.

### `PayloadHashObject` ###

Base class for objects that store data, with buffers attached to them, such as
session data, or an FPC data chunk. Derived from `HashObject`; contains the
following fields (in addition to the `HashObject` fields):

- `uint8*             pho_buffer`: Buffer with data associated with the object,
- `PayloadHashObject* pho_opt_prev`: Previous object in optimizer's list of objects, or `null`,
- `PayloadHashObject* pho_opt_next` : Next object in optimizer's list of objects, or `null`,
- `uint32             pho_size` : Buffer size, bytes (size of the data in `pho_buffer`),
- `uint32             pho_usize` : Size of uncompressed data, bytes,
- `uint32             pho_mod_time` : Last modification timestamp,
- `uint32             pho_exp_time` : Expiration timestamp,
- `uint32             pho_count` : Used by derived classes (number of session writes for session object,
  number of tag records for `PageObject`),
- `uint16             pho_nreaders` : Number of readers that are transmitting or saving `pho_buffer`,
- `uint8              pho_opt_useragent` : User agent (see Command Interface description for values),
- `uint8              pho_opt_comp` : Type of compressor used on `pho_buffer` contents.

Until the object is processed by `Binlog` and its `pho_nreaders` field becomes
zero, physical removal/deallocaton of the buffer pointed to by the `pho_buffer`
field is not allowed; the object can still be locked for reading and [some]
modifications, but the data buffer must not be changed in any way (this includes
buffer re-compression). If a `DESTROY`/`REMOVE` request is received, they can
still be processed by setting the "deleted" flag.

All `pho_opt_xxx` fields are owned by the optimizer, so they are never set or
changed directly by a connection thread, even during object creation; instead,
their values are sent in the "add" request to the optimizer.

### `TagRef` ###

The `PageObject` contains an array of these, with their number in the
`pho_count` field of `PayloadHashObject`; if `PageObject` object is not
actually tagged (which is reflected by cleared respective flag in `ho_flags`),
it still has one `TagRef` object in it's array -- the "untagged objects" chain,
which makes it trivial to retrieve all untagged objects. The `TagRef` object has
the following fields:

- `PageObject* tr_page` : Page object that contains this reference,
- `TagObject*  tr_tag` : The tag with which containing object is marked,
- `TagRef*     tr_prev` : Previous object in the list, or `null`,
- `TagRef*     tr_next` : Next object in the list, or `null`.

### `TagObject` ###

Derived from `HashObject`, so it's not lockable -- to do any manipulation with
tags (such as adding or deleting them), a global write lock on the FPC domain
has to be obtained; Tagging a `PageObject` is implemented as linking it to (i.e.
adding to the list of objects of) a `TagObject`. Has the following fields:

- `TagRef* to_first` : First object in the list of marked with this tag, or `null`,
- `uint32  to_num` : Number of references to this tag from `PageObject`s marked with this tag,
- `bool    to_untagged` : If `true`, the tag is actually a list of untagged `PageObject`s.

The `to_num` field is used for two purposes:

1. It can be incremented with `add_ref()` method to add a reference to the tag
   without linking in a new `PageObject`; this is necessary during `PageObject`
   updates, when new `PageObject` location is not known yet (it can be the same
   if numbers of tags match, or it can be dirrerent if as a result of the update
   `PageObject` gets new buffer), but old tag references must be removed.
   Whenever a reference is removed, `to_num` is decremented, and when it becomes
   zero, the tag is removed itself (unless it's a special `__UNTAGGED__` tag;
   consequently, `add_ref()`/`remove_ref()` have no effect on its `to_num`
   field, as there is no need for that), so we need a way to prevent removal of
   tags that might still be needed.

2. While performing boolean operations on tags, it can be benefitial from the
   performance standpoint to know which tag has least number of objects marked
   with it, and start with that tag's list of objects.

### `TagVector` ###

A class that serves as a temporary set of vectors for various operations; can be
sorted to facilitate quick searches. Has the following fields:

- `TagObject** tv_buffer` : Buffer with `TagObject` pointers,
- `uint32      tv_num` : Current number of elements,
- `uint32      tv_capacity` : Current capacity of the buffer (how many elements will fit before resizing).

Default capacity is zero; every thread has its own `TagVector` object. The 
`remove()` method puts very last element in place of the one being removed, so 
the vector becomes unsorted.

### `PageObject` ###

Derived from `PayloadHashObject`; see `TagRef` description. Contains the
following fields:

- `TagRef po_tags[1]` : Array of references to the tags this object is marked with.

The `po_tags` array always has at least one element; if the page data is not
tagged, then this element will contain reference to the `__UNTAGGED__` tag, and
the "tagged" bit in `ho_flags` will be clear. Actual number of elements in
`po_tags` is stored in `pho_count`.

> *IMPORTANT*: tag references are kept in `po_tags[]` array in sorted (by the
> `tr_tag` field) order, so binary search can be used to quickly find out if a tag
> is in the array or not (and `PageObject` has a special method for that). Tags
> are never re-allocated; whenever an FPC record is updated during processing of
> the `SAVE` command, the server first creates links to all tags based on new
> record data, and only then unlinks tag references of the old record, thus
> ensuring that a tag wouldn't be deleted because its last reference was removed
> and then immediately re-inserted.

### `SessionObject` ###

Derived from `PayloadHashObject` but does not add any new functionality; it is 
merely a convenience type (its parent already does everything that's needed).

### `HashTable` ###

Hash table that contains objects derived from `HashObject`; maintains fill
factor set for its `Store`; may or may not calculate the hash codes for the
objects being inserted: it has versions of `find()`/`insert()`/`remove()`
methods that accept 64-bit hash codes instead of (or along with) keys.

There are static methods generating hash codes out of strings, generic data
buffers, or `HashObject`[-derived] classes. `HashTable` uses configuration
option to pick algorithm, and `Hasher` class to calculate hash codes.

Important notes on the use of hash methods and hash codes:

- Hash method can be set using config file using `hash_method` statement but,
  just like `admin_password`, `user_password`, and `bulk_password`
  password-setting commands, it **cannot** be changed during run time, as that
  would invalidate entire cache contents; what this means is that the very first
  configuration file read at startup sets hash method (or, if no config file
  found, default hash method is set), but if hash method-setting commands are
  found in any configuration files loaded using `LOCALCONFIG` or `REMOTECONFIG`,
  or issued using `SET` command, they will be ignored with a warning written to
  the log file.

- Main and replication servers *can* use different hash methods, since data is
  pumped to the replication server in raw form, as it was received from TCP/IP 
  port/socket.

### `Store` ###

A `Store` is a template base class for all containers for particular kinds of
objects: session data chunks, FPC records, or FPC record tags. It is derived
classes that know exect data types and thus are able to call proper overloaded
methods as well as do other housekeeping (e.g. figure out which table the object
belongs to, as nymber of hash tables for different containers may differ; see
below): for instance, when a tagged FPC record has to be added, it is up to
respective object store to also add tags if needed.

Each `Store` also maintains global list of objects (pointer to the first object,
and total number) that it stores in its hash tables.

A `Store` has multiple `HashTable` objects to further lower the stress of hash
table remapping that happens when that table's fill factor grows above maximum
set for the object store; here, "further" refers to the fact that the
anticipated stress is already going to be lower than, say, in Redis because
there are separate tables for sessions, FPC data, and FPC tags. The number of
`HashTable` objects per store is set at compile time, and is a power of `2`.

Even though locking hash tables individually might be beneficial due to the cost
of hash table re-mapping, we do not do that because it would oly help a bit
during cache warm-up; afterwards, when it matters the most (the cache is at full
capacity), separate table locks would hurt more than help.

### `Domain` ###

Template base class for server domains; one of the main function is to maintain
memory quota assigned to the domain, so it has own `Memory` object and queue of
deleted objects; additionally, there is one global object for all allocations
that do not pertain to any particular domain (like, say, allocation of temporary
buffers for `Connection` objects, or `LogMessage` objects). Likewise, 
there is one array of `PerfCounter` objects per domain, and one additional 
array for counters that measure operations that do not pertain to any 
particular domain, like accepting a connection.

Each derived class instantiate one or more instances of the `Store`-derived
classes, one `Optimizer`, one `Binlog`, and one `Replicator` object.
