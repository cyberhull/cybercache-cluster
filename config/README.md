
CyberCache Server Configuratiion and Documentstion
==================================================

CyberCache server configuration and documentation files are generated from a
single source, which is `cybercached.mdcfg` file found in this directory.

The `generate_md_cfg` is used to generate `.cfg` and `.md` files that, in turn,
are then used by the packaging scripts; the `.md` files serve as sources for the
server documentation available (after CyberCache DEB or RPM package is
installed in the system) via `man cybercached` command.
