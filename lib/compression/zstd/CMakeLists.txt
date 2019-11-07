
# CyberCache Cluster
# Written by Vadim Sytnikov.
# Copyright (C) 2016-2019 CyberHULL, Ltd.
# All rights reserved.
# -----------------------------------------------------------------------------

project(Zstd)

# fine-tune library compilation mode
if (NOT CYGWIN)
    # generate position-independent code
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif ()

add_library(comp_zstd
    zstd.h
    entropy_common.c
    fse_compress.c
    fse_decompress.c
    huf_compress.c
    huf_decompress.c
    xxhash.c
    zbuff_compress.c
    zbuff_decompress.c
    zstd_common.c
    zstd_compress.c
    zstd_decompress.c)

target_compile_definitions(comp_zstd PRIVATE NDEBUG=1)
target_compile_options(comp_zstd PRIVATE -O3)
