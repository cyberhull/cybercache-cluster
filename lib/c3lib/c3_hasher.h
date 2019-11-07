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
 * Hasher: interface to hash algorithms implemented in external libraries
 */
#ifndef _C3_HASHER_H
#define _C3_HASHER_H

#include "c3_types.h"

namespace CyberCache {

// whether to compile `Hasher::hash(const char*)`
#define INCLUDE_HASHER_HASH_CSTR 0

/// Format specifier for `fprintf()` family of functions
#define C3_HASH_FORMAT "%016llX"

/// Hash methods
enum c3_hash_method_t: c3_byte_t {
  HM_INVALID,     // invalid hash algorithm (placeholder)
  HM_XXHASH,      // "xxhash" by Yann Collet
  HM_FARMHASH,    // "FarmHash" by Geoff Pike (Google); this is successor to "CityHash"
  HM_SPOOKYHASH,  // "SpookyHashV2" by Bob Jenkins
  HM_MURMURHASH2, // "MurmurHash2" by Austin Appleby; this version is used by Redis
  HM_MURMURHASH3, // "MurmurHash3" by Austin Appleby
  HM_NUMBER_OF_ELEMENTS
};

/// Pointer to a hash function
typedef c3_hash_t (*hash_proc_t)(const void* buff, size_t size, c3_ulong_t seed);

/// Base class for all hashers
class Hasher {
  const char*      h_name;   // currently active method's name
  hash_proc_t      h_proc;   // pointer to actual hash algorithm implementation
  c3_ulong_t       h_seed;   // hash seed
  c3_hash_method_t h_method; // hash algorithm

  static c3_hash_t invalid_proc(const void* buff, size_t size, c3_ulong_t seed) C3_FUNC_COLD;
  static c3_hash_t farmhash_proc(const void* buff, size_t size, c3_ulong_t seed);
  static c3_hash_t murmurhash2_proc(const void* buff, size_t size, c3_ulong_t seed);
  static c3_hash_t murmurhash3_proc(const void* buff, size_t size, c3_ulong_t seed);
  static c3_hash_t spookyhash_proc(const void* buff, size_t size, c3_ulong_t seed);
  static c3_hash_t xxhash_proc(const void* buff, size_t size, c3_ulong_t seed);

protected:
  Hasher(c3_hash_method_t method, c3_ulong_t seed) noexcept {
    set_method(method);
    h_seed = seed;
  }

public:
  c3_hash_method_t get_method() const { return h_method; }
  void set_method(c3_hash_method_t method) C3_FUNC_COLD;
  const char* get_method_name() const { return h_name; }
  c3_ulong_t get_seed() const { return h_seed; }
  void set_seed(c3_ulong_t seed) { h_seed = seed; }

  c3_hash_t hash(const void* buff, size_t size) {
    return h_proc(buff, size, h_seed);
  }

  #if INCLUDE_HASHER_HASH_CSTR
  c3_hash_t hash(const char* str);
  #endif // INCLUDE_HASHER_HASH_CSTR
};

/// Hashing engine for hash tables
class TableHasher: public Hasher {
  static const c3_hash_method_t DEFAULT_METHOD = HM_XXHASH;
  static const c3_ulong_t       DEFAULT_SEED   = 0xA7E792DE6A72D8E0LL;

public:
  TableHasher() noexcept: Hasher(DEFAULT_METHOD, DEFAULT_SEED) {}
};

/// Hashing engine for one-way password encryption
class PasswordHasher: public Hasher {
  static const c3_hash_method_t DEFAULT_METHOD = HM_MURMURHASH2;
  static const c3_ulong_t       DEFAULT_SEED   = 0x2CFC6D033D509131LL;

public:
  PasswordHasher() noexcept: Hasher(DEFAULT_METHOD, DEFAULT_SEED) {}
  explicit PasswordHasher(c3_hash_method_t method): Hasher(method, DEFAULT_SEED) {}
};

extern TableHasher table_hasher;
extern PasswordHasher password_hasher;

}

#endif // _C3_HASHER_H
