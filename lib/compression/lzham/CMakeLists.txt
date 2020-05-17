
# CyberCache Cluster
# Written by Vadim Sytnikov.
# Copyright (C) 2016-2019 CyberHULL, Ltd.
# All rights reserved.
# -----------------------------------------------------------------------------

project(Lzham)

option(BUILD_X64 "build 64-bit" ON)

# -fno-strict-aliasing is *required* to compile LZHAM
set(CMAKE_CXX_FLAGS "-fno-strict-aliasing -D_LARGEFILE64_SOURCE=1 -D_FILE_OFFSET_BITS=64 -Wall -Wextra")

if (NOT BUILD_X64)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
endif()

# fine-tune library compilation mode
if (NOT CYGWIN)
	# generate position-independent code
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif ()

add_library(comp_lzham
    lzham.h
    lzham_assert.cc
    lzham_checksum.cc
    lzham_huffman_codes.cc
    lzham_lib.cc
    lzham_lzbase.cc
    lzham_lzcomp.cc
    lzham_lzcomp_internal.cc
    lzham_lzcomp_state.cc
    lzham_lzdecomp.cc
    lzham_lzdecompbase.cc
    lzham_match_accel.cc
    lzham_mem.cc
    lzham_platform.cc
    lzham_prefix_coding.cc
    lzham_pthreads_threading.cc
    lzham_symbol_codec.cc
    lzham_timer.cc
    lzham_vector.cc)

target_compile_definitions(comp_lzham PRIVATE NDEBUG=1)
target_compile_options(comp_lzham PRIVATE -O3)

c3_install(LICENSES LICENSE DESTINATION lzham)
