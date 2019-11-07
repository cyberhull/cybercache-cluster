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
#include "cc_subsystems.h"
#include "cc_server.h"

namespace CyberCache {

Logger            server_logger;
ServerListener    server_listener;
SessionStore      session_store;
PageStore         fpc_store;
TagManager        tag_manager;
SessionReplicator session_replicator;
PageReplicator    fpc_replicator;
SessionBinlog     session_binlog;
PageBinlog        fpc_binlog;
BinlogLoader      binlog_loader;
BinlogSaver       binlog_saver;
SessionOptimizer  session_optimizer;
PageOptimizer     fpc_optimizer;

} // CyberCache
