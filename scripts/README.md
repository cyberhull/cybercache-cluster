
Configuration Data and Scripts
==============================

This directory contains build configuration data, as well as some text
processing, configuration, and maintenance scripts.

The `c3p.cfg` is the main version configuration file used by various build and
configuration script as *the* source of the current version number. Promoting
CyberCache to a new version and building it is described in the
`doc/version_upgrade.md` file.

The `c3p` and `c3mp` scripts are used to build configuration and documentation
files from the same source; the `c3p.md` file contains documentation for the
former (script).

The `copy_extension_modules` script copies compiled PHP extensions' files to
appropriate system directories, and restarts running services that might be
using them (such as Apache server and php-fpm daemon).

The `switch_php_version` script can be used to select specified PHP version as
default in the system, which is useful for testing; it, too, restarts relevant
services as needed.

The `project_line_count` script calculates number of lines in all project's
source files, *excluding* third-party components.
