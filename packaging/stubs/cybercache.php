<?php
/**
 * CyberCache Cluster PHP API definition:
 *   constants and functions exported by `cybercache` PHP extension.
 * Automatically generated by `generate_api_stub` utility by Vadim Sytnikov.
 * Copyright (C) 2019 CyberHULL.
 *
 * The API was architected to match Zend/Magento conventions as closely as possible, so that translation
 * layer in PHP code would be absolutely minimal. For instance, `c3_get_metadatas()` returns `false` (and
 * not an empty array) because that is what `Zend_Cache_Backend_ExtendedInterface::getMetadatas()` does.
 */

/**
 * Global domain ID (denotes what does not pertain to a session or FPC store).
 *
 * An integer. See man entry cybercache(1) for more information.
 */
define('C3_DOMAIN_GLOBAL', 1);

/**
 * ID of the session store.
 *
 * An integer. See man entry cybercache(1) for more information.
 */
define('C3_DOMAIN_SESSION', 2);

/**
 * ID of the FPC (full page cache) store.
 *
 * An integer. See man entry cybercache(1) for more information.
 */
define('C3_DOMAIN_FPC', 4);

/**
 * Comination of all domain IDs supported by the cache.
 *
 * An integer. See man entry cybercache(1) for more information.
 */
define('C3_DOMAIN_ALL', 7);

/**
 * User agent type: an unknown user.
 *
 * An integer. See man entry cybercache(1) for more information.
 */
define('C3_UA_UNKNOWN', 0);

/**
 * User agent type: a known bot.
 *
 * An integer. See man entry cybercache(1) for more information.
 */
define('C3_UA_BOT', 1);

/**
 * User agent type: CyberHULL cache warmer.
 *
 * An integer. See man entry cybercache(1) for more information.
 */
define('C3_UA_WARMER', 2);

/**
 * User agent type: a regular user.
 *
 * An integer. See man entry cybercache(1) for more information.
 */
define('C3_UA_USER', 3);

/**
 * Synchronization mode: none (fully buffered); fastest but least reliable.
 *
 * An integer. See man entry cybercache(1) for more information.
 */
define('C3_SYNC_NONE', 0);

/**
 * Synchronization mode: everything except file/directory meta-data.
 *
 * An integer. See man entry cybercache(1) for more information.
 */
define('C3_SYNC_DATA_ONLY', 1);

/**
 * Synchronization mode: complete; most reliable but slowest.
 *
 * An integer. See man entry cybercache(1) for more information.
 */
define('C3_SYNC_FULL', 2);

/**
 * Creates session resource; does NOT connect to the server.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param array $options Array with one or more of the following keys: `user`, `admin`, `address`, `port`, `marker`, `hasher`; all values MUST be strings. Optional; default value is NULL.
 *
 * @return resource Handle of the resource to manage session store.
 */
function c3_session(array $options = NULL) {}

/**
 * Loads specified session record.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param string $id Cache record ID.
 * @param integer $request_id Request ID; 0 disables session locking/unlocking, -1 forces use of "real" request ID. Optional; default value is -1.
 *
 * @return string Session record data on success, empty string if record does not exist.
 */
function c3_read($resource, string $id, int $request_id = -1) {}

/**
 * Saves specified session record.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param string $id Cache record ID.
 * @param integer $lifetime Record lifetime, seconds; 0 mean infinite, -1 means use value specified in configuration file.
 * @param string $data Data to be stored in the cache store record.
 * @param integer $request_id Request ID; 0 disables session locking/unlocking, -1 forces use of "real" request ID. Optional; default value is -1.
 *
 * @return boolean `true` on success, `false` on error.
 */
function c3_write($resource, string $id, int $lifetime, string $data, int $request_id = -1) {}

/**
 * Deletes specified session record.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param string $id Cache record ID.
 *
 * @return boolean `true` on success (or if record did not exist), `false` on error.
 */
function c3_destroy($resource, string $id) {}

/**
 * Performs garbage collection in session store.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param integer $seconds How many seconds the record was not updated (to be eligible for purging).
 *
 * @return boolean `true` on success, `false` on errors.
 */
function c3_gc($resource, int $seconds) {}

/**
 * Creates FPC (full page cache) resource; does NOT connect to the server.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param array $options Array with one or more of the following keys: `user`, `admin`, `address`, `port`, `marker`, `hasher`; all values MUST be strings. Optional; default value is NULL.
 *
 * @return resource Handle of the resource to manage FPC store.
 */
function c3_fpc(array $options = NULL) {}

/**
 * Loads specified FPC record.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param string $id Cache record ID.
 *
 * @return string FPC record data on success, empty string if record does not exist.
 */
function c3_load($resource, string $id) {}

/**
 * Checks whether specified FPC record exists.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param string $id Cache record ID.
 *
 * @return mixed Integer last modification timestamp of the record, or `false` if the record does not exist.
 */
function c3_test($resource, string $id) {}

/**
 * Saves specified FPC record.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param string $id Cache record ID.
 * @param integer $lifetime Record lifetime, seconds; 0 mean infinite, -1 means use value specified in configuration file.
 * @param array $tags List of tags specified as a regular array.
 * @param string $data Data to be stored in the cache store record.
 *
 * @return boolean `true` on success, `false` on error.
 */
function c3_save($resource, string $id, int $lifetime, array $tags, string $data) {}

/**
 * Deletes specified FPC record.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param string $id Cache record ID.
 *
 * @return boolean `true` on success (or if record did not exist), `false` on error.
 */
function c3_remove($resource, string $id) {}

/**
 * Deletes specified FPC records.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param string $mode Cleaning mode; one of the following: `all`, `old`, `matchingTag`, `notMatchingTag`, `matchingAnyTag`.
 * @param array $tags List of tags specified as a regular array. Optional; default value is NULL.
 *
 * @return boolean `true` on success, `false` on errors.
 */
function c3_clean($resource, string $mode, array $tags = NULL) {}

/**
 * Retrieves list of IDs of all records in FPC store.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 *
 * @return array Array of strings on success, empty array otherwise.
 */
function c3_get_ids($resource) {}

/**
 * Retrieves list of IDs of all tags in FPC store.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 *
 * @return array Array of strings on success, empty array otherwise.
 */
function c3_get_tags($resource) {}

/**
 * Retrieves list of IDs of FPC records marked with all specified tags.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param array $tags List of tags specified as a regular array.
 *
 * @return array Array of strings on success, empty array otherwise.
 */
function c3_get_ids_matching_tags($resource, array $tags) {}

/**
 * Retrieves list of IDs of FPC records not marked with any of the specified tags.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param array $tags List of tags specified as a regular array.
 *
 * @return array Array of strings on success, empty array otherwise.
 */
function c3_get_ids_not_matching_tags($resource, array $tags) {}

/**
 * Retrieves list of IDs of FPC records marked with any of the specified tags.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param array $tags List of tags specified as a regular array.
 *
 * @return array Array of strings on success, empty array otherwise.
 */
function c3_get_ids_matching_any_tags($resource, array $tags) {}

/**
 * Returns current filling percentage of the FPC store.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 *
 * @return integer Number in 0..100 range; on errors, returns 0.
 */
function c3_get_filling_percentage($resource) {}

/**
 * Fetches metadata of the specified record.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param string $id Cache record ID.
 *
 * @return mixed Array with `expire`, `mtime`, and `tags` keys on success, `false` otherwise.
 */
function c3_get_metadatas($resource, string $id) {}

/**
 * Adds specified number of seconds to the lifetime of the record with given ID.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param string $id Cache record ID.
 * @param integer $extra_lifetime Number of seconds to add to the lifetime of a cache record.
 *
 * @return boolean `true` on success, `false` otherwise.
 */
function c3_touch($resource, string $id, int $extra_lifetime) {}

/**
 * Returns array with capabilities of the cache backend.
 *
 * See man entry cybercache(1) for more information.
 *
 * This method does not take any arguments.
 *
 * @return array Array with `automatic_cleaning`, `tags`, `expired_read`, `priority`, `infinite_lifetime`, and `get_list` keys.
 */
function c3_get_capabilities() {}

/**
 * Sends `PING` command to the server.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 *
 * @return boolean `true` on success, `false` on error.
 */
function c3_ping($resource) {}

/**
 * Sends `CHECK` command to the server`.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 *
 * @return array Array with three `int`s on success, empty array on errors.
 */
function c3_check($resource) {}

/**
 * Retrieves information on specified (set of) domain(s).
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param integer $domain Specifies to which subsystem(s) the command applies; a combination of `C3_DOMAIN_xxx` constants. Optional; default value is C3_DOMAIN_ALL.
 *
 * @return array Array with info strings on success, empty array on errors.
 */
function c3_info($resource, int $domain = C3_DOMAIN_ALL) {}

/**
 * Retrieves performance counters for specified domain(s) matching mask; requires instrumented server.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param integer $domain Specifies to which subsystem(s) the command applies; a combination of `C3_DOMAIN_xxx` constants. Optional; default value is C3_DOMAIN_ALL.
 * @param string $name_mask Mask of names of performance counters whose values are to be retrieved. Optional; default value is "*".
 *
 * @return array Array with performance info strings on success, empty array on errors.
 */
function c3_stats($resource, int $domain = C3_DOMAIN_ALL, string $name_mask = "*") {}

/**
 * Initiates server shutdown procedure.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 *
 * @return boolean `true` on success, `false` on errors.
 */
function c3_shutdown($resource) {}

/**
 * Loads local configuration file and sends it to the server.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param string $path Linux file path; please see documentation as to how exactly it is interpreted.
 *
 * @return boolean `true` on success, `false` on errors.
 */
function c3_local_config($resource, string $path) {}

/**
 * Instructs server to load remote configuration file.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param string $path Linux file path; please see documentation as to how exactly it is interpreted.
 *
 * @return boolean `true` on success, `false` on errors.
 */
function c3_remote_config($resource, string $path) {}

/**
 * Instructs server to load and play back binlog file.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param string $path Linux file path; please see documentation as to how exactly it is interpreted.
 *
 * @return boolean `true` on success, `false` on errors.
 */
function c3_restore($resource, string $path) {}

/**
 * Stores specified cache records into database file.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param integer $domain Specifies to which subsystem(s) the command applies; a combination of `C3_DOMAIN_xxx` constants.
 * @param string $path Linux file path; please see documentation as to how exactly it is interpreted.
 * @param integer $user_agent Specifies "least important" user agent whose records to include; a `C3_UA_xxx` constant. Optional; default value is C3_UA_UNKNOWN.
 * @param integer $sync_mode What synchronization mode to use for writing files; a `C3_SYNC_xxx` constant. Optional; default value is C3_SYNC_NONE.
 *
 * @return boolean `true` on success, `false` on errors.
 */
function c3_store($resource, int $domain, string $path, int $user_agent = C3_UA_UNKNOWN, int $sync_mode = C3_SYNC_NONE) {}

/**
 * Fetches current value of server configuration option(s).
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param string $option_names Masks of names of the server options whose values are to be retrieved. One or more values can be specified.
 *
 * @return array Array of string values on success, empty array on errors.
 */
function c3_get($resource, string ...$option_names) {}

/**
 * Sets server configuration option to a new value.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param string $option_name Name of the server option whose value is to be set.
 * @param string $option_value New value of the server option.
 *
 * @return boolean `true` on success, `false` on errors.
 */
function c3_set($resource, string $option_name, string $option_value) {}

/**
 * Instructs server to log provided string to its log file.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param string $message Message to write to the server log file.
 *
 * @return boolean `true` on success, `false` on errors.
 */
function c3_log($resource, string $message) {}

/**
 * Forces log or binlog rotation.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 * @param integer $domain Specifies to which subsystem(s) the command applies; a combination of `C3_DOMAIN_xxx` constants. Optional; default value is C3_DOMAIN_GLOBAL.
 *
 * @return boolean `true` on success, `false` on errors.
 */
function c3_rotate($resource, int $domain = C3_DOMAIN_GLOBAL) {}

/**
 * Fetches last error message returned by the server, if any.
 *
 * See man entry cybercache(1) for more information.
 *
 * @param resource $resource CyberCache resource handle returned by `c3_session()` or `c3_fpc()` call.
 *
 * @return string String with error message, empty string is there was no error.
 */
function c3_get_last_error($resource) {}
