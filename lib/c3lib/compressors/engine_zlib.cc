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
#include "engine_zlib.h"
#include "compression/zlib/zlib.h"

namespace CyberCache {

const char* CompressorZlib::get_name() {
  return "Zlib";
}

size_t CompressorZlib::get_compressed_size(c3_uint_t size) {
  return compressBound(size);
}

c3_uint_t CompressorZlib::pack(const c3_byte_t* src, c3_uint_t src_size, c3_byte_t* dst, size_t dst_size,
  comp_level_t level, comp_data_t hint) {
  static_assert(CL_NUMBER_OF_ELEMENTS == 4, "Number of compression levels is not four");
  static const int compression_levels[CL_NUMBER_OF_ELEMENTS] = {
    1,                     // CL_FASTEST
    Z_DEFAULT_COMPRESSION, // CL_AVERAGE
    9,                     // CL_BEST
    9                      // CL_EXTREME
  };
  uLongf compressed_size = dst_size;
  int result = compress2(dst, &compressed_size, src, src_size, compression_levels[level]);
  if (result == Z_OK && compressed_size < (size_t) src_size) {
    return (c3_uint_t) compressed_size;
  }
  return 0;
}

bool CompressorZlib::unpack(const c3_byte_t *src, c3_uint_t src_size, c3_byte_t *dst, c3_uint_t dst_size) {
  uLongf uncompressed_size = dst_size;
  int result = uncompress(dst, &uncompressed_size, src, src_size);
  return result == Z_OK && uncompressed_size == (size_t) dst_size;
}

} // CyberCache
