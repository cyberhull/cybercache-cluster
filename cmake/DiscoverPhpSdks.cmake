
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

# discover_php_sdks(VERSIONS <php-version>... [REQUIRED])
#
# Find all installed PHP SDKs from the `<php-version>...` list, e.g.
#
#   discover_php_sdks(VERSIONS 7.0 7.1 7.2 7.3)
#
# If REQUIRED option is present, a missing SDK is an error. Otherwise missing
# SDK versions are reported in the log.
#
# This script creates the following variables:
#
# PHP_FOUND - set to TRUE if at least one version of PHP SDK is found
# PHP_APIS - a list of API versions of installed PHP SDKs
# PHP_VERSION_<API> - defined for each API version in PHP_APIS and contains
#   the corresponding PHP version
# PHP_LIBRARIES_<API> - defined for each API version in PHP_APIS and contains
#   the name of a CMake target for linking

function(DISCOVER_PHP_SDKS)
    if(PHP_FOUND)
        # Avoid duplicate searches
        return()
    endif()

    set(OPTIONS REQUIRED)
    set(ONEVAL)
    set(MULTIVAL VERSIONS)
    cmake_parse_arguments(ARG "${OPTIONS}" "${ONEVAL}" "${MULTIVAL}" ${ARGN})

    if(NOT ARG_VERSIONS)
        message(SEND_ERROR
            "discover_php_sdks: At least one PHP version must be specified")
        return()
    endif()

    set(APIS)

    foreach(PHP_VERSION IN ITEMS ${ARG_VERSIONS})
        _DPS_TEST_PHP_VERSION(${PHP_VERSION} TARGET API)
        if(TARGET "${TARGET}")
            message(STATUS "Found PHP ${PHP_VERSION}")
            set(APIS ${APIS} ${API})
            set(PHP_VERSION_${API} ${PHP_VERSION} PARENT_SCOPE)
            set(PHP_LIBRARIES_${API} ${TARGET} PARENT_SCOPE)
        elseif(ARG_REQUIRED)
            message(SEND_ERROR "Could NOT find PHP ${PHP_VERSION}")
        else()
            message(STATUS "Could NOT find PHP ${PHP_VERSION}")
        endif()
    endforeach()

    set(PHP_APIS ${APIS} PARENT_SCOPE)

    if(APIS)
        set(PHP_FOUND TRUE PARENT_SCOPE)
    else()
        string(REPLACE ";" ", " VLIST "${ARG_VERSIONS}")
        message(STATUS "Could NOT find PHP. Tried ${VLIST}")
    endif()
endfunction()

function(_DPS_TEST_PHP_VERSION PHP_VERSION TARGET_VAR API_VAR)
    set(PHP_CONFIG_TOOL_VAR PHP_CONFIG${PHP_VERSION})

    # Check if PHP is installed
    find_program(${PHP_CONFIG_TOOL_VAR} php-config${PHP_VERSION})
    if(NOT ${PHP_CONFIG_TOOL_VAR})
        set(${TARGET_VAR} FALSE PARENT_SCOPE)
        set(${API_VAR} FALSE PARENT_SCOPE)
        return()
    endif()

    # Query the API version
    execute_process(
        COMMAND ${${PHP_CONFIG_TOOL_VAR}} --phpapi
        OUTPUT_VARIABLE API
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    # Query includes
    execute_process(
        COMMAND ${${PHP_CONFIG_TOOL_VAR}} --includes
        OUTPUT_VARIABLE INCLUDES
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    separate_arguments(INCLUDES UNIX_COMMAND "${INCLUDES}")
    # Strip the -I options from includes
    string(REPLACE -I "" INCLUDE_DIRS "${INCLUDES}")

    # Create the import library
    set(TARGET PHP::API${API})
    add_library(${TARGET} INTERFACE IMPORTED)
    set_target_properties(${TARGET} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${INCLUDE_DIRS}")

    set(${TARGET_VAR} ${TARGET} PARENT_SCOPE)
    set(${API_VAR} ${API} PARENT_SCOPE)
endfunction()
