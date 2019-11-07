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
 * Keyboard input implementation (single line of text); replacement for Linux/FreeBSD readline().
 */
#ifndef _LINE_INPUT_H
#define _LINE_INPUT_H

#include "c3lib/c3lib.h"
#include "key_defs.h"

#include <cstdio>

namespace CyberCache {

/// Implementation of console line input.
class LineInput {
  static constexpr c3_uint_t BUFFER_SIZE = 4096;
  static constexpr c3_uint_t DEFAULT_WINDOW_WIDTH = 80; // assume this if system call fails
  static constexpr c3_uint_t MIN_ENTRY_WIDTH = 4; // use at least this many chars even in the smallest window

  char      li_buffer[BUFFER_SIZE]; // buffer with current line contents
  c3_uint_t li_size;                // current size of data in the buffer
  bool      li_beeps;               // whether to play sound on "invalid" commands

  c3_uint_t get_window_width();
  bool is_word_char(c3_uint_t i) const;
  static void print_with_hidden_cursor(const char* text);
  void beep();

public:
  explicit LineInput(bool beeps = false) {
    li_buffer[0] = 0;
    li_size = 0;
    li_beeps = beeps;
  }

  /**
   * Get size of internal buffer used for line editing.
   *
   * @return Buffer size, number of characters.
   */
  static constexpr c3_uint_t get_buffer_size() { return BUFFER_SIZE; }

  /**
   * Moves caret to the new line.
   */
  static void line_feed() { std::putc('\n', stdout); }

  /**
   * Moves caret to the very beginning of the current line.
   */
  static void carriage_return() { std::putc('\r', stdout); }

  /**
   * Reads line of text from standard input. Initial cursor position is at the very end of the provided
   * initial contents, which will be scrolled if it does not fit terminal window.
   *
   * @param prompt Text to display before editing area
   * @param padding Number of spaces between prompt and entry area
   * @param contents Initial contents (to edit)
   * @param keys Array of key IDs which should end entry/editing
   * @param num_keys Number of keys that can end text entry/editing
   * @return Code of the key that has actually ended line entry/editing
   */
  c3_key_t get_line(const char* prompt, c3_uint_t padding, const char* contents,
    const c3_key_t* keys, c3_uint_t num_keys);

  /**
   * Reads password from standard input. Contrary to `get_line()`, it
   *
   * a) does not echo characters it receives,
   * b) does not accept non-printable characters and spaces,
   * c) supports Backspace but *not* Delete for editing already entered text,
   * d) does not support (ignores) cursor movement keys,
   * e) returns entered string upon Enter.
   *
   * @param prompt Text to display before editing area
   * @param padding Number of spaces between prompt and entry area
   * @return Password string on Enter, `NULL` if entered string was empty (zero-length).
   */
  const char* get_password(const char* prompt, c3_uint_t padding);

  /**
   * Get contents of just last entered command or password.
   *
   * @return Pointer to read-only contents of the last entered line
   */
  const char* get_line_contents() const { return li_buffer; }

  /**
   * Get length of just entered command or password; same as `strlen()` of last entered line's contents.
   *
   * @return Length of the last entered line.
   */
  c3_uint_t get_line_length() const { return li_size; }
};

} // CyberCache

#endif // _LINE_INPUT_H
