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
 * Implementation of the Zlib (gzip) compression engine.
 */
#ifndef _ENGINE_ZLIB_H
#define _ENGINE_ZLIB_H

#include "c3lib/c3_compressor.h"

namespace CyberCache {

/**
 * Wrapper around Zlib/Gzip compressor.
 */
class CompressorZlib: public CompressorEngine {
  friend class CompressorLibrary;

  const char* get_name() override;
  size_t get_compressed_size(c3_uint_t size) override;
  c3_uint_t pack(const c3_byte_t* src, c3_uint_t src_size, c3_byte_t* dst, size_t dst_size,
    comp_level_t level, comp_data_t hint) override;
  bool unpack(const c3_byte_t *src, c3_uint_t src_size, c3_byte_t *dst, c3_uint_t dst_size) override;
};

} // CyberCache

#endif // _ENGINE_ZLIB_H
