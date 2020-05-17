
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
# CyberCache warmer.
#

if(NOT C3_EDITION STREQUAL enterprise)
    return()
endif()

install(PROGRAMS cybercachewarmer DESTINATION bin)

c3_install(
    FILES
        warmer.js
        c3logger.js
        c3options.js
        c3processor.js
        c3urls.js
        c3version.js.c3p
    DESTINATION lib/cybercache/warmer)
