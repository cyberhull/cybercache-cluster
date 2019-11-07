<?php
/*
 * CyberCache Cluster Test Suite
 * Written by Vadim Sytnikov
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
 */
require_once 'test-extension.inc';

if (extension_loaded('cybercache')) {
  echo "CyberCache extension loaded successfully.\n";
} else {
  fail("CyberCache extension did NOT load!");
}

/*
 * Options that are to be used during both session and FPC resource creation.
 * --------------------------------------------------------------------------
 */
$c3_options = [
// "address"    => "188.94.230.146",
// "address"    => "cyberhull.com",
// "address"    => "127.0.0.1",
   "address"    => "localhost",
// "port"       => "1000",
// "port"       => "65535",
   "port"       => "8120",
// "persistent" => "on",
// "persistent" => "yes",
// "persistent" => "true",
   "persistent" => "on",
// "compressor" => "lzham",
// "compressor" => "zlib",
// "compressor" => "zstd",
// "compressor" => "brotli",
// "compressor" => "lzss3",
// "compressor" => "lz4",
   "compressor" => "snappy",
// "compressor" => "lzf",
// "marker"     => "false",
// "marker"     => "true",
// "marker"     => "no",
// "marker"     => "yes",
// "marker"     => "off",
   "marker"     => "on",
// "admin"      => "",
   "admin"      => "Test-Admin-Password-1",
// "user"       => "samplePassword",
   "user"       => "",
// "hasher"     => "xxhash",
// "hasher"     => "farmhash",
// "hasher"     => "spookyhash",
// "hasher"     => "murmurhash3",
   "hasher"     => "murmurhash2"
];
/*
 * Test user-level session functions.
 * ----------------------------------
 */
run_test("create session resource", ERV_RESOURCE,
  $c3session = c3_session($c3_options));
run_test("read non-existent session record", ERV_EMPTY_STRING,
  c3_read($c3session, 'session-record-id-that-does-not-exist', 0));
run_test("write session record", ERV_TRUE,
  c3_write($c3session, 'session-1', -1, 'sample session record', 0));
run_test("read existing session record", ERV_STRING,
  c3_read($c3session, 'session-1', 0), 'sample', 'record');
run_test("read existing session record with session lock", ERV_STRING,
  c3_read($c3session, 'session-1'), 'sample', 'record');
run_test("write locked session record", ERV_TRUE,
  c3_write($c3session, 'session-1', -1, 'unlocked'));
run_test("check that session record was unlocked and modified", ERV_STRING,
  c3_read($c3session, 'session-1', 0), 'unlocked');
run_test("delete session record", ERV_TRUE,
  c3_destroy($c3session, 'session-1'));
run_test("check that deleted session record does not exist", ERV_EMPTY_STRING,
  c3_read($c3session, 'session-1', 0));
run_test("run garbage collector with '1 minute' argument", ERV_TRUE,
  c3_gc($c3session, 60));
run_test("run garbage collector with '0 seconds' argument", ERV_TRUE,
  c3_gc($c3session, 0));
run_test("check that last session error message is empty", ERV_EMPTY_STRING,
  c3_get_last_error($c3session));

/*
 * Test user-level FPC functions.
 * ------------------------------
 */
run_test("create FPC resource", ERV_RESOURCE,
  $c3fpc = c3_fpc($c3_options));
run_test("load non-existent FPC record", ERV_FALSE,
  c3_load($c3fpc, 'fpc-record-id-that-does-not-exist'));
run_test("test non-existent FPC record", ERV_FALSE,
  c3_test($c3fpc, 'fpc-record-id-that-does-not-exist'));
run_test("get metadata of a non-existent FPC record", ERV_FALSE,
  c3_get_metadatas($c3fpc, 'fpc-record-id-that-does-not-exist'));
run_test("touch non-existent FPC record", ERV_FALSE_AND_ERROR,
  c3_touch($c3fpc, 'fpc-record-id-that-does-not-exist', 60), c3_get_last_error($c3fpc), "did not exist");
run_test("save FPC record", ERV_TRUE,
  c3_save($c3fpc, 'fpc-1', 3600, ['tag-1', 'tag-2'], 'sample FPC record'));
run_test("load existing FPC record", ERV_STRING,
  c3_load($c3fpc, 'fpc-1'), 'sample', 'record');
run_test("test existing FPC record", ERV_TIMESTAMP,
  c3_test($c3fpc, 'fpc-1'));
run_test("get metadata of an existing FPC record", ERV_METADATA,
  c3_get_metadatas($c3fpc, 'fpc-1'));
run_test("touch existing FPC record", ERV_TRUE,
  c3_touch($c3fpc, 'fpc-1', 60));
run_test("save medium-size FPC record with 'null' lifitime", ERV_TRUE,
  c3_save($c3fpc, 'fpc-2', null, ['tag-2', 'tag-3'], get_medium_record()));
run_test("load medium-size FPC record", ERV_STRING,
  c3_load($c3fpc, 'fpc-2'), 'Fhtagn');
run_test("save large FPC record with default ('false') lifetime", ERV_TRUE,
  c3_save($c3fpc, 'fpc-3', false, ['tag-3', 'tag-4'], get_large_record()));
run_test("load large FPC record", ERV_STRING,
  c3_load($c3fpc, 'fpc-3'), 'Fhtagn');
run_test("get list of record IDs", ERV_STR_ARRAY,
  c3_get_ids($c3fpc), 'fpc-1', 'fpc-2', 'fpc-3');
run_test("get list of record tags", ERV_STR_ARRAY,
  c3_get_tags($c3fpc), 'tag-1', 'tag-2', 'tag-3', 'tag-4');
run_test("get list of IDs of records marked with a tag", ERV_STR_ARRAY,
  c3_get_ids_matching_tags($c3fpc, ['tag-2']), 'fpc-1', 'fpc-2');
run_test("get list of IDs of records marked with several tags", ERV_STR_ARRAY,
  c3_get_ids_matching_tags($c3fpc, ['tag-2', 'tag-3']), 'fpc-2');
run_test("check that list of IDs marked with a non-existent tag is empty", ERV_EMPTY_ARRAY,
  c3_get_ids_matching_tags($c3fpc, ['non-existent-tag']));
run_test("check that list of IDs marked with non-intersecting tags is empty", ERV_EMPTY_ARRAY,
  c3_get_ids_matching_tags($c3fpc, ['tag-1', 'tag-4']));
run_test("get list of IDs of records NOT marked with a tag", ERV_STR_ARRAY,
  c3_get_ids_not_matching_tags($c3fpc, ['tag-4']), 'fpc-1', 'fpc-2');
run_test("get list of IDs of records NOT marked with several tags", ERV_STR_ARRAY,
  c3_get_ids_not_matching_tags($c3fpc, ['tag-3', 'tag-4']), 'fpc-1');
run_test("get list of IDs of records marked with any of the specified tags", ERV_STR_ARRAY,
  c3_get_ids_matching_any_tags($c3fpc, ['tag-2', 'tag-3']), 'fpc-1', 'fpc-2', 'fpc-3');
run_test("clean FPC records marked with all specified tags", ERV_TRUE,
  c3_clean($c3fpc, "matchingTag", ['tag-1', 'tag-2']));
run_test("check that only specified records were removed", ERV_STR_ARRAY,
  c3_get_ids($c3fpc), 'fpc-2', 'fpc-3');
run_test("try cleaning FPC records NOT marked with tags shared by all records", ERV_TRUE,
  c3_clean($c3fpc, "notMatchingTag", ['tag-2', 'tag-3']));
run_test("check that no records were removed", ERV_STR_ARRAY,
  c3_get_ids($c3fpc), 'fpc-2', 'fpc-3');
run_test("clean FPC records marked with any of the specified tags", ERV_TRUE,
  c3_clean($c3fpc, "matchingAnyTag", ['tag-1', 'tag-2']));
run_test("check that only specified records were removed", ERV_STR_ARRAY,
  c3_get_ids($c3fpc), 'fpc-3');
run_test("clean old FPC records", ERV_TRUE,
  c3_clean($c3fpc, "old"));
run_test("check that no records were removed", ERV_STR_ARRAY,
  c3_get_ids($c3fpc), 'fpc-3');
run_test("clean ALL FPC records", ERV_TRUE,
  c3_clean($c3fpc, "all"));
run_test("check that there are no records", ERV_EMPTY_ARRAY,
  c3_get_ids($c3fpc));
run_test("create FPC record with no tags", ERV_TRUE,
  c3_save($c3fpc, 'fpc-4', 3600, NULL, 'FPC record without tags'));
run_test("load FPC record with no tags", ERV_STRING,
  c3_load($c3fpc, 'fpc-4'), 'record', 'without');
run_test("get metadata of FPC record without tags", ERV_METADATA,
  c3_get_metadatas($c3fpc, 'fpc-4'));
run_test("create FPC record with one tag specified as a string", ERV_TRUE,
  c3_save($c3fpc, 'fpc-5', 3600, 'tag-5', 'FPC record with single tag'));
run_test("clean FPC records marked with a tag specified as string", ERV_TRUE,
  c3_clean($c3fpc, "matchingAnyTag", 'tag-5'));
run_test("remove single FPC record", ERV_TRUE,
  c3_remove($c3fpc, 'fpc-4'));
run_test("check that FPC store is empty", ERV_EMPTY_ARRAY,
  c3_get_ids($c3fpc));
run_test("save FPC record with zero-length buffer", ERV_TRUE,
  c3_save($c3fpc, 'empty-record', 3600, NULL, ''));
run_test("load FPC record with zero-length buffer", ERV_EMPTY_STRING,
  c3_load($c3fpc, 'empty-record'));
run_test("get FPC store filling percentage", ERV_NUMBER,
  c3_get_filling_percentage($c3fpc));
run_test("fetch FPC back-end capabilities", ERV_CAPABILITIES,
  c3_get_capabilities());

/*
 * See if we're running instrumented version of the server.
 * --------------------------------------------------------
 */
$values = c3_get($c3session, "version");
if (is_array($values)) {
  $c3_instrumented = strpos($values[0], 'i]') !== false;
  echo 'Got server information: running ', ($c3_instrumented? '': 'non-'), "instrumented version\n";
  if ($c3_instrumented) {
  run_test("request STATS without specifying a domain or mask", ERV_STR_ARRAY,
    c3_stats($c3session), "Connections");
  run_test("request STATS without specifying a mask", ERV_STR_ARRAY,
    c3_stats($c3session, C3_DOMAIN_GLOBAL), "Connections");
  run_test("request STATS with specific domain and mask", ERV_STR_ARRAY,
    c3_stats($c3session, C3_DOMAIN_FPC, "*queue*"), "Queue");
  }
} else {
  fail("Could not retrieve server version information");
}

/*
 * Test information functions.
 * ---------------------------
 */
run_test("send PING via session resource", ERV_TRUE,
  c3_ping($c3session));
run_test("send PING via FPC resource", ERV_TRUE,
  c3_ping($c3fpc));
run_test("send CHECK via session resource", ERV_NUM3_ARRAY,
  c3_check($c3session));
run_test("send CHECK via FPC resource", ERV_NUM3_ARRAY,
  c3_check($c3fpc));
run_test("request INFO without specifying a domain", ERV_STR_ARRAY,
  c3_info($c3session), "Global", "Session", "FPC", "binlog");
run_test("request INFO on all domains", ERV_STR_ARRAY,
  c3_info($c3fpc, C3_DOMAIN_ALL), "Global", "Session", "FPC", "binlog");
run_test("request INFO on global domain", ERV_STR_ARRAY,
  c3_info($c3session, C3_DOMAIN_GLOBAL), "Global", "CyberCache", "Health check");
run_test("request INFO on session domain", ERV_STR_ARRAY,
  c3_info($c3session, C3_DOMAIN_SESSION), "Session", "store", "replicator");
run_test("request INFO on FPC domain", ERV_STR_ARRAY,
  c3_info($c3fpc, C3_DOMAIN_FPC), "FPC", "store", "replicator");
if ($c3_instrumented) {
  // TODO
} else {
  run_test("request STATS without specifying a domain", ERV_EMPTY_ARRAY_AND_ERROR,
    c3_stats($c3session), c3_get_last_error($c3session), "instrumented version");
  run_test("request STATS on all domains", ERV_EMPTY_ARRAY_AND_ERROR,
    c3_stats($c3fpc, C3_DOMAIN_ALL), c3_get_last_error($c3fpc), "instrumented version");
  run_test("request STATS on global domain", ERV_EMPTY_ARRAY_AND_ERROR,
    c3_stats($c3session, C3_DOMAIN_GLOBAL), c3_get_last_error($c3session), "instrumented version");
  run_test("request STATS on session domain", ERV_EMPTY_ARRAY_AND_ERROR,
    c3_stats($c3session, C3_DOMAIN_SESSION), c3_get_last_error($c3session), "instrumented version");
  run_test("request STATS on FPC domain", ERV_EMPTY_ARRAY_AND_ERROR,
    c3_stats($c3fpc, C3_DOMAIN_FPC), c3_get_last_error($c3fpc), "instrumented version");
}

/*
 * Test administrative functions.
 * ------------------------------
 */
run_test("fetch values of server options by one mask", ERV_STR_ARRAY,
  c3_get($c3session, "max_*memory"), "max_memory", "max_session_memory", "max_fpc_memory");
run_test("fetch values of server options by several names", ERV_STR_ARRAY,
  c3_get($c3fpc, "max_session_memory", "max_fpc_memory"), "max_session_memory", "max_fpc_memory");
run_test("set new value of server option", ERV_TRUE,
  c3_set($c3session, "max_session_memory", "256M"));
run_test("verify that new option value had been set", ERV_STR_ARRAY,
  c3_get($c3session, "max_session_memory"), "max_session_memory", "256m");
run_test("send local configuration file to the server", ERV_TRUE,
  c3_local_config($c3session, "config/local-test.cfg"));
run_test("verify that settings from local config file took effect", ERV_STR_ARRAY,
  c3_get($c3session, "max_session_memory"), "max_session_memory", "512m");
run_test("send path of remote configuration file to the server", ERV_TRUE,
  c3_local_config($c3fpc, "config/remote-test.cfg"));
run_test("verify that settings from remote config file took effect", ERV_STR_ARRAY,
  c3_get($c3fpc, "max_fpc_memory"), "max_fpc_memory", "2g");
run_test("restore data from session binlog", ERV_TRUE,
  c3_restore($c3session, "data/session-1.binlog"));
usleep(100 * 1000);
run_test("read session record loaded from binlog", ERV_STRING,
  c3_read($c3session, 'binlog-record', 0), 'Sample session record');
run_test("store session records to file database", ERV_TRUE,
  c3_store($c3session, C3_DOMAIN_SESSION, 'logs/session-php-store.blf', C3_UA_USER, C3_SYNC_FULL));
run_test("restore data from FPC binlog", ERV_TRUE,
  c3_restore($c3fpc, "data/fpc-1.binlog"));
usleep(100 * 1000);
run_test("load FPC record loaded from binlog", ERV_STRING,
  c3_load($c3fpc, 'binlog-record'), 'Sample FPC record');
run_test("check tags loaded from FPC binlog", ERV_STR_ARRAY,
  c3_get_tags($c3fpc), 'binlog-tag-one', 'binlog-tag-two');
run_test("store FPC records to file database", ERV_TRUE,
  c3_store($c3fpc, C3_DOMAIN_FPC, 'logs/fpc-php-store.blf', C3_UA_BOT));
run_test("send message to server log", ERV_TRUE,
  c3_log($c3session, 'CyberCache PHP extension test: sample message'));
run_test("force log rotation without specifying domain", ERV_TRUE,
  c3_rotate($c3session));
// run_test("force 'global' server log rotation", ERV_TRUE,
//   c3_rotate($c3fpc, C3_DOMAIN_GLOBAL));
run_test("force session binlog rotation", ERV_TRUE,
  c3_rotate($c3session, C3_DOMAIN_SESSION));
run_test("force FPC binlog rotation", ERV_TRUE,
  c3_rotate($c3fpc, C3_DOMAIN_FPC));
// run_test("force rotation of all [bin]logs", ERV_TRUE,
//  c3_rotate($c3fpc, C3_DOMAIN_ALL));
run_test("shut down the server", ERV_TRUE,
  c3_shutdown($c3fpc));

/*
 * All tests passed!
 * -----------------
 */

echo "ALL tests PASSED!\n";
