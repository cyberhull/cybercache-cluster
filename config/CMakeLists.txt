
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
# CyberCache daemon configuration and documentation.
#

find_package(PHP REQUIRED)

set(PP ${SCRIPT_DIR}/c3p)
set(UTIL ${SCRIPT_DIR}/c3mcp)

set(SFILE ${CMAKE_CURRENT_SOURCE_DIR}/cybercached.mdcfg)
set(OBASE ${CMAKE_CURRENT_BINARY_DIR}/cybercached)

if(C3_EDITION STREQUAL enterprise)
    set(ABBR EE)
elseif(C3_EDITION STREQUAL community)
    set(ABBR CE)
else()
    message(FATAL_ERROR "Unsupported edition '${C3_EDITION}'")
endif()

add_custom_command(OUTPUT ${OBASE}.pp
    COMMAND ${PP} -f ${SFILE} -e ${C3_EDITION} -o ${OBASE}.pp
    MAIN_DEPENDENCY ${SFILE}
    DEPENDS ${PP})

add_custom_command(OUTPUT ${OBASE}.cfg
    COMMAND ${UTIL} -f ${OBASE}.pp -c -o ${OBASE}.cfg
    MAIN_DEPENDENCY ${OBASE}.pp
    DEPENDS ${UTIL})

add_custom_command(OUTPUT ${OBASE}.md
    COMMAND ${UTIL} -f ${OBASE}.pp -m -o ${OBASE}.md
    MAIN_DEPENDENCY ${OBASE}.pp
    DEPENDS ${UTIL})

add_custom_target(config ALL DEPENDS ${OBASE}.cfg
    COMMENT "Building daemon configurations")

install(FILES ${OBASE}.cfg DESTINATION etc/cybercache)

add_manual(${OBASE}.md cybercached 1 "CyberCache ${ABBR} ${C3_VERSION} Configuration")
