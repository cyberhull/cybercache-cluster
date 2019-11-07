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
#include <new>
#include <cstring>

#include "c3_compressor.h"
#include "c3lib/compressors/engine_lzss3.h"
#include "c3lib/compressors/engine_snappy.h"
#include "c3lib/compressors/engine_zlib.h"
#include "c3lib/compressors/engine_zstd.h"
#include "c3lib/compressors/engine_lz4.h"
#include "c3lib/compressors/engine_lzf.h"
#if C3_ENTERPRISE
#include "c3lib/compressors/engine_brotli.h"
#endif
#include "c3lib/compressors/engine_lzham.h"

namespace CyberCache {

CompressorLibrary global_compressor;

thread_local CompressorEngine* CompressorLibrary::cl_engines[CT_NUMBER_OF_ELEMENTS];
thread_local c3_byte_t*        CompressorLibrary::cl_buffer;
thread_local size_t            CompressorLibrary::cl_buff_size;

CompressorEngine* CompressorLibrary::get_engine(c3_compressor_t type) const {
  CompressorEngine* engine = nullptr;
  assert(type < CT_NUMBER_OF_ELEMENTS);
  engine = cl_engines[type];
  if (engine == nullptr) {
    switch (type) {
      case CT_LZSS3:
        engine = instantiate_engine<CompressorLZSS3>();
        break;
      case CT_SNAPPY:
        engine = instantiate_engine<CompressorSnappy>();
        break;
      case CT_ZLIB:
        engine = instantiate_engine<CompressorZlib>();
        break;
      case CT_ZSTD:
        engine = instantiate_engine<CompressorZstd>();
        break;
      case CT_LZ4:
        engine = instantiate_engine<CompressorLz4>();
        break;
      case CT_LZF:
        engine = instantiate_engine<CompressorLzf>();
        break;
      #if C3_ENTERPRISE
      case CT_BROTLI:
        engine = instantiate_engine<CompressorBrotli>();
        break;
      #endif
      case CT_LZHAM:
        engine = instantiate_engine<CompressorLzham>();
        break;
      default:
        assert_failure();
    }
    cl_engines[type] = engine;
  }
  return engine;
}

void CompressorLibrary::initialize() {
  cl_buffer = nullptr;
  cl_buff_size = 0;
  std::memset(cl_engines, 0, sizeof cl_engines);
}

void CompressorLibrary::cleanup() {
  if (cl_buffer != nullptr) {
    c3_assert(cl_buff_size);
    global_memory.free(cl_buffer, cl_buff_size);
    cl_buffer = nullptr;
    cl_buff_size = 0;
  } else {
    c3_assert(cl_buff_size == 0);
  }

  delete_engine<CompressorLZSS3>(CT_LZSS3);
  delete_engine<CompressorSnappy>(CT_SNAPPY);
  delete_engine<CompressorZlib>(CT_ZLIB);
  delete_engine<CompressorZstd>(CT_ZSTD);
  delete_engine<CompressorLz4>(CT_LZ4);
  delete_engine<CompressorLzf>(CT_LZF);
  #if C3_ENTERPRISE
  delete_engine<CompressorBrotli>(CT_BROTLI);
  #endif
  delete_engine<CompressorLzham>(CT_LZHAM);
}

bool CompressorLibrary::is_supported(c3_compressor_t type) {
  return get_engine(type) != nullptr;
}

const char* CompressorLibrary::get_name(c3_compressor_t type) {
  switch (type) {
    case CT_NONE:
      return "NONE";
    case CT_NUMBER_OF_ELEMENTS:
      return "<INVALID>";
    default:
      CompressorEngine* engine = get_engine(type);
      return engine != nullptr? engine->get_name(): "<INACTIVE>";
  }
}

c3_byte_t * CompressorLibrary::pack(c3_compressor_t type, const c3_byte_t *src, c3_uint_t src_size,
  c3_uint_t &dst_size, Allocator& allocator, comp_level_t level, comp_data_t hint) {

  assert(src && src_size > 0 && dst_size <= src_size &&
    level < CL_NUMBER_OF_ELEMENTS && hint < CD_NUMBER_OF_ELEMENTS);

  c3_byte_t* result = nullptr;
  CompressorEngine* engine = get_engine(type);
  if (engine != nullptr) {
    size_t guessed_size = engine->get_compressed_size(src_size);
    // some compressors (e.g. LZ4) may return 0 to indicate they cannot compress that many bytes
    if (guessed_size > 0) {
      if (guessed_size > cl_buff_size) {
        if (cl_buffer != nullptr) {
          cl_buffer = (c3_byte_t*) global_memory.realloc(cl_buffer, guessed_size, cl_buff_size);
        } else {
          cl_buffer = (c3_byte_t*) global_memory.alloc(guessed_size);
        }
        cl_buff_size = guessed_size;
      }
      c3_assert(cl_buffer);
      c3_uint_t actual_size = engine->pack(src, src_size, cl_buffer, guessed_size, level, hint);
      if (actual_size != 0 && actual_size < dst_size) {
        result = allocator.alloc(actual_size);
        c3_assert(result);
        std::memcpy(result, cl_buffer, actual_size);
        dst_size = actual_size;
      }
    }
  }
  return result;
}

c3_byte_t* CompressorLibrary::pack(c3_compressor_t type, const c3_byte_t *src, c3_uint_t src_size,
  c3_uint_t &dst_size, Memory& memory, comp_level_t level, comp_data_t hint) {
  DefaultAllocator allocator(memory);
  return pack(type, src, src_size, dst_size, allocator, level, hint);
}

c3_byte_t* CompressorLibrary::unpack(c3_compressor_t type, const c3_byte_t *src, c3_uint_t src_size,
  c3_uint_t dst_size, Allocator& allocator) {

  assert(src && src_size > 0 && src_size < dst_size);

  c3_byte_t* result = nullptr;
  CompressorEngine* engine = get_engine(type);
  if (engine != nullptr) {
    result = allocator.alloc(dst_size);
    c3_assert(result);
    if (!engine->unpack(src, src_size, result, dst_size)) {
      allocator.free(result, dst_size);
      result = nullptr;
    }
  }
  return result;
}

c3_byte_t* CompressorLibrary::unpack(c3_compressor_t type, const c3_byte_t *src, c3_uint_t src_size,
  c3_uint_t dst_size, Memory& memory) {
  DefaultAllocator allocator(memory);
  return unpack(type, src, src_size, dst_size, allocator);
}

} // CyberCache
