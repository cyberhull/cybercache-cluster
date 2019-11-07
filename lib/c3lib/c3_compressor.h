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
 * Compressor implementing various algorithms.
 */
#ifndef _C3_COMPRESSOR_H
#define _C3_COMPRESSOR_H

#include "c3_build.h"
#include "c3_memory.h"

namespace CyberCache {

/// Types of compression engines
enum c3_compressor_t: c3_byte_t {
  CT_NONE = 0, // no compression
  // fast compressors, listed in (descending) order of their strength/practicality
  CT_LZF,      // LZF by Marc Alexander Lehmann
  CT_SNAPPY,   // Snappy by Google
  CT_LZ4,      // LZ4 by Yann Collet
  CT_LZSS3,    // LZSS by Haruhiko Okumura
  // strong compressors, listed in (descending) order of their strength
  CT_BROTLI,   // Brotli by Jyrki Alakuijala and Zoltán Szabadka
  CT_ZSTD,     // Zstd by Yann Collet (Facebook, Inc.)
  CT_ZLIB,     // Zlib (gzip) by Jean-loup Gailly and Mark Adler
  CT_LZHAM,    // Lzham by Richard Geldreich, Jr.
  CT_NUMBER_OF_ELEMENTS,
  CT_DEFAULT = CT_SNAPPY
};

static_assert(CT_NUMBER_OF_ELEMENTS == 9, "Adjust 'Recompressions_Xxx' perf counter array sizes");

/// Compression levels
enum comp_level_t {
  CL_FASTEST = 0, // weakest but fastest compression
  CL_AVERAGE,     // fast compression
  CL_BEST,        // strongest *practical* level of compression
  CL_EXTREME,     // may incur severe performance penalties; suitable for background re-compressions only
  CL_NUMBER_OF_ELEMENTS,
  CL_DEFAULT = CL_BEST
};

/// Hint to the compressors (what kind of data is being compressed)
enum comp_data_t {
  CD_GENERIC = 0, // unspecified data
  CD_TEXT,        // textual data
  CD_BINARY,      // binary data
  CD_NUMBER_OF_ELEMENTS,
  CD_DEFAULT = CD_GENERIC
};

class CompressorEngine {
  // `CompressorLibrary` must be the only class capable of creating `CompressorEngine` instances
  friend class CompressorLibrary;

  /**
   * Returns name of the compression engine.
   *
   * @return Pointer to static string containing name of the compression engine.
   */
  virtual const char* get_name() = 0;
  /**
   * Get minimal size of memory buffer necessary to store result (compressed data) of pack() invocation.
   *
   * @param size Size of uncompressed (source) data, bytes
   * @return Minimum allowed size of destination buffer for pack(), bytes
   */
  virtual size_t get_compressed_size(c3_uint_t size) = 0;
  /**
   * Compresses data buffer specified by `src` storing result in `dst`.
   *
   * @param src Data to compress
   * @param src_max Size of the data to compress
   * @param dst Buffer for compressed data
   * @param dst_max Maximum allowed size of the compressed data
   * @return Actual size of compressed data on success, 0 on failure (if source buffer could not be
   * compressed into _less_ than `dst_max` bytes).
   */
  virtual c3_uint_t pack(const c3_byte_t* src, c3_uint_t src_size, c3_byte_t* dst, size_t dst_size,
    comp_level_t level, comp_data_t hint) = 0;

  /**
   * Uncompresses data in the buffer specified by `src` storing result in `dst`.
   *
   * @param src Data to uncompress
   * @param src_max Size of the data to uncompress
   * @param dst Buffer for uncompressed data
   * @param dst_max Expected size of uncompressed data
   * @return `true` on success, `false` on failure.
   */
  virtual bool unpack(const c3_byte_t *src, c3_uint_t src_size, c3_byte_t *dst, c3_uint_t dst_size) = 0;

protected:
  CompressorEngine() = default;
 ~CompressorEngine() = default;

public:
  CompressorEngine(CompressorEngine&) = delete;
  CompressorEngine(CompressorEngine&&) = delete;

  void operator=(CompressorEngine&) = delete;
  void operator=(CompressorEngine&&) = delete;
};

class CompressorLibrary {
  static thread_local CompressorEngine* cl_engines[CT_NUMBER_OF_ELEMENTS];
  static thread_local c3_byte_t*        cl_buffer;
  static thread_local size_t            cl_buff_size;

  CompressorEngine* get_engine(c3_compressor_t type) const;

  template <typename T> T* instantiate_engine() const {
    T* engine = alloc<T>();
    return new (engine) T();
  }
  template <typename T> void delete_engine(c3_compressor_t type) {
    auto engine = (T*) cl_engines[type];
    if (engine) {
      dispose<T>(engine);
      cl_engines[type] = nullptr;
    }
  }

public:
   CompressorLibrary() = default;
  ~CompressorLibrary() { cleanup(); }

  CompressorLibrary(CompressorLibrary&) = delete;
  CompressorLibrary(CompressorLibrary&&) = delete;

  void operator=(CompressorLibrary&) = delete;
  void operator=(CompressorLibrary&&) = delete;

  /// Every thread that might employ compression MUST call this method when it starts
  void initialize() C3_FUNC_COLD;
  /// Every thread that might employ compression MUST call this method when it stops
  void cleanup() C3_FUNC_COLD;

  bool is_supported(c3_compressor_t type) C3_FUNC_COLD;
  const char* get_name(c3_compressor_t type) C3_FUNC_COLD;
  c3_byte_t *pack(c3_compressor_t type, const c3_byte_t *src, c3_uint_t src_size, c3_uint_t &dst_size,
    Allocator& allocator, comp_level_t level = CL_DEFAULT, comp_data_t hint = CD_DEFAULT);
  c3_byte_t *pack(c3_compressor_t type, const c3_byte_t *src, c3_uint_t src_size, c3_uint_t &dst_size,
    Memory& memory = global_memory, comp_level_t level = CL_DEFAULT, comp_data_t hint = CD_DEFAULT);
  c3_byte_t* unpack(c3_compressor_t type, const c3_byte_t *src, c3_uint_t src_size, c3_uint_t dst_size,
    Allocator& allocator);
  c3_byte_t* unpack(c3_compressor_t type, const c3_byte_t *src, c3_uint_t src_size, c3_uint_t dst_size,
    Memory& memory = global_memory);
};

extern CompressorLibrary global_compressor;

} // CyberCache

#endif // _C3_COMPRESSOR_H
