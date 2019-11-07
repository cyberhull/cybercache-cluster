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
 * Implementation of the LZSS3 compression engine.
 */
#ifndef _ENGINE_LZSS3_H
#define _ENGINE_LZSS3_H

#include "c3lib/c3_compressor.h"

namespace CyberCache {

/**
 * Third version of LZSS compressor based on LZSS by Haruhiko Okumura.
 */
class CompressorLZSS3: public CompressorEngine {
  friend class CompressorLibrary;

  /// Size of ring buffer
  static const c3_uint_t LZSS_SIZE = 4096;
  /// Upper limit for lzss_match_length
  static const c3_uint_t LZSS_HI = 18;
  /// Encode string into position/length if lzss_match_length is greater than this
  static const c3_uint_t LZSS_LO = 2;
  /// Index for root of binary search trees
  static const c3_uint_t LZSS_NIL = LZSS_SIZE;

  /// Ring buffer of size LZSS_SIZE, with extra LZSS_HI-1 bytes to facilitate string comparison
  c3_byte_t lzss_ring_buffer[LZSS_SIZE + LZSS_HI - 1];
  /// Length of the longest match; set by the insert_node()
  c3_uint_t lzss_match_length;
  /// Position of the longest match; set by the insert_node()
  c3_uint_t lzss_match_position;

  /// "left children" in the binary search trees
  c3_uint_t lzss_lchild[LZSS_SIZE + 1];
  /// "right children" in the binary search trees
  c3_uint_t lzss_rchild[LZSS_SIZE + 257];
  /// "parents" in the binary search trees
  c3_uint_t lzss_parent[LZSS_SIZE + 1];

  const char* get_name() override;
  size_t get_compressed_size(c3_uint_t size) override;
  void initialize_trees();
  void insert_node(c3_uint_t node);
  void delete_node(c3_uint_t node);
  c3_uint_t pack(const c3_byte_t* src, c3_uint_t src_size, c3_byte_t* dst, size_t dst_size,
    comp_level_t level, comp_data_t hint) override;
  bool unpack(const c3_byte_t *src, c3_uint_t src_size, c3_byte_t *dst, c3_uint_t dst_size) override;
};

} // CyberCache

#endif // _ENGINE_LZSS3_H
