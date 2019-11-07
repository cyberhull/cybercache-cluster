/**
 * This file is a part of the implementation of the CyberCache Cluster.
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
 */
#include "io_protocol.h"

#if C3_DEBUG_ON

namespace CyberCache {

const char* c3_get_command_name(command_t cmd) {
  switch (cmd) {
    case CMD_PING:
      return "PING";
    case CMD_CHECK:
      return "CHECK";
    case CMD_INFO:
      return "INFO";
    case CMD_STATS:
      return "STATS";
    case CMD_SHUTDOWN:
      return "SHUTDOWN";
    case CMD_LOADCONFIG:
      return "LOADCONFIG";
    case CMD_RESTORE:
      return "RESTORE";
    case CMD_STORE:
      return "STORE";
    case CMD_GET:
      return "GET";
    case CMD_SET:
      return "SET";
    case CMD_LOG:
      return "LOG";
    case CMD_ROTATE:
      return "ROTATE";
    case CMD_READ:
      return "READ";
    case CMD_WRITE:
      return "WRITE";
    case CMD_DESTROY:
      return "DESTROY";
    case CMD_GC:
      return "GC";
    case CMD_LOAD:
      return "LOAD";
    case CMD_TEST:
      return "TEST";
    case CMD_SAVE:
      return "SAVE";
    case CMD_REMOVE:
      return "REMOVE";
    case CMD_CLEAN:
      return "CLEAN";
    case CMD_GETIDS:
      return "GETIDS";
    case CMD_GETTAGS:
      return "GETTAGS";
    case CMD_GETIDSMATCHINGTAGS:
      return "GETIDSMATCHINGTAGS";
    case CMD_GETIDSNOTMATCHINGTAGS:
      return "GETIDSNOTMATCHINGTAGS";
    case CMD_GETIDSMATCHINGANYTAGS:
      return "GETIDSMATCHINGANYTAGS";
    case CMD_GETFILLINGPERCENTAGE:
      return "GETFILLINGPERCENTAGE";
    case CMD_GETMETADATAS:
      return "GETMETADATAS";
    case CMD_TOUCH:
      return "TOUCH";
    default:
      return "<INVALID>";
  }
}

const char* c3_get_response_name(c3_byte_t response) {
  switch (response) {
    case RESP_TYPE_OK:
      return "OK";
    case RESP_TYPE_DATA:
      return "DATA";
    case RESP_TYPE_LIST:
      return "LIST";
    case RESP_TYPE_ERROR:
      return "ERROR";
    default:
      return "<INVALID>";
  }
}

}

#endif // C3_DEBUG_ON
