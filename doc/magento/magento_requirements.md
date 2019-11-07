
Magento Requirements
====================

This document outlines requirements that have to be met in order to run
CyberCache Cluster as session store and page cache for Magento.

Since CyberCache Cluster does not have any particular requirements of its own
(it is a self-contained application, with compression, hashing, and all other
libraries statically linked in), the requirements boil down to those of Magento.

The complete list of Magento requirements, recommended components, as well as
information on recommended configuration settings can be found [at Magento
site](http://devdocs.magento.com/guides/v2.1/install-gde/system-requirements-tech.html).

System Requirements
-------------------

The gist, important for CyberCache Cluster, is this:

- 2G or more RAM
- Apache 2.2 or 2.4, or nginx 1.8+; required Cygwin applications:
  - `httpd` (2.4.25-1 or later),
  - `httpd-mod_ssl` (2.4.25-1 or later), *OR*
  - `nginx` (1.10.3-1 or later).
- NT/W2K service initiator: `cygrunsrv` from the `Cygwin` package.
- PHP 7.0.6+ (but not 7.1); required Cygwin applications:
  - `php` (7.0.15-1 or later),
  - `httpd-mod_php7`
- PHP extensions; exact module names from Cygwin distribution are given in parenthes:
  - bc-math (`php-bcmath`)
      - Note: required for Magento Enterprize Edition only
  - ctype (`php-ctype`; not listed in the official Magento requirements, but is
    still needed)
  - curl (`php-curl`)
  - gd (`php-gd`)
      - or ImageMagick 6.3.7+, or both GD and ImageMagick; the latter can be
        downloaded [from this page](https://www.imagemagick.org/script/download.php).
  - intl (`php-intl`)
  - mbstring (`php-mbstring`)
  - mcrypt (`php-mcrypt`)
  - mhash (none; emulated using `hash` library since PHP 5.3, which (the
    library, `HASH Message Digest Framework`) is now part of PHP core; see
    [PHP documentation on mhash](http://php.net/manual/en/intro.mhash.php).)
  - openssl (none; see [PHP documentation](http://php.net/manual/en/openssl.installation.php) for
    configuration info; the SSL library itself can be obtained e.g. from [this
    page](http://gnuwin32.sourceforge.net/packages/openssl.htm))
  - PDO/MySQL (`php-pdo_mysql`)
  - SimpleXML (`php-simplexml`)
  - soap (`php-soap`)
  - xml (`php-XML_Util`, `php-xmlreader`, `php-xmlwriter`)
  - xsl (`php-xsl`)
  - zip (`php-zip`)
  - json (`php-json`)
      - Note: PHP 7 only, but that's what we use
  - iconv (`php-iconv`)
      - Note: PHP 7 only, but that's what we use
- MySQL 5.6

CyberCache Cluster can be run and tested on Windows using **64-bit** Cygwin
software downloaded from [Cygwin site](https://cygwin.com/install.html), and
that's what those extension module name (given above) are for: PHP has to be
installed with those modules.

Apache Settings
---------------

The Apache web server should be configured as described on [this Magento site
page](http://devdocs.magento.com/guides/v2.1/install-gde/prereq/apache.html).
Specifically, for Apache 2.4:

- Enter `a2enmod rewrite` command to enable server rewrites (or, if under 
  Cygwin, just uncomment the following line in `httpd.conf`):

  `LoadModule rewrite_module modules/mod_rewrite.so`

  > Alternatively (if `a2enmod` does not exist, which is the case if Cygwin 
  > version of Apache is used), just uncomment the following line in
  > `/etc/httpd/conf/httpd.conf`:
  >
  > `LoadModule rewrite_module modules/mod_rewrite.so`

- Add the following to the end of the configuration file (the `/var/www/html`
  directory path may differ depending on installation; it is `/srv/www/htdocs`
  under Cygwin by default):

        <Directory "/var/www/html">
      	AllowOverride <value from Apache site>
        </Directory>

  > In case of Apache 2.2, add/modify the following instead:
  >
  >     <Directory "/var/www/html">
  > 	  AllowOverride All
  >     </Directory>

### Installing Apache Under Cygwin

When setting up Apache under Cygwin, use the following steps:

1. Start Cygwin terminal (usually `Cygwin.bat`, located in Cygwin installation
   folder) **AS ADMINISTRATOR**. Even if currently active account belongs to 
   `Administrators` group, still use `Run as administrator` item from Explorer's 
   context menu.

   Failure to do so results in inability to start `httpd` service: it won't be 
   able to open its log files, or PID file, or both; and, since log files will
   not be written, you wouldn't even know what went wrong.

2. Configure Cygwin service manager (answer `yes` to its question(s)):

   `cygserver-config`

3. Install Apache service:

   `/etc/rc.d/init.d/httpd install`

4. Start `cygserver` service:

   `cygrunsrv -S cygserver`

5. Edit `/etc/httpd/conf/httpd.conf`:

   1. Uncomment the following line:

      `LoadModule slotmem_shm_module modules/mod_slotmem_shm.so`

      The service would fail with "Failed to lookup provider 'shm' for 'slotmem'" 
      if this module is not enabled.

   2. Add the following line below all other module references:

      `LoadModule unixd_module modules/mod_unixd.so`

      The server would fail with `Server MUST relinquish startup privileges before accepting connections`
      if this module if not enabled.

      > NOTE: this module seems to be present in the most recent `httpd` package 
      > available in Cygwin.

6. Start Apache service:

   `cygrunsrv -S httpd`

PHP Settings
------------

The detailed list of recommended PHP settings can be found on [Magento
site](http://devdocs.magento.com/guides/v2.1/install-gde/prereq/php-settings.html).
Important recommendations include:

- Uncomment `date.timezone` and set it to actual time zone, e.g.
  `Europe/Moscow`; the list of recognized time zones is available in [PHP
  documentation](http://php.net/manual/en/timezones.php).

- Set proper `memory_limit`, e.g. `memory_limit = 4G`.

- Set `always_populate_raw_post_data` to `-1` (pre-PHP7).

- Set `asp_tags` to `Off` (pre-PHP7).

Magento Database
----------------

Despite documentation stating `localhost` can be used as database address, under 
Windows / Cygwin, one has to specify `127.0.0.1` (`localhost` does not get 
resolved, producing `SQLSTATE[HY000] [2002] No such file or directory` error).
