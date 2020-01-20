<?php
/**
 * CyberCache Cluster - Magento 2 Extension
 * Written by Vadim Sytnikov.
 * Copyright (C) 2016-2019 CyberHULL, Ltd.
 * All rights reserved.
 * ----------------------------------------------------------------------------
 *
 * Implementation of session store based on CyberCache Cluster.
 */

namespace CyberHULL\CyberCache\C3\Session;

/**
 * Class implementing standard PHP's `SessionHandlerInterface` using methods
 * available in the `cybercache` PHP extension (which MUST be loaded).
 *
 * @copyright Copyright (c) 2019 CyberHULL (http://www.cyberhull.com)
 * @author Vadim Sytnikov
 */
class CyberCacheSession implements \SessionHandlerInterface
{
    /**
     * @var resource Handle of the resource that serves as container for CyberCache options.
     */
    private $c3_resource;

    /**
     * @var \Magento\Framework\App\State Application state object, used to differentiate front- and back-end calls.
     */
    private $c3_state;

    /**
     * @var \Magento\Framework\App\Config\ScopeConfigInterface Scope configuration object.
     */
    private $c3_scope;

    /**
     * @var string Lifetime management mode: whether to honor Magento internal settings ('none'/'admin'/'all').
     */
    private $c3_magento_lifetime;

    /**
     * CyberCacheSession constructor.
     *
     * @param \Magento\Framework\App\DeploymentConfig $config Deployment configuration object.
     * @param \Magento\Framework\App\State $state Application state object.
     * @param \Magento\Framework\App\Config\ScopeConfigInterface $scope Scope configuration object.
     * @throws \Exception If CyberCache SESSION resource cannot be initialized (e.g. because of wrong option value)
     */
    public function __construct(\Magento\Framework\App\DeploymentConfig $config,
      \Magento\Framework\App\State $state,
      \Magento\Framework\App\Config\ScopeConfigInterface $scope)
    {
        $this->c3_state = $state;
        $this->c3_scope = $scope;
        $options = $config->get('session/cybercache');
        if (is_array($options)) {
            $this->c3_magento_lifetime = array_key_exists('magento_lifetime', $options)?
                $options['magento_lifetime']: 'all';
            $this->c3_resource = c3_session($options);
        } else {
            throw new \Exception('CyberCache session is not configured properly');
        }
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
        $lifetime = 0;
        if ($this->c3_state->getAreaCode() == \Magento\Framework\App\Area::AREA_ADMINHTML) {
            if ($this->c3_magento_lifetime != `none`) { // `all` or `admin`
                $lifetime = (int) $this->c3_scope->getValue('admin/security/session_lifetime');
            }
        } else {
            if ($this->c3_magento_lifetime == `all`) {
                $lifetime = (int) $this->c3_scope->getValue('web/cookie/cookie_lifetime',
                    Magento\Store\Model\ScopeInterface::SCOPE_STORE);
            }
        }
        return c3_write($this->c3_resource, $id, $lifetime ?: -1, $data);
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
