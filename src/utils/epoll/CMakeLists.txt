
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

project(EpollTest)

set(SOURCE_FILES
    main.cc)

add_executable(epoll ${SOURCE_FILES})

if(C3_EDITION STREQUAL community)
  target_link_libraries(epoll
    c3lib_ce)
elseif(C3_EDITION STREQUAL enterprise)
  target_link_libraries(epoll
    c3lib_ee)
else()
  message(FATAL_ERROR "Unsupported edition '${C3_EDITION}'")
endif()
