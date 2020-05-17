
# CyberCache Cluster
# Written by Vadim Sytnikov.
# Copyright (C) 2016-2019 CyberHULL, Ltd.
# All rights reserved.
# -----------------------------------------------------------------------------

project(Brotli)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare")

# fine-tune library compilation mode
if (NOT CYGWIN)
    # generate position-independent code
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif ()

add_library(comp_brotli
    include/brotli/types.h
    include/brotli/encode.h
    include/brotli/decode.h
    common/dictionary.c
    enc/backward_references.c
    enc/bit_cost.c
    enc/block_splitter.c
    enc/brotli_bit_stream.c
    enc/cluster.c
    enc/compress_fragment.c
    enc/compress_fragment_two_pass.c
    enc/encode.c
    enc/entropy_encode.c
    enc/histogram.c
    enc/literal_cost.c
    enc/memory.c
    enc/metablock.c
    enc/static_dict.c
    enc/utf8_util.c
    dec/bit_reader.c
    dec/decode.c
    dec/huffman.c
    dec/state.c)

target_compile_definitions(comp_brotli PRIVATE NDEBUG=1)
target_compile_options(comp_brotli PRIVATE -O3)

c3_install(LICENSES LICENSE DESTINATION brotli)
