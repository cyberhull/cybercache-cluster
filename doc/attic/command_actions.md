
Handling of Server Commands
===========================

This document describes, in pseudocode, server actions upon receiving each
supported command. Authentication checks are *not* shown. Likewise, "Actions" do
not include handling of "obvious" errors, like insufficient memory, or a spin
lock not being released during max wait time, or a record being persisted or
replicated for too long, etc. Also, all user-level and admin-level commands
return error messages if authorization was needed but the command did not
contain correct hash code of respective password.

If the server enters shutdown mode, all commands continue their normal
processing and would return results as during normal operation; it's just that
new connections will not be accepted.

Also not shown is processing of queues of deleted object by commands that 
acquire write locks on respective domains.

The exact order of actions may not be final; specifically, on some occasions the
order will be changed as it is possible to release locks before, not after
certain actions; such optimizations are not shown, as they would require
introduction of flags that are not essential for the described algorithms.
Likewise, for the same reasons, sometimes actions like adding messages to
optimizer queue are shown to be executed earlier than they should (for instance,
"add" request should only be issued after the object is fully initialized); some
pseudocode may imply creation of temporary response structures, whereas actual
implementation will always try to pump data to the connection directly.

To speed-up processing, many actions first check "deleted" flag of a record
without memory barrier, and without first obtaining spin lock on that record. If
they see that the onbject is not "deleted", only then they grab spin lock and
re-check the flag using memory barrier (ensuring that other threads' writes took
effect). The flag can only be set, but never cleared, so this technique is safe
(e.g. a thread deleting *all* records cannot miss a record).

Administrative Commands
-----------------------

### `SHUTDOWN` ###

Actions:

    set global shutdown flag
    stop accepting new connections
    response = `Ok`
    set thread statuses of all connection threads to "request to quit"
    wait `perf_shutdown_wait` seconds
    if there are still connection threads running or other threads busy:
      log warning message
      wait `perf_shutdown_final_wait` seconds
      if there are still connection threads running or other threads busy:
        log error message for each connection thread that still runs or other thread that is still busy
        response = error messages
    do global heap check
    if there were errors during global heap check:
      set / add error message to the response
    send back response
    exit main()

### `LOADCONFIG` ###

Actions:

    lock shared mutex on `Config` object for writing
    load specified configuration file into the buffer
    parse loaded file and sets configuration options
    if there are errors while parsing the buffer:
      log error message
      response = error message
    else:
      response = `Ok`
    send back response
    unlock shared mutex on `Config` object

### `RESTORE` ###

Actions:

    if specified file exists
      if specified domain is session
        send LOAD FILE message to session binlog loader
        send back `Ok` response
      else if specified domain is FPC
        send LOAD FILE message to FPC binlog loader
        send back `Ok` response
    else
      send back error response

### `GET` ###

Actions:

    result list = empty
    for each specified mask:
      get names of all options that match mask
      for each collected name:
        get option value
        add option name and its value as to items to the result list
    send back list response

### `SET` ###

Actions:

    lock shared mutex on `Config` object for writing
    if specified variable does not exist OR new value is invalid:
      log error message
      response = error message
    else:
      if new value is specified:
        value = new value
      else:
        value = default value
      set variable to value
      response = new value
    send back response
    unlock shared mutex on `Config` object

### `LOG` ###

Actions:

    insert new message into `Log` object's message queue
    send back `Ok`

### `ROTATE` ###

Actions:

    error list = empty
    for each service specified in the argument mask:
      if service is active:
        send "rotate binlog" command to specified service
        if sending command failed:
          add error message to the error list
      else:
        add error message to the error list
    if error list is empty:
      send back `Ok`
    else:
      send back error message

Information Commands
--------------------

### `PING` ###

Actions:

    send back `Ok`

### `CHECK` ###

Actions:

    calculate server load as 100*<busy-connection-threads>/<total-connection-threads>
    send back server load and total number of warnings and errors since server start

### `INFO` ###

Actions:

    if domain argument is missing:
      send back server version, whether it's instrumented, used global memory, etc.
    if domain argument is missing or is `SESSION`:
      lock shared downgradable mutex on session domain for reading
      send back number of session cache entries, fill factor, used memory etc.
      unlock the mutex
    if domain argument is missing or is `FPC`:
      lock shared downgradable mutex on FPC domain for reading
      send back number of FPC cache entries, number of tags, fill factor, used memory etc.
      unlock the mutex
    send back empty list item

### `STATS` ###

Actions:

    if server is not instrumented:
      send back error message
    else:
      if domain argument is missing:
        clculate all "average" global performance counters
        for each global performance counter:
          send back `<counter-name> : <counter-value>`
      if domain argument is missing or is `SESSION`:
        clculate all "average" session performance counters
        for each session performance counter:
          send back `<counter-name> : <counter-value>`
      if domain argument is missing or is `FPC`:
        clculate all "average" FPC performance counters
        for each FPC performance counter:
          send back `<counter-name> : <counter-value>`
      send back empty list item

Session Cache Commands
----------------------

### `READ` ###

Actions:

    calculate hash code for the session key
    calculate index of hash table in session container
    lock shared downgradable mutex on session domain for reading
    find session record by hash code and key
    if the record exists:
      lock the record using spin lock
      if the record is deleted:
        did_not_exist = true
      else:
        if the record has expired
          or eviction mode for session domain is `lru` or `strict-lru`:
            ET = max of current expiration time and current time plus useragent-dependent extra time
            set expiration timestamp to ET
        put "update" request into session optimizer queue
        send back compressed data chunk
      unlock the record using spin lock
    else:
      did_not_exist = true
    unlock shared downgradable mutex on session domain
    if did_not_exist:
      send back `Ok`

### `WRITE` ###

Actions:

    if payload is present and has non-zero length
      calculate hash code for the session key
      calculate index of hash table in session container
      lock shared downgradable mutex on session domain for writing
      find session record by hash code and key
      if the record exists:
        lock the record using spin lock
        downgrade shared downgradable mutex to reading mode on session domain
        while the record is being read-locked:
          wait
        deallocate and set to `null` data buffer
        put "update" request into session optimizer queue
      else:
        create new session record
        insert it into hash table
        insert it into global list of session records
        lock the record using spin lock
        downgrade shared downgradable mutex to reading mode on session domain
        set number of writes to `0`
        put "add" request into session optimizer queue
      set new data buffer, its uncompressed/compressed sizes, and compression method
      set modification timestamp
      if very first write
        set expiration timestamp to `session_first_write_lifetime`
      else if number of writes is less than `session_first_write_num`
        STEP = `(session_default_lifetime - session_first_write_lifetime) / session_first_write_num`
        set expiration to `session_first_write_lifetime + STEP * <number-of_writes>`
      else
        set expiration timestamp to `session_default_lifetime`
      clear all flags
      push record to binlog queue
      unlock the record using spin lock
      unlock shared downgradable mutex on session domain
    else
      delete the record (see `DESTROY` command actions)
    send back `Ok`

### `DESTROY` ###

Actions:

    calculate hash code for the session key
    calculate index of hash table in session container
    lock shared downgradable mutex on session domain for reading
    find session record by hash code and key
    if the record exists and not flagged as "deleted":
      do_delete = false
      lock the record using spin lock
      if "deleted" flag is not set:
        set "deleted" flag
        while the record is being read-locked:
          wait
        deallocate and set to `null` data buffer
        do_delete = true
      unlock the record using spin lock
      if do_delete:
        put "delete" request into session optimizer queue
    unlock shared downgradable mutex on session domain
    send back `Ok`

### `GC` ###

Actions:

    if eviction mode for session domain is not `strict-lru`:
      put "GC" request into session domain optimizer's message queue
    send back `Ok`

Full Page Cache Commands
------------------------

### `LOAD` ###

Actions:

    calculate hash code for the FPC key
    calculate index of hash table in FPC container
    lock shared downgradable mutex on FPC domain for reading
    find FPC record by hash code and key
    if the record exists:
      lock the record using spin lock
      if the record is deleted:
        did_not_exist = true
      else:
        if the record has expired
          or eviction mode for FPC domain is `lru` or `strict-lru`:
            ET = max of current expiration time and current time plus useragent-dependent extra time
            set expiration timestamp to ET
        send back compressed data chunk
        put "update" request into FPC optimizer queue
      unlock the record using spin lock
    else:
      did_not_exist = true
    unlock shared downgradable mutex on FPC domain
    if did_not_exist:
      send back `Ok`

### `TEST` ###

Actions:

    calculate hash code for the FPC key
    calculate index of hash table in FPC container
    lock shared downgradable mutex on FPC domain for reading
    find FPC record by hash code and key
    if the record exists:
      lock the record using spin lock
      if the record is deleted:
        did_not_exist = true
      else:
        if the record has expired
          or eviction mode for FPC domain is not `expiration-lru` or `strict-expiration-lru`:
            ET = max of current expiration time and current time plus useragent-dependent extra time
            set expiration timestamp to ET
        send back last modification timestamp as number chunk
        put "update" request into FPC optimizer queue
      unlock the record using spin lock
    else:
      did_not_exist = true
    unlock shared downgradable mutex on FPC domain
    if did_not_exist:
      send back `Ok`

### `SAVE` ###

Actions:

    if payload exists and has non-zero length
      if lifetime is -1:
        set lifetime to `fpc_default_lifetime`
      else if lifetime is bigger than `fpc_max_lifetime`:
        set lifetime to `fpc_max_lifetime`
      calculate hash code for the FPC key
      calculate index of hash table in FPC container
      lock shared downgradable mutex on FPC domain for writing
      reset thread's vector of tag pointers
      if new record data contains tag references:
        for each tag reference:
          find or create tag and put pointer to it into vector of tag pointers
          increment reference counter of the tag
        sort vector of tag pointers
      else:
        put pointer to `__UNTAGGED__` tag into vector of tag pointers
      find FPC record by hash code and key
      create_new_record = true
      if the record exists:
        lock old record using spin lock
        if "deleted" flag is set:
          unlock old record using spin lock
        else:
          create_new_record = false
          for each tag of the existing record:
            unlink the record from the list of objects for that tag
          while the record is being read-locked:
            wait
          deallocate and set to `null` data buffer
          clear "being-optimized" and "optimized" flags
          if old and new numbers of tags do not match:
            try to shrink/expand record memory block in-place
            if in-place reallocation failed:
              create_new_record = true
          if create_new_record
            set "deleted" flag
            remove old record from the hash table
            remove old record from the global list of FPC records
            unlock old record using spin lock
            put "deallocate" request into FPC optimizer queue
          else:
            for each element in vector of tag pointers:
              initialize tag reference in the record
              link the record into the list of objects for that tag
              decrement reference counter of the tag
            set data buffer and other fields of the record
            unlock old record using spin lock
            put "update" request into FPC optimizer queue
      if create_new_record:    
        allocate new record
        add new record to the hash table
        add new record to the global list of FPC records
        for each element in vector of tag pointers:
          initialize tag reference in the record
          link the record into the list of objects for that tag
          decrement reference counter of the tag
        set data buffer and other fields of the record
        put "add" request into FPC optimizer queue
      push record to binlog queue
      unlock shared downgradable mutex on FPC domain
    else
      delete the record (see `REMOVE` command actions)
    send back `Ok`

### `REMOVE` ###

Actions:

    calculate hash code for the FPC key
    calculate index of hash table in FPC container
    lock shared downgradable mutex on FPC domain for reading
    find FPC record by hash code and key
    if the record exists and not flagged as "deleted":
      do_delete = false
      lock the record using spin lock
      if "deleted" flag is not set:
        set "deleted" flag
        if the record is not read-locked:
          deallocate and set to `null` data buffer
        do_delete = true
      unlock the record using spin lock
      if do_delete:
        put "delete" request into FPC optimizer queue
    unlock shared downgradable mutex on FPC domain
    send back `Ok`

### `CLEAN` ###

Below, `DELETE_THE_RECORD` means the following sequence of actions:

    if "deleted" flag is not set:
      lock the record using spin lock
      if "deleted" flag is not set:
        set "deleted" flag
        if the record is not read-locked:
          deallocate and set to `null` data buffer
        put "delete" request into FPC optimizer queue

Actions:

    if method is `old`:
      if `fpc_eviction_mode` is not `strict-lru`
        put "gc" request into FPC optimizer queue
    else:
      lock shared downgradable mutex on FPC domain for reading

      if method is `all`:
        for each object in the global list of FPC records:
          DELETE_THE_RECORD

      if method is `matchall`:
        reset thread's tag vector
        for each tag in the request:
          get reference to the tag
          if the tag does not exist:
            unlock shared downgradable mutex on FPC domain
            send back `Ok`
            quit
          if this tag has least number of records tagged with it so far:
            remember this tag
          put reference to the tag into tag vector
        if there are no tags in tag vector:
          unlock shared downgradable mutex on FPC domain
          send back `Ok`
          quit
        else if there is only one tag in the request:
          for each record marked with that tag:
            DELETE_THE_RECORD
        else:
          for each record marked with tag having least number of records:
            number_of_matches = 0
            for each tag reference of the record:
              if referenced tag is in the tag vector:
                increment number_of_matches
            if number_of_matches equals number of elements in tag vector:
              DELETE_THE_RECORD
      
      if method is `matchnot`:
        reset thread's tag vector
        for each tag in the request:
          get reference to the tag
          if the tag exists:
            put reference to the tag into tag vector
        if there are no tags in tag vector:
          for each record in global list of FPC records:
            DELETE_THE_RECORD
        else:
          for each record in global list of FPC records:
            number_of_matches = 0
            for each tag reference of the record:
              if tag reference is in the tag vector
                increment number_of_matches
            if number_of_matches does not equals number of elements in tag vector:
              DELETE_THE_RECORD

      if method is `matchany`:
        reset thread's tag vector
        for each tag in the request:
          get reference to the tag
          if the tag exists:
            put reference to the tag into tag vector
        if there is at least one tag in tag vector:
          if there is only one tag in tag vector:
            for each record in the list of that tag:
              DELETE_THE_RECORD
          else:
            for each record in global list of FPC records:
              for each tag reference of the record:
                if tag reference is in the tag vector
                  DELETE_THE_RECORD
                  quit the loop

      unlock shared downgradable mutex on FPC domain
    send back `Ok`

### `GETIDS` ###

Actions:

    lock shared downgradable mutex on FPC domain for reading
    for each record in global list of FPC records:
      if the record is not flagged as "deleted":
        send back ID of the record
    unlock shared downgradable mutex on FPC domain
    send back empty list item

### `GETTAGS` ###

Actions:

    lock shared downgradable mutex on FPC domain for reading
    for each tag in global list of FPC tags:
      for each record marked with this tag:
        if the record is not marked as "deleted":
          send back tag name
          quit the loop
    unlock shared downgradable mutex on FPC domain
    send back empty list item

### `GETIDSMATCHINGTAGS` ###

The algorithm selects the same records as those that would be deleted by the
`CLEAN` command in `matchall` mode, so actual implementations should share most
of the code.

Actions:

    lock shared downgradable mutex on FPC domain for reading
    reset thread's tag vector
    for each tag in the request:
      get reference to the tag
      if the tag does not exist:
        unlock shared downgradable mutex on FPC domain
        send back empty list element
        quit
      if this tag has least number of records tagged with it so far:
        remember this tag
      put reference to the tag into tag vector
    if there are tags in tag vector:
      if there is only one tag in the request:
        for each record marked with that tag:
          send back ID of the record
      else:
        for each record marked with tag having least number of records:
          number_of_matches = 0
          for each tag reference of the record:
            if referenced tag is in the tag vector:
              increment number_of_matches
          if number_of_matches equals number of elements in tag vector:
            send back ID of the record
    unlock shared downgradable mutex on FPC domain
    send back empty list element

### `GETIDSNOTMATCHINGTAGS` ###

The algorithm selects the same records as those that would be deleted by the
`CLEAN` command in `matchnot` mode, so actual implementations should share most
of the code.

Actions:

    lock shared downgradable mutex on FPC domain for reading
    reset thread's tag vector
    for each tag in the request:
      get reference to the tag
      if the tag exists:
        put reference to the tag into tag vector
    if there are no tags in tag vector:
      for each record in global list of FPC records:
        send back ID of the record
    else:
      for each record in global list of FPC records:
        number_of_matches = 0
        for each tag reference of the record:
          if tag reference is in the tag vector
            increment number_of_matches
        if number_of_matches does not equals number of elements in tag vector:
          send back ID of the record
    unlock shared downgradable mutex on FPC domain
    send back empty list element

### `GETIDSMATCHINGANYTAGS` ###

The algorithm selects the same records as those that would be deleted by the
`CLEAN` command in `matchany` mode, so actual implementations should share most
of the code.

Actions:

    lock shared downgradable mutex on FPC domain for reading
    reset thread's tag vector
    for each tag in the request:
      get reference to the tag
      if the tag exists:
        put reference to the tag into tag vector
    if there is at least one tag in tag vector:
      if there is only one tag in tag vector:
        for each record in the list of that tag:
          send back ID of the record
      else:
        for each record in global list of FPC records:
          for each tag reference of the record:
            if tag reference is in the tag vector
              send back ID of the record
              quit the loop
    unlock shared downgradable mutex on FPC domain
    send back empty list element

### `GETFILLINGPERCENTAGE` ###

    send back FPC memory allocated, divided by FPC memory quota, times 100

### `GETMETADATAS` ###

Actions:

    calculate hash code for the FPC key
    calculate index of hash table in FPC container
    lock shared downgradable mutex on FPC domain for reading
    find FPC record by hash code and key
    if the record exists:
      lock the record using spin lock
      if the record is deleted:
        did_not_exist = true
      else:
        if the record has expired
          or eviction mode for FPC domain is not `expiration-lru` or `strict-expiration-lru`:
            ET = max of current expiration time and current time plus useragent-dependent extra time
            set expiration timestamp to ET
        send back header of the counted uncompressed list with `<num-of-tags>+2` entries
        send back last expiration time as an uncompressed chunk
        send back last modification time as an uncompressed chunk
        for each tag with which the entry is marked:
          send back tag name
        put "update" request into FPC optimizer queue
      unlock the record using spin lock
    else:
      did_not_exist = true
    unlock shared downgradable mutex on FPC domain
    if did_not_exist:
      send back `Ok`

### `TOUCH` ###

Actions:

    calculate hash code for the FPC key
    calculate index of hash table in FPC container
    lock shared downgradable mutex on FPC domain for reading
    response = `Ok`
    find FPC record by hash code and key
    if the record exists:
      lock the record using spin lock
      if the record is deleted:
        response = error message
      else:
        if the record has expired
          set expiration timestamp to current
        if expiration timestamp plus argument minus current timestamp is bigger than `fpc_max_lifetime`:
          set expiration timestamp to current plus `fpc_max_lifetime`
        else:
          add command argument to expiration timestamp
        put "update" request into FPC optimizer queue
      unlock the record using spin lock
    else:
      response = error message
    unlock shared downgradable mutex on FPC domain
    send back response
