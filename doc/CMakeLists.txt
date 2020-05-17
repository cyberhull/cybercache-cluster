
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
#
# CyberCache common documentation.
#

if(C3_EDITION STREQUAL community)
    set(ABBR CE)
elseif(C3_EDITION STREQUAL enterprise)
    set(ABBR EE)
else()
    message(FATAL_ERROR "Unsupported edition '${C3_EDITION}'")
endif()

add_manual(command_interface.md cybercache 1
    "CyberCache ${ABBR} ${C3_VERSION} Command Interface")
add_manual(protocol.md cybercache 5
    "CyberCache ${ABBR} ${C3_VERSION} Low-Level Protocol")

if(C3_EDITION STREQUAL enterprise)
    add_manual(warmer.md cybercachewarmer 1
        "CyberCache ${ABBR} ${C3_VERSION} Warmer/Profiler")
endif()
