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
 * All server subsystems except connection threads and the server object itself.
 */
#ifndef _CC_SUBSYSTEMS_H
#define _CC_SUBSYSTEMS_H

#include "c3lib/c3lib.h"
#include "ls_system_logger.h"
#include "ls_logger.h"
#include "ht_session_store.h"
#include "ht_page_store.h"
#include "ht_tag_manager.h"
#include "ht_optimizer.h"
#include "pl_socket_pipelines.h"
#include "pl_file_pipelines.h"

namespace CyberCache {

/// Incoming server traffic listener
class ServerListener: public SystemLogger, public SocketInputPipeline {
  static constexpr c3_uint_t DEFAULT_INPUT_QUEUE_CAPACITY = 64;
  static constexpr c3_uint_t DEFAULT_OUTPUT_QUEUE_CAPACITY = 64;

public:
  C3_FUNC_COLD ServerListener() noexcept:
    SocketInputPipeline("Listener", DOMAIN_GLOBAL, HO_LISTENER,
      DEFAULT_INPUT_QUEUE_CAPACITY, DEFAULT_OUTPUT_QUEUE_CAPACITY, 0) {
  }
};

/// Session store
class SessionStore: public SystemLogger, public SessionObjectStore {};

/// FPC store
class PageStore: public SystemLogger, public PageObjectStore {};

/// Tag manager
class TagManager: public SystemLogger, public TagStore {};

/// Replication service for the session domain
class SessionReplicator: public SystemLogger, public SocketOutputPipeline {
  static constexpr c3_uint_t DEFAULT_INPUT_QUEUE_CAPACITY = 32;

public:
  C3_FUNC_COLD SessionReplicator() noexcept:
    SocketOutputPipeline("Session replicator", DOMAIN_SESSION, HO_REPLICATOR,
      DEFAULT_INPUT_QUEUE_CAPACITY, 0 /* no output queue */, 0) {
  }
};

/// Replication service for the FPC domain
class PageReplicator: public SystemLogger, public SocketOutputPipeline {
  static constexpr c3_uint_t DEFAULT_INPUT_QUEUE_CAPACITY = 32;

public:
  C3_FUNC_COLD PageReplicator() noexcept:
    SocketOutputPipeline("FPC replicator", DOMAIN_FPC, HO_REPLICATOR,
      DEFAULT_INPUT_QUEUE_CAPACITY, 0 /* no output queue */, 0) {
  }
};

/// Binlog service for session domain
class SessionBinlog: public SystemLogger, public FileOutputPipeline {
public:
  C3_FUNC_COLD SessionBinlog() noexcept:
    FileOutputPipeline("Session binlog", DOMAIN_SESSION, HO_BINLOG, 0) {}
};

/// Binlog service for FPC domain
class PageBinlog: public SystemLogger, public FileOutputPipeline {
public:
  C3_FUNC_COLD PageBinlog() noexcept:
    FileOutputPipeline("FPC binlog", DOMAIN_FPC, HO_BINLOG, 0) {}
};

/// Binlog loader service
class BinlogLoader: public SystemLogger, public FileInputPipeline {
public:
  C3_FUNC_COLD BinlogLoader() noexcept:
    FileInputPipeline("Binlog loader", DOMAIN_GLOBAL, HO_BINLOG, 0) {}
};

/// Cache database saver service
class BinlogSaver: public SystemLogger, public FileOutputNotifyingPipeline {
public:
  C3_FUNC_COLD BinlogSaver() noexcept:
    FileOutputNotifyingPipeline("Binlog saver", DOMAIN_GLOBAL, HO_BINLOG, 1) {}
};

extern Logger            server_logger;
extern ServerListener    server_listener;
extern SessionStore      session_store;
extern PageStore         fpc_store;
extern TagManager        tag_manager;
extern SessionReplicator session_replicator;
extern PageReplicator    fpc_replicator;
extern SessionBinlog     session_binlog;
extern PageBinlog        fpc_binlog;
extern BinlogLoader      binlog_loader;
extern BinlogSaver       binlog_saver;
extern SessionOptimizer  session_optimizer;
extern PageOptimizer     fpc_optimizer;

} // CyberCache

#endif // _CC_SUBSYSTEMS_H
