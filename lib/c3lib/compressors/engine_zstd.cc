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
#include "engine_zstd.h"
#include "compression/zstd/zstd.h"

namespace CyberCache {

const char* CompressorZstd::get_name() {
  return "Zstd";
}

size_t CompressorZstd::get_compressed_size(c3_uint_t size) {
  return ZSTD_compressBound(size);
}

c3_uint_t CompressorZstd::pack(const c3_byte_t* src, c3_uint_t src_size, c3_byte_t* dst, size_t dst_size,
  comp_level_t level, comp_data_t hint) {
  static_assert(CL_NUMBER_OF_ELEMENTS == 4, "Number of compression levels is not four");
  // see `ZSTD_compressionParameters` in `zstd_compress.c`
  static const int compression_levels[CL_NUMBER_OF_ELEMENTS] = {
    1,  // CL_FASTEST
    12, // CL_AVERAGE
    20, // CL_BEST
    22  // CL_EXTREME
  };
  size_t result = ZSTD_compress(dst, dst_size, src, src_size, compression_levels[level]);
  if (result < (size_t) src_size) {
    return (c3_uint_t) result;
  }
  return 0;
}

bool CompressorZstd::unpack(const c3_byte_t *src, c3_uint_t src_size, c3_byte_t *dst, c3_uint_t dst_size) {
  size_t result = ZSTD_decompress(dst, dst_size, src, src_size);
  return result == (size_t) dst_size;
}

} // CyberCache
