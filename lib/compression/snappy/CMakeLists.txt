
# CyberCache Cluster
# Written by Vadim Sytnikov.
# Copyright (C) 2016-2019 CyberHULL, Ltd.
# All rights reserved.
# -----------------------------------------------------------------------------

project(Snappy)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare")

# fine-tune library compilation mode
if (NOT CYGWIN)
    # generate position-independent code
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif ()

add_library(comp_snappy
    snappy.h
    snappy-stubs-public.h
    snappy.cc
    snappy-c.cc
    snappy-sinksource.cc
    snappy-stubs-internal.cc)

target_compile_definitions(comp_snappy PRIVATE NDEBUG=1)
target_compile_options(comp_snappy PRIVATE -O3)

c3_install(LICENSES COPYING DESTINATION snappy)
