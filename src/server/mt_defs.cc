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
#include "mt_defs.h"

#include <cstdio>

namespace CyberCache {

const char* SyncObject::get_host_name() const {
  switch (so_host) {
    case HO_SERVER:
      return "SERVER";
    case HO_LISTENER:
      return "LISTENER";
    case HO_LOGGER:
      return "LOGGER";
    case HO_STORE:
      return "STORE";
    case HO_TAG_MANAGER:
      return "TAG_MANAGER";
    case HO_REPLICATOR:
      return "REPLICATOR";
    case HO_BINLOG:
      return "BINLOG";
    case HO_OPTIMIZER:
      return "OPTIMIZER";
    default:
      return "<INVALID>";
  }
}

const char* SyncObject::get_type_name() const {
  switch (so_type) {
    case SO_SHARED_MUTEX:
      return "SHARED_MUTEX";
    case SO_DOWNGRADABLE_MUTEX:
      return "DOWNGRADABLE_MUTEX";
    case SO_MESSAGE_QUEUE:
      return "MESSAGE_QUEUE";
    default:
      return "<INVALID>";
  }
}

const char* SyncObject::get_text_info(char* buff, size_t length) const {
  c3_assert(buff && length >= INFO_BUFF_LENGTH);
  std::snprintf(buff, length, "%s:%s:%s:%u", get_domain_name(), get_host_name(), get_type_name(), so_id);
  return buff;
}

} // CyberCache
