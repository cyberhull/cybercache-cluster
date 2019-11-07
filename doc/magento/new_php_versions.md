
Adding Support for New PHP Versions
===================================

New versions of PHP come with new API IDs, and, since CyberCache includes
specialized PHP extension, that extension has to be built against specific PHP
API in order to be loadable by PHP cli, php-fpm, and/or Apache service.

Installing New PHP Version
--------------------------

The following steps are necessary to install multiple versions of PHP, including
those that are either already or yet not supported by host operating system.

1. Add PPA that has all required versions:

    sudo add-apt-repository ppa:ondrej/php

2. Install required PHP version:

    sudo apt install php7.0
    sudo apt install php7.1
    sudo apt install php7.2
    sudo apt install php7.3

3. Install necessary PHP modules:

  These include various optional extensions *not* installed along with PHP (e.g.
  `php7.x-common` is installed and thus not listed below):

  * Development package used to build CyberCache PHP extension:

      sudo apt install php7.x-dev

  * PHP-fpm package (in case CyberCache is used with `nginx` web server):

      sudo apt install php7.x-fpm

  * Extensions required by Magento. Magento 2.1 requires (use `sudo apt install`
    just like in the above examples):

      php7.x-curl
      php7.x-gd (as well as ImageMagick > 6.3.7, installed separately)
      php7.x-intl
      php7.x-mbstring
      php7.x-mcrypt (NOT included in PHP 7.1+; Magento 2.1 has to be patched)
      php7.x-mhash (NOT included in PHP 7.1+; Magento 2.1 has to be patched)
      php7.x-openssl (now part part of PHP core)
      php7.x-mysql
      SimpleXML (now part of php7.x-xml; see below)
      php7.x-soap
      php7.x-xml
      php7.x-xsl
      php7.x-zip
      php7.x-json (now part of another package)
      php7.x-iconv (now part of php7.x-intl package)

  * Additional extensions required by Magento 2.2:

      php7.x-ctype
      php7.x-dom
      php7.x-spl
      php7.x-libxml

  * Additional extensions required by Magento 2.3:

      php7.x-bc-math (Magento Commerce only)
      php7.x-hash
      php7.x-azip

4. Switch to the newly installed PHP version

  Switching between PHP versions might be a bit complicated, as it involves not
  only setting active CLI version, but also disabling/enabling Apache modules
  and restarting Apache server (if it's running), stopping old and starting new
  `php-fpm` daemon, etc.

  Luckily, CyberCache comes with `scripts/switch_php_version` script, that does
  all that, along with necessary checks (e.g. it won't restart Apache and/or
  start php-fpm if previous configuration did not have them running). Command line:

      `switch_php_version <php-version>`

  where `<php-version>` is version specified as `<major>.<minor>`, e.g. `7.2`.

Adding New PHP Version to the Build Process
-------------------------------------------

The above-mentioned `php7.x-dev` extension adds PHP API headers to the folder

    `/usr/include/php/<api-version-id>/`

In order to configure CyberCache build and packaging systems, we need to know
major and minor versions of the new PHP (referred to as `<php-version>`), along
with that `<api-version-id>`, which BTW is 8-digit string having `YYYYMMDD`
format (encoded release date; e.g. "20170718"). Knowing these two version
numbers, we can proceed with CyberCache build/packaging system configuration:

1. Add `<api-version-id>` to the `PHP_APIS` list variable in
  `src/client/CMakeLists.txt` file. That's all that is needed to re-configure
  build system; building the project after that will yield new versions of the
  PHP extension:

      `build/lib/cybercache-ce-<api-version-id>.so`    and
      `build/lib/cybercache-ee-<api-version-id>.so`

2. Add new `cybercache.ini` to `packaging/files/php-ini/<php-version>`
  directory; can be a copy of an existing file for some of the old PHP versions.

3. Add `<php-version>` to the lists of PHP versions in the two `for php_ver...`
  loops in the `packaging/inst/after_install` file.

4. Add `<php-version>` to the list of PHP versions in the `for php_ver...` loop
  in the `packaging/inst/after_uninstall` file.

5. Modify `packaging/create_package` file:

  * Add `C3_EDITION_FILES["../build/lib/cybercache-ce-<api-version-id>.so"]="usr/lib/php/<api-version-id>/cybercache.so"`
    line to the `community)` branch of the edition case.

  * Add `C3_EDITION_FILES["../build/lib/cybercache-ee-<api-version-id>.so"]="usr/lib/php/<api-version-id>/cybercache.so"`
    line to the `enterprise)` branch of the edition case.

  * Add `C3_FILES["files/php-ini/<php-version>/cybercache.ini"]="etc/php/<php-version>/mods-available"`
    line to the definition of the `C3_FILES` array.

  * Add `"<api-version-id>"` to the list of PHP API IDs of the
    `for php_api_ver ...` loop.

6. Modify `scripts/switch_php_version`: make sure pattern in shell function
  `is_invalid_version()` recognizes new `<php-version>` as valid.

7. Modify `scripts/copy_extension_modules`:

  * Add `"<api-version-id>"` to the `for API_VER ...` loop.

  * Add `"<php-version>"` to the `for PHP_VER ...` loop.

8. Build the project.

9. Execute `create_all_packages` script in the `packaging` directory.

10. Uninstall current version of CyberCache (assuming Enterprise Edition here):

    `sudo apt purge cybercache-ee`

11. Install newly assembled package (Enterprise Edition is recommended):

    `sudo dpkg -i cybercache-ee_<version>-1_amd64.deb`

After executing the above items, CyberCache should be up and running with fully
enabled support for the new PHP version.
