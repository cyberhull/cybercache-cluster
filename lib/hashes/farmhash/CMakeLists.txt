
# CyberCache Cluster
# Written by Vadim Sytnikov.
# Copyright (C) 2016-2019 CyberHULL, Ltd.
# All rights reserved.
# -----------------------------------------------------------------------------

project(farmhash)

# fine-tune library compilation mode
if (NOT CYGWIN)
    # generate position-independent code
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif ()

add_library(hash_farmhash
    farmhash.h
    farmhash.cc)

target_compile_definitions(hash_farmhash PRIVATE NDEBUG=1)
target_compile_options(hash_farmhash PRIVATE -O3)

c3_install(LICENSES COPYING DESTINATION farmhash)
