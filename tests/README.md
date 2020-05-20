
CyberCache Cluster Test Suite
=============================

The test suite is a set of CyberCache console scripts that test all console and 
server commands; specifically:

- `all-tests.cfg` : main test script that calls out all other scripts,
- `console-test.cfg` : main console test script,
- `console-help-test.cfg` : script that tests console help facilities,
- `server-test.cfg` : script testing general-purpose server commands,
- `server-session-test.cfg` : script testing server's session store,
- `server-fpc-test.cfg` : script testing server's FPC store,
- `server-option-test.cfg` : script that tests server option setting and retrieval.

One can run either entire suite using `test-console` script, or individual tests
using `<path-to-console-execuitable> <config-file1> [ <config-file2> [...]]`;
the server **must** be running if any `server-...` configuration files are used,
and it should be "clean" (i.e. have no session or FPC records etc.)

> **IMPORTANT**: the server must be run with the `config/cybercached-test.cfg` 
> configuration file that is part of this test suite. This file uses default 
> values for vast majority of options, except for the administrative password 
> and some paths (log- and binlog-related). Also, both server and console must
> be started from the test suite directory (where this README file is located), 
> for them to be able to find various files necessary for the test.

Use of `test-console` script is perferred over running individual tests manually
as it does proper housekeeping (e.g. takes care of temporary files).

How to test the server
----------------------

The following steps should be executed after successful installation of the
`cybercache` package:

1) Switch to required CyberCache edition:

    dev-switch-edition <edition> <php-version>

where `<edition>` is `community`, `enterprize`, or `instrumented`, and
`<php-version>` is `7.0`, `7.1`, `7.2`, or `7.3`.
    
> The above command should be executed at least once; the reason is that it
> creates `dev-console` and `dev-server` links in the current directory, which
> are then used during tests; it also copies development version of the PHP
> extension and, if necessary, restarts Apache server and php-fpm daemon.

2) Make sure that the server is running:

    sudo /usr/lib/cybercache/check-cybercached     (expected result: 'CyberCache is running')
    /usr/lib/cybercache/ping-cybercached           (expected result: 'Response: OK')
    /usr/lib/cybercache/query-cybercached          (expected result: 'Response: list of 19 strings ...')

3) Stop the server (this is a **REQUIRED** step: test suite in
  `/usr/lib/cybercache/tests` relies on special configuration file that is a part
  of the suite, so server will have to be started manually):
  
    sudo /usr/lib/cybercache/stop-cybercached

4) Change directory to `/usr/lib/cybercache/tests` (it should remain "current"
  throughout all testing).

5) Start the server again, this time from the test suite directory (the server
  will be started as *foreground* process, so for the tests themselves it will
  be necessary to open another console window):
  
    sudo ./start-cybercache-server
    
  Note that the the above script starts the server with `root` privileges,
  whereas normally the server starts under user `cybercache`; this is necessary
  to avoid running server within its home directory, `/var/lib/cybercache`, but
  instead stay in the current directory to access certain test files.
  
6) Run console script test (should print "All tests PASSED!"):

    ./test-console
    
  (Note: if there are any errors during command execution, information about
  them will be saved to the `~/cybercache.console-errors` file)
    
7) Start the server again (last test in `test-console` shuts down the server):

    sudo ./start-cybercache-server

> If `./test-console` crashed the server, then the server should be started
> using `sudo ./start-cybercache-server --force`; this will remove PID file and
> thus allow new server instance to start.
    
8) Run PHP extension test (should print "ALL tests PASSED!"):

    ./test-extension

9) Start server in "normal" mode:

    sudo /usr/lib/cybercache/start-cybercached
