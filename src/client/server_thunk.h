/**
 * CyberCache Cluster
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
 * Interface to CyberCache server.
 */
#ifndef _SERVER_THUNK_H
#define _SERVER_THUNK_H

/**
 * What to return to PHP code if server call succeeded (AND returned a particular type of data).
 *
 * Most of codes are generic in that they suit many use cases, while two (an array of three integers, and
 * metadata array) cover special cases that are not covered by generic codes. The only alternative to
 * that would be to implement much more elaborate system of data retrieval, with a sort of "data request
 * language" capable of describing hierarchical structures, array key names, and the likes, which would
 * be highly impractical given the task at hand.
 */
enum c3_ok_return_t {
  OSR_TRUE_FROM_OK,                // return TRUE if server response is 'ok'
  OSR_NUMBER_FROM_DATA_HEADER,     // return number if server response is 'data' with one number in header
  OSR_NUM3_ARRAY_FROM_DATA_HEADER, // return array of 3 numbers if server response is 'data' (special case)
  OSR_METADATA_FROM_DATA_HEADER,   // return data formatted for GETMETADATAS command (special case)
  OSR_STRING_FROM_DATA_PAYLOAD,    // return string if server response is 'data' with valid payload
  OSR_ARRAY_FROM_LIST_PAYLOAD      // return array if server response is 'list' with valid payload
};

/**
 * What to return to PHP code if server call failed (for ANY reason other than syntax error in PHP code).
 *
 * IMPORTANT: all PHP functions return NULL if function arguments cannot be successfully *parsed* by PHP
 * interpreter (wrong number of arguments, arguments are of wrong types, etc.). However, if it is then
 * found out that arguments are out of range or otherwise invalid, methods will report failures based on
 * the their error codes from this enumeration.
 *
 * Some functions yield error returns on 'ok' server responses, but those functions would also produce
 * that same error return if server sends back 'error' response.
 */
enum c3_error_return_t {
  ESR_FALSE_FROM_OK,         // return FALSE if server response is 'ok'
  ESR_EMPTY_STRING_FROM_OK,  // return empty string if server response is 'ok'
  ESR_FALSE_FROM_ERROR,      // return FALSE if server sent 'error' response
  ESR_ZERO_FROM_ERROR,       // return integer 0 if server sent 'error' response
  ESR_EMPTY_ARRAY_FROM_ERROR // return empty array if server sent 'error' response
};

/// Authentication type required by a command
enum c3_auth_type_t {
  AUT_USER,  // a user-level command
  AUT_ADMIN, // an administrative command
  AUT_INFO   // an information command (authentication type controlled by INI option)
};

/**
 * Container for the arguments passed to command executor; when command executor sees a particular argument
 * specification, it pulls data from respective argument container as follows:
 *
 * - 'N' : number: `a_number` (pointers are ignored),
 * - 'S' : pointer: `a_string`, length: `a_size`,
 * - 'L' : list: `a_list` (integers are ignored),
 * - 'P' : pointer: `a_buffer`, length: `a_size`.
 */
struct c3_arg_t {
  union {
    const char*      a_string;  // pointer to a string
    HashTable*       a_list;    // pointer to a hash table
    const c3_byte_t* a_buffer;  // pointer to payload buffer
  };
  union {
    zend_long a_number; // a signed integer number
    size_t    a_size;   // string length, or size of the payload buffer
  };
};

/**
 * Issues a call to CyberCache Cluster and returns call result to PHP code.
 *
 * @param rc Pointer to PHP variable that holds an instance of C3Resource class.
 * @param return_value Pointer to PHP variable to which call result should be stored.
 * @param ok_return An element of enumeration that specifies what is considered a "success" return value.
 * @param error_return An element of enumeration that specifies what should be returned upon "failure".
 * @param cmd ID of the command that should be sent to the server.
 * @param auth Authentication level required by the command.
 * @param format Specifies what is being passed to the server as call arguments; possible values:
 * - 'N' : number passed as `zend_long`,
 * - 'S' : string passed as pointer/size_t pair,
 * - 'L' : list passed as `HashTable` pointer,
 * - 'A' : user agent; does NOT have corresponding `arg[]` element, data is taken from the resource,
 * - 'P' : payload buffer passed as pointer/size_t pair,
 * @param args Additional data specified by the format string (optional array of `c3_arg_t` structures)
 */
void call_c3(const zval* rc,
  zval* return_value, c3_ok_return_t ok_return, c3_error_return_t error_return,
  command_t cmd, c3_auth_type_t auth, const char* format, c3_arg_t* args = nullptr);

#endif // _SERVER_THUNK_H
