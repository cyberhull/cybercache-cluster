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
#include "c3_hasher.h"
#include "hashes/xxhash/xxhash.h"
#include "hashes/farmhash/farmhash.h"
#include "hashes/spookyhash/SpookyV2.h"
#include "hashes/murmurhash/MurmurHash2.h"
#include "hashes/murmurhash/MurmurHash3.h"
#include <cstring>

namespace CyberCache {

TableHasher table_hasher;
PasswordHasher password_hasher;

c3_hash_t Hasher::invalid_proc(const void* buff, size_t size, c3_ulong_t seed) {
  return INVALID_HASH_VALUE;
}

c3_hash_t Hasher::farmhash_proc(const void* buff, size_t size, c3_ulong_t seed) {
  return util::Hash64WithSeed((const char*) buff, size, seed);
}

c3_hash_t Hasher::murmurhash2_proc(const void* buff, size_t size, c3_ulong_t seed) {
  return MurmurHash64A(buff, (int) size, seed);
}

c3_hash_t Hasher::murmurhash3_proc(const void* buff, size_t size, c3_ulong_t seed) {
  struct hash128 { c3_ulong_t h_lo, h_high; } result;
  MurmurHash3_x64_128(buff, (int) size, (uint32_t) seed, &result);
  return result.h_lo;
}

c3_hash_t Hasher::spookyhash_proc(const void* buff, size_t size, c3_ulong_t seed) {
  return SpookyHash::Hash64(buff, size, seed);
}

c3_hash_t Hasher::xxhash_proc(const void* buff, size_t size, c3_ulong_t seed) {
  return XXH64(buff, size, seed);
}

void Hasher::set_method(c3_hash_method_t method) {
  switch (h_method = method) {
    case HM_XXHASH:
      h_proc = xxhash_proc;
      h_name = "xxhash";
      break;
    case HM_FARMHASH:
      h_proc = farmhash_proc;
      h_name = "farmhash";
      break;
    case HM_SPOOKYHASH:
      h_proc = spookyhash_proc;
      h_name = "spookyhash";
      break;
    case HM_MURMURHASH2:
      h_proc = murmurhash2_proc;
      h_name = "murmurhash2";
      break;
    case HM_MURMURHASH3:
      h_proc = murmurhash3_proc;
      h_name = "murmurhash3";
      break;
    default:
      h_proc = invalid_proc;
      h_name = "<INVALID>";
      h_method = HM_INVALID;
      assert_failure();
  }
}

#if INCLUDE_HASHER_HASH_CSTR
c3_hash_t Hasher::hash(const char* str) {
  if (str != nullptr) {
    return h_proc(str, std::strlen(str), h_seed);
  } else {
    assert_failure();
    return INVALID_HASH_VALUE;
  }
}
#endif // INCLUDE_HASHER_HASH_CSTR

}
