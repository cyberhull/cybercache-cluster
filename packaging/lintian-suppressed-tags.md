
Rationale for not fixing dome of the issues detected by `lintian`
=================================================================

Certain issues with DEB package are not yet resolved. This file contains
rationale for that.

unstripped-binary-or-object
---------------------------

Package is intended for testing, keepng symbols in executables is therefore
essential for dumps and such. Production version will ship w/o symbols.

embedded-library
----------------

`zlib` (detected by `lintian`) is just one of many compression libraries used
by (and embedded in) `cybercache` components. Even if/when they are moved into
a separate `.so`, they will stay essentially embedded. This is by design.

dir-or-file-in-opt
------------------

CyberCache extras *are* add-on software.

manpage-has-bad-whatis-entry
----------------------------

Markdown files are not yet tailored for `man` generation. Specifically, `NAME`
top-level section is missing. To be addressed in the future.

package-contains-timestamped-gzip
---------------------------------

`fpm` does not use `gzip`'s `-n` option. If script tries to generate proper
`changelog.Debian.gz` by itself, using correct options, then `fpm` (not being
given changelog file name) produces default one, overwriting everything.

binary-without-manpage
----------------------

To be addressed in the future.

php-script-but-no-phpX-cli-dep
------------------------------

The script in question (`opt/cybercache/stub/generate_api_stub`) is not
essential, and may never run.

init.d-script-does-not-source-init-functions
--------------------------------------------

This does not seem to be important, other packages don't do that as well. To be
reviewed in the future releases.
