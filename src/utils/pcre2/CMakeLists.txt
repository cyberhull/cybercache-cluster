
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

project(PCRE2Test)

set(SOURCE_FILES
    main.c)

add_executable(pcre2 ${SOURCE_FILES})

target_link_libraries(pcre2
    -s -O
    regex_pcre2)

target_compile_options(pcre2 PRIVATE -O3 -DNDEBUG)
