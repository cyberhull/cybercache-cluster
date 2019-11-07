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
#include "io_handlers.h"

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// SocketResponseReader
///////////////////////////////////////////////////////////////////////////////

SocketResponseReader::SocketResponseReader(Memory& memory, int fd, c3_ipv4_t ipv4, SharedBuffers* sb):
  ResponseReader(memory, IO_FLAG_IS_RESPONSE | IO_FLAG_IS_READER | IO_FLAG_NETWORK, fd, ipv4, sb) {
}

SocketResponseReader::SocketResponseReader(Memory& memory, const ReaderWriter& rw, int fd, c3_ipv4_t ipv4):
  ResponseReader(memory, rw, IO_FLAG_IS_RESPONSE | IO_FLAG_IS_READER | IO_FLAG_NETWORK, fd, ipv4) {
}

c3_uint_t SocketResponseReader::get_object_size() const {
  return sizeof(SocketResponseReader);
}

///////////////////////////////////////////////////////////////////////////////
// SocketResponseWriter
///////////////////////////////////////////////////////////////////////////////

SocketResponseWriter::SocketResponseWriter(Memory& memory, int fd, c3_ipv4_t ipv4, SharedBuffers* sb):
  ResponseWriter(memory, IO_FLAG_IS_RESPONSE | IO_FLAG_NETWORK, fd, ipv4, sb) {
}

SocketResponseWriter::SocketResponseWriter(Memory& memory, const ReaderWriter& rw, int fd, c3_ipv4_t ipv4):
  ResponseWriter(memory, rw, IO_FLAG_IS_RESPONSE | IO_FLAG_NETWORK, fd, ipv4) {
}

c3_uint_t SocketResponseWriter::get_object_size() const {
  return sizeof(SocketResponseWriter);
}

///////////////////////////////////////////////////////////////////////////////
// SocketCommandReader
///////////////////////////////////////////////////////////////////////////////

SocketCommandReader::SocketCommandReader(Memory& memory, int fd, c3_ipv4_t ipv4, SharedBuffers* sb):
  CommandReader(memory, IO_FLAG_IS_READER | IO_FLAG_NETWORK, fd, ipv4, sb) {
}

SocketCommandReader::SocketCommandReader(Memory& memory, const ReaderWriter& rw, int fd, c3_ipv4_t ipv4):
  CommandReader(memory, rw, IO_FLAG_IS_READER | IO_FLAG_NETWORK, fd, ipv4) {
}

c3_uint_t SocketCommandReader::get_object_size() const {
  return sizeof(SocketCommandReader);
}

///////////////////////////////////////////////////////////////////////////////
// SocketCommandWriter
///////////////////////////////////////////////////////////////////////////////

SocketCommandWriter::SocketCommandWriter(Memory& memory, int fd, c3_ipv4_t ipv4, SharedBuffers* sb):
  // "new" commands can only be created outside of the server where lockable hash objects are not available
  CommandWriter(memory, IO_FLAG_NETWORK, fd, ipv4, sb) {
}

SocketCommandWriter::SocketCommandWriter(Memory& memory, const ReaderWriter& rw, int fd, c3_ipv4_t ipv4):
  CommandWriter(memory, rw, IO_FLAG_NETWORK, fd, ipv4) {
}

c3_uint_t SocketCommandWriter::get_object_size() const {
  return sizeof(SocketCommandWriter);
}

///////////////////////////////////////////////////////////////////////////////
// FileResponseReader
///////////////////////////////////////////////////////////////////////////////

#if INCLUDE_FILERESPONSEREADER

FileResponseReader::FileResponseReader(Memory& memory, int fd, SharedBuffers* sb):
  ResponseReader(memory, IO_FLAG_IS_RESPONSE | IO_FLAG_IS_READER, fd, INVALID_IPV4_ADDRESS, sb) {
}

FileResponseReader::FileResponseReader(Memory& memory, const ReaderWriter& rw, int fd):
  ResponseReader(memory, rw, IO_FLAG_IS_RESPONSE | IO_FLAG_IS_READER, fd, INVALID_IPV4_ADDRESS) {
}

c3_uint_t FileResponseReader::get_object_size() const {
  return sizeof(FileResponseReader);
}

#endif // INCLUDE_FILERESPONSEREADER

///////////////////////////////////////////////////////////////////////////////
// FileResponseWriter
///////////////////////////////////////////////////////////////////////////////

#if INCLUDE_FILERESPONSEWRITER

FileResponseWriter::FileResponseWriter(Memory& memory, int fd, SharedBuffers* sb):
  ResponseWriter(memory, IO_FLAG_IS_RESPONSE, fd, INVALID_IPV4_ADDRESS, sb) {
}

FileResponseWriter::FileResponseWriter(Memory& memory, const ReaderWriter& rw, int fd):
  ResponseWriter(memory, rw, IO_FLAG_IS_RESPONSE, fd, INVALID_IPV4_ADDRESS) {
}

c3_uint_t FileResponseWriter::get_object_size() const {
  return sizeof(FileResponseWriter);
}

#endif // INCLUDE_FILERESPONSEWRITER

///////////////////////////////////////////////////////////////////////////////
// FileCommandReader
///////////////////////////////////////////////////////////////////////////////

FileCommandReader::FileCommandReader(Memory& memory, int fd, SharedBuffers* sb):
  CommandReader(memory, IO_FLAG_IS_READER, fd, INVALID_IPV4_ADDRESS, sb) {
}

FileCommandReader::FileCommandReader(Memory& memory, const ReaderWriter& rw, int fd):
  CommandReader(memory, rw, IO_FLAG_IS_READER, fd, INVALID_IPV4_ADDRESS) {
}

c3_uint_t FileCommandReader::get_object_size() const {
  return sizeof(FileCommandReader);
}

///////////////////////////////////////////////////////////////////////////////
// FileCommandWriter
///////////////////////////////////////////////////////////////////////////////

FileCommandWriter::FileCommandWriter(Memory& memory, int fd, SharedBuffers* sb):
// "new" commands can only be created outside of the server where lockable hash objects are not available
  CommandWriter(memory, 0, fd, INVALID_IPV4_ADDRESS, sb) {
}

FileCommandWriter::FileCommandWriter(Memory& memory, const ReaderWriter& rw, int fd):
  CommandWriter(memory, rw, 0, fd, INVALID_IPV4_ADDRESS) {
}

c3_uint_t FileCommandWriter::get_object_size() const {
  return sizeof(FileCommandWriter);
}

} // CyberCache
