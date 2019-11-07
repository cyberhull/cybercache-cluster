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
#include "engine_lz4.h"
#include "compression/lz4/lz4.h"

namespace CyberCache {

const char* CompressorLz4::get_name() {
  return "LZ4";
}

size_t CompressorLz4::get_compressed_size(c3_uint_t size) {
  // returns 0 if `size` > LZ4_MAX_INPUT_SIZE
  return size <= INT_MAX_VAL? (size_t) LZ4_compressBound(size): 0;
}

c3_uint_t CompressorLz4::pack(const c3_byte_t* src, c3_uint_t src_size, c3_byte_t* dst, size_t dst_size,
  comp_level_t level, comp_data_t hint) {
  // from `lz4.h`: "... each successive value providing roughly +~3% to speed"
  static_assert(CL_NUMBER_OF_ELEMENTS == 4, "Number of compression levels is not four");
  static const int compression_accelerations[CL_NUMBER_OF_ELEMENTS] = {
    5, // CL_FASTEST
    2,  // CL_AVERAGE
    1,  // CL_BEST
    1   // CL_EXTREME
  };
  if (dst_size <= INT_MAX_VAL) {
    int result = LZ4_compress_fast((const char*) src, (char*) dst, src_size, (int) dst_size,
      compression_accelerations[level]);
    if ((size_t) result < src_size) {
      // `result` == 0 indicates an error for both LZ4 and our `pack()`
      return (c3_uint_t) result;
    }
  }
  return 0;
}

bool CompressorLz4::unpack(const c3_byte_t *src, c3_uint_t src_size, c3_byte_t *dst, c3_uint_t dst_size) {
  int result = LZ4_decompress_safe((const char*) src, (char*) dst, src_size, dst_size);
  return result > 0 && (size_t) result == dst_size;
}

} // CyberCache
