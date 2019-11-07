
Required APIs
=============

The following interfaces has to be implemented by Magento extension providing 
session storage and full page cache functionality.

Session Cache
-------------

The only interface required to cache session data is
`SessionHandlerInterface`, which part of PHP core handling session storage,
see [PHP docs](http://php.net/manual/en/class.sessionhandlerinterface.php):

    SessionHandlerInterface {
      /**
       * Re-initialize existing session, or creates a new one. Called when a 
       * session starts or when session_start() is invoked.
       *
       * @param save_path The path where to store/retrieve the session.
       * @param name The session name.
       *
       * @return TRUE on success, FALSE on failure. Note that this value is
       *   returned internally to PHP for processing.
       */
      bool open(string $save_path, string $name)

      /**
       * Reads the session data from the session storage, and returns the
       * results. Called right after the session starts or when
       * session_start() is called. Please note that before this method is
       * called SessionHandlerInterface::open() is invoked.
       *
       * This method is called by PHP itself when the session is started.
       * This method should retrieve the session data from storage by the
       * session ID provided. The string returned by this method must be in
       * the same serialized format as when originally passed to the
       * SessionHandlerInterface::write() If the record was not found,
       * return an empty string.
       *
       * The data returned by this method will be decoded internally by PHP
       * using the unserialization method specified in
       * session.serialize_handler. The resulting data will be used to
       * populate the $_SESSION superglobal.
       *
       * Note that the serialization scheme is not the same as
       * unserialize() and can be accessed by session_decode().
       *
       * @param session_id The session id.
       *
       * @return Returns an encoded string of the read data. If nothing was 
       *   read, it must return an empty string. Note that this value is
       *   returned internally to PHP for processing.
       */
      string read(string $session_id)

      /**
       * Writes the session data to the session storage. Called by
       * session_write_close(), when session_register_shutdown() fails, or
       * during a normal shutdown. Note: SessionHandlerInterface::close()
       * is called immediately after this function.
       *
       * PHP will call this method when the session is ready to be saved
       * and closed. It encodes the session data from the $_SESSION
       * superglobal to a serialized string and passes this along with the
       * session ID to this method for storage. The serialization method
       * used is specified in the session.serialize_handler setting.
       *
       * Note this method is normally called by PHP after the output
       * buffers have been closed unless explicitly called by
       * session_write_close()
       *
       * @param session_id The session id.
       * @param session_data The encoded session data. This data is the result
       *   of the PHP internally encoding the $_SESSION superglobal to a
       *   serialized string and passing it as this parameter. Please note
       *   sessions use an alternative serialization method.
       *
       * @return TRUE on success, FALSE on failure. Note that this value is
       *   returned internally to PHP for processing.
       */
      bool write(string $session_id, string $session_data)

      /**
       * Destroys a session. Called by session_regenerate_id() (with $destroy = 
       * TRUE), session_destroy() and when session_decode() fails.
       *
       * @param session_id The session ID being destroyed.
       *
       * @return TRUE on success, FALSE on failure. Note that this value is
       *   returned internally to PHP for processing.
       */
      bool destroy(string $session_id)

      /**
       * Cleans up expired sessions. Called by session_start(), based on 
       * session.gc_divisor, session.gc_probability and session.gc_lifetime 
       * settings.
       *
       * @param maxlifetime Sessions that have not updated for the last
       *   maxlifetime seconds will be removed.
       *
       * @return TRUE on success, FALSE on failure. Note that this value is
       *   returned internally to PHP for processing.
       */
      bool gc(int $maxlifetime)

      /**
       * Closes the current session. This function is automatically executed 
       * when closing the session, or explicitly via session_write_close().
       *
       * @return TRUE on success, FALSE on failure. Note that this value is
       *   returned internally to PHP for processing.
       */
      bool close(void)
    }

Redis-based implementation can be found in 
`vendor/colinmollenhour/php-redis-session-abstract/src/Cm/RedisSession/Handler.php`. 
It has the following features:

- `open($savePath, $sessionName)`: does nothing, just returns `true`; `$sessionName` is *ignored*,
  connection is established in `__construct()`,
- `gc($maxLifeTime)`: does nothing; implementation completely relies on auto-expiration.

Full Page Cache
---------------

In order to support full page cache, Magento extension class should:

1. Be derived from `Zend_Cache_Backend`,
2. Implement `Zend_Cache_Backend_ExtendedInterface` (which is derived from `Zend_Cache_Backend_Interface`),
3. Implement `void ___expire($id)` method in order to pass unit tests; the method must unconditionally
   delete record with passed ID.

Redis-based implementation, the `Cm_Cache_Backend_Redis` class, can be found in 
`lib/internal/Cm/Cache/Backend/Redis.php`.

### `Zend_Cache_Backend` ###

Zend framework class defining core cache functionality definition can be
found in `vendor/magento/zendframework1/library/Zend/Cache/Backend.php`, and 
documentation is [available here](http://framework.zend.com/apidoc/1.7/Zend_Cache/Zend_Cache_Backend/Zend_Cache_Backend.html).

*Note*: `setDirectives()` method is also defined in the
`Zend_Cache_Backend_Interface` interface.

    class Zend_Cache_Backend
    {
      /**
       * Frontend or Core directives.
       *
       * @var array directives
       */
      protected $_directives = array(
          'lifetime' => 3600,  // cache lifetime (in seconds); if null, the cache is valid forever
          'logging'  => false, // if set to true, a logging is activated through Zend_Log
          'logger'   => null
      );

      /**
       * Available options.
       *
       * @var array available options
       */
      protected $_options = array();

      /**
       * Constructor.
       *
       * @param array $options Associative array of options
       */
      public function __construct(array $options = array())

      /**
       * Set the frontend directives; ONLY sets directives if respective keys 
       * already exist in the `_directives` array.
       *
       * @param array $directives Associative array of directives
       * @throws Zend_Cache_Exception
       * @return void
       */
      public function setDirectives($directives)

      /**
       * Set an option; ONLY sets an option if respective key already 
       * exists in the `_options` array.
       *
       * @param string $name Option name
       * @param mixed $value Option value
       * @throws Zend_Cache_Exception
       * @return void
       */
      public function setOption($name, $value)

      /**
       * Returns an option; if option with specified name is not [yet] defined, 
       * then it tries to retrieve directive with that name.
       *
       * @param string $name Optional, the options name to return
       * @throws Zend_Cache_Exceptions
       * @return mixed
       */
      public function getOption($name)

      /**
       * Get the life time.
       *
       * if $specificLifetime is not false, the given specific life time is used
       * else, the global lifetime is used
       *
       * @param int $specificLifetime
       * @return int Cache life time
       */
      public function getLifetime($specificLifetime)

      /**
       * Return true if the automatic cleaning is available for the backend
       *
       * DEPRECATED : use getCapabilities() instead
       *
       * @deprecated
       * @return boolean
       */
      public function isAutomaticCleaningAvailable()

      /**
       * Determine system TMP directory and detect if we have read access.
       *
       * inspired from Zend_File_Transfer_Adapter_Abstract.
       *
       * @return string
       * @throws Zend_Cache_Exception if unable to determine directory
       */
      public function getTmpDir()

      /**
       * Verify if the given temporary directory is readable and writable.
       *
       * @param string $dir temporary directory
       * @return boolean true if the directory is ok
       */
      protected function _isGoodTmpDir($dir)

      /**
       * Make sure if we enable logging that the Zend_Log class is available.
       * Create a default log object if none is set.
       *
       * @throws Zend_Cache_Exception
       * @return void
       */
      protected function _loggerSanity()

      /**
       * Log a message at the WARN (4) priority.
       *
       * @param string $message Message to log
       * @param int $priority Logging level
       * @return void
       */
      protected function _log($message, $priority = 4)
    }

Redis-based implementation has the following features:

- `__construct(array $options = array())`: does NOT call parent ctor; expects extra options, like
  `server` or `port`, to be passed in the argument array,
- `setDirectives($directives)`: calls parent implementation; checks lifetime and throws an
  exception if that is exceeded.
- `isAutomaticCleaningAvailable()`: overwritten and still returns `true` (i.e. what the implementation
  in the parent class does anyway).
- all other methods are not overwritten, and fall back to implementations
  in the the parent `Zend_Cache_Backend` class.

### `Zend_Cache_Backend_Interface` ###

Basic Zend interface for backend cache, see definition in
`vendor/magento/zendframework1/library/Zend/Cache/Backend/Interface.php`;
documentation can be found
[here](http://framework.zend.com/apidoc/1.7/Zend_Cache/Zend_Cache_Backend/Zend_Cache_Backend_Interface.html).

*Note*: `setDirectives()` method is also defined in the
`Zend_Cache_Backend` class.

    Zend_Cache_Backend_Interface {
      /**
       * Load value with given id from cache.
       *
       * @param string $id Cache id
       * @param boolean $doNotTestCacheValidity If set to true, the cache validity won't be tested
       * @return bool|string FALSE if record with fiven ID is not found
       */
      string|false load(string $id, [bool $doNotTestCacheValidity = false])

      /**
       * Test if a cache is available or not (for the given id).
       *
       * @param  string $id Cache id
       * @return bool|int False if record is not available or "last modified"
       *   timestamp of the available cache record
       */
      mixed|false test(string $id)

      /**
       * Save some string datas into a cache record.
       *
       * NOTE: $data is always a "string" (serialization is done by the core,
       * not by the backend)
       *
       * @param string $data Datas to cache
       * @param string $id Cache id
       * @param array  $tags Array of strings (not an associative array), the
       *   cache record will be tagged by each string entry
       * @param bool|int $specificLifetime If != false, set a specific lifetime
       *   for this cache record (null => infinite lifetime)
       * @return boolean True if no problem
       */
      bool save(string $data, string $id, [array $tags = array()], [int $specificLifetime = false])

      /**
       * Remove a cache record.
       *
       * @param string $id Cache id
       * @return boolean True if no problem
       */
      bool remove(string $id)

      /**
       * Clean some cache records. Available modes are:
       *
       * - Zend_Cache::CLEANING_MODE_ALL (default): remove all cache entries ($tags is not used)
       * - Zend_Cache::CLEANING_MODE_OLD: remove too old cache entries ($tags is not used)
       * - Zend_Cache::CLEANING_MODE_MATCHING_TAG: remove cache entries matching all given tags
       *   ($tags can be an array of strings or a single string)
       * - Zend_Cache::CLEANING_MODE_NOT_MATCHING_TAG: remove cache entries not {matching one of
       *   the given tags} ($tags can be an array of strings or a single string)
       * - Zend_Cache::CLEANING_MODE_MATCHING_ANY_TAG => remove cache entries matching any given
       *   tags ($tags can be an array of strings or a single string)
       *
       * @param string $mode Clean mode
       * @param array $tags Array of tags
       * @throws Zend_Cache_Exception
       * @return boolean True if no problem
       */
      bool clean([string $mode = Zend_Cache::CLEANING_MODE_ALL], [array $tags = array()])

      /**
       * Set the frontend directives.
       *
       * @param  array $directives Associative array of directives
       * @throws Zend_Cache_Exception
       * @return void
       */
      void setDirectives(array $directives)
    }

Redis-based implementation has the following features:

- `load($id, $doNotTestCacheValidity = false)`: $doNotTestCacheValidity argument is ignored,
- `clean(Zend_Cache::CLEANING_MODE_OLD)`: even if list of all IDs is maintained, it does use it to
  clean entries not matching any tags,
- `setDirectives($directives)`: calls parent implementation; checks
  currently set record lifetime and throws an exception if that is exceeded.

### `Zend_Cache_Backend_ExtendedInterface` ###

Zend interface for backend cache that extends
`Zend_Cache_Backend_Interface`, see definition in
`vendor/magento/zendframework1/library/Zend/Cache/Backend/ExtendedInterface.php`;
documentation can be found
[here](http://framework.zend.com/apidoc/1.7/Zend_Cache/Zend_Cache_Backend/Zend_Cache_Backend_ExtendedInterface.html).

    Zend_Cache_Backend_ExtendedInterface {
      /**
       * Return an array of stored cache ids.
       *
       * @return array array of stored cache ids (string)
       */
      array getIds()

      /**
       * Return an array of stored tags.
       *
       * @return array array of stored tags (string)
       */
      array getTags()

      /**
       * Return an array of stored cache ids which match given tags.
       *
       * In case of multiple tags, a logical AND is made between tags.
       *
       * @param array $tags array of tags
       * @return array array of matching cache ids (string)
       */
      array getIdsMatchingTags([array $tags = array()])

      /**
       * Return an array of stored cache ids which don't match given tags.
       *
       * In case of multiple tags, a negated logical AND is made between tags.
       *
       * @param array $tags array of tags
       * @return array array of not matching cache ids (string)
       */
      array getIdsNotMatchingTags([array $tags = array()])

      /**
       * Return an array of stored cache ids which match any given tags.
       *
       * In case of multiple tags, a logical OR is made between tags.
       *
       * @param array $tags array of tags
       * @return array array of any matching cache ids (string)
       */
      array getIdsMatchingAnyTags([array $tags = array()])

      /**
       * Return the filling percentage of the backend storage.
       *
       * @throws Zend_Cache_Exception
       * @return int integer between 0 and 100
       */
      int getFillingPercentage()

      /**
       * Return an array of metadatas for the given cache id.
       *
       * The array must include these keys :
       * - expire : the expire timestamp
       * - tags : a string array of tags
       * - mtime : timestamp of last modification time
       *
       * @param string $id cache id
       * @return array array of metadatas (false if the cache id is not found)
       */
      array getMetadatas(string $id)

      /**
       * Give (if possible) an extra lifetime to the given cache id.
       *
       * @param string $id cache id
       * @param int $extraLifetime Extra time, in seconds
       * @return boolean true if ok
       */
      bool touch(string $id, int $extraLifetime)

      /**
       * Return an associative array of capabilities (booleans) of the backend.
       *
       * The array must include boolean values with these keys:
       * - automatic_cleaning (is automating cleaning necessary)
       * - tags (are tags supported)
       * - expired_read (is it possible to read expired cache records (for
       *   doNotTestCacheValidity option for example))
       * - priority does the backend deal with priority when saving
       * - infinite_lifetime (is infinite lifetime can work with this backend)
       * - get_list (is it possible to get the list of cache ids and the complete list of tags)
       *
       * @return array associative array with boolean values of capabilities
       */
      array getCapabilities()
    }

Redis-based implementation has the following features:

- `getIds()`: if list (set) of all IDs is not kept, uses costly Redis wildcard search,
- `getFillingPercentage()`: returns filling percentage of entire Redis server, 
  not distinguishing session cache and FPC.
- `touch($id, $extraLifetime)`: does not check if `touch($id, $extraLifetime)` 
  causes overflow, or if it exceeds maximum allowed lifetime,
- `getCapabilities()`: returns 'expired_read' and 'priority' as `false`.
