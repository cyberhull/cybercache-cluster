
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
# Find the currently installed PHP SDK
#
# This module defines the following import targets:
#
#   PHP::PHP
#
# This module defines the following variables:
#
#   PHP_FOUND - set to TRUE if PHP of the requested version is found
#   PHP_LIBRARIES - set to the value "PHP::PHP"
#   PHP_VERSION - contains the version of the installed PHP
#   PHP_SHORT_VERSION - contains only the MAJOR.MINOR part of the PHP version
#   PHP_API - contains the API version of the installed PHP
#
function(_findphp_impl)
    # Check if PHP is installed
    find_program(PHP_CONFIG php-config)
    if(NOT PHP_CONFIG)
        set(PHP_LIBRARIES PHP-NOTFOUND PARENT_SCOPE)
        return()
    endif()

    # Query the PHP version
    execute_process(
        COMMAND ${PHP_CONFIG} --version
        OUTPUT_VARIABLE version
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REGEX MATCH "^[0-9]+.[0-9]+" short_version ${version})

    # Query the API version
    execute_process(
        COMMAND ${PHP_CONFIG} --phpapi
        OUTPUT_VARIABLE api
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    # Query includes
    execute_process(
        COMMAND ${PHP_CONFIG} --includes
        OUTPUT_VARIABLE includes
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    separate_arguments(includes UNIX_COMMAND "${includes}")

    # --includes can be a mix of include directories and macro
    # definitions. Split them.
    set(include_dirs)
    set(compile_defs)
    foreach(x IN LISTS includes)
        if(x MATCHES ^-I)
            string(REPLACE -I "" x "${x}")
            list(APPEND include_dirs "${x}")
        elseif(x MATCHES ^-D)
            string(REPLACE -D "" x "${x}")
            list(APPEND compile_defs "${x}")
        else()
            message(SEND_ERROR "Unsupported PHP include ${x}")
        endif()
    endforeach()

    # Create the import library
    set(target PHP::PHP)
    add_library(${target} INTERFACE IMPORTED)
    set_target_properties(${target} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${include_dirs}"
        INTERFACE_COMPILE_DEFINITIONS "${compile_defs}")

    set(PHP_LIBRARIES ${target} PARENT_SCOPE)
    set(PHP_VERSION ${version} PARENT_SCOPE)
    set(PHP_SHORT_VERSION ${short_version} PARENT_SCOPE)
    set(PHP_API ${api} PARENT_SCOPE)
endfunction()

_findphp_impl()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PHP
    REQUIRED_VARS PHP_SHORT_VERSION PHP_VERSION PHP_LIBRARIES PHP_API
    VERSION_VAR PHP_VERSION)
