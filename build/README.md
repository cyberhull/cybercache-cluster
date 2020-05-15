
Building CyberCache Cluster
===========================

To build CyberCache components, you need CMake version 3.5 or higher.

CMake **MUST** be run from this direcory (`build`) so that packaging, testing,
and various maintenance scripts could find executables and dynamic libraries
produced by the build process.

CyberCache Cluster build process is configured to create PHP extensions for
versions 7.0, 7.1, 7.2, and 7.3 of the PHP. If you do *not* have `-dev` packages
for any of these versions installed, you need to "undo" respective settings in
the build (`../src/client/CMakeLists.txt`) and maintenance files; please refer
to the `../doc/magento/new_php_versions.md` file for more information.
