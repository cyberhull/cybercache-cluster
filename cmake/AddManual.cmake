
# CyberCache Cluster
# Written by Vadim Sytnikov.
# Copyright (C) 2016-2020 CyberHULL. All rights reserved.
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
option(BUILD_DOCS "Build documentation. Requires pandoc" ON)

if(NOT BUILD_DOCS)
    function(add_manual)
    endfunction()
    return()
endif()

# Find pandoc.
find_program(PANDOC pandoc DOC "Pandoc documentation generator")
if(NOT PANDOC)
    message(SEND_ERROR "pandoc is required to generate documentation. "
        "You can disable documentation generation using the BUILD_DOCS option.")
endif()

function(add_manual md_file man_base_file man_section header)
    if(NOT IS_ABSOLUTE ${md_file})
        set(md_file ${CMAKE_CURRENT_SOURCE_DIR}/${md_file})
    endif()

    set(man_file ${CMAKE_CURRENT_BINARY_DIR}/${man_base_file}.${man_section})
    set(gz_file ${man_file}.gz)

    set(compile_doc ${SCRIPT_DIR}/compile_doc)

    add_custom_command(
        OUTPUT ${man_file}
        COMMAND ${compile_doc} ${md_file} ${man_file} ${man_section} ${header}
        MAIN_DEPENDENCY ${md_file}
        DEPENDS ${compile_doc})

    add_custom_command(
        OUTPUT ${gz_file}
        COMMAND gzip --no-name --best --keep --force ${man_file}
        MAIN_DEPENDENCY ${man_file})

    get_filename_component(md_name ${md_file} NAME)
    add_custom_target(man_${md_name} ALL
        DEPENDS ${gz_file}
        COMMENT "Building ${man_base_file} MAN file")

    install(FILES ${gz_file} DESTINATION share/man/man${man_section})
endfunction()

