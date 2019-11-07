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
 * File I/O support specifically designed for [bin]logging.
 */
#ifndef _C3_FILES_H
#define _C3_FILES_H

#include "c3_build.h"
#include "c3_types.h"
#include "c3_memory.h"

// whether to compile `c3_get_free_disk_space(const char*)`
#define INCLUDE_C3_GET_FREE_DISK_SPACE 0

namespace CyberCache {

/// File open modes
enum file_mode_t {
  FM_READ,      // file is being opened for reading only; must exist
  FM_READWRITE, // file is being opened for reading & writing; must exist; will *not* be truncated
  FM_CREATE,    // file is being opened for writing; existing file *will* be truncated
  FM_APPEND     // file is being opened for appending; current position will be set to the very end
};

/// File writing synchronization modes
enum sync_mode_t {
  SM_NONE = 0,  // no synchronization (`write()` returns before data is flushed to hardware)
  SM_DATA_ONLY, // file contents is synchronized (file bytes and size)
  SM_FULL,      // full synchronization (including file timestamps)
  SM_NUMBER_OF_ELEMENTS
};

/// Providing constants from <unistd.h> so that inclusion of that file wouldn't be necessary
enum position_mode_t: int {
  PM_START = 0,   // from the beginning of the file
  PM_CURRENT = 1, // from current position in the file
  PM_END = 2      // from the very end of the file
};

/// Modes for file access checks; AM_EXISTS should be used alone, while other modes can be combined (ORed)
enum access_mode_t: int {
  AM_EXISTS     = 0x01, // file exists; this constant should *NOT* be combined with others
  AM_READABLE   = 0x02, // file exists and can be read
  AM_WRITABLE   = 0x04, // file exists and can be written
  AM_EXECUTABLE = 0x08  // file exists and can be executed
};

bool c3_file_access(const char* path, access_mode_t mode = AM_EXISTS) C3_FUNC_COLD;
char* c3_get_home_path(char *buff, size_t size, const char *path = nullptr) C3_FUNC_COLD;
c3_long_t c3_get_file_size(const char* path) C3_FUNC_COLD;
c3_long_t c3_get_file_size(int fd) C3_FUNC_COLD;
#if INCLUDE_C3_GET_FREE_DISK_SPACE
c3_long_t c3_get_free_disk_space(const char* path) C3_FUNC_COLD;
#endif // INCLUDE_C3_GET_FREE_DISK_SPACE
c3_long_t c3_get_free_disk_space(int fd) C3_FUNC_COLD;
int c3_open_file(const char* path, file_mode_t mode = FM_READ, sync_mode_t sync = SM_NONE) C3_FUNC_COLD;
c3_long_t c3_seek_file(int fd, c3_long_t pos, position_mode_t from = PM_START) C3_FUNC_COLD;
ssize_t c3_read_file(int fd, void *buffer, size_t size);
ssize_t c3_write_file(int fd, const void* buffer, size_t size);
bool c3_save_file(const char* path, const void* buffer, size_t size) C3_FUNC_COLD;
void* c3_load_file(const char *path, size_t &size, Memory& memory = global_memory) C3_FUNC_COLD;
bool c3_rename_file(const char *src_path, const char *dst_path) C3_FUNC_COLD;
bool c3_close_file(int fd) C3_FUNC_COLD;
bool c3_delete_file(const char* path) C3_FUNC_COLD;

}

#endif // _C3_FILES_H
