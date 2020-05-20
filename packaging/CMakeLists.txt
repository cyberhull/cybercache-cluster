
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
# Miscellaneous files.
#
find_package(PHP REQUIRED)

c3_install(FILES files/default/cybercached DESTINATION etc/default)
c3_install(PROGRAMS files/init.d/cybercached DESTINATION etc/init.d)
c3_install(FILES files/php-ini/cybercache.ini
    DESTINATION etc/php/${PHP_SHORT_VERSION}/mods-available)

set(appdir lib/cybercache)
c3_install(
    PROGRAMS
        app/check-cybercached
        app/ping-cybercached
        app/query-cybercached
        app/restart-cybercached
        app/start-cybercached
        app/stop-cybercached
    DESTINATION ${appdir})

set(stubdir lib/cybercache/stub)
c3_install(FILES stubs/README.md DESTINATION ${stubdir})
c3_install(PROGRAMS stubs/update_phpstorm_stub_library
    DESTINATION ${stubdir})
install(DIRECTORY DESTINATION ${stubdir}/stubs/cybercache)

# Magento 1 module
install(DIRECTORY ../src/magento1 ../src/magento2
    DESTINATION share/cybercache)
