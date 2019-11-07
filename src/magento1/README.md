
CyberCache Magento 1 Extension
==============================

The extension provides session store and default cache implementation for
Magento 1. It does not provide, however, full page cache functionality: in order
to get FPC support, you have to either use Magento 1 Enterprise Edition, or
install another extension implementing FPC, such as [Amasty Full Page Cache](
https://amasty.com/magento-full-page-cache.html) Magento 1 extension.

1) System Requirements
----------------------

CyberCache Magento extension talks to CyberCache server through specialized PHP
extension developed by CyberHULL and automatically installed along with the
server and various support utilities (console, cache warmer, etc.). That PHP
extension *requires* PHP 7.0, thus for Magento 1 to use CyberCache, Magento 1
store should be using PHP 7.0.x.

It is recommended that you use Magento 1.9.4.0 or later.

2) Installation
---------------

Installation is pretty simple: just copy **all** files found in subdirectories
of this directory (i.e. where this `readme.md` file resides) into respective
subdirecories of the Magento 1 installation; if a subdirectory does not exist in
Magento 1 directory, create it.

That is, copy

    ./app/code/community/CH/CyberCacheSession/etc/config.xml    to
      <magento-1-root>/app/code/community/CH/CyberCacheSession/etc/config.xml

    ./app/code/community/CH/CyberCacheSession/Model/Session.php    to
      <magento-1-root>/app/code/community/CH/CyberCacheSession/Model/Session.php

    ./app/etc/modules/CH_CyberCacheSession.xml    to
      <magento-1-root>/app/etc/modules/CH_CyberCacheSession.xml

    ./lib/CH/Cache/Backend/CyberCache.php    to
      <magento-1-root>/lib/CH/Cache/Backend/CyberCache.php

After all four files are successfully copied, you're ready to configure the
extension.

> **IMPORTANT**: copied files must be readable by Magento code! For instance,
> for `Apache` server to be able to access them, set group owner of each copied
> file and created folder to `www-data`.

3) Configuration
----------------

To configure the extension and enable CyberCache as both session store and
default cache, it is necessary to edit the following file:

    <magento-1-root>/app/etc/local.xml

Add the following sections into the `<global>` section, right after
`<resources>`:

    <!-- start of CyberCache-related options -->
    <session_save>db</session_save>
    <c3_session> <!-- defaults for the options -->
        <address>127.0.0.1</address>     <!-- connection address; might be an internet address, or an IP -->
        <port>8120</port>                <!-- connection port -->
        <persistent>true</persistent>    <!-- whether to use persistent connections (must match server settings) -->
        <hasher>murmurhash2</hasher>     <!-- hash method to use for passwords -->
        <admin><![CDATA[]]></admin>      <!-- password for administrative commands -->
        <user><![CDATA[]]></user>        <!-- password for user-level commands -->
        <compressor>snappy</compressor>  <!-- *initial* compressor for various data buffers -->
        <threshold>2048</threshold>      <!-- minimal size of the buffer to compress -->
        <marker>true</marker>            <!-- whether to use integrity check marker -->
        <magento_lifetime>all<magento_lifetime> <!-- whether to honor lifetimes set in Magento -->
    </c3_session>
    <cache>
      <backend>CH_Cache_Backend_CyberCache</backend>
      <backend_options> <!-- defaults for the options -->
        <address>127.0.0.1</address>     <!-- connection address; might be an internet address, or an IP -->
        <port>8120</port>                <!-- connection port -->
        <persistent>true</persistent>    <!-- whether to use persistent connections (must match server settings) -->
        <hasher>murmurhash2</hasher>     <!-- hash method to use for passwords -->
        <admin><![CDATA[]]></admin>      <!-- password for administrative commands -->
        <user><![CDATA[]]></user>        <!-- password for user-level commands -->
        <compressor>snappy</compressor>  <!-- *initial* compressor for various data buffers -->
        <threshold>2048</threshold>      <!-- minimal size of the buffer to compress -->
        <marker>true</marker>            <!-- whether to use integrity check marker -->
      </backend_options>
    </cache>
    <!-- end of CyberCache-related options -->

If `<session_save>` and/or `<cache>` sections already existed (and chances are
they did), delete them before inserting the above snippet.
