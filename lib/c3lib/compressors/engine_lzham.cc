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
#include "engine_lzham.h"
#include "compression/lzham/lzham.h"

namespace CyberCache {

const char* CompressorLzham::get_name() {
  return "Lzham";
}

size_t CompressorLzham::get_compressed_size(c3_uint_t size) {
  return size;
}

c3_uint_t CompressorLzham::pack(const c3_byte_t* src, c3_uint_t src_size, c3_byte_t* dst, size_t dst_size,
  comp_level_t level, comp_data_t hint) {

  static_assert(CL_NUMBER_OF_ELEMENTS == 4, "Number of compression levels is not four");
  /*
   * From README.md: "the m_table_update_rate decompression parameter MUST match the setting used during
   * compression (same for the dictionary size)". Since we do not store compression settings anywhere,
   * the `m_table_update_rate` and `m_dict_size_log2` must be set to some reasonable permanent values
   * that do not depend on compression level.
   */
  lzham_compress_params params;
  params.m_struct_size = sizeof(params);
  params.m_dict_size_log2 = 20; // 1Mb, *must* match decompressor setting
  params.m_level = LZHAM_COMP_LEVEL_FASTEST;
  params.m_table_update_rate = LZHAM_DEFAULT_TABLE_UPDATE_RATE; // *must* match decompressor setting
  params.m_max_helper_threads = 0;
  params.m_compress_flags = 0;
  params.m_num_seed_bytes = 0;
  params.m_pSeed_bytes = nullptr;
  params.m_table_max_update_interval = 0; // == "don't care"
  params.m_table_update_interval_slow_rate = 0; // == "don't care"
  switch (level) {
    case CL_FASTEST:
      params.m_level = LZHAM_COMP_LEVEL_FASTEST;
      break;
    case CL_AVERAGE:
      params.m_level = LZHAM_COMP_LEVEL_DEFAULT;
      break;
    case CL_BEST:
      params.m_level = LZHAM_COMP_LEVEL_BETTER;
      break;
    case CL_EXTREME:
      params.m_level = LZHAM_COMP_LEVEL_UBER;
      params.m_compress_flags = LZHAM_COMP_FLAG_EXTREME_PARSING;
      break;
    default:
      c3_assert_failure();
  }
  size_t compressed_size = dst_size;
  lzham_compress_status_t result = lzham_compress_memory(&params, dst, &compressed_size,
    src, src_size, nullptr /* Adler32 checksum */);
  if (result == LZHAM_COMP_STATUS_SUCCESS && compressed_size < (size_t) src_size) {
    return (c3_uint_t) compressed_size;
  }
  return 0;
}

bool CompressorLzham::unpack(const c3_byte_t *src, c3_uint_t src_size, c3_byte_t *dst, c3_uint_t dst_size) {

  lzham_decompress_params params;
  params.m_struct_size = sizeof(params);
  params.m_dict_size_log2 = 20; // 1Mb, *must* match compressor setting
  params.m_table_update_rate = LZHAM_DEFAULT_TABLE_UPDATE_RATE; // *must* match compressor setting
  params.m_decompress_flags = LZHAM_DECOMP_FLAG_OUTPUT_UNBUFFERED;
  params.m_num_seed_bytes = 0;
  params.m_pSeed_bytes = nullptr;
  params.m_table_max_update_interval = 0; // == "don't care"
  params.m_table_update_interval_slow_rate = 0; // == "don't care"

  size_t uncompressed_size = dst_size;
  lzham_decompress_status_t result = lzham_decompress_memory(&params, dst, &uncompressed_size,
    src, src_size, nullptr /* Adler32 checksum */);
  return result == LZHAM_DECOMP_STATUS_SUCCESS && uncompressed_size == (size_t) dst_size;
}

} // CyberCache
