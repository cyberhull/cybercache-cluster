
CyberCache Warmer
=================

This directory contain source code of the CyberCache warmer and profiler
application.

The `dev-cybercachewarmer` script can be used to run development version of the
application when older version is already installed in the system.

The `dev-upgrade-version` script is used in the process of promoting CyberCache
to a new version; detailed version promotion instructions can be found in the
`doc/version_upgrade.md` file.

This directory also contains several auxiliary files:

c3version.js.c3p
----------------

This is the source file for `c3version.js`; please note that `c3version.js` is
*not* under git, while `c3version.js.c3p` is.

The `dev-upgrade-version` script should be used to generate `c3version.js` from
`c3version.js.c3p` using version information from `../scripts/c3p.cfg`.

options.txt
-----------

This file contains information that `cybercachewarmer` prints upon `--help`.

If it is necessary to change `--help` text, `options.txt` should be edited,
copied to clipboard, and inserted into value of the local `helpText` variable
of the `printInfo()` function of the `c3options.js` module *using WebStorm or
similar IDE* that would automatically process newlines, escape quotes, etc.
