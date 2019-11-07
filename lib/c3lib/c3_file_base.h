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
 * Base class for [bin]loggers.
 */
#ifndef _C3_FILE_BASE_H
#define _C3_FILE_BASE_H

#include "c3_types.h"

namespace CyberCache {

/**
 * Base for classes that maintain current and maximum sizes of some file storage, AND that can be accessed
 * "semi-concurrently" for informational purposes, meaning that other threads can request current state
 * of classes based on this one, but they (other threads) cannot perform operations that alter state (i.e.
 * open/close files, change max allowed size etc.)
 *
 * It is implied that exact synchronization is not required (meaning that it is not critical that any
 * changes that a thread operating on a class derived from this are immediately seen in other threads: it
 * is OK if, say, current file size seen in other threads is "lagging" by few nano-/micro-seconds), hence
 * we use "relaxed" synchronization for maximum performance.
 */
class FileBase {
  std::atomic_ullong fb_current_size; // how many bytes had been read or written
  std::atomic_ullong fb_max_size;     // size of file the being read, or max size of the file being written
  std::atomic_int    fb_fd;           // current file descriptor, or -1

  void set_fd(int fd) { fb_fd.store(fd, std::memory_order_relaxed); }
  void reset_fd() { set_fd(-1); }

protected:
  explicit FileBase(c3_ulong_t max_size);
  virtual ~FileBase() { close_file(); }

  // file descriptor accessors
  int get_fd() const { return fb_fd.load(std::memory_order_relaxed); }
  bool is_fd_valid() const { return get_fd() > 0; }
  bool is_fd_invalid() const { return get_fd() <= 0; }

  // size accessors
  void set_current_size(c3_ulong_t size) {
    fb_current_size.store(size, std::memory_order_relaxed);
  }
  void increment_current_size(c3_ulong_t delta) {
    fb_current_size.fetch_add(delta, std::memory_order_relaxed);
  }
  void reset_current_size() { set_current_size(0); }

  // maximum/threshold size accessors
  void set_max_size(c3_ulong_t size) {
    fb_max_size.store(size, std::memory_order_relaxed);
  }
  void reset_max_size() { set_max_size(0); }

  // opening and closing the file
  bool open_file(const char* path, file_mode_t mode = FM_READ, sync_mode_t sync = SM_NONE) C3_FUNC_COLD;
  bool close_file() C3_FUNC_COLD;

public:
  FileBase(FileBase&) = delete;
  FileBase(FileBase&&) = delete;

  void operator=(FileBase&) = delete;
  void operator=(FileBase&&) = delete;

  // information retrieval methods that can be called concurrently (from other threads)
  bool is_service_active() const { return get_fd() > 0; }
  c3_ulong_t get_current_size() const {
    return fb_current_size.load(std::memory_order_relaxed);
  }
  c3_ulong_t get_max_size() const {
    return fb_max_size.load(std::memory_order_relaxed);
  }
  c3_long_t get_available_space() const C3_FUNC_COLD;
};

} // CyberCache

#endif // _C3_FILE_BASE_H
