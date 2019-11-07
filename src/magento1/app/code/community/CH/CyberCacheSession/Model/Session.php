<?php
/**
 * CyberCache Cluster - Magento 1 Extension
 * Written by Vadim Sytnikov.
 * Copyright (C) 2016-2019 CyberHULL. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * ----------------------------------------------------------------------------
 *
 * Implementation of session store based on CyberCache Cluster.
 */

/**
 * Class overriding Magento 1 class `Mage_Core_Model_Mysql4_Session` using methods
 * available in the `cybercache` PHP extension (which MUST be loaded).
 *
 * @copyright Copyright (c) 2019 CyberHULL (http://www.cyberhull.com)
 * @author Vadim Sytnikov
 */
class CH_CyberCacheSession_Model_Session extends Mage_Core_Model_Mysql4_Session
{
    /**
     * @var resource Handle of the resource that serves as container for CyberCache options.
     */
    private $c3_resource;

    /**
     * @var resource Session life time, seconds.
     */
    private $c3_lifetime;

    /**
     * @var string Lifetime management mode: whether to honor Magento internal settings ('none'/'admin'/'all').
     */
    private $c3_magento_lifetime;

    private static function object2array($data)
    {
        if ($data) {
            if (is_object($data)) {
                $result = [];
                foreach ($data as $field => $value) {
                    $result[$field] = $value;
                }
                return $result;
            } else if (is_array($data)) {
                return $data;
            }
        }
        return [];
    }

    /**
     * Constructor.
     */
    public function __construct()
    {
        $options = self::object2array(Mage::getConfig()->getNode('global/c3_session'));
        $this->c3_magento_lifetime = array_key_exists('magento_lifetime', $options)? $options['magento_lifetime']: 'all';
        $this->c3_resource = c3_session($options);
        $this->c3_lifetime = 0;
    }

    /**
     * Check CyberCache initialization status.
     *
     * @return bool
     */
    public function hasConnection()
    {
        return $this->c3_resource != null;
    }

    /**
     * Open session (does nothing; CyberCache does not have a notion of opening a session)
     *
     * @param string $savePath Ignored
     * @param string $sessionName Ignored
     * @return boolean Always `true`
     * @SuppressWarnings(PHPMD.UnusedFormalParameter)
     */
    public function open($savePath, $sessionName)
    {
        return true;
    }

    /**
     * Fetch session record.
     *
     * The record will become locked in that all future reads of the same `$id` by different requests (e.g.
     * AJAX requests building the same page) will be put on hold until `c3_write()` is executed by the
     * current request.
     *
     * Please see description of the `session_lock_wait_time` option in `/etc/cybercache/cybercache.cfg` for
     * detailed description of session locking.
     *
     * @param string $id Session record ID
     * @return string Session data; empty strings if the record does not exist
     */
    public function read($id)
    {
        return c3_read($this->c3_resource, $id);
    }

    /**
     * Create or update session record.
     *
     * Currently, it is, unfortunately, impossible to manage Magento session lifetimes using only
     * server configuration settings. The reason is that, for security reasons, Magento uses different
     * lifetimes for front-end and back-end (admin panel) requests, and the latter are supposed to
     * be significantly shorter than the former. To fetch those settings, we *have* to use PHP code.
     *
     * Whether or not Magento settings are honored is controlled by the 'magento_lifetime' key of the
     * 'session/cybercache' section of the Magento environment configuration (`app/etc/env.php`). If
     * 'magento_lifetime' entry is set to 'all' (the default for when the entry is missing), both front-
     * and back-end session lifetime set in Magento will be honored. If the entry is set to 'admin',
     * only back-end Magento lifetime will be used, but lifetimes for user sessions will be calculated
     * by the server (this is the preferred, potentially most efficient setting). If the entry is 'none',
     * CyberCache extension will defer *all* lifetime calculations to the server.
     *
     * If there was `read()` call within the same request, then this method will release session
     * lock (maintained by CyberCache server) that was acquired during `read()`. Please see description
     * of the `session_lock_wait_time` option in `/etc/cybercache/cybercache.cfg` for detailed description
     * of session locking.
     *
     * @param string $id Session ID
     * @param string $data Session data
     * @return boolean
     */
    public function write($id, $data)
    {
        if ($this->c3_lifetime === 0) {
            $lifetime = -1; // use value calculated by CyberCache by default
            $is_admin = Mage::app()->getStore()->isAdmin();
            if ($this->c3_magento_lifetime == 'all' || $this->c3_magento_lifetime == 'admin' && $is_admin) {
                // the following is functionally equivalent to the code in `Mage_Core_Model_Resource_Session::getLifeTime()`
                $lifetime = (int) Mage::getStoreConfig($is_admin? 'admin/security/session_cookie_lifetime':
                    'web/cookie/cookie_lifetime');
                if ($lifetime < 60) {
                    $lifetime = ini_get('session.gc_maxlifetime');
                    if ($lifetime < 60) {
                        $lifetime = 3600; // one hour
                    }
                }
                // default visibility of class constants even in PHP 7.1 is `public`, so the following works
                if ($lifetime > parent::SEESION_MAX_COOKIE_LIFETIME) {
                    $lifetime = parent::SEESION_MAX_COOKIE_LIFETIME; // 100 years
                }
            }
            $this->c3_lifetime = $lifetime;
        }
        return c3_write($this->c3_resource, $id, $this->c3_lifetime, $data);
    }

    /**
     * Delete session record
     *
     * @param string $id Session ID
     * @return boolean
     */
    public function destroy($id)
    {
        return c3_destroy($this->c3_resource, $id);
    }

    /**
     * Close session (does nothing; CyberCache does not have a notion of closing a session)
     *
     * @return boolean Always `true`
     */
    public function close()
    {
        return true;
    }

    /**
     * Collect garbage: delete all the records not updated during last `$maxLifeTime` seconds
     *
     * @param int $maxLifeTime Number of seconds since the records were last updated
     * @return boolean
     */
    public function gc($maxLifeTime)
    {
        return c3_gc($this->c3_resource, $maxLifeTime);
    }
}
