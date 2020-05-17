
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
# A wrapper around CMake's install function which performs additional tasks.
#
# c3_install(FILES|PROGRAMS files... [DESTINATION <dir>])
#
#   Wraps CMake's install(FILES|PROGRAMS) but preprocesses files with .c3p
#   extension before installation.
#
# c3_install(LICENSES files... [DESTINATION <dir>])
#
#   Wraps CMake'3 install(FILES) but if <dir> is not absolute, it is
#   interpreted relative to share/cybercache/license inside the installation
#   directory.
#
function(c3_install)
    set(options)
    set(oneval DESTINATION)
    set(multival LICENSES FILES PROGRAMS)
    cmake_parse_arguments(ARG "${options}" "${oneval}" "${multival}" "${ARGN}")

    if(ARG_UNPARSED_ARGUMENTS)
        message(SEND_ERROR "Unknown arguments: '${ARG_UNPARSED_ARGUMENTS}'")
    endif()

    if(ARG_FILES)
        _c3_preprocess_and_install(FILES "${ARG_FILES}" "${ARG_DESTINATION}")
    endif()

    if(ARG_PROGRAMS)
        _c3_preprocess_and_install(PROGRAMS
            "${ARG_PROGRAMS}" "${ARG_DESTINATION}")
    endif()

    if(ARG_LICENSES)
        _c3_install_licenses("${ARG_LICENSES}" "${ARG_DESTINATION}")
    endif()
endfunction()

function(_c3_preprocess_and_install type files dest)
    set(inst_files)

    foreach(file IN LISTS files)
        if(NOT ${file} MATCHES \\.c3p$)
            list(APPEND inst_files ${file})
            continue()
        endif()

        # preprocess
        set(input ${file})
        if(NOT IS_ABSOLUTE ${input})
            set(input ${CMAKE_CURRENT_SOURCE_DIR}/${input})
        endif()

        string(REGEX REPLACE \\.c3p$ "" output ${file})
        set(output ${CMAKE_CURRENT_BINARY_DIR}/${output})
        get_filename_component(outdir ${output} DIRECTORY)

        set(c3p ${SCRIPT_DIR}/c3p)

        add_custom_command(OUTPUT ${output}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${outdir}
            COMMAND ${c3p} -f ${input} -e ${C3_EDITION} -o ${output}
            MAIN_DEPENDENCY ${input}
            DEPENDS ${c3p})

        string(REGEX REPLACE "[^a-zA-Z0-9_]" _ ident ${output})
        add_custom_target(pp_${ident} ALL
            DEPENDS ${output}
            COMMENT "Preprocessing: ${file}")

        list(APPEND inst_files ${output})
    endforeach()

    install(${type} ${inst_files} DESTINATION ${dest})
endfunction()

function(_c3_install_licenses files dest)
    if(NOT IS_ABSOLUTE ${dest})
        set(dest share/cybercache/license/${dest})
    endif()
    install(FILES ${files} DESTINATION ${dest})
endfunction()
