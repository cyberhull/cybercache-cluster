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
#include "engine_lzss3.h"

namespace CyberCache {

const char* CompressorLZSS3::get_name() {
  return "LZSS3";
}

size_t CompressorLZSS3::get_compressed_size(c3_uint_t size) {
  return size;
}

void CompressorLZSS3::initialize_trees() {
  /*
   * For i = 0 to LZSS_SIZE - 1, lzss_rchild[ i ] and lzss_lchild[ i ] will be the right and left children of node i.
   * These nodes need not be initialized. Also, dad[ i ] is the lzss_parent of node i. These are initialized
   * to NIL (== LZSS_SIZE), which stands for <not used>.
   */
  c3_uint_t i = 0;
  do {
   lzss_parent[i] = LZSS_NIL;
  } while (++ i < LZSS_SIZE);

  /*
   * For i = 0 to 255, lzss_rchild[ LZSS_SIZE+i+1 ] is the root of the tree for strings that begin with character
   * i. These are initialized to NIL. Note there are 256 trees.
   */
  i ++; // = LZSS_SIZE + 1
  do {
   lzss_rchild[i] = LZSS_NIL;
  } while (++ i <= LZSS_SIZE + 256);
}

void CompressorLZSS3::insert_node(c3_uint_t node) {
  /*
   * Inserts string of length LZSS_HI, lzss_ring_buffer[r..r+LZSS_HI-1], into one of the trees (lzss_ring_buffer[r]'th
   * tree) and returns the longest-match position and length via the global variables lzss_match_position and
   * lzss_match_length.
   *
   * `r` plays double role, as tree node and position in buffer. So if we guarantee that `r` <
   * last-byte-of-text+LZSS_HI than 'space' chars are never involved in comparisons.
   */
  int cmp = 1; // cmp = key[0] - lzss_ring_buffer[p]
  c3_byte_t* key = &lzss_ring_buffer[node]; // search key
  c3_uint_t p = key[0] + LZSS_SIZE + 1; // current node = root of tree for 1st byte
  lzss_rchild[node] = lzss_lchild[node] = LZSS_NIL; // no sons (terminal node)
  lzss_match_length = 0; // no matches yet

  for(;;) {
    if (cmp >= 0) {
      if (lzss_rchild[p] == LZSS_NIL) {
         lzss_parent[lzss_rchild[p] = node] = p;
         return;
      } else {
        p = lzss_rchild[p];
      }
    }
    else
    {
      if (lzss_lchild[p] == LZSS_NIL) {
        lzss_parent[lzss_lchild[p] = node] = p;
        return;
      } else {
        p = lzss_lchild[p];
      }
    }
    c3_uint_t i = 1;
    do {
      if((cmp = key[i] - lzss_ring_buffer[p + i]) != 0) {
        break;
      }
    } while (++ i < LZSS_HI);
    if (i > lzss_match_length) {
       lzss_match_position = p;
       if ((lzss_match_length = i) == LZSS_HI) {
         break;
       }
    }
  }
  /*
   * lzss_match_length == LZSS_HI, so remove the old node `p` in favor of the new one, because the old one will
   * be deleted soon.
   */
  c3_uint_t root = lzss_parent[p]; // get old node's lzss_parent
  lzss_parent[node] = root; // copy node
  lzss_lchild[node] = lzss_lchild[p];
  lzss_rchild[node] = lzss_rchild[p];
  lzss_parent[lzss_lchild[p]] = node; // "prune & graft" branches
  lzss_parent[lzss_rchild[p]] = node;

  if (lzss_rchild[root] == p) {
    lzss_rchild[root] = node; // inform dad of sone's removal
  } else {
    lzss_lchild[root] = node;
  }
  lzss_parent[p] = LZSS_NIL; // remove node
}

void CompressorLZSS3::delete_node(c3_uint_t node) {
  if (lzss_parent[node] != LZSS_NIL) { // is node in the tree?
    c3_uint_t q;
    if (lzss_rchild[node] == LZSS_NIL) { // q might be NIL
      q = lzss_lchild[node];
    } else if (lzss_lchild[node] == LZSS_NIL) { // q is not NIL
      q = lzss_rchild[node];
    } else {
      if (lzss_rchild[q = lzss_lchild[node]] != LZSS_NIL) {
        do {
          q = lzss_rchild[q];
        } while (lzss_rchild[q] != LZSS_NIL);

        c3_uint_t t = lzss_parent[q];
        lzss_parent[lzss_rchild[t] = lzss_lchild[q]] = t;
        lzss_parent[lzss_lchild[q] = lzss_lchild[node]] = q;
      }
      lzss_parent[lzss_rchild[q] = lzss_rchild[node]] = q;
    }
    int t = lzss_parent[q] = lzss_parent[node];
    if (lzss_rchild[t] == node) {
      lzss_rchild[t] = q;
    } else {
      lzss_lchild[t] = q;
    }
    lzss_parent[node] = LZSS_NIL;
  }
}

c3_uint_t CompressorLZSS3::pack(const c3_byte_t* src, c3_uint_t src_size, c3_byte_t* dst, size_t dst_size,
  comp_level_t level, comp_data_t hint) {
  c3_assert(dst_size <= UINT_MAX_VAL); // `get_compressed_size()` should guarantee this
  c3_uint_t max_dst_dize = (c3_uint_t) dst_size;
  c3_uint_t src_pos = 0; // input counter
  c3_uint_t dst_pos = 0; // output counter
  c3_byte_t code_buf[17], mask;

  initialize_trees();

  /*
   * code_buf[1..16] saves eight units of code, and code_buf[0] works as eight flags, `1` representing
   * that the unit is an unencoded letter (1 byte), `0` a position-and-length pair (2 bytes). Thus, eight
   * units require at most 16 bytes of code.
   */
  code_buf[0] = 0;

  int code_buf_index = mask = 1;
  c3_uint_t s = LZSS_HI;
  c3_uint_t r = 0;

  // read up to LZSS_HI bytes into the first LZSS_HI bytes of the buffer
  c3_uint_t len = 0; do {
    if (src_pos >= src_size) {
      break;
    }
    lzss_ring_buffer[len++] = src[src_pos++];
  } while (len < LZSS_HI);

  // Insert the whole string just read. This sets lzss_match_length and lzss_match_position.
  insert_node(r);

  do {
    // at the end of text, len < LZSS_HI
    if (lzss_match_length > len) {
      lzss_match_length = len;
    }
    if (lzss_match_length <= LZSS_LO) {
      lzss_match_length = 1; // not long enough match; send 1 byte
      code_buf[0] |= mask; // 'send one byte' flag
      code_buf[code_buf_index++] = lzss_ring_buffer[r];
    } else {
      // send offset-length pair
      int off = r - lzss_match_position;
      if (off < 0) {
        off += LZSS_SIZE;
      }
      code_buf[code_buf_index++] = (c3_byte_t) off;
      code_buf[code_buf_index++] = (c3_byte_t) (((off >> 4) & 0xF0) | (lzss_match_length - (LZSS_LO + 1)));
    }
    if ((mask <<= 1) == 0) { // is control byte ready ?
      int i = 0; // send at most 8 code units together
      do {
        if (dst_pos >= max_dst_dize) {
          return 0;
        }
        dst[dst_pos++] = code_buf[i];
      } while (++ i < code_buf_index);
      code_buf[0] = 0;
      code_buf_index = mask = 1;
    }
    int last_match_length = lzss_match_length; // at least 1

    int i = 0; do
    {
      delete_node(s); // delete old strings and read new bytes
      if (src_pos >= src_size) {
        break;
      }
      c3_byte_t c = src[src_pos++];
      lzss_ring_buffer[s] = c;

      // if the position is near the end of buffer, extend the buffer to make string comparison easier
      if (s < LZSS_HI - 1) {
        lzss_ring_buffer[s + LZSS_SIZE] = c;
      }

      // Since this is a ring buffer, increment the position modulo LZSS_SIZE
      s++;
      s &= LZSS_SIZE - 1;
      r++;
      r &= LZSS_SIZE - 1;

      insert_node(r); // register string in lzss_ring_buffer[r..r+LZSS_HI-1]
    } while (++i < last_match_length);

    while (i ++ < last_match_length)
    {
      delete_node(s);    // after the end of text,
      s++;               // no need to read, but
      s &= LZSS_SIZE - 1; // buffer may not be empty
      r++;
      r &= LZSS_SIZE - 1;

      if (--len != 0) {
        insert_node(r);
      }
    }
  } while (len > 0); // until length of string to be processed is zero

  if (code_buf_index > 1) { // send remaining code
    int i = 0;
    do {
      if (dst_pos >= max_dst_dize) {
        return 0;
      }
      dst[dst_pos++] = code_buf[i];
    } while (++ i < code_buf_index);
  }
  return dst_pos < max_dst_dize? dst_pos: 0;
}

bool CompressorLZSS3::unpack(const c3_byte_t *src, c3_uint_t src_size, c3_byte_t *dst, c3_uint_t dst_size) {
  c3_uint_t src_pos = 0; // input counter
  c3_uint_t dst_pos = 0; // output counter
  c3_uint_t flags = 0;

  for(;;) {
    if (((flags >>= 1) & 0x100) == 0) {
      if (src_pos >= src_size) {
        break;
      }
      flags = src[src_pos++] | 0xFF00u; // higher byte == '8 done' sign
    }
    if ((flags & 1) != 0) {
      if (src_pos >= src_size) {
        break;
      }
      if (dst_pos >= dst_size) {
        return false;
      }
      dst[dst_pos++] = src[src_pos++];
    } else {
      if (src_pos >= src_size) {
        break;
      }
      int i = src[src_pos++];
      if (src_pos >= src_size) {
        break;
      }
      int j = src[src_pos++];
      i |= ((j & 0xF0) << 4); // offset
      i = dst_pos - i; // ==> position
      j = (j & 0x0F) + LZSS_LO + 1; // length

      do {
        if (dst_pos >= dst_size) {
          return false;
        }
        dst[dst_pos++] = dst[i++];
      } while (--j != 0);
    }
  }
  return dst_pos == dst_size;
}

} // CyberCache
