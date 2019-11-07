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
 * Classes implementing high-level I/O.
 *
 * If an I/O object is being created based on an existing one, it is caller's responsibility to either
 * provide a valid file/socket descriptor suitable for the object, or make sure that new and reference
 * object operate on the same type of device (i.e. both work with files, or both work with TCP/IP
 * sockets). `ReaderWriter` ctor will check this condition, but if there is a mismatch, it won't be able to
 * recover, and will leave the new object in invalid state (which can be tested using `is_valid()`).
 */
#ifndef _IO_HANDLERS_H
#define _IO_HANDLERS_H

#include "io_device_handlers.h"
#include "io_command_handlers.h"
#include "io_response_handlers.h"

// whether to compile `FileResponseReader` class
#define INCLUDE_FILERESPONSEREADER 0
// whether to compile `FileResponseWriter` class
#define INCLUDE_FILERESPONSEWRITER 0

namespace CyberCache {

/**
 * Handle to pass to command writers to create them in "inactive" state. A call to `io_rewind()` will
 * then be necessary to activate the [socket or file] command writer.
 */
constexpr int INACTIVE_HANDLE = 0;

/// Class implementing reading responses from TCP/IP sockets
class SocketResponseReader: public ResponseReader, public SocketReader {
public:
  SocketResponseReader(Memory& memory, int fd, c3_ipv4_t ipv4, SharedBuffers* sb);
  SocketResponseReader(Memory& memory, const ReaderWriter& rw,
    int fd = -1, c3_ipv4_t ipv4 = INVALID_IPV4_ADDRESS);
  SocketResponseReader(const ReaderWriter& rw, bool full): ResponseReader(rw, full) {}

  c3_uint_t get_object_size() const override;
};

/// Class implementing writing responses to TCP/IP sockets
class SocketResponseWriter: public ResponseWriter, public SocketWriter {
public:
  SocketResponseWriter(Memory& memory, int fd, c3_ipv4_t ipv4, SharedBuffers* sb);
  SocketResponseWriter(Memory& memory, const ReaderWriter& rw,
    int fd = -1, c3_ipv4_t ipv4 = INVALID_IPV4_ADDRESS);
  SocketResponseWriter(const ReaderWriter& rw, bool full): ResponseWriter(rw, full) {}

  c3_uint_t get_object_size() const override;
};

/// Class implementing reading commands from TCP/IP sockets
class SocketCommandReader: public CommandReader, public SocketReader {
public:
  SocketCommandReader(Memory& memory, int fd, c3_ipv4_t ipv4, SharedBuffers* sb);
  SocketCommandReader(Memory& memory, const ReaderWriter& rw,
    int fd = -1, c3_ipv4_t ipv4 = INVALID_IPV4_ADDRESS);
  SocketCommandReader(const ReaderWriter& rw, bool full): CommandReader(rw, full) {}

  c3_uint_t get_object_size() const override;
};

/// Class implementing writing commands to TCP/IP sockets
class SocketCommandWriter: public CommandWriter, public SocketWriter {
public:
  SocketCommandWriter(Memory& memory, int fd, c3_ipv4_t ipv4, SharedBuffers* sb);
  SocketCommandWriter(Memory& memory, const ReaderWriter& rw,
    int fd = -1, c3_ipv4_t ipv4 = INVALID_IPV4_ADDRESS);
  SocketCommandWriter(const ReaderWriter& rw, bool full): CommandWriter(rw, full) {}

  c3_uint_t get_object_size() const override;
};

// the following is necessary for some memory usage optimizations
static_assert(sizeof(SocketCommandReader) == sizeof(SocketResponseWriter),
  "Sizes of 'SocketCommandReader' and 'SocketResponseWriter' differ");

#if INCLUDE_FILERESPONSEREADER
/// Class implementing reading responses from files (we do not write responses to the binlog, so this
/// class will likely be never used; provided for the sake of implementation completeness).
class FileResponseReader: public ResponseReader, public FileReader {
public:
  FileResponseReader(Memory& memory, int fd, SharedBuffers* sb);
  FileResponseReader(Memory& memory, const ReaderWriter& rw, int fd = -1);
  FileResponseReader(const ReaderWriter& rw, bool full): ResponseReader(rw, full) {}

  c3_uint_t get_object_size() const override;
};
#endif // INCLUDE_FILERESPONSEREADER

#if INCLUDE_FILERESPONSEWRITER
/// Class implementing writing responses to files (we do not write responses to the binlog, so this
/// class will likely be never used; provided for the sake of implementation completeness).
class FileResponseWriter: public ResponseWriter, public FileWriter {
public:
  FileResponseWriter(Memory& memory, int fd, SharedBuffers* sb);
  FileResponseWriter(Memory& memory, const ReaderWriter& rw, int fd = -1);
  FileResponseWriter(const ReaderWriter& rw, bool full): ResponseWriter(rw, full) {}

  c3_uint_t get_object_size() const override;
};
#endif // INCLUDE_FILERESPONSEWRITER

/// Class implementing reading commands from [binlog] files
class FileCommandReader: public CommandReader, public FileReader {
public:
  FileCommandReader(Memory& memory, int fd, SharedBuffers* sb);
  FileCommandReader(Memory& memory, const ReaderWriter& rw, int fd = -1);
  FileCommandReader(const ReaderWriter& rw, bool full): CommandReader(rw, full) {}

  c3_uint_t get_object_size() const override;
};

/// Class implementing writing commands to [binlog] files
class FileCommandWriter: public CommandWriter, public FileWriter {
public:
  FileCommandWriter(Memory& memory, int fd, SharedBuffers* sb);
  FileCommandWriter(Memory& memory, const ReaderWriter& rw, int fd = -1);
  FileCommandWriter(const ReaderWriter& rw, bool full): CommandWriter(rw, full) {}

  c3_uint_t get_object_size() const override;
};

} // CyberCache

#endif // _IO_HANDLERS_H
