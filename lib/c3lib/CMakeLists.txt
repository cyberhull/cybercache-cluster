
# CyberCache Cluster
# Written by Vadim Sytnikov.
# Copyright (C) 2016-2019 CyberHULL. All rights reserved.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# -----------------------------------------------------------------------------
#
# CyberCache main API library.
#
project(C3Lib)

# fine-tune library compilation mode
if (NOT CYGWIN)
    # generate position-independent code
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif ()

set(C3LIB_COMMON_SOURCES
    compressors/engine_lzss3.cc compressors/engine_lzss3.h
    compressors/engine_snappy.cc compressors/engine_snappy.h
    compressors/engine_zlib.cc compressors/engine_zlib.h
    compressors/engine_zstd.cc compressors/engine_zstd.h
    compressors/engine_lz4.cc compressors/engine_lz4.h
    compressors/engine_lzf.cc compressors/engine_lzf.h
    compressors/engine_lzham.cc compressors/engine_lzham.h
    c3lib.h c3_build_defs.h c3_build.h c3_types.h
    c3_build_assert.cc c3_build_assert.h
    c3_vector.h c3_descriptor_vector.h
    c3_queue.h
    c3_utils.cc c3_utils.h
    c3_version.cc c3_version.h
    c3_stackdump.cc c3_stackdump.h
    c3_parser.cc c3_parser.h
    c3_compressor.cc c3_compressor.h
    c3_hasher.cc c3_hasher.h
    c3_system.cc c3_system.h
    c3_errors.cc c3_errors.h
    c3_memory.cc c3_memory.h
    c3_string.cc c3_string.h
    c3_timer.cc c3_timer.h
    c3_files.cc c3_files.h
    c3_file_base.cc c3_file_base.h
    c3_signals.cc c3_signals.h
    c3_epoll.cc c3_epoll.h
    c3_sockets.cc c3_sockets.h
    c3_sockets_io.cc c3_sockets_io.h
    c3_logger.cc c3_logger.h
    io_protocol.h io_protocol.cc
    io_payload.h
    io_net_config.cc io_net_config.h
    io_data_buffer.cc io_data_buffer.h
    io_shared_buffers.cc io_shared_buffers.h
    io_reader_writer.cc io_reader_writer.h
    io_chunk_builders.cc io_chunk_builders.h
    io_chunk_iterators.cc io_chunk_iterators.h
    io_device_handlers.cc io_device_handlers.h
    io_response_handlers.cc io_response_handlers.h
    io_command_handlers.cc io_command_handlers.h
    io_handlers.cc io_handlers.h)

set(C3LIB_ENTERPRISE_SOURCES
    compressors/engine_brotli.cc compressors/engine_brotli.h)

if(C3_EDITION STREQUAL community)

  # Community Edition build
  add_library(c3lib_ce
    ${C3LIB_COMMON_SOURCES})
  target_compile_definitions(c3lib_ce PUBLIC C3_FASTER=1)
  target_compile_options(c3lib_ce PUBLIC -O2)
  target_link_libraries(c3lib_ce PUBLIC -s -O)

elseif(C3_EDITION STREQUAL enterprise)

  # Enterprise Edition build
  add_library(c3lib_ee
    ${C3LIB_COMMON_SOURCES}
    ${C3LIB_ENTERPRISE_SOURCES})
  target_compile_definitions(c3lib_ee PUBLIC C3_FASTEST=1 C3_ENTERPRISE=1)
  target_compile_options(c3lib_ee PUBLIC -O3)
  target_link_libraries(c3lib_ee PUBLIC -s -O)

  # Instrumented Enterprise Edition build
  add_library(c3lib_iee
      c3_profiler.cc c3_profiler.h c3_profiler_defs.cc c3_profiler_defs.h
      ${C3LIB_COMMON_SOURCES}
      ${C3LIB_ENTERPRISE_SOURCES})
  target_compile_definitions(c3lib_iee PUBLIC C3_SAFEST=1 C3_ENTERPRISE=1 C3_INSTRUMENTED=1)
  target_compile_options(c3lib_iee PUBLIC -g -Og -rdynamic -funwind-tables)

else()

  message(FATAL_ERROR "Unsupported edition '${C3_EDITION}'")

endif()
