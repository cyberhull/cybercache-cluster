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
 *
 * Extension test framework.
 *
 * Test scripts are supposed to call `run_test()` function with `ERV_xxx` 
 * constants, and optionally call `get_medium_record()` and/or
 * `get_large_record()` to generate test records; everything else in this module
 * is implementation code.
 */

/*
 * The following constants represent full set of expected return values of
 * `c3_xxx()` functions. There are two exceptions:
 *
 * - all `c3_xxx()` functions return `NULL` if PHP code attempts to call them 
 *   with wrong number and/or type of the arguments; this would cause PHP 
 *   parsing error, and PHP would bail out anyway,
 *
 * - except when there's the above-described parsing error, `c3_session()` and 
 *   `c3_fpc()` functions will always return valid resources; that is, if, say, 
 *   argument array passed to them contains wrong element (e.g. "addres" instead 
 *   of "address"), or if "address" contains internet address that cannot be 
 *   resolved, or an invalid IP, a valid resource will still be returned; here,
 *   "valid" means valid in terms of being usable by PHP; it may or may not be 
 *   usable by the extension.
 */
const ERV_RESOURCE               = 0x01; // resource of type 'CyberCache'
const ERV_TRUE                   = 0x02; // boolean `true`
const ERV_NUMBER                 = 0x03; // single integer number
const ERV_TIMESTAMP              = 0x04; // integer number that is a timestamp
const ERV_NUM3_ARRAY             = 0x05; // array of three integers (a special case)
const ERV_METADATA               = 0x06; // associative array, result of GETMETADATAS call (a special case)
const ERV_STRING                 = 0x07; // single string
const ERV_STR_ARRAY              = 0x08; // array of strings
const ERV_FALSE                  = 0x10; // boolean `false`; no error message
const ERV_EMPTY_STRING           = 0x11; // single empty string; no error message
const ERV_EMPTY_ARRAY            = 0x12; // single empty string; no error message
const ERV_FALSE_AND_ERROR        = 0x20; // boolean `false` and an error message
const ERV_ZERO_AND_ERROR         = 0x21; // integer `0` and an error message
const ERV_EMPTY_ARRAY_AND_ERROR  = 0x22; // empty array and an error message
const ERV_CAPABILITIES           = 0x40; // array with FPC backend capabilities (a special case)

function fail($message) {
  echo $message;
  exit(1);
}

function get_test_type_name(int $erv) {
  switch ($erv) {
    case ERV_RESOURCE:
      return 'RESOURCE';
      break;
    case ERV_TRUE:
      return 'TRUE';
      break;
    case ERV_NUMBER:
      return 'NUMBER';
      break;
    case ERV_TIMESTAMP:
      return 'TIMESTAMP';
      break;
    case ERV_NUM3_ARRAY:
      return 'NUM3_ARRAY';
      break;
    case ERV_METADATA:
      return 'METADATA';
      break;
    case ERV_STRING:
      return 'STRING';
      break;
    case ERV_STR_ARRAY:
      return 'STR_ARRAY';
      break;
    case ERV_FALSE:
      return 'FALSE';
      break;
    case ERV_EMPTY_STRING:
      return 'EMPTY_STRING';
      break;
    case ERV_EMPTY_ARRAY:
      return 'EMPTY_ARRAY';
      break;
    case ERV_FALSE_AND_ERROR:
      return 'FALSE_AND_ERROR';
      break;
    case ERV_ZERO_AND_ERROR:
      return 'ZERO_AND_ERROR';
      break;
    case ERV_EMPTY_ARRAY_AND_ERROR:
      return 'EMPTY_ARRAY_AND_ERROR';
      break;
    case ERV_CAPABILITIES:
      return 'CAPABILITIES';
      break;
    default:
      return '<<<INVALID>>>';
  }
}

function check_no_expected(string $test, int $erv, array $expected) {
  if (count($expected)) {
    fail("Test '$test' [" . get_test_type_name($erv) . "] does not accept expected values");
  }
}

function check_timestamp($ts, bool $can_be_zero = true) {
  if (is_int($ts)) {
    if ($ts) {
      $date_info = getdate($ts);
      if (is_array($date_info) && $date_info['year'] >= 2019) {
        return true;
      }
    } else {
      return $can_be_zero;
    }
  }
  return false;
}

function check_string_array($values) {
  if (is_array($values)) {
    foreach ($values as $key => $value) {
      if (!is_int($key) || !is_string($value) || strlen($value) == 0) {
        return false;
      }
    }
    return true;
  }
  return false;
}

function check_string_against_expected($value, array $expected_values) {
  if (is_string($value)) {
    foreach ($expected_values as $expected_value) {
      if (strpos($value, $expected_value) === false) {
        return false;
      }
    }
    return true;
  }
  return false;
}

function check_string_array_against_expected($values, array $expected_values) {
  /*
   * Checks that each expected string is present as a substring in _at_least_ 
   * one result array element (some result array's elements may not contain any 
   * expected strings; that's OK).
   */
  if (is_array($values)) {
    foreach ($expected_values as $expected_value) {
      $found = false;
      foreach ($values as $key => $value) {
        if (!is_int($key) || !is_string($value)) {
          return false;
        }
        if (strpos($value, $expected_value) !== false) {
          $found = true;
          break;
        }
      }
      if (!$found) {
        return false;
      }
    }
    return true;
  }
  return false;
}

function check_error_message_against_expected(string $test, int $erv, array $expected_values) {
  switch (count($expected_values)) {
    case 0: // error message is not checked
      return true;
    case 1: // wrong usage: if message is specified, it must be followed by at least one [sub]string
      fail("Test '$test' [${get_test_type_name($erv)}] requires expected values if message is specified");
    default:
      $message = array_shift($expected_values);
      return check_string_against_expected($message, $expected_values);
  }
}

function check_bool_array_element(array $elements, string $key) {
  return array_key_exists($key, $elements) && is_bool($elements[$key]);
}

/**
 * Returns record of size larger than 256 bytes, but smaller than 64k.
 *
 * The record is guaranteed to contain word 'Fhtagn'.
 */
function get_medium_record() {
  $path = 'data/sample-record.txt';
  $record = @file_get_contents($path);
  if ($record === false) {
    fail("Could not load sample record from '$path'");
  }
  return $record;
}

/**
 * Returns record of size larger than 64k.
 *
 * The record is guaranteed to contain word 'Fhtagn'.
 */
function get_large_record() {
  return str_repeat('[= CyberCache Cluster PHP Extension Test: sample cache record =]', 1024) .
    get_medium_record();
}

/**
 * Main extension framework test method.
 *
 * @param test Test description, a string
 * @param erv Expected Return Value, an `ERV_xxx` constant
 * @param retval Value returned from a `c3_xxx()` call
 * @param expected Substrings that has to be present in returned value (string or array)
 */
function run_test(string $test, int $erv, $retval, string ...$expected) {
  $result = false;
  switch ($erv) {
    case ERV_RESOURCE:
      check_no_expected($test, $erv, $expected);
      $result = is_resource($retval) && get_resource_type($retval) === 'CyberCache';
      break;
    case ERV_TRUE:
      check_no_expected($test, $erv, $expected);
      $result = $retval === true;
      break;
    case ERV_NUMBER:
      check_no_expected($test, $erv, $expected);
      $result = is_int($retval);
      break;
    case ERV_TIMESTAMP:
      check_no_expected($test, $erv, $expected);
      $result = check_timestamp($retval, false);
      break;
    case ERV_NUM3_ARRAY:
      check_no_expected($test, $erv, $expected);
      if (is_array($retval) && count($retval) == 3 &&
        array_key_exists(0, $retval) && is_int($retval[0]) &&
        array_key_exists(1, $retval) && is_int($retval[1]) &&
        array_key_exists(2, $retval) && is_int($retval[2])) {
        $result = true;
      }
      break;
    case ERV_METADATA:
      if (is_array($retval) && count($retval) == 3 &&
        array_key_exists('expire', $retval) && check_timestamp($retval['expire']) &&
        array_key_exists('mtime', $retval) && check_timestamp($retval['mtime']) &&
        array_key_exists('tags', $retval) && check_string_array($retval['tags'])) {
        $result = check_string_array_against_expected($retval['tags'], $expected);
      }
      break;
    case ERV_STRING:
      $result = check_string_against_expected($retval, $expected);
      break;
    case ERV_STR_ARRAY:
      $result = check_string_array_against_expected($retval, $expected);
      break;
    case ERV_FALSE:
      check_no_expected($test, $erv, $expected);
      $result = $retval === false;
      break;
    case ERV_EMPTY_STRING:
      check_no_expected($test, $erv, $expected);
      $result = is_string($retval) && strlen($retval) == 0;
      break;
    case ERV_EMPTY_ARRAY:
      check_no_expected($test, $erv, $expected);
      $result = is_array($retval) && count($retval) == 0;
      break;
    case ERV_FALSE_AND_ERROR:
      $result = $retval === false && check_error_message_against_expected($test, $erv, $expected);
      break;
    case ERV_ZERO_AND_ERROR:
      $result = $retval === 0 && check_error_message_against_expected($test, $erv, $expected);
      break;
    case ERV_EMPTY_ARRAY_AND_ERROR:
      $result = is_array($retval) && count($retval) == 0 &&
        check_error_message_against_expected($test, $erv, $expected);
      break;
    case ERV_CAPABILITIES:
      $result = is_array($retval) && count($retval) == 6 &&
        check_bool_array_element($retval, 'automatic_cleaning') &&
        check_bool_array_element($retval, 'tags') &&
        check_bool_array_element($retval, 'expired_read') &&
        check_bool_array_element($retval, 'priority') &&
        check_bool_array_element($retval, 'infinite_lifetime') &&
        check_bool_array_element($retval, 'get_list');
      break;
    default:
      fail("Test '$test': invalid Expected Return Value type ($erv)");
  }
  $test_result = $result? 'PASSED': 'FAILED';
  static $number = 1;
  echo "Test $number: $test => $test_result\n";
  $number++;
  if (!$result) {
    echo "Expected result: " . get_test_type_name($erv) . "\n";
    echo "Expected values: ";
    if ($expected) {
      $separator = '';
      foreach ($expected as $value) {
        echo "$separator'$value'";
        $separator = ', ';
      }
      echo "\n";
    } else {
      echo "(not specified)\n";
    }
    echo "Actual result:\n";
    var_dump($retval);
    exit(1);
  }
}
