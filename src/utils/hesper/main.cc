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
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

typedef unsigned char c3_byte_t;
typedef unsigned int c3_uint_t;

#if 0
  #define DEBUG_CODE(stmt) stmt
  #define DEBUG_RAW(stmt) stmt
  #define DEBUG_STATUS(stmt) stmt
#else
  #define DEBUG_CODE(stmt)
  #define DEBUG_RAW(stmt)
  #define DEBUG_STATUS(stmt)
#endif

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

///////////////////////////////////////////////////////////////////////////////
// ALGORITHM IMPLEMENTATION
///////////////////////////////////////////////////////////////////////////////

/**
 * Hesper is a simple ASCII stream compressor meant to be used when other compressors would fail or underperform
 * because the input text is so short that there are not enough repeating sequences, or overhead of dynamic dictionary
 * would significantly worsen compression ratio... or the cost of initializing/shutting down other compressors makes
 * using it on tiny data buffers not worthy of the trouble.
 *
 * Hesper can successfully compress even tiny buffers, but at a cost of having severe limitations as to what data
 * it can compress: it can handle only printable ASCII characters and newlines (NL, '\n'), and will immediately stop
 * compression attempt and return "failure" code if it stumbles upon anything else, even a TAB ('\t') character.
 *
 * Its primary intended use are Magento 2 "general-purpose" cache records: they often contain sets of names that
 * consist of uppercase characters, numbers, underscores, and spaces. Hesper compresses input bytes into "runs" of
 * 5-bit codes, first code in each run being type/length control code, and remaining codes being characters' offsets
 * withing ASCII table's part that corresponds to run's type. The length is in range 1..16 for `HBT_CAPITALS` (see
 * `hesper_byte_type_t` enum), or 1..8 for `HBT_DIGITS` and `HBT_LETTERS`, which produces most efficient encoding
 * for Magento data (that favors capitals).
 *
 * The algorithm is named after Hesperonychus, the smallest known carnivorous dinosaur (member of the Microraptorinae,
 * discovered in 1982), only about 50 cm in length -- although "Hesper" is also the name of planet Venus when it is
 * seen as an evening star. The name it meant to stress that Hesper was designed to work on small data.
 */
class Hesper {

public:
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
  c3_uint_t pack(const c3_byte_t* src, c3_uint_t src_size, c3_byte_t* dst, c3_uint_t dst_size,
    comp_level_t level, comp_data_t hint);

  /**
   * Uncompresses data in the buffer specified by `src` storing result in `dst`.
   *
   * @param src Data to uncompress
   * @param src_max Size of the data to uncompress
   * @param dst Buffer for uncompressed data
   * @param dst_max Expected size of uncompressed data
   * @return `true` on success, `false` on failure.
   */
  bool unpack(const c3_byte_t* src, c3_uint_t src_size, c3_byte_t* dst, c3_uint_t dst_size);
};

/// Classification of input bytes from the Hesper's standpoint
enum hesper_byte_type_t {
  HBT_INVALID = 0, // "unsupported"
  HBT_DIGITS,      // space, digits, and most punctuation marks
  HBT_CAPITALS,    // capital letters, underscore, and some punctuation characters
  HBT_LETTERS      // small letters, some punctuation, and '\n'
};

inline hesper_byte_type_t get_byte_type(c3_byte_t byte) {
  if (byte == '\n' || (byte >= 0x60 && byte < 0x7F)) {
    return HBT_LETTERS;
  }
  if (byte >= 0x20 && byte <= 0x3F) {
    return HBT_DIGITS;
  }
  if (byte >= 0x40 && byte <= 0x5F) {
    return HBT_CAPITALS;
  }
  // an "unsupported" character: cannot be compressed
  DEBUG_STATUS(printf("unsupported character: '%c'\n", byte));
  return HBT_INVALID;
}

inline c3_byte_t get_byte_code(c3_byte_t byte) {
  if (byte == '\n') {
    return 0x7F - 0x60;
  }
  if (byte >= 0x20 && byte <= 0x3F) {
    return byte - (c3_byte_t) 0x20;
  }
  if (byte >= 0x40 && byte <= 0x5F) {
    return byte - (c3_byte_t) 0x40;
  }
  return byte - (c3_byte_t) 0x60;
}

inline bool put_bits(c3_byte_t& bit_mask, c3_byte_t*& byte_pos, const c3_byte_t* const end_pos, c3_byte_t value) {
  for (c3_byte_t i = 0; i < 5; i++) {
    if ((value & (1 << i)) != 0) {
      *byte_pos |= bit_mask;
    }
    if ((bit_mask <<= 1) == 0) {
      bit_mask = 0x01;
      if (++byte_pos >= end_pos) {
        /*
         * Compressed buffer is going to be of the same size or even bigger than the uncompressed (source) data,
         * so the compression essentially has failed.
         */
        DEBUG_STATUS(printf("compressed data too big\n"));
        return false;
      }
      *byte_pos = 0;
    }
  }
  DEBUG_RAW(printf("  saved: 0x%02X\n", value & 0x1F));
  return true;
}

c3_uint_t Hesper::pack(const c3_byte_t* src, c3_uint_t src_size, c3_byte_t* dst, c3_uint_t dst_size,
  comp_level_t level, comp_data_t hint) {
  c3_byte_t* dst_pos = dst;
  if (dst_size > 0) {
    c3_byte_t* dst_end = dst + dst_size;
    const c3_byte_t* src_pos = src;
    const c3_byte_t* src_end = src + src_size;
    c3_byte_t bit_mask = 0x01;
    *dst_pos = 0;
    while (src_pos < src_end) {
      // get run of the "same type" bytes
      const c3_byte_t* run = src_pos;
      hesper_byte_type_t type = get_byte_type(*run);
      c3_uint_t max_length, length_mask;
      switch (type) {
        case HBT_INVALID:
          return 0;
        case HBT_DIGITS:
          max_length = 8;
          length_mask = 0x10; // 10xxx
          break;
        case HBT_CAPITALS:
          max_length = 16;
          length_mask = 0x00; // 0xxxx
          break;
        default: // HBT_LETTERS
          max_length = 8;
          length_mask = 0x18; // 11xxx
      }
      while (++run < src_end && run - src_pos < max_length) {
        hesper_byte_type_t next = get_byte_type(*run);
        if (next == HBT_INVALID) {
          return 0;
        }
        if (next != type) {
          break;
        }
      }
      // encode the run
      DEBUG_CODE(printf("control: 0x%02X (%d)\n", length_mask, (int)(run - src_pos)));
      if (!put_bits(bit_mask, dst_pos, dst_end, (c3_byte_t)(length_mask | ((run - src_pos) - 1)))) {
        return 0;
      }
      do {
        DEBUG_CODE(printf("   byte: '%c'\n", *src_pos));
        if (!put_bits(bit_mask, dst_pos, dst_end, get_byte_code(*src_pos))) {
          return 0;
        }
      } while (++src_pos < run);
    }
    if (bit_mask > 0x01) {
      // some bits in the "next byte" had already been set, so include it
      if (++dst_pos >= dst_end) {
        return 0;
      }
    }
  }
  return (c3_uint_t)(dst_pos - dst);
}

inline bool get_bits(c3_byte_t& code, c3_byte_t& bit_mask, const c3_byte_t*& byte_pos, const c3_byte_t* const end_pos) {
  c3_byte_t value = *byte_pos;
  code = 0;
  for (c3_byte_t i = 0; i < 5; i++) {
    if ((value & bit_mask) != 0) {
      code |= 1 << i;
    }
    if ((bit_mask <<= 1) == 0) {
      bit_mask = 0x01;
      if (++byte_pos >= end_pos) {
        DEBUG_CODE(printf(" BOUNDS: %u\n", i));
        /*
         * We just advanced source pointer beyond source buffer. If we got the last bit from the very last byte,
         * then it's OK; if, however, there remained any bits to fetch, we have a problem, since we would be reading
         * them outside of source data.
         */
        return i == 4;
      }
      value = *byte_pos;
    }
  }
  DEBUG_RAW(printf(" loaded: 0x%02X\n", code));
  return true;
}

inline c3_byte_t get_byte_value(hesper_byte_type_t type, c3_byte_t code) {
  if (type == HBT_DIGITS) {
    return code + (c3_byte_t) 0x20;
  }
  if (type == HBT_CAPITALS) {
    return code + (c3_byte_t) 0x40;
  }
  // type == HBT_LETTERS
  if (code == 0x7F - 0x60) {
    return '\n';
  }
  return code + (c3_byte_t) 0x60;
}

bool Hesper::unpack(const c3_byte_t* src, c3_uint_t src_size, c3_byte_t* dst, c3_uint_t dst_size) {
  c3_byte_t* dst_pos = dst;
  c3_byte_t* dst_end = dst + dst_size;
  if (src_size > 0) {
    const c3_byte_t* src_pos = src;
    const c3_byte_t* src_end = src + src_size;
    c3_byte_t bit_mask = 0x01;
    do {
      c3_byte_t code;
      if (!get_bits(code, bit_mask, src_pos, src_end)) {
        return false;
      }
      c3_uint_t length;
      hesper_byte_type_t type;
      if ((code & 0x10) != 0) {
        length = (code & 0x07) + 1u;
        type = (code & 0x08) != 0? HBT_LETTERS: HBT_DIGITS;
      } else {
        length = (code & 0x0F) + 1u;
        type = HBT_CAPITALS;
      }
      DEBUG_CODE(printf("control: 0x%02X (%d)\n", code & 0x18, length));
      if (dst_pos + length > dst_end) {
        return false;
      }
      for (c3_uint_t i = 0; i < length; i++) {
        if (!get_bits(code, bit_mask, src_pos, src_end)) {
          return false;
        }
        c3_byte_t value = get_byte_value(type, code);
        DEBUG_CODE(printf("   byte: '%c'\n", value));
        *dst_pos++ = value;
      }
    } while (dst_pos < dst_end);
    return true;
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// HOUSEKEEPING AND ENTRY POINT
///////////////////////////////////////////////////////////////////////////////

static void fail(const char* message) {
  fprintf(stderr, "ERROR: %s\n", message);
  exit(3);
}

static const c3_byte_t* load_file(const char* path, size_t& size, c3_uint_t* stored_size) {
  struct stat stats;
  if (stat(path, &stats) == 0) {
    if (stats.st_size > 0 && stats.st_size <= 0xFFFFFFFF) {
      size = (size_t) stats.st_size;
      auto buffer = (c3_byte_t*) std::malloc(size);
      if (buffer != nullptr) {
        FILE* file = std::fopen(path, "r");
        if (file != nullptr) {
          if (stored_size != nullptr) {
            if (std::fread(stored_size, 1, sizeof *stored_size, file) != sizeof *stored_size) {
              fail("could not read stored size from the source file");
            }
            size -= sizeof *stored_size;
            // sanity checks
            if (*stored_size <= size) {
              fail("stored source size is too small");
            }
            if (*stored_size * 5 > size * 8) {
              // actual compression ratio should actually be even worse than 8:5 because of control codes
              fail("stored source size is too big");
            }
          }
          if (std::fread(buffer, 1, size, file) == size) {
            std::fclose(file);
            return buffer;
          } else {
            fail("could not read source file");
          }
        } else {
          fail("could not open source file");
        }
      } else {
        fail("could not allocate source file buffer");
      }
    } else {
      fail("source file is of zero size or too big");
    }
  } else {
    fail("could not get source file size");
  }
  return nullptr;
}

static void save_file(const char* path, const c3_byte_t* buffer, c3_uint_t size, c3_uint_t* original_size) {
  FILE* file = std::fopen(path, "w");
  if (file != nullptr) {
    if (original_size != nullptr) {
      if (std::fwrite(original_size, 1, sizeof *original_size, file) != sizeof *original_size) {
        fail("could not store original size");
      }
    }
    if (std::fwrite(buffer, 1, size, file) == size) {
      std::fclose(file);
    } else {
      fail("could not write destination file");
    }
  } else {
    fail("could not create destination file");
  }
}

int main(int argc, char** argv) {
  if (argc != 4 || (std::strcmp(argv[1], "encode") != 0 && std::strcmp(argv[1], "decode") != 0)) {
    puts("Use: hesper {encode|decode} <source-file> <destination-file>");
    return 1;
  }
  bool encode = std::strcmp(argv[1], "encode") == 0;
  size_t size;
  c3_uint_t stored_size = 0;
  // we will rely on C++ runtime to free up resources...
  const c3_byte_t* src = load_file(argv[2], size, (encode? nullptr: &stored_size));
  auto dst = (c3_byte_t*) std::malloc(encode? size: stored_size);
  if (dst != nullptr) {
    Hesper hesper;
    c3_uint_t src_size = (c3_uint_t) size;
    if (std::strcmp(argv[1], "encode") == 0) {
      c3_uint_t dst_size = hesper.pack(src, src_size, dst, src_size, CL_DEFAULT, CD_DEFAULT);
      if (dst_size != 0) {
        printf("Compressed '%s': %u => %u [%u%%]\n", argv[2], src_size, dst_size, dst_size * 100 / src_size);
        save_file(argv[3], dst, dst_size, &src_size);
        printf("Saved compressed file '%s'\n", argv[3]);
      } else {
        printf("Could not compress '%s' (%u bytes)\n", argv[2], src_size);
        return 2;
      }
    } else {
      bool result = hesper.unpack(src, src_size, dst, stored_size);
      if (result) {
        printf("Uncompressed '%s': %u => %u\n", argv[2], src_size, stored_size);
        save_file(argv[3], dst, stored_size, nullptr);
        printf("Saved uncompressed file '%s'\n", argv[3]);
      } else {
        fail("Could not uncompress source file");
      }
    }
  } else {
    fail("could not allocate destination buffer");
  }
  return 0;
}
