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
#include "engine_snappy.h"
#include "compression/snappy/snappy.h"

namespace CyberCache {

const char* CompressorSnappy::get_name() {
  return "Snappy";
}

size_t CompressorSnappy::get_compressed_size(c3_uint_t size) {
  return snappy::MaxCompressedLength(size);
}

c3_uint_t CompressorSnappy::pack(const c3_byte_t* src, c3_uint_t src_size, c3_byte_t* dst, size_t dst_size,
  comp_level_t level, comp_data_t hint) {
  size_t compressed_size;
  snappy::RawCompress((const char*) src, src_size, (char*) dst, &compressed_size);
  if (compressed_size < (size_t) src_size) {
    return (c3_uint_t) compressed_size;
  }
  return 0;
}

bool CompressorSnappy::unpack(const c3_byte_t *src, c3_uint_t src_size, c3_byte_t *dst, c3_uint_t dst_size) {
  return snappy::RawUncompress((const char*) src, src_size, (char*) dst);
}

} // CyberCache
