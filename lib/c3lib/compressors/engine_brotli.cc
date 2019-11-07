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
#include <brotli/encode.h>
#include <brotli/decode.h>

#include "engine_brotli.h"

namespace CyberCache {

const char* CompressorBrotli::get_name() {
  return "Brotli";
}

size_t CompressorBrotli::get_compressed_size(c3_uint_t size) {
  return BrotliEncoderMaxCompressedSize(size);
}

c3_uint_t CompressorBrotli::pack(const c3_byte_t* src, c3_uint_t src_size, c3_byte_t* dst, size_t dst_size,
  comp_level_t level, comp_data_t hint) {
  BrotliEncoderMode mode = hint == CD_TEXT? BROTLI_MODE_TEXT: BROTLI_MODE_GENERIC;
  static_assert(CL_NUMBER_OF_ELEMENTS == 4, "Number of compression levels is not four");
  static const int brotli_quality[CL_NUMBER_OF_ELEMENTS] = {
    1,  // CL_FASTEST => FAST_TWO_PASS_COMPRESSION_QUALITY
    5,  // CL_AVERAGE => MIN_QUALITY_FOR_EXTENSIVE_REFERENCE_SEARCH, MIN_QUALITY_FOR_CONTEXT_MODELING
    11, // CL_BEST ==> MIN_QUALITY_FOR_RECOMPUTE_DISTANCE_PREFIXES + 1
    99  // CL_EXTREME => Brotli seems to accept any two-digit numbers, even though it doesn't matter much
  };
  size_t compressed_size = dst_size;
  BROTLI_BOOL result = BrotliEncoderCompress(brotli_quality[level], BROTLI_DEFAULT_WINDOW, mode,
    src_size, src, &compressed_size, dst);
  if (result == BROTLI_TRUE && compressed_size > 0 && compressed_size < (size_t) src_size) {
    return (c3_uint_t) compressed_size;
  }
  return 0;
}

bool CompressorBrotli::unpack(const c3_byte_t *src, c3_uint_t src_size, c3_byte_t *dst, c3_uint_t dst_size) {
  size_t uncompressed_size = dst_size;
  BrotliDecoderResult result = BrotliDecoderDecompress(src_size, src, &uncompressed_size, dst);
  return result == BROTLI_DECODER_RESULT_SUCCESS && uncompressed_size == (size_t) dst_size;
}

} // CyberCache
