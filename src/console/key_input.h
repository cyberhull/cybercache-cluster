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
 * Keyboard input implementation (single character).
 */
#ifndef _KEY_INPUT_H
#define _KEY_INPUT_H

#include "c3lib/c3lib.h"
#include "key_defs.h"

namespace CyberCache {

/**
 * Keyboard handler; provides method for reading keys w/o echo and translating standard escape sequences
 * and control characters into "virtual" keys (`KEY_xxx` constants).
 */
class KeyInput {
  /// Escape sequence prefixes per ECMA-48 standard, plus non-standard double-escape
  enum key_sequence_type_t: c3_byte_t {
    KST_INVALID, // invalid type (placeholder)
    KST_CSI,     // Control Sequence Introducer ("ESC [")
    KST_SS3,     // Single Shift Select of G3 Character Set ("ESC O")
    KST_NUMBER_OF_ELEMENTS
  };

  /// Helper container for escape sequence detector/translator
  struct key_sequence_t {
    const char                ks_sequence[4]; // character sequence that corresponds to the "virtual" key
    const c3_byte_t           ks_length;      // length of sequence, chars
    const key_sequence_type_t ks_type;        // sequence prefix (*not* stored in the sequence array)
    const c3_key_t            ks_key;         // "virtual" key that corresponds to the sequence
  };

  static constexpr c3_uint_t  SEQUENCE_BUFFER_LENGTH = 8;       // size of escape sequence buffer
  static constexpr c3_uint_t  MAX_SEQUENCE_LENGTH = 6;          // max escape sequence len(2-char prefix + 4 chars)
  static const key_sequence_t ki_sequences[];                   // array of key sequence descriptors
  c3_byte_t                   ki_chars[SEQUENCE_BUFFER_LENGTH]; // current character sequence (*includes* prefix)
  c3_uint_t                   ki_length;                        // current sequence length
  key_sequence_type_t         ki_seq_type;                      // current sequence type

  static int get_raw_key();
  void reset_sequence() {
    ki_length = 0;
    ki_seq_type = KST_INVALID;
  }
  void store_char(int key) {
    // sequence cannot be longer than 2-char prefix + 3 control chars
    c3_assert(key <= BYTE_MAX_VAL && ki_length < MAX_SEQUENCE_LENGTH);
    ki_chars[ki_length++] = (c3_byte_t) key;
  }
  c3_key_t store_and_check_char(c3_byte_t c);

public:
  KeyInput() {
    ki_length = 0;
    ki_seq_type = KST_INVALID;
  }

  /**
   * Waits till a key is pressed on the keyboard and returns code corresponding to the pressed key. The
   * key can be a regular printable character, a non-printable control character (e.g. '\0' is returned
   * upon Ctrl-SPACE), or a `KEY_xxx` constant.
   *
   * @return Code corresponding to the pressed key.
   */
  c3_key_t get_key();
};

} // CyberCache

#endif // _KEY_INPUT_H
