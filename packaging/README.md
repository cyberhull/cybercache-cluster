
Packaging Scripts
=================

This directory contain packaging scripts and auxiliary data used to create DEB
and RPM packages.

The only script that should be used directly is `create_all_packages`; when
called, it produces `.deb` and `.rpm` files according to the configuration found
in `scripts/c3p.cfg`. How to promote version number(s) and build a new version
of the CyberCache cluster along with its components is described in the
`doc/version-upgrade.md` document.

On rare occasions when it's necessary to increment package *iteration* number
(say, after making changes to DEB meta-data) w/o affecting server etc. numbers,
it's necessary to modify `C3_ITERATION` variable in the `create_all_packages`
script; iteration number is not pulled from the configuration file yet.
