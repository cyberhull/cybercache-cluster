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
 * Line editor with history support.
 */
#ifndef _LINE_EDITOR_H
#define _LINE_EDITOR_H

#include "c3lib/c3lib.h"
#include "key_defs.h"

// whether compile `LineEditor::set_max_history_size()` method
#define LINEEDITOR_SET_MAX_HISTORY_SIZE 0

namespace CyberCache {

/// Line editor with support of history and command search support.
class LineEditor {
  static constexpr c3_uint_t NUM_SPECIAL_KEYS = 8;
  static constexpr c3_uint_t DEFAULT_MAX_NUM_ENTRIES = 100;
  static constexpr c3_uint_t ENTRY_NOT_FOUND = UINT_MAX_VAL;
  static const c3_key_t le_special_keys[NUM_SPECIAL_KEYS]; // keys that end line input

  const char** le_history;              // array of previously entered commands
  c3_uint_t    le_hist_num_entries;     // current number of entries in history list
  c3_uint_t    le_hist_max_num_entries; // current max allowed number of entries in history list
  String       le_search_prefix;        // prefix being searched for
  c3_uint_t    le_search_index;         // index of the last found entry, or UINT_MAX_VAL
  bool         le_beeps;                // whether to play sound on "invalid" commands

  bool is_valid_entry(c3_uint_t i) const { return i < le_hist_num_entries; }
  void add_entry(const char* text, c3_uint_t length);
  const char* get_entry(c3_uint_t i);
  void remove_entry(c3_uint_t i);
  c3_uint_t find_first_entry(const char* prefix);
  c3_uint_t find_next_entry();
  void beep();

public:
  explicit LineEditor(c3_uint_t num_entries = DEFAULT_MAX_NUM_ENTRIES, bool beeps = false);
  LineEditor(const LineEditor&) = delete;
  LineEditor(LineEditor&&) = delete;
  ~LineEditor();

  LineEditor& operator=(const LineEditor&) = delete;
  LineEditor& operator=(LineEditor&&) = delete;

  void set_beeps(bool beeps) { le_beeps = beeps; }

  #if LINEEDITOR_SET_MAX_HISTORY_SIZE
  void set_max_history_size(c3_uint_t num_entries);
  #endif // LINEEDITOR_SET_MAX_HISTORY_SIZE

  /**
   * Enters one line of text; does not return until a non-empty line is entered and Enter is pressed, or if
   * Ctrl-C/Ctrl-Break is pressed. Processes a number of special keys internally to support history list:
   *
   * - ArrowUp / Ctrl-ArrowUp : brings up previous entry in the history list (if any),
   * - ArrowDown / Ctrl-ArrowDown : brings up next entry in the history list (if any),
   * - Escape : clears input line,
   * - Tab : brings up string(s) that start with prefix entered so far.
   *
   * @param prompt Text to display before editing area
   * @param padding Number of spaces between prompt and entry area
   * @return Pointer to static buffer with non-empty entered string, or NULL if Ctrl-C/Ctrl-Break was
   *   pressed while `SIGINT` signal was blocked.
   */
  const char* get_line(const char* prompt, c3_uint_t padding);
};

} // CyberCache

#endif // _LINE_EDITOR_H
