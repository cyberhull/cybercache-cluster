
CyberCache Cluster by CyberHULL
===============================

CyberCache Cluster is a high-performance key/value store designed to be used as
Magento session store and cache back-end in high-load applications. Its
architecture pursued two main goals:

- to fully utilize all CPU cores available in modern servers by performing not
  only store/load requests (coming from different sources), but also all
  maintenance tasks concurrently,

- to match Zend cache API as closely as possible to minimize any overhead.

In addition to the above, CyberCache is a web-centric server; so for instance:

- one of its stores is a session store fully aware of session data flows and
  supporting internal session locks preventing data corruption,

- it is user agent-aware and gives different data sources different priorities,

- it comes with native-code PHP extension (supporting PHP 7.0..7.3) guaranteeing
   most efficient data exchange with site code,

- it supports *eight* compressors; some of them (e.g. Brotli) were specially
  designed to perform best on web page data (CyberCache's internal optimizers
  constantly check stores and re-compress data to free up space and also make
  subsequent data load requests more efficient),

- it is shipped with `cybercachewarmer` utility that can be used to warm up
  cache servicing a magento site, as well as for a web site profiling,

- it supports replication (master/slave/both), binlogs (that can be played
  back), monitoring and control via specialized console app, more than 100
  configuration options, on-the-fly configuration w/o server restart, saving
  and loading data stores, etc. etc. etc., you name it.

An overwiew of CyberCache features and architecture can be found in
`doc/whitepaper/whitepaper.md` file. Once CyberCache is installed, one can get
help by executing `man cybercached` (server configuration), `man cybercache`
(console; it also has extensive internal help), and `man cybercachewarmer`.

How Can CyberCache Be Useful To You?
------------------------------------

First of all, CyberCache is free software distributed under GPL license, so you
can utilize it and/or its components and source code [almost] any way you want.
For instance:

- CaberCache can be used as highly efficient Magento session store and cache,
  especially suitable for big sites with, say, 50,000+ products.

- You can adapt CyberCache for any PHP-based framework or CMS having Zend
  framework as its core.

- C/C++ developers may benefit from various tools and techniques found in the
  source code; many solutions used during implementation are original.

As to the last item: as of the time of this writing, number of source code lines
of the CyberCache Cluster project is 44,493 -- this does **not** include third-
party libraries, as well as most scripts; so there's plenty of stuff, it was
being developed over the course of three years, after all. What kind of stuff it
is? Well, few examples:

- Many high-performance servers (e.g. `nginx`) use readily available networking
  libraries; one of the most popular is `libevent`. CyberCache contains
  networking support that was developed from scratch.

- CyberCache console does not use GNU `readline` library; instead, it has its
  own `readline` counterpart developed from scratch.

- Overall CyberCache architecture is based on queues and message exchange
  similar in concept to those found in `Go` programming language; in fact, `Go`
  was considered as potential implementation language early on. What we have in
  CyberCache now are `Go`-like queues and messaging system without `Go`.

  > By the way, this is the main reason of why it is possible to confire the
  > server on the fly w/o restarting it; almost all of its 100+ options (all but
  > select few, maily those having to do with security) can be configured this
  > way: even TCP data port, even queue sizes... Whatever the server is doing,
  > it does *not* have to stop to change compression algorithm set, or process
  > a request to save or load entire store, or do a health check, etc.

- Quite a few synchronization primitives were developed for CyberCache in the
  spirit of so-called lock-free programming; atomics were used whenever
  possible, and when waiting was inevitable, futexes (*the* most efficient
  system-level primitives available in Linux) were used for that.

If you do find CyberCache useful (in any way), please drop us a line:
[cybercache@cyberhull.com](mailto:cybercache@cyberhull.com).

The CyberHULL Team.
