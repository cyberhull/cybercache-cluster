
# CyberCache Cluster
# Written by Vadim Sytnikov.
# Copyright (C) 2016-2019 CyberHULL, Ltd.
# All rights reserved.
# -----------------------------------------------------------------------------

project(spookyhash)

# fine-tune library compilation mode
if (NOT CYGWIN)
    # generate position-independent code
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif ()

add_library(hash_spookyhash
    SpookyV2.cpp
    SpookyV2.h)

target_compile_definitions(hash_spookyhash PRIVATE NDEBUG=1)
target_compile_options(hash_spookyhash PRIVATE -O3)
