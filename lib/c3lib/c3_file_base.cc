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
#include "c3_files.h"
#include "c3_file_base.h"

#include <unistd.h>

namespace CyberCache {

FileBase::FileBase(c3_ulong_t max_size) {
  reset_current_size();
  set_max_size(max_size);
  reset_fd();
}

bool FileBase::open_file(const char* path, file_mode_t mode, sync_mode_t sync) {
  c3_assert(get_fd() == -1);
  reset_current_size();
  int fd = c3_open_file(path, mode, sync);
  if (fd > 0) {
    set_fd(fd);
    return true;
  } else {
    return false;
  }
}

bool FileBase::close_file() {
  bool result = false;
  int fd = get_fd();
  if (fd > 0) {
    result = c3_close_file(fd);
    reset_fd();
    reset_current_size();
  }
  return result;
}

c3_long_t FileBase::get_available_space() const {
  int fd = get_fd();
  if (fd > 0) {
    int dup_fd = dup(fd);
    if (dup_fd > 0) {
      // this method is usually called from another thread, and we do not want the descriptor to get
      // closed while we're retrieving available disk space information
      c3_long_t result = c3_get_free_disk_space(get_fd());
      close(dup_fd);
      return result;
    }
  }
  return -1; // == "currently unused"
}

} // CyberCache
