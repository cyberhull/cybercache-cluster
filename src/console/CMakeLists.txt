
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
# -----------------------------------------------------------------------------
#
# CyberCache console application.
#

project(CyberCacheConsole)

set(CONSOLE_SOURCE_FILES
    key_defs.h
    key_input.cc key_input.h
    line_input.cc line_input.h
    line_editor.cc line_editor.h
    string_list.cc string_list.h
    net_configuration.cc net_configuration.h
    server_api.cc server_api.h
    command_help.cc command_help.h
    console_commands.cc console_commands.h
    main.cc)

set(CONSOLE_COMMON_LIBRARIES
    comp_lz4
    comp_lzf
    comp_lzham
    comp_snappy
    comp_zlib
    comp_zstd
    hash_farmhash
    hash_murmurhash
    hash_spookyhash
    hash_xxhash
    ${CMAKE_THREAD_LIBS_INIT})

# Community Edition build
add_executable(cybercache_ce ${CONSOLE_SOURCE_FILES})
set_property(TARGET cybercache_ce PROPERTY OUTPUT_NAME cybercache-ce)
target_link_libraries(cybercache_ce
    c3lib_ce
    ${CONSOLE_COMMON_LIBRARIES})

# Enterprise Edition build
add_executable(cybercache_ee ${CONSOLE_SOURCE_FILES})
set_property(TARGET cybercache_ee PROPERTY OUTPUT_NAME cybercache-ee)
target_link_libraries(cybercache_ee
    c3lib_ee
    comp_brotli
    ${CONSOLE_COMMON_LIBRARIES})
