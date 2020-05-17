
# CyberCache Cluster
# Written by Vadim Sytnikov.
# Copyright (C) 2016-2019 CyberHULL, Ltd.
# All rights reserved.
# -----------------------------------------------------------------------------
#
# This script was build brom scratch using "generic instructions" found in the 
# `NON-AUTOTOOLS-BUILD` file that came with the PCRE2 package.
#
# The `pcre2_jit_match.c` and `pcre2_jit_misc.c` files are not part of the 
# compilation set as they are included from within `pcre2_jit_compile.c`.
#

project(PCRE2)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_CONFIG_H -DPCRE2_CODE_UNIT_WIDTH=8")

add_library(regex_pcre2
    config.h
    pcre2.h
    pcre2_internal.h
    pcre2_intmodedep.h
    pcre2_ucp.h
    pcre2_auto_possess.c
    pcre2_chartables.c
    pcre2_compile.c
    pcre2_config.c
    pcre2_context.c
    pcre2_dfa_match.c
    pcre2_error.c
    pcre2_find_bracket.c
    pcre2_jit_compile.c
    pcre2_maketables.c
    pcre2_match.c
    pcre2_match_data.c
    pcre2_newline.c
    pcre2_ord2utf.c
    pcre2_pattern_info.c
    pcre2_serialize.c
    pcre2_string_utils.c
    pcre2_study.c
    pcre2_substitute.c
    pcre2_substring.c
    pcre2_tables.c
    pcre2_ucd.c
    pcre2_valid_utf.c
    pcre2_xclass.c)

target_compile_definitions(regex_pcre2 PRIVATE NDEBUG=1)
target_compile_options(regex_pcre2 PRIVATE -O3 -Wno-maybe-uninitialized)

c3_install(LICENSES LICENCE DESTINATION pcre2)
