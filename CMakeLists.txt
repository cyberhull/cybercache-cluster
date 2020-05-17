
# CyberCache Cluster
# Written by Vadim Sytnikov.
# Copyright (C) 2016-2019 CyberHULL, Ltd.
# All rights reserved.
# -----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.5)
project(CyberCacheCluster)

set(C3_EDITION community CACHE STRING
    "Edition of CyberCache to build. One of community or enterprise")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR}/cmake)

set(CMAKE_C_OUTPUT_EXTENSION ".o")
set(CMAKE_CXX_OUTPUT_EXTENSION ".o")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# use "-s -O3" for optimized builds
set(C3_COMMON_FLAGS "-Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C3_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C3_COMMON_FLAGS} -std=c++14")

set(SCRIPT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/scripts)

# figure out target environment
if (CYGWIN)
  message(STATUS "Building for Cygwin...")
  set (CMAKE_THREAD_LIBS_INIT "")
else ()
  message(STATUS "Building for Linux...")
  find_package(Threads)
endif (CYGWIN)

# extract CyberCache version into C3_VERSION
execute_process(
    COMMAND /bin/bash -c "echo C3P[VERSION] | ${SCRIPT_DIR}/c3p"
    OUTPUT_VARIABLE C3_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE)

message(STATUS "CyberCache ${C3_EDITION} version ${C3_VERSION}")

include(AddManual)
include(C3Install)

include_directories(lib)
include_directories(lib/compression/brotli/include)

add_subdirectory(config)
add_subdirectory(doc)
add_subdirectory(packaging)
add_subdirectory(tests)
add_subdirectory(lib/c3lib)
add_subdirectory(lib/compression/brotli)
add_subdirectory(lib/compression/lz4)
add_subdirectory(lib/compression/lzf)
add_subdirectory(lib/compression/lzham)
add_subdirectory(lib/compression/snappy)
add_subdirectory(lib/compression/zlib)
add_subdirectory(lib/compression/zstd)
add_subdirectory(lib/hashes/farmhash)
add_subdirectory(lib/hashes/murmurhash)
add_subdirectory(lib/hashes/spookyhash)
add_subdirectory(lib/hashes/xxhash)
add_subdirectory(lib/regex/pcre2)
add_subdirectory(src/utils/epoll)
add_subdirectory(src/utils/hesper)
add_subdirectory(src/utils/pcre2)
add_subdirectory(src/utils/regexp)
add_subdirectory(src/client)
add_subdirectory(src/console)
add_subdirectory(src/server)
add_subdirectory(src/warmer)

c3_install(LICENSES GPL_v2.txt LICENSE.md)
