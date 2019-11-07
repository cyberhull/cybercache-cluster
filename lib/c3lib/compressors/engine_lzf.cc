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
#include "engine_lzf.h"

extern "C" {
#include "compression/lzf/lzf.h"
}

namespace CyberCache {

const char* CompressorLzf::get_name() {
  return "LZF";
}

size_t CompressorLzf::get_compressed_size(c3_uint_t size) {
  return size;
}

c3_uint_t CompressorLzf::pack(const c3_byte_t* src, c3_uint_t src_size, c3_byte_t* dst, size_t dst_size,
  comp_level_t level, comp_data_t hint) {
  c3_assert(dst_size <= UINT_MAX_VAL); // `get_compressed_size()` should guarantee it
  c3_uint_t result = lzf_compress(src, src_size, dst, (c3_uint_t) dst_size);
  if (result < src_size) {
    return result;
  }
  return 0;
}

bool CompressorLzf::unpack(const c3_byte_t *src, c3_uint_t src_size, c3_byte_t *dst, c3_uint_t dst_size) {
  c3_uint_t result = lzf_decompress (src, src_size, dst, dst_size);
  return result == dst_size;
}

} // CyberCache
