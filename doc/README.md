
CyberCache Documentation
========================

This directory contains all CyberCache documentation source files -- except for
the server daemon documentation that is located in the `config` directory;
please see `README.txt` in that directory for more information.

The `command_interface.md` file contains CyberCache console documentation and
serves as a source for the manual available via `man cybercache` command.
In addition to the inpormation on console commands, it contains names of the
PHP functions available in PHP extensions shipped with CyberCache, as well as
descriptions of low-level binary request and response sequences.

The `protocol.md` file contains description of the low-level binary protocol
used by the server. For command IDs, see `command_interface.md`.

The `warmer.md` file is the documentation for the CyberCache warmer/profiler; it
is used as a source for the manual available (after Enterprise version of the
CyberCache is installed in the system) via `man cybercachewarmer` command.

The `version_upgrade.md` contains detailed instructions on how to promote
CyberCache version number so that it would be reflected in all components,
including binaries, scripts, and documentation.

Directories
-----------

`whitepaper`: contains CyberCache whitepaper source and media files.

`magento`: contains Magento-related documentation, including detailed
instructions on how to implement support for a new version of PHP (i.e. how to
create new CyberCache PHP extension that works with a new version of PHP).

`attic`: it's a directory that contains documentation that is partially
obsolete, but can still be somewhat useful. For instance,
`attic/command_actions.md` contains pseudo-code descriptions of how server
executes various requests; those descriptions, however, may not be entirely
accurate. As an example, the `LOADCONFIG` command no longer locks a mutex,
it's been re-worked to be more flexible and efficient.
