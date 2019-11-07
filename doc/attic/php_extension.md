
CyberCache PHP Extension
========================

For Cygwin version of PHP, the extension has to be installed using the following steps:

1. Copy `cybercache.dll` to PHP extension folder:

    <cigwin-root>/lib/php/<api-version>/cybercache.dll

    > NOTE: The <api-version> part of the path is a decimal number; usually,
    > there is only one such directory in `lib/php`; if there are several and
    > you are not sure which one is current, run `php -i` and find
    > `PHP Extension => <api-version>` line in its output; the number to the
    > right of `=>` is the API version you need.

2. Create INI file for the extension in the PHP configuration folder:

    <cigwin-root>/etc/php.d/cybercache.ini

3. Put the following line into the above-mentioned `cybercache.ini`:

    extension = cybercache.dll

4. Make sure `cybercache` extension works using *either* of the following methods:

    1. Run `test_cybercache_ext.php` PHP script (this will display all PHP functions
       defined by the extension).

    2. Run `php -m` and make sure `cybercache` is on the list of loaded modules.

    3. Run `php -i` OR `php -r "phpinfo();"` and make sure `cybercache` support is `enabled`.

    4. Run `php --re cybercache` to check extension version, INI entries, and exported functions.

    5. Run `test_cybercache_ext.cmd`; this is essentially what item #4 (above) 
       does, but it also saves output to `test_cybercache_ext.log`.
