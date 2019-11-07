
CyberCache Magento 2 Extension
==============================

The extension consists of two separate modules: default cache/FPC store, and
session store.

1) Default Cache and FPC Store
------------------------------

To make Magento use CyberCache as its default cache as well as FPC store, it is
necessary to do the following:

### 1.1) Copy Extension Files

Copy all the files from the directory where this `readme.md` file is located, as
well as *all* its subdirectories, to

    <Magento-installation-directory>/app/code/CyberHULL/CyberCache/

If there was previous version of the CyberCache extension installed at that
location, you may want to delete old files before copying new.

> **IMPORTANT**: files must be readable by Magento code! For `Apache` server to
> be able to access them, change group owner to `www-data`.

> After installing the module, it can be enabled in Magento as described in the
> [Magento documentation](http://devdocs.magento.com/guides/v2.0/extension-dev-guide/build/enable-module.html),
> but that is *optional*: CyberCache extension will work even without that. The
> "component name" of the extension (in case you choose to enable it) is
> `CyberHULL_CyberCache`; see `etc/module.xml`.

### 1.2) Modify `composer.json`

Add the following subsection to the `"autoload"` section of the `composer.json`
file located in Magento installation directory:

    "classmap": [
        "app/code/CyberHULL/CyberCache/src/C3"
    ],

If `"classmap"` section already exists, just add `"app/code/CyberHULL/CyberCache/src/C3"`
to its end (individual records has to be separated with commas).

### 1.3) Update Autoload Map

From the Magento installation directory, execute the following command (as
`magento` user!):

    composer dump-autoload

> If `composer` application has not been installed, one can either
>
> 1) install it using `sudo apt install composer` (on Debian-based systems), or
>
> 2) use one shipped with Magento; in this case, composer has to be run as
>
>     php vendor/composer/composer/bin/composer dump-autoload

The result can be checked by searching `vendor/composer/autoload_map.php` for
the `C3_Store_CyberCacheStore` string; if it exists as a key in the return
array, then composer has re-configured Magento successfully.

### 1.4) Configure Magento Environment

Add the following sections to the `app/etc/env.php` file (or, if `cache` entry
already exists in the return array, it has to be completely replaced with the
below value):

    'cache' =>
    array(
      'frontend' =>
      array(
        'default' =>
        array(
          'backend' => 'C3_Store_CyberCacheStore',
          'backend_options' =>
          array( // missing entries will get default values
            'address' => '127.0.0.1',  // connection address; might be an internet address, or an IP
            'port' => '8120',          // connection port
            'persistent' => 'true',    // whether to use persistent connections (must match server settings)
            'hasher' => 'murmurhash2', // hash method to use for passwords
            'admin' => '',             // password for administrative commands
            'user' => '',              // password for user-level commands
            'compressor' => 'snappy',  // *initial* compressor for various data buffers
            'threshold' => '2048',     // minimal size of the buffer to compress
            'marker' => 'true'         // whether to use integrity check marker
          ),
        ),
        'page_cache' =>
        array(
          'backend' => 'C3_Store_CyberCacheStore',
          'backend_options' =>
          array( // missing entries will get default values
            'address' => '127.0.0.1',  // connection address; might be an internet address, or an IP
            'port' => '8120',          // connection port
            'persistent' => 'true',    // whether to use persistent connections (must match server settings)
            'hasher' => 'murmurhash2', // hash method to use for passwords
            'admin' => '',             // password for administrative commands
            'user' => '',              // password for user-level commands
            'compressor' => 'snappy',  // *initial* compressor for FPC data buffers
            'threshold' => '2048',     // minimal size of the buffer to compress
            'marker' => 'true'         // whether to use integrity check marker
          )
        )
      )
    ),

After the above four steps, Magento 2 will run with `CyberCache` being its
default and Full Page cache.

2) Session Store
----------------

Unlike general-purpose and full page cache handlers, session handlers are more
deeply integrated into Magento 2, so it is necessary to patch some Magento core
files to start using CyberCache as Magento session store.

> Redis session extension was available for Magento 2 since very first
> releases, but it only became supported in version `2.0.6`: because it was
> necessary to patch Magento itself.

To enable CyberCache as Magento 2 session store, it is necessary to do the
following:

### 2.1) Copy Extension Files

See section 1.1, above.

### 2.2) Modify Core Magento Files

CyberCache extension does not require a class wrapper added to Magento core
since its data-handling methods do not throw exceptions that would need to be
caught (which is the case with `ConcurrentConnectionsExceededException` thrown
by Redis extension, for example). Still, some changes have to be made.

> **NOTE**: all code change instructions imply Magento **2.1.7**. Line numbers
> may differ for other versions.

#### 2.2.1) Add Session Saver Constant

Modify `vendor/Magento/Framework/Config/ConfigOptionsListConstants.php`: add
`const SESSION_SAVE_CYBERCACHE = 'cybercache';` on line 60.

#### 2.2.2) Make Magento Recognize CyberCache as "User" Handler

Modify `vendor/Magento/Framework/Session/SaveHandler.php`: change line 154 from
`if ($saveHandler === 'db' || $saveHandler === 'redis') {` to
`if ($saveHandler === 'db' || $saveHandler === 'redis' || $saveHandler === 'cybercache') {`.

### 2.3) Modify `composer.json`

#### 2.3.1) Add "psr-4" Record

Add the following line to "autoload":"psr-4" section of the `composer.json`
located in Magento installation folder:

    "C3\\": "app/code/CyberHULL/CyberCache/src/C3"

Individual entries in the "psr-4" section has to be separated with commas.

#### 2.3.2) Add "psr-0" Record

Add the following line to "autoload":"psr-0" section of the `composer.json`
located in Magento installation folder:

    "C3\\Session\\CyberCacheSession": "app/code/CyberHULL/CyberCache/src/"

Individual entries in the "psr-0" section has to be separated with commas.

### 2.4) Update Autoload Map

Run the following command in the Magento root directory (as `magento` user!):

    composer dump-autoload

See section 1.3, above, on how to install/use the composer.

Whether update was successful can be checked by searching
`autoload_classmap.php` and `autoload_namespaces.php` files in
`vendor/composer` directory for the string `CyberCacheSession`; additionally,
`CyberCache` string has to be present in the `autoload_psr4.php` file (in the
same directory).

### 2.5) Configure Dependencies

#### 2.5.1) Modify `app/etc/di.xml`

Modify array of handlers in `app/etc/di.xml` by adding `cybercache` (the last
`<item>` line in the below snippet):

    <type name="Magento\Framework\Session\SaveHandlerFactory">
        <arguments>
            <argument name="handlers" xsi:type="array">
                <item name="db" xsi:type="string">Magento\Framework\Session\SaveHandler\DbTable</item>
                <item name="redis" xsi:type="string">Magento\Framework\Session\SaveHandler\Redis</item>
                <item name="cybercache" xsi:type="string">C3\Session\CyberCacheSession</item>
            </argument>
        </arguments>
    </type>

#### 2.5.1) Modify `vendor/magento/magento2-base/app/etc`

Modify array of handlers in `vendor/magento/magento2-base/app/etc` by adding
`cybercache` (the last `<item>` line in the below snippet):

    <type name="Magento\Framework\Session\SaveHandlerFactory">
        <arguments>
            <argument name="handlers" xsi:type="array">
                <item name="db" xsi:type="string">Magento\Framework\Session\SaveHandler\DbTable</item>
                <item name="redis" xsi:type="string">Magento\Framework\Session\SaveHandler\Redis</item>
                <item name="cybercache" xsi:type="string">C3\Session\CyberCacheSession</item>
            </argument>
        </arguments>
    </type>

### 2.6) Re-compile DI Files

In Magento installation folder, execute the following command (as `magento`
user!):

    bin/magento setup:di:compile

Compilation should end with the `Generated code and dependency injection
configuration successfully.` message.

### 2.7) Configure Magento Environment

Default Magento environment configuration located in `app/etc/env.php` contains
the followin entry:

    'session' => 
       array (
         'save' => 'files',
       ),

To configure CyberCache-based session store, the `session` section in
`app/etc/env.php` has to be changed to the following:

    'session' => 
       array (
       'save' => 'cybercache', // replaces 'files'
       'cybercache' => 
         array ( // missing entries will get default values (the array might as well be empty)
            'address' => '127.0.0.1',   // connection address; might be an internet address, or an IP
            'port' => '8120',           // connection port
            'persistent' => 'true',     // whether to use persistent connections (must match server settings)
            'hasher' => 'murmurhash2',  // hash method to use for passwords
            'admin' => '',              // password for administrative commands
            'user' => '',               // password for user-level commands
            'compressor' => 'snappy',   // *initial* compressor for various data buffers
            'threshold' => '2048',      // minimal size of the buffer to compress
            'marker' => 'true',         // whether to use integrity check marker
            'magento_lifetime' => 'all' // whether to honor lifetimes set in Magento
        )
    ),

After the above seven steps, Magento 2 will run with `CyberCache` being its
session store.
