
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
# CyberCache server/daemon.
#

project(CyberCacheServer)

set(SERVER_SOURCE_FILES
    cc_signal_handler.cc cc_signal_handler.h
    cc_server_queue.h
    cc_subsystems.cc cc_subsystems.h
    cc_worker_threads.cc cc_worker_threads.h
    cc_configuration.cc cc_configuration.h
    cc_server.cc cc_server.h
    cc_entry_point.cc
    ht_objects.cc ht_objects.h
    ht_shared_buffers.cc ht_shared_buffers.h
    ht_stores.cc ht_stores.h
    ht_tag_manager.cc ht_tag_manager.h
    ht_session_store.cc ht_session_store.h
    ht_page_store.cc ht_page_store.h
    ht_optimizer.cc ht_optimizer.h
    ls_utils.cc ls_utils.h
    ls_system_logger.cc ls_system_logger.h
    ls_logger.cc ls_logger.h
    mt_monitoring.h
    mt_defs.h mt_defs.cc
    mt_spinlock.cc mt_spinlock.h
    mt_quick_event.cc mt_quick_event.h
    mt_quick_semaphore.cc mt_quick_semaphore.h
    mt_lockable_object.cc mt_lockable_object.h
    mt_events.cc mt_events.h
    mt_message_queue.h
    mt_mutexes.cc mt_mutexes.h
    mt_threads.cc mt_threads.h mt_thread_guards.h
    pl_net_configuration.cc pl_net_configuration.h
    pl_pipeline_commands.h pl_pipeline_commands.cc
    pl_socket_events.cc pl_socket_events.h
    pl_socket_pipelines.cc pl_socket_pipelines.h
    pl_file_pipelines.cc pl_file_pipelines.h)

set(SERVER_COMMON_LIBRARIES
    comp_lz4
    comp_lzf
    comp_lzham
    comp_snappy
    comp_zlib
    comp_zstd
    hash_farmhash
    hash_murmurhash
    hash_spookyhash
    hash_xxhash
    ${CMAKE_THREAD_LIBS_INIT})

if(C3_EDITION STREQUAL community)

    # Community Edition build
    add_executable(cybercached_ce ${SERVER_SOURCE_FILES})
    set_property(TARGET cybercached_ce PROPERTY OUTPUT_NAME cybercached-ce)
    target_link_libraries(cybercached_ce
        c3lib_ce
        ${SERVER_COMMON_LIBRARIES})

    install(TARGETS cybercached_ce RUNTIME DESTINATION bin)

elseif(C3_EDITION STREQUAL enterprise)

    # Enterprise Edition build
    add_executable(cybercached_ee ${SERVER_SOURCE_FILES})
    set_property(TARGET cybercached_ee PROPERTY OUTPUT_NAME cybercached-ee)
    target_link_libraries(cybercached_ee
        c3lib_ee
        comp_brotli
        ${SERVER_COMMON_LIBRARIES})

    # Instrumented Enterprise Edition build
    add_executable(cybercached_iee ${SERVER_SOURCE_FILES})
    set_property(TARGET cybercached_iee PROPERTY OUTPUT_NAME cybercached-iee)
    target_link_libraries(cybercached_iee
        c3lib_iee
        comp_brotli
        ${SERVER_COMMON_LIBRARIES})
    target_compile_definitions(cybercached_iee PRIVATE C3LM_ENABLED=1)

    install(TARGETS cybercached_ee cybercached_iee RUNTIME DESTINATION bin)

else()

    message(FATAL_ERROR "Unsupported edition '${C3_EDITION}'")

endif()

install(DIRECTORY DESTINATION var/lib/cybercache)
install(DIRECTORY DESTINATION var/log/cybercache)
