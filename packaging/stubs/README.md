
CyberCache PHP Extension API Stub
=================================

This document explains how to make PhpStorm (and, potentially, other IDEs) aware of the
API provided by the `cybercache` PHP extension.

First Method
------------

If you use PhpStorm IDE and have it installed on this computer, you may want to execute

    [sudo] update_phpstorm_stub_library <path-to-phpstorm-installation-directory>

to patch its library of API stubs to make entire API of the `cybercache` PHP extension
"known" to PhpStorm: it will then be able to validate arguments passed to `cybercache`
API functions, make sure that return values are used properly, etc. Additionally,
positioning caret within the name of a function that is part of the `cybercache` API and
pressing Ctrl-Q will bring up "quick help" window with that function's documentation.

The `<path-to-phpstorm-installation-directory>` may be specified either with, or without
trailing slash.

The script requires `jar` executable that is a part of many packages, e.g. `fastjar` that
is available on all Linux systems.

If the directory into which PhpStorm is installed is not writable by current user, you
will need to run the `update_phpstorm_stub_library` script with root privileges.

> Do not forget to refresh PhpStorm's cache: select `File -> Invalidate caches / Restart...`
> after successfully patching its stub library.

Second Method
-------------

If you do not want to patch PhpStorm, you may want to just point it to `cybercache` API
stub directory, which is `/usr/lib/cybercache/stub` (please note that it is *NOT*
`/usr/lib/cybercache/stub/stubs/cybercache`!). To do that, open PhpStorm settings (`Settings`
in the `File` menu), go to `Languages & Frameworks -> PHP`, switch to `PHP Runtime` tab,
and enter `/usr/lib/cybercache/stub` as `Default stubs path`. This method is inferior to
patching in that it necessary to alter a global setting; however, if you have this path
already set to a different location, you may want to just copy `cybercache.php` to
`stubs/cybercache` subfolder at that location.

> **IMPORTANT**: you should use *either* PhpStorm patching, *or* setting `Default stubs
> path`, **not** both; using both methods confuses PhpStorm.

Third Method
------------

If you're using a different IDE altogether, you may want to just put `cybercache.php` file
found in `stubs/cybercache` subdirectory somewhere in your project tree -- most IDEs will
pick it up. Note however that you must **not** `include` or `require` it from your code:
PHP costants and functions found in `cybercache.php` are just "stubs", actual
implementations are in the `cybercache` extension (`cybercache.so` dynamic library) that
had been installed along with the rest of the CyberCache package.

Remaining scripts in this directory can be used to re-generate `cybercache.php`, and
are provided as a reference: the API stub you've received is up to date.

Happy coding!
