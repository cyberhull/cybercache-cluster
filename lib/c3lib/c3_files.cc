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
#include "c3_errors.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <cstdio>
#include <cstdlib>
#include <pwd.h>

namespace CyberCache {

bool c3_file_access(const char* path, access_mode_t mode) {
  if (path != nullptr && (mode & ~0x0F) == 0) {
    int mode_mask;
    if (mode == AM_EXISTS) {
      mode_mask = F_OK;
    } else {
      mode_mask = 0;
      if ((mode & AM_READABLE) != 0) {
        mode_mask = R_OK;
      }
      if ((mode & AM_WRITABLE) != 0) {
        mode_mask |= W_OK;
      }
      if ((mode & AM_EXECUTABLE) != 0) {
        mode_mask |= X_OK;
      }
    }
    return access(path, mode_mask) == 0;
  } else {
    c3_set_einval_error_message();
    return false;
  }
}

char *c3_get_home_path(char *buff, size_t size, const char *path) {
  assert(buff && size);
  const char* home_dir = getenv("HOME");
  if (home_dir == nullptr) {
    const passwd* pwd_data = getpwuid(getuid());
    if (pwd_data != nullptr) {
      home_dir = pwd_data->pw_dir;
    } else {
      home_dir = ".";
    }
  }
  if (path != nullptr) {
    snprintf(buff, size, "%s/%s", home_dir, path);
  } else {
    snprintf(buff, size, "%s", home_dir);
  }
  return buff;
}

c3_long_t c3_get_file_size(const char* path) {
  if (path != nullptr) {
    struct stat stats;
    if (stat(path, &stats) == 0) {
      return stats.st_size;
    } else {
      return c3_set_stdlib_error_message();
    }
  } else {
    return c3_set_einval_error_message();
  }
}

c3_long_t c3_get_file_size(int fd) {
  if (fd > 0) {
    struct stat stats;
    if (fstat(fd, &stats) == 0) {
      return stats.st_size;
    } else {
      return c3_set_stdlib_error_message();
    }
  } else {
    return c3_set_einval_error_message();
  }
}

static c3_long_t get_free_disk_space(struct statvfs& stats) {
  // get free space either for privileged or regular user depending on "effective user" running the app
  return (c3_long_t)(geteuid() == 0? stats.f_bfree: stats.f_bavail) * (c3_long_t) stats.f_bsize;
}

#if INCLUDE_C3_GET_FREE_DISK_SPACE
c3_long_t c3_get_free_disk_space(const char* path) {
  if (path != nullptr) {
    struct statvfs stats;
    if (statvfs(path, &stats) == 0) {
      return get_free_disk_space(stats);
    } else {
      return c3_set_stdlib_error_message();
    }
  } else {
    return c3_set_einval_error_message();
  }
}
#endif // INCLUDE_C3_GET_FREE_DISK_SPACE

c3_long_t c3_get_free_disk_space(int fd) {
  if (fd > 0) {
    struct statvfs stats;
    if (fstatvfs(fd, &stats) == 0) {
      return get_free_disk_space(stats);
    } else {
      return c3_set_stdlib_error_message();
    }
  } else {
    return c3_set_einval_error_message();
  }
}

int c3_open_file(const char* path, file_mode_t mode, sync_mode_t sync) {
  if (path != nullptr && mode >= FM_READ && mode <= FM_APPEND && (mode != FM_READ || sync == SM_NONE)) {
    int access_mode;
    mode_t mode_flags;
    switch (mode) {
      case FM_READ:
        access_mode = O_RDONLY;
        mode_flags = 0;
        break;
      case FM_READWRITE:
        access_mode = O_RDWR;
        mode_flags = 0;
        break;
      case FM_CREATE:
        access_mode = O_CREAT | O_WRONLY | O_TRUNC; // equivalent of creat()
        mode_flags = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
        break;
      default: // FM_APPEND
        access_mode = O_CREAT | O_WRONLY | O_APPEND;
        mode_flags = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
    }
    switch (sync) {
      /*
       * From http://man7.org/linux/man-pages/man2/open.2.html:
       *
       * Before Linux 2.6.33, Linux implemented only the O_SYNC flag for open(). However, when that flag
       * was specified, most filesystems actually provided the equivalent of synchronized I/O data
       * integrity completion (i.e., O_SYNC was actually implemented as the equivalent of O_DSYNC).
       *
       * Since Linux 2.6.33, proper O_SYNC support is provided. However, to ensure backward binary
       * compatibility, O_DSYNC was defined with the same value as the historical O_SYNC, and O_SYNC was
       * defined as a new (two-bit) flag value that includes the O_DSYNC flag value. This ensures that
       * applications compiled against new headers get at least O_DSYNC semantics on pre-2.6.33 kernels.
       */
      case SM_DATA_ONLY:
        access_mode |= O_DSYNC;
        break;
      case SM_FULL:
        access_mode |= O_SYNC;
        break;
      default:
        ; // just to satisfy the compiler
    }
    int fd = open(path, access_mode, mode_flags);
    if (fd > 0) {
      return fd;
    } else {
      return c3_set_stdlib_error_message();
    };
  } else {
    return c3_set_einval_error_message();
  }
}

c3_long_t c3_seek_file(int fd, c3_long_t pos, position_mode_t from) {
  if (fd > 0 && from >= SEEK_SET && from <= SEEK_END) {
    c3_long_t result = lseek(fd, pos, from);
    if (result >= 0) {
      return result;
    } else {
      return c3_set_stdlib_error_message();
    }
  } else {
    return c3_set_einval_error_message();
  }
}

ssize_t c3_read_file(int fd, void *buffer, size_t size) {
  if (fd > 0 && buffer != nullptr && size > 0) {
    ssize_t result = read(fd, buffer, size);
    if (result >= 0) {
      return result;
    } else {
      return c3_set_stdlib_error_message();
    }
  } else {
    return c3_set_einval_error_message();
  }
}

ssize_t c3_write_file(int fd, const void* buffer, size_t size) {
  if (fd > 0 && buffer != nullptr && size > 0) {
    ssize_t result = write(fd, buffer, size);
    if (result >= 0) {
      return result;
    } else {
      return c3_set_stdlib_error_message();
    }
  } else {
    return c3_set_einval_error_message();
  }
}

bool c3_save_file(const char* path, const void* buffer, size_t size) {
  // it is possible to just create zero-length files
  if (path != nullptr && ((buffer != nullptr && size > 0) || (buffer == nullptr && size == 0))) {
    int fd = c3_open_file(path, FM_CREATE);
    if (fd > 0) {
      bool result = size > 0? c3_write_file(fd, buffer, size) == (ssize_t) size: true;
      return c3_close_file(fd) && result;
    }
  } else {
    c3_set_einval_error_message();
  }
  return false;
}

void* c3_load_file(const char *path, size_t &size, Memory& memory) {
  size = 0;
  char* buffer = nullptr;
  if (path != nullptr) {
    int fd = c3_open_file(path);
    if (fd > 0) {
      size_t buffer_size = 0;
      c3_long_t file_size = c3_get_file_size(fd);
      if (file_size >= 0) {
        if (file_size == 0) {
          buffer_size = 1;
          buffer = (char*) memory.alloc(buffer_size);
          buffer[0] = 0;
          // `size` had already been set to zero
        } else if ((c3_ulong_t) file_size > (c3_ulong_t) SIZE_MAX) {
          c3_set_error_message("c3_load_file('%s'): file too big (%llu bytes)", path, file_size);
          // buffer had already been set to `NULL`, and `size` to zero
        } else {
          buffer_size = (size_t) file_size + 1;
          buffer = (char*) memory.alloc(buffer_size);
          ssize_t read = c3_read_file(fd, buffer, (size_t) file_size);
          if (read == file_size) {
            buffer[file_size] = 0;
            size = (size_t) file_size;
          } else {
            memory.free(buffer, buffer_size);
            buffer = nullptr;
          }
        }
      }
      if (!c3_close_file(fd)) {
        if (buffer != nullptr) {
          memory.free(buffer, buffer_size);
          buffer = nullptr;
          size = 0;
        }
      }
    }
  } else {
    c3_set_einval_error_message();
  }
  return buffer;
}

bool c3_rename_file(const char *src_path, const char *dst_path) {
  if (src_path != nullptr && dst_path != nullptr) {
    if (rename(src_path, dst_path) != 0) {
      c3_set_stdlib_error_message();
      return false;
    } else {
      return true;
    }
  } else {
    c3_set_einval_error_message();
    return false;
  }
}

bool c3_close_file(int fd) {
  if (fd > 0) {
    if (close(fd) != 0) {
      c3_set_stdlib_error_message();
      return false;
    }
    return true;
  } else {
    c3_set_einval_error_message();
    return false;
  }
}

bool c3_delete_file(const char* path) {
  if (path != nullptr && *path != '\0') {
    if (unlink(path) != 0) {
      c3_set_stdlib_error_message();
      return false;
    }
    return true;
  } else {
    c3_set_einval_error_message();
    return false;
  }
}

} // CyberCache
