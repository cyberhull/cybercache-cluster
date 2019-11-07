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
 * Classes implementing low-level I/O.
 *
 * It is very important that reading/writing methods of these classes *ONLY* return 'OK' result if at least
 * one byte was successfully read or written.
 */
#ifndef _IO_DEVICE_HANDLERS_H
#define _IO_DEVICE_HANDLERS_H

#include "io_reader_writer.h"

namespace CyberCache {

/// Class implementing low-level reading from a TCP/IP socket
class SocketReader: virtual public DeviceReaderWriter {
public:
  io_result_t read_bytes(int fd, c3_byte_t* buff, c3_uint_t nbytes, c3_uint_t &nread) const override;
};

/// Class implementing low-level writing to a TCP/IP socket
class SocketWriter: virtual public DeviceReaderWriter {
  io_result_t write_bytes(int fd, const c3_byte_t* buff, c3_uint_t nbytes, c3_uint_t &nwritten) const override;
};

/// Class implementing low-level reading from a file (e.g. during restoration from binlog)
class FileReader: virtual public DeviceReaderWriter {
public:
  io_result_t read_bytes(int fd, c3_byte_t* buff, c3_uint_t nbytes, c3_uint_t &nread) const override;
};

/// Class implementing low-level writing to a file (e.g. to a binlog)
class FileWriter: virtual public DeviceReaderWriter {
  io_result_t write_bytes(int fd, const c3_byte_t* buff, c3_uint_t nbytes, c3_uint_t &nwritten) const override;
};

} // CyberCache

#endif // _IO_DEVICE_HANDLERS_H
