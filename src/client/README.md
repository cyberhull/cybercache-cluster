
PHP Extension
=============

This directory contains source code for the `cybercache.so` PHP extension.

CyberCache build script automatically compiles/links the extension for all PHP
versions it is configured for. The list of supported PHP versions is configured
using `PHP_APIS` variable within `CMakeLists.txt` file found in this directory;
what else is needed to implement support for a new PHP version, is described in
detail in the `doc/magento/new_php_versions.md` file.

The scripts found in this directory are used by packaging scripts (although can
also be run manually), and they **require** that a CyberCache PHP extension is
already installed in the system.
