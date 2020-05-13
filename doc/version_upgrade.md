
Promoting CyberCache to a New Version
=====================================

In order to promote CyberCache version, it is necessary to do the following
(all paths are relative to the project root where `TODO.md` resides):

* Change `C3_VERSION_MAJOR`, `C3_VERSION_MINOR`, and `C3_VERSION_PATCH` defines
  in `lib/c3lib/c3_version.h` header as needed.

  * Go to `build` directory and execute `make`; this will take care of all the
    tools built using C/C++ (server, console, and PHP extension -- for all
    editions).

* Change value of the `VERSION` variable in `scripts/c3p.cfg`.

  * Go to `src/warmer` directory and execute `dev-upgrade-version`. This will
    generate `c3version.js` module and thus take care of CyberCache
    warmer/profiler that is written in JavaScript. The `VERSION` value is also
    used by documentation/package creation scripts; see below.

    > NOTE: other variables in `c3p.cfg` are only assigned *default* values,
    > which packaging scripts will overwrite using `c3p` script options; in
    > other words, you should not worry about them.

* Change version number in the Magento 1 extension.

  * Change `version` attrubute in `src/magento1/app/code/community/CH/CyberCacheSession/etc/config.xml` file.

* Update `packaging/deb_files/changelog` and `packaging/deb_files/upstream-changelog`
  adding records for new versions; `deb_date` script can be used to generate
  timestamps in the format required by Debian changelogs (hint: you can use
  `Alt-U` in Midnight Commander'e editor to run `deb_date` and insert its output).

  * Go to `packaging` directory and execute `create_all_packages` script. This
    will create `.deb` and `.rpm` packages for both Community and Enterprise
    editions; version information will be taken from the `scripts/c3p.cfg`
    configuration file that we already modified (above).

  > NOTE: make sure `C3_ITERATION` variable in `create_all_packages` is [re]set
  > to `1`; this variable should be incremented in case there are changes that
  > do not affect CyberCache per se, but do affect DEB/RPM packaging.

  * Move old packages (with previous version numbers in the names of `.deb` and
    `.rpm` archives) to `releases` directory.

These instructions can also be viewed as "How to build CyberCache packages";
version change is just a special kind of build that requires some extra steps.

Testing Built Applications
--------------------------

Development build can be tested using the following scripts:

  * `dev-switch-edition`: accepts `community`, `enterprise`, or `instrumented`
    as first argument, and PHP version as second; configures symlinks, installs
    proper version of the PHP extension, and re-starts Apache server and
    `php-fpm` daemon for the changes to take effect; because of those restarts,
    requires root privileges.

  * `dev-start-cybercache-server`: starts CyberCache server in currently active
    configuration (which can be changed using `dev-switch-edition` script); the
    server is started as an application, not a daemon, so the script should be
    run in a separate console window, for convenience; running this script
    requires root privileges.

  * `dev-test-console`: runs console application (the edition configured by the
    `dev-switch-edition` script) and performs all CyberCache console tests.

  * `test-extension`: runs PHP script that executes PHP API tests.
