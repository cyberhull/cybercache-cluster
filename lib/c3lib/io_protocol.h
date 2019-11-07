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
 * Low-level protocol definition: command and response codes, field masks, etc.
 */
#ifndef _IO_PROTOCOL_H
#define _IO_PROTOCOL_H

#include "c3_build.h"
#include "c3_types.h"

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// COMMAND IDS
///////////////////////////////////////////////////////////////////////////////

enum command_t: c3_byte_t {

  /// placeholder, "not a valid command"
  CMD_INVALID = 0x00,

  /// DESCRIPTOR 0x01 [ PASSWORD ] [ MARKER ]
  CMD_PING = 0x01,

  /// DESCRIPTOR 0x02 [ PASSWORD ] [ MARKER ]
  CMD_CHECK = 0x02,

  /// DESCRIPTOR HEADER { 0x10 [ PASSWORD ] CHUNK(NUMBER) } [ MARKER ]
  CMD_INFO = 0x10,

  /// DESCRIPTOR HEADER { 0x11 [ PASSWORD ] CHUNK(NUMBER) CHUNK(STRING) } [ MARKER ]
  CMD_STATS = 0x11,

  /// DESCRIPTOR 0xF0 [ PASSWORD ] [ MARKER ]
  CMD_SHUTDOWN = 0xF0,

  /// DESCRIPTOR HEADER { 0xF1 [ PASSWORD ] CHUNK(STRING) } [ MARKER ]
  CMD_LOADCONFIG = 0xF1,

  /// DESCRIPTOR HEADER { 0xF2 [ PASSWORD ] CHUNK(STRING) } [ MARKER ]
  CMD_RESTORE = 0xF2,

  /// DESCRIPTOR HEADER { 0xF3 [ PASSWORD ] CHUNK(NUMBER) CHUNK(STRING) CHUNK(NUMBER) CHUNK(NUMBER) } [ MARKER ]
  CMD_STORE = 0xF3,

  /// DESCRIPTOR HEADER { 0xF5 [ PASSWORD ] CHUNK(LIST) } [ MARKER ]
  CMD_GET = 0xF5,

  /// DESCRIPTOR HEADER { 0xF6 [ PASSWORD ] CHUNK(STRING) ] } [ MARKER ]
  CMD_SET = 0xF6,

  /// DESCRIPTOR HEADER { 0xFA [ PASSWORD ] CHUNK(STRING) } [ MARKER ]
  CMD_LOG = 0xFA,

  /// DESCRIPTOR HEADER { 0xFB [ PASSWORD ] CHUNK(NUMBER) } [ MARKER ]
  CMD_ROTATE = 0xFB,

  /// DESCRIPTOR HEADER { 0x21 [ PASSWORD ] CHUNK(STRING) CHUNK(NUMBER) [ CHUNK(NUMBER) ] } [ MARKER ]
  CMD_READ = 0x21,

  /// DESCRIPTOR HEADER { 0x22 [ PASSWORD ] [ PAYLOAD_INFO ] CHUNK(STRING) CHUNK(NUMBER) CHUNK(NUMBER) [ CHUNK(NUMBER) ]
  /// } [ PAYLOAD ] [ MARKER ]
  CMD_WRITE = 0x22,

  /// DESCRIPTOR HEADER { 0x23 [ PASSWORD ] CHUNK(STRING) } [ MARKER ]
  CMD_DESTROY = 0x23,

  /// DESCRIPTOR HEADER { 0x24 [ PASSWORD ] CHUNK(NUMBER) } [ MARKER ]
  CMD_GC = 0x24,

  /// DESCRIPTOR HEADER { 0x41 [ PASSWORD ] CHUNK(STRING) CHUNK(NUMBER) } [ MARKER ]
  CMD_LOAD = 0x41,

  /// DESCRIPTOR HEADER { 0x42 [ PASSWORD ] CHUNK(STRING) CHUNK(NUMBER) } [ MARKER ]
  CMD_TEST = 0x42,

  /// DESCRIPTOR HEADER 0x43 {
  ///   [ PASSWORD ] [ PAYLOAD_INFO ] CHUNK(STRING) CHUNK(NUMBER) CHUNK(NUMBER) CHUNK(LIST)
  ///   } PAYLOAD [ MARKER ]
  CMD_SAVE = 0x43,

  /// DESCRIPTOR HEADER { 0x44 [ PASSWORD ] CHUNK(STRING) } [ MARKER ]
  CMD_REMOVE = 0x44,

  /// DESCRIPTOR HEADER { 0x45 [ PASSWORD ] CHUNK(NUMBER) [ CHUNK(LIST) ] } [ MARKER ]
  CMD_CLEAN = 0x45,

  /// DESCRIPTOR 0x61 [ PASSWORD ] [ MARKER ]
  CMD_GETIDS = 0x61,

  /// DESCRIPTOR 0x62 [ PASSWORD ] [ MARKER ]
  CMD_GETTAGS = 0x62,

  /// DESCRIPTOR HEADER { 0x63 [ PASSWORD ] CHUNK(LIST) } [ MARKER ]
  CMD_GETIDSMATCHINGTAGS = 0x63,

  /// DESCRIPTOR HEADER { 0x64 [ PASSWORD ] CHUNK(LIST) } [ MARKER ]
  CMD_GETIDSNOTMATCHINGTAGS = 0x64,

  /// DESCRIPTOR HEADER { 0x65 [ PASSWORD ] CHUNK(LIST) } [ MARKER ]
  CMD_GETIDSMATCHINGANYTAGS = 0x65,

  /// DESCRIPTOR 0x67 [ PASSWORD ] [ MARKER ]
  CMD_GETFILLINGPERCENTAGE = 0x67,

  /// DESCRIPTOR HEADER { 0x68 [ PASSWORD ] CHUNK(STRING) } [ MARKER ]
  CMD_GETMETADATAS = 0x68,

  /// DESCRIPTOR HEADER { 0x69 [ PASSWORD ] CHUNK(STRING) CHUNK(NUMBER) } [ MARKER ]
  CMD_TOUCH = 0x69
};

///////////////////////////////////////////////////////////////////////////////
// COMMAND DESCRIPTOR ENCODING
///////////////////////////////////////////////////////////////////////////////

constexpr c3_byte_t DESC_AUTH_BITS  = 0x03;
constexpr c3_byte_t DESC_NO_AUTH    = 0x00; // no authentication
constexpr c3_byte_t DESC_USER_AUTH  = 0x01; // user-level authentication
constexpr c3_byte_t DESC_ADMIN_AUTH = 0x02; // admin-level authentication
constexpr c3_byte_t DESC_BULK_AUTH  = 0x03; // bulk authentication (e.g. during replication or binlog ops)

constexpr c3_byte_t DESC_HEADER_BITS  = 0x0C;
constexpr c3_byte_t DESC_NO_HEADER    = 0x00; // no header size bytes
constexpr c3_byte_t DESC_BYTE_HEADER  = 0x04; // header size is one byte
constexpr c3_byte_t DESC_WORD_HEADER  = 0x08; // header size is a 16-bit word
constexpr c3_byte_t DESC_DWORD_HEADER = 0x0C; // header size is a 32-bit word

constexpr c3_byte_t DESC_PAYLOAD_BITS  = 0x30;
constexpr c3_byte_t DESC_NO_PAYLOAD    = 0x00; // no payload chunk
constexpr c3_byte_t DESC_BYTE_PAYLOAD  = 0x10; // [un]compressed payload chunk, sizes are bytes
constexpr c3_byte_t DESC_WORD_PAYLOAD  = 0x20; // [un]compressed payload chunk, sizes are 16-bit words
constexpr c3_byte_t DESC_DWORD_PAYLOAD = 0x30; // [un]compressed payload chunk, sizes are 32-bit words

constexpr c3_byte_t DESC_PAYLOAD_IS_COMPRESSED = 0x40; // a compressor was used to pack payload
constexpr c3_byte_t DESC_MARKER_IS_PRESENT     = 0x80; // data is followed by integrity check marker `0xC3`

///////////////////////////////////////////////////////////////////////////////
// COMMAND AND RESPONSE HEADER DATA CHUNKS ENCODING
///////////////////////////////////////////////////////////////////////////////

constexpr c3_byte_t CHNK_TYPE_BITS = 0xC0;
constexpr c3_byte_t CHNK_INTEGER   = 0x00; // positive integer 8..71
constexpr c3_byte_t CHNK_STRING    = 0x40; // string of length 8..71
constexpr c3_byte_t CHNK_LIST      = 0x80; // a list with 9..71 elements, each element is a VLQ string
constexpr c3_byte_t CHNK_SUBTYPE   = 0xC0; // type encoded by bits 3..5
constexpr c3_byte_t CHNK_LONG_MASK = 0x3F; // bias mask for the value or length

constexpr c3_byte_t CHNK_SUBTYPE_BITS   = 0x38;
constexpr c3_byte_t CHNK_SMALL_NEGATIVE = 0;      // negative integer -1..-8
constexpr c3_byte_t CHNK_BIG_NEGATIVE   = 1 << 3; // negative integer -9..INT_MIN_VAL, 1..4 bytes follow
constexpr c3_byte_t CHNK_SMALL_INTEGER  = 2 << 3; // integer 0..7
constexpr c3_byte_t CHNK_SHORT_STRING   = 3 << 3; // string of length 0..7
constexpr c3_byte_t CHNK_SHORT_LIST     = 4 << 3; // list with 0..7 elements, each element is a VLQ string
constexpr c3_byte_t CHNK_BIG_INTEGER    = 5 << 3; // integer 72..UINT_MAX_VAL, 1..4 bytes follow
constexpr c3_byte_t CHNK_LONG_STRING    = 6 << 3; // string of len 72..UINT_MAX_VAL, 1..4 size bytes follow
constexpr c3_byte_t CHNK_LONG_LIST      = 7 << 3; // list with 72..UINT_MAX_VAL elems, 1..4 length bytes follow
constexpr c3_byte_t CHNK_SHORT_MASK     = 0x07;

constexpr c3_uint_t CHNK_MEDIUM_BIAS  = 8;
constexpr c3_uint_t CHNK_INTEGER_BIAS = CHNK_MEDIUM_BIAS;
constexpr c3_uint_t CHNK_STRING_BIAS  = CHNK_MEDIUM_BIAS;
constexpr c3_uint_t CHNK_LIST_BIAS    = CHNK_MEDIUM_BIAS;

constexpr c3_uint_t CHNK_LARGE_BIAS          = 72;
constexpr c3_uint_t CHNK_BIG_INTEGER_BIAS    = CHNK_LARGE_BIAS;
constexpr c3_uint_t CHNK_LONG_STRING_BIAS    = CHNK_LARGE_BIAS;
constexpr c3_uint_t CHNK_LONG_LIST_BIAS      = CHNK_LARGE_BIAS;
constexpr c3_int_t  CHNK_SMALL_NEGATIVE_BIAS = -1;
constexpr c3_int_t  CHNK_BIG_NEGATIVE_BIAS   = -9;

///////////////////////////////////////////////////////////////////////////////
// RESPONSE DESCRIPTOR ENCODING
///////////////////////////////////////////////////////////////////////////////

constexpr c3_byte_t RESP_TYPE_BITS  = 0x03;
constexpr c3_byte_t RESP_TYPE_OK    = 0x00; // no extra data
constexpr c3_byte_t RESP_TYPE_DATA  = 0x01; // payload is some binary data
constexpr c3_byte_t RESP_TYPE_LIST  = 0x02; // payload is a list of strings, count must be passed in the header
constexpr c3_byte_t RESP_TYPE_ERROR = 0x03; // response is an error message

constexpr c3_byte_t RESP_HEADER_BITS  = DESC_HEADER_BITS;
constexpr c3_byte_t RESP_NO_HEADER    = DESC_NO_HEADER;
constexpr c3_byte_t RESP_BYTE_HEADER  = DESC_BYTE_HEADER;
constexpr c3_byte_t RESP_WORD_HEADER  = DESC_WORD_HEADER;
constexpr c3_byte_t RESP_DWORD_HEADER = DESC_DWORD_HEADER;

constexpr c3_byte_t RESP_PAYLOAD_BITS  = DESC_PAYLOAD_BITS;
constexpr c3_byte_t RESP_NO_PAYLOAD    = DESC_NO_PAYLOAD;
constexpr c3_byte_t RESP_BYTE_PAYLOAD  = DESC_BYTE_PAYLOAD;
constexpr c3_byte_t RESP_WORD_PAYLOAD  = DESC_WORD_PAYLOAD;
constexpr c3_byte_t RESP_DWORD_PAYLOAD = DESC_DWORD_PAYLOAD;

constexpr c3_byte_t RESP_PAYLOAD_IS_COMPRESSED = DESC_PAYLOAD_IS_COMPRESSED;
constexpr c3_byte_t RESP_MARKER_IS_PRESENT     = DESC_MARKER_IS_PRESENT;

///////////////////////////////////////////////////////////////////////////////
// INTEGRITY CHECK
///////////////////////////////////////////////////////////////////////////////

/// Value of the optional integrity check marker
constexpr c3_byte_t C3_INTEGRITY_MARKER = 0xC3;

///////////////////////////////////////////////////////////////////////////////
// USER AGENT TYPES
///////////////////////////////////////////////////////////////////////////////

/// Constants for "user agent" types passed with certain commands (define cache data "priorities")
enum user_agent_t: c3_byte_t {
  UA_UNKNOWN = 0x00, // an unknown user agent
  UA_BOT     = 0x01, // a known bot, such as a Google crawler
  UA_WARMER  = 0x02, // CyberHULL cache warmer
  UA_USER    = 0x03, // probably a valid user
  UA_NUMBER_OF_ELEMENTS
};

///////////////////////////////////////////////////////////////////////////////
// `CLEAN` COMMAND MODES FOR FPC OPTIMIZER AND TAG MANAGER
///////////////////////////////////////////////////////////////////////////////

/// Cleanup modes passed with `CLEAN` FPC command
enum clean_mode_t {
  CM_INVALID = 0,          // an invalid mode (placeholder)
  CM_ALL,                  // remove all cache entries
  CM_OLD,                  // remove old cache entries (do garbage collection)
  CM_MATCHING_ALL_TAGS,    // remove entries that match all specified tags
  CM_NOT_MATCHING_ANY_TAG, // remove entries that do not match any of the specified tags
  CM_MATCHING_ANY_TAG,     // remove entries that match at least one of the specified tags
  CM_NUMBER_OF_ELEMENTS
};

///////////////////////////////////////////////////////////////////////////////
// DOMAIN MODES FOR VARIOUS INFORMATION / ADMIN COMMANDS
///////////////////////////////////////////////////////////////////////////////

// `INFO`, `STATS` etc. commands send combinations of these constants
constexpr c3_byte_t DM_NONE    = 0x00;
constexpr c3_byte_t DM_GLOBAL  = 0x01;
constexpr c3_byte_t DM_SESSION = 0x02;
constexpr c3_byte_t DM_FPC     = 0x04;
constexpr c3_byte_t DM_ALL     = DM_GLOBAL | DM_SESSION | DM_FPC;

///////////////////////////////////////////////////////////////////////////////
// DEBUGGING FACILITIES
///////////////////////////////////////////////////////////////////////////////

#if C3_DEBUG_ON
const char* c3_get_command_name(command_t cmd) C3_FUNC_COLD;
const char* c3_get_response_name(c3_byte_t response) C3_FUNC_COLD;
#endif // C3_DEBUG_ON

} // CyberCache

#endif // _IO_PROTOCOL_H
