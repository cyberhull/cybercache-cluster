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
 * Master include file for the library.
 *
 * Modules are grouped by their types, which is encoded in prefixes:
 * - C3: system/utility functions ("C3" always stands for CyberCache Cluster),
 * - HT: hash tables, stores, and objects that can be kept in them,
 * - MT: multi-threading and concurrency support,
 * - IO: input/output services,
 * - LS: logging services,
 * - PL: pipelines.
 */
#ifndef _C3LIB_H
#define _C3LIB_H

#include "c3_build.h"
#include "c3_types.h"
#include "c3_utils.h"
#include "c3_version.h"
#include "c3_stackdump.h"
#include "c3_memory.h"
#include "c3_string.h"
#include "c3_timer.h"
#include "c3_profiler.h"
#include "c3_profiler_defs.h"
#include "c3_vector.h"
#include "c3_descriptor_vector.h"
#include "c3_queue.h"
#include "c3_system.h"
#include "c3_errors.h"
#include "c3_signals.h"
#include "c3_epoll.h"
#include "c3_sockets.h"
#include "c3_sockets_io.h"
#include "c3_files.h"
#include "c3_file_base.h"
#include "c3_compressor.h"
#include "c3_hasher.h"
#include "c3_parser.h"
#include "c3_logger.h"
#include "io_protocol.h"
#include "io_net_config.h"
#include "io_data_buffer.h"
#include "io_shared_buffers.h"
#include "io_reader_writer.h"
#include "io_chunk_builders.h"
#include "io_chunk_iterators.h"
#include "io_device_handlers.h"
#include "io_command_handlers.h"
#include "io_response_handlers.h"
#include "io_handlers.h"

#endif // _C3LIB_H
