
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
# CyberCache tests.
#
set(testdir lib/cybercache/tests)

c3_install(
    FILES
        all-tests-no-shutdown.cfg
        all-tests.cfg
        console-help-test.cfg
        console-test.cfg.c3p
        README.md
        server-fpc-test.cfg
        server-option-test.cfg.c3p
        server-session-test.cfg
        server-test.cfg
    DESTINATION ${testdir})
c3_install(
    PROGRAMS
        test-console-no-shutdown
        test-extension
        test-console
        start-cybercache-server
    DESTINATION ${testdir})

c3_install(
    FILES
        extension/test-extension.inc
        extension/test-extension.php
    DESTINATION ${testdir}/extension)

c3_install(
    FILES
        config/cybercached-test.cfg.c3p
        config/local-test.cfg
        config/remote-test.cfg
    DESTINATION ${testdir}/config)

c3_install(
    FILES
        data/fpc-1.binlog
        data/fpc-2.binlog
        data/sample-record.txt
        data/session-1.binlog
        data/session-2.binlog
    DESTINATION ${testdir}/data)

install(DIRECTORY DESTINATION ${testdir}/logs)

if(C3_EDITION STREQUAL enterprise)
    c3_install(PROGRAMS optimize-network-settings DESTINATION lib/cybercache)
endif()
