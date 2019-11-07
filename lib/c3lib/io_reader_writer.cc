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
#include "io_reader_writer.h"
#include "c3_profiler_defs.h"

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// DeviceReaderWriter
///////////////////////////////////////////////////////////////////////////////

io_result_t DeviceReaderWriter::read_bytes(int fd, c3_byte_t* buff, c3_uint_t nbytes, c3_uint_t &nread) const {
  assert_failure();
  nread = 0;
  return IO_RESULT_ERROR;
}

io_result_t DeviceReaderWriter::write_bytes(int fd, const c3_byte_t* buff, c3_uint_t nbytes,
  c3_uint_t& nwritten) const {
  assert_failure();
  nwritten = 0;
  return IO_RESULT_ERROR;
}

///////////////////////////////////////////////////////////////////////////////
// ReaderWriter
///////////////////////////////////////////////////////////////////////////////

ReaderWriter::ReaderWriter(Memory& memory, c3_byte_t flags, int fd, c3_ipv4_t ipv4, SharedBuffers* sb):
  rw_fd(fd), rw_ipv4(ipv4), rw_domain(memory.get_domain()), rw_flags(flags) {
  c3_assert(sb && fd >= 0);
  PERF_INCREMENT_VAR_DOMAIN_COUNTER(rw_domain, IO_Objects_Active)
  PERF_INCREMENT_VAR_DOMAIN_COUNTER(rw_domain, IO_Objects_Created)
  rw_sb = sb;
  rw_sb->add_reference();
  rw_pos = 0;
  rw_remains = 0;
  rw_state = IO_STATE_CREATED;
}

ReaderWriter::ReaderWriter(Memory& memory, const ReaderWriter& rw, c3_byte_t flags, int fd, c3_ipv4_t ipv4):
  rw_fd(fd != -1? fd: rw.rw_fd), rw_ipv4(ipv4 != INVALID_IPV4_ADDRESS? ipv4: rw.rw_ipv4),
  rw_domain(memory.get_domain()), rw_flags(flags) {
  /*
   * Make sure that either valid `fd` is provided, or the object being created and reference object
   * operate on the same type of the device (i.e. both work with files, or both work with TCP/IP sockets),
   * so that we wouldn't end up trying to use file descriptor for network I/O, or vice versa.
   */
  c3_assert(rw_fd >= 0 && (fd != -1 || (flags & IO_FLAG_NETWORK) == (rw.rw_flags & IO_FLAG_NETWORK)));
  c3_assert(rw.rw_sb);
  PERF_INCREMENT_VAR_DOMAIN_COUNTER(rw_domain, IO_Objects_Active)
  PERF_INCREMENT_VAR_DOMAIN_COUNTER(rw_domain, IO_Objects_Copied)
  rw_sb = rw.rw_sb;
  rw_sb->add_reference();
  rw_pos = 0;
  rw_remains = 0;
  rw_state = IO_STATE_CREATED;
}

ReaderWriter::ReaderWriter(const ReaderWriter &rw, bool full): rw_domain(rw.rw_domain), rw_flags(rw.rw_flags) {
  c3_assert(rw.rw_sb);
  PERF_INCREMENT_VAR_DOMAIN_COUNTER(rw_domain, IO_Objects_Active)
  PERF_INCREMENT_VAR_DOMAIN_COUNTER(rw_domain, IO_Objects_Cloned)
  rw_sb = rw.rw_sb->clone(full);
  rw_sb->add_reference();
  rw_fd = rw.rw_fd;
  rw_ipv4 = rw.rw_ipv4;
  rw_pos = rw.rw_pos;
  rw_remains = rw.rw_remains;
  rw_state = rw.rw_state;
}

ReaderWriter::~ReaderWriter() {
  c3_assert(rw_sb);
  PERF_DECREMENT_VAR_DOMAIN_COUNTER(rw_domain, IO_Objects_Active)
  SharedBuffers::remove_reference(rw_sb);
}

void ReaderWriter::dispose(ReaderWriter* rw) {
  assert(rw);
  c3_uint_t size = rw->get_object_size();
  Memory& memory = rw->get_memory_object();
  rw->~ReaderWriter(); // call dtor
  memory.free(rw, size);
}

io_result_t ReaderWriter::set_error_state() {
  rw_pos = UINT_MAX_VAL;
  rw_remains = 0;
  rw_state = IO_STATE_ERROR;
  return IO_RESULT_ERROR;
}

Memory& ReaderWriter::get_memory_object() const {
  return Memory::get_memory_object(rw_domain);
}

io_result_t ReaderWriter::read(c3_ulong_t& ntotal) {
  ntotal = 0;
  assert_failure();
  return IO_RESULT_ERROR;
}

io_result_t ReaderWriter::write(c3_ulong_t& ntotal) {
  ntotal = 0;
  assert_failure();
  return IO_RESULT_ERROR;
}

void ReaderWriter::io_rewind(int fd, c3_ipv4_t ipv4) {
  // only command writers are allowed to reset their state
  assert_failure();
}

} // CyberCache
