
# CyberCache Cluster
# Written by Vadim Sytnikov.
# Copyright (C) 2016-2019 CyberHULL, Ltd.
# All rights reserved.
# -----------------------------------------------------------------------------

project(Lz4)

# fine-tune library compilation mode
if (NOT CYGWIN)
    # generate position-independent code
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif ()

add_library(comp_lz4
    lz4.h
    lz4.c)

target_compile_definitions(comp_lz4 PRIVATE NDEBUG=1)
target_compile_options(comp_lz4 PRIVATE -O3)

c3_install(LICENSES LICENSE DESTINATION lz4)
