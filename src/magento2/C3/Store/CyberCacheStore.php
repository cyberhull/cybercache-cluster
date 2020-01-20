<?php
/**
 * CyberCache Cluster - Magento 2 Extension
 * Written by Vadim Sytnikov.
 * Copyright (C) 2016-2019 CyberHULL, Ltd.
 * All rights reserved.
 * ----------------------------------------------------------------------------
 *
 * Implementation of FPC/default Magento 2 cache store based on CyberCache Cluster.
 */

namespace CyberHULL\CyberCache\C3\Store;

/**
 * Class implementing Zend's `Zend_Cache_Backend_Interface` and
 * `Zend_Cache_Backend_ExtendedInterface` interfaces (and thus capable of being
 * used as both default and full page Magento 2 caches) using methods
 * available in the `cybercache` PHP extension (which MUST be loaded).
 *
 * @copyright Copyright (c) 2019 CyberHULL (http://www.cyberhull.com)
 * @author Vadim Sytnikov
 */
class CyberCacheStore extends \Zend_Cache_Backend implements \Zend_Cache_Backend_ExtendedInterface
{
    /**
     * @var $c3_resource resource Handle of the resource that serves as container for CyberCache options.
     */
    private $c3_resource;

    /**
     * Class constructor.
     *
     * The argument array comes from either 'cache' => 'frontend', or 'cache' =>
     * 'back_end' section of the option array in `app/etc/env.php`.
     *
     * @param array $options Array of options
     * @throws \Exception If CyberCache FPC resource cannot be initialized (e.g. because of wrong option value)
     */
    public function __construct($options = array())
    {
        parent::__construct($options);
        $this->c3_resource = c3_fpc($options);
    }

    /**
     * Load and return cached data by ID
     *
     * @param string $id Record ID
     * @param boolean $doNotTestCacheValidity Ignored
     * @return bool|string Cached data, or `false` if the record does not exist
     * @SuppressWarnings(PHPMD.UnusedFormalParameter)
     */
    public function load($id, $doNotTestCacheValidity = false)
    {
        return c3_load($this->c3_resource, $id);
    }

    /**
     * Test if cache record with given ID exists
     *
     * @param string $id Record id
     * @return bool|int "Last modified" timestamp if record exists, `false` otherwise
     */
    public function test($id)
    {
        return c3_test($this->c3_resource, $id);
    }

    /**
     * Store new record, or update an existing one.
     *
     * @param string $data Record data
     * @param string $id Record ID
     * @param array $tags Array of tags, or a single string; the record will be tagged with each tag
     * @param  bool|int $lifetime Record TTL, an integer; `null` means "infinite", `false` means "default"
     * @return boolean `true` on success
     */
    public function save($data, $id, $tags = array(), $lifetime = false)
    {
        return c3_save($this->c3_resource, $id, $lifetime, $tags, $data);
    }

    /**
     * Delete cache record specified by its ID
     *
     * @param string $id Record ID
     * @return boolean `true` on success
     */
    public function remove($id)
    {
        return c3_remove($this->c3_resource, $id);
    }

    /**
     * Delete specified cache records.
     *
     * @param string $mode Cleaning mode; valid modes are:
     *   'all' (default)  : remove all cache entries (tags are unused)
     *   'old'            : remove expired cache records (tags are unused)
     *   'matchingTag'    : remove entries marked with all specified tags
     *   'notMatchingTag' : remove entries *not* marked with all specified tags
     *   'matchingAnyTag' : remove entries marked with at least one of the specified tags
     * @param array $tags Optional array of tags (only needed for some modes)
     * @return boolean `true` on success
     */
    public function clean($mode = \Zend_Cache::CLEANING_MODE_ALL, $tags = array())
    {
        return c3_clean($this->c3_resource, $mode, $tags);
    }

    /**
     * Return true if backend supports automatic cleaning
     *
     * @return boolean Always `true`
     */
    public function isAutomaticCleaningAvailable()
    {
        return TRUE;
    }

    /**
     * Set the frontend directives (does nothing by itself; calls parent implementation)
     *
     * @param array $directives Associative array of directives
     * @return void
     */
    public function setDirectives($directives)
    {
        parent::setDirectives($directives);
    }

    /**
     * Return an array of stored cache record IDs
     *
     * @return array Array of all stored record IDs
     */
    public function getIds()
    {
        return c3_get_ids($this->c3_resource);
    }

    /**
     * Return an array of all used (marking at least one record) tags
     *
     * @return array Array of stored record tags
     */
    public function getTags()
    {
        return c3_get_tags($this->c3_resource);
    }

    /**
     * Return an array of stored record IDs marked with all specified tags. If
     * specified list of tags is empty, returns empty array immediately without
     * calling CyberCache server.
     *
     * @param array $tags Array of tags
     * @return array Array of matching record IDs
     */
    public function getIdsMatchingTags($tags = array())
    {
        return c3_get_ids_matching_tags($this->c3_resource, $tags);
    }

    /**
     * Return an array of stored record IDs that are not marked with all
     * specified tags. If tag array is empty, server will return array of IDs of
     * all stored records.
     *
     * @param array $tags Array of tags
     * @return array Array IDs that are not marked with all specified tags
     */
    public function getIdsNotMatchingTags($tags = array())
    {
        return c3_get_ids_not_matching_tags($this->c3_resource, $tags);
    }

    /**
     * Return an array of stored record IDs marked with any of the specified
     * tags. If specified list of tags is empty, returns empty array immediately
     * without calling CyberCache server.
     *
     * @param array $tags Array of tags
     * @return array Array of matching record IDs
     */
    public function getIdsMatchingAnyTags($tags = array())
    {
        return c3_get_ids_matching_any_tags($this->c3_resource, $tags);
    }

    /**
     * Return filling percentage of the FPC server store.
     *
     * @return int An integer in range [0 and 100]
     */
    public function getFillingPercentage()
    {
        return c3_get_filling_percentage($this->c3_resource);
    }

    /**
     * Return an array of metadata for the given record ID; the array will have
     * the following keys:
     *
     * - `expire` : Expiration timestamp (max possible timestamp if lifetime is infinite)
     * - `mtime` : Last modification timestamp
     * - `tags` : Array of tags with which the record is marked
     *
     * @param string $id Record ID
     * @return array Array with record metadata, or `false` if the record with diven ID does not exist
     */
    public function getMetadatas($id)
    {
        return c3_get_metadatas($this->c3_resource, $id);
    }

    /**
     * Add specified number of seconds to the lifetime of the record with given ID
     *
     * @param string $id Record ID
     * @param int $extraLifetime Number of seconds to add to record's lifetime
     * @return boolean `true` on success
     */
    public function touch($id, $extraLifetime)
    {
        return c3_touch($this->c3_resource, $id, $extraLifetime);
    }

    /**
     * Return capabilities of the backend as associative array of boolean flags;
     * contains the following entries:
     *
     * - 'automatic_cleaning' : Whether automating cleaning is necessary
     * - 'tags' : Whether tags are supported
     * - 'expired_read' : Whether expired records can be read
     * - 'priority' : Whether priority is supported when saving
     * - 'infinite_lifetime' : Whether records can have infinite lifetimes
     * - 'get_list' : Is it possible to get the list of record IDs and the complete list of tags
     *
     * @return array Associative array of boolean flags
     */
    public function getCapabilities()
    {
        return c3_get_capabilities();
    }
}
