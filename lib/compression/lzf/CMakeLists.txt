
# CyberCache Cluster
# Written by Vadim Sytnikov.
# Copyright (C) 2016-2019 CyberHULL, Ltd.
# All rights reserved.
# -----------------------------------------------------------------------------

project(Lzf)

# fine-tune library compilation mode
if (NOT CYGWIN)
    # generate position-independent code
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif ()

add_library(comp_lzf
    lzf.h
    lzf_c.c
    lzf_d.c)

target_compile_definitions(comp_lzf PRIVATE NDEBUG=1)
target_compile_options(comp_lzf PRIVATE -O3)

c3_install(LICENSES LICENSE DESTINATION lzf)
