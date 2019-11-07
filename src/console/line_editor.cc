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
#include "line_editor.h"
#include "line_input.h"

namespace CyberCache {

const c3_key_t LineEditor::le_special_keys[NUM_SPECIAL_KEYS] = {
  KEY_ARROW_UP,              // select previous entry in the history list
  KEY_CTRL | KEY_ARROW_UP,   // same as above
  KEY_ARROW_DOWN,            // select next entry in the history list
  KEY_CTRL | KEY_ARROW_DOWN, // same as above
  KEY_TAB,                   // select [next] entry starting with already entered text
  KEY_ESCAPE,                // clear input line
  KEY_ENTER,                 // return current contents of the input line
  KEY_BREAK                  // break execution
};

LineEditor::LineEditor(c3_uint_t num_entries, bool beeps) {
  le_history = nullptr;
  le_hist_num_entries = 0;
  le_hist_max_num_entries = num_entries;
  le_search_index = ENTRY_NOT_FOUND;
  le_beeps = beeps;
}

LineEditor::~LineEditor() {
  if (le_history != nullptr) {
    c3_assert(le_hist_num_entries);
    for (c3_uint_t i = 0; i < le_hist_num_entries; i++) {
      remove_entry(i);
    }
    free_memory(le_history, le_hist_num_entries * sizeof(const char*));
  }
}

void LineEditor::add_entry(const char* text, c3_uint_t length) {
  c3_assert(text && length && std::strlen(text) == length);
  if (le_hist_num_entries == 0) {
    c3_assert(le_history == nullptr);
    le_history = (const char**) allocate_memory(sizeof(char*));
    le_hist_num_entries = 1;
  } else {
    c3_assert(le_history);
    // see if the entry already exists
    c3_uint_t i = 0;
    do {
      const char* entry = le_history[i];
      c3_assert(entry);
      if (std::strcmp(entry, text) == 0) {
        c3_uint_t last = le_hist_num_entries - 1;
        if (i < last) { // if the entry is already very last, we have nothing to do
          std::memmove(le_history + i, le_history + i + 1, (last - i) * sizeof(char*));
          le_history[last] = entry;
        }
        return;
      }
    } while (++i < le_hist_num_entries);

    if (le_hist_num_entries < le_hist_max_num_entries) {
      c3_assert(le_history);
      le_history = (const char**) reallocate_memory(le_history,
        (le_hist_num_entries + 1) * sizeof(char*), le_hist_num_entries * sizeof(char*));
      le_hist_num_entries++;
    } else {
      c3_assert(le_history && le_hist_num_entries == le_hist_max_num_entries);
      remove_entry(0);
      std::memmove(le_history, le_history + 1, (le_hist_num_entries - 1) * sizeof(char*));
    }
  }
  length++; // include terminating '\0'
  auto entry = (char*) allocate_memory(length);
  le_history[le_hist_num_entries - 1] = (const char*) std::memcpy(entry, text, length);
}

const char* LineEditor::get_entry(c3_uint_t i) {
  c3_assert(i < le_hist_num_entries && le_history && le_history[i]);
  return le_history[i];
}

void LineEditor::remove_entry(c3_uint_t i) {
  c3_assert(i < le_hist_num_entries && le_history && le_history[i]);
  auto entry = (char*) le_history[i];
  free_memory(entry, std::strlen(entry) + 1);
}

c3_uint_t LineEditor::find_first_entry(const char* prefix) {
  if (le_hist_num_entries > 0) {
    c3_uint_t i = le_hist_num_entries - 1;
    do {
      if (c3_starts_with(get_entry(i), prefix)) {
        le_search_prefix.set(DOMAIN_GLOBAL, prefix);
        return le_search_index = i;
      }
    } while (i-- != 0);
  }
  le_search_prefix.empty();
  return le_search_index = ENTRY_NOT_FOUND;
}

c3_uint_t LineEditor::find_next_entry() {
  // this method should *only* be called after successful search for the first matching entry
  c3_assert(is_valid_entry(le_search_index) && le_history && le_hist_num_entries &&
    le_search_prefix.not_empty());
  c3_uint_t index = le_search_index - 1;
  // we search *all* entries, so at the very least we should return the same entry again
  for (c3_uint_t i = 0; i < le_hist_num_entries; i++) {
    if (index > le_hist_num_entries) {
      index = le_hist_num_entries - 1;
    }
    if (c3_starts_with(get_entry(index), le_search_prefix.get_chars())) {
      return le_search_index = index;
    }
    index--;
  }
  c3_assert_failure();
  return ENTRY_NOT_FOUND;
}

void LineEditor::beep() {
  if (le_beeps) {
    std::putc('\a', stdout);
  }
}

#if LINEEDITOR_SET_MAX_HISTORY_SIZE
void LineEditor::set_max_history_size(c3_uint_t num_entries) {
  c3_assert(num_entries);
  if (le_hist_num_entries > num_entries) {
    c3_uint_t num_to_remove = le_hist_num_entries - num_entries;
    for (c3_uint_t i = 0; i < num_to_remove; i++) {
      remove_entry(i);
    }
    std::memmove(le_history, le_history + num_to_remove, num_entries * sizeof(char*));
    le_history = (const char**) reallocate_memory(le_history,
      num_entries * sizeof(char*), le_hist_num_entries * sizeof(char*));
    le_hist_num_entries = num_entries;

    // just in case (history size should not be set between search calls anyway)
    le_search_index = ENTRY_NOT_FOUND;
    le_search_prefix.empty();
  }
  le_hist_max_num_entries = num_entries;
}
#endif // LINEEDITOR_SET_MAX_HISTORY_SIZE

const char* LineEditor::get_line(const char* prompt, c3_uint_t padding) {
  LineInput line_input(le_beeps);
  c3_uint_t search_index = ENTRY_NOT_FOUND;
  c3_uint_t history_index = ENTRY_NOT_FOUND;
  static char initial_contents[LineInput::get_buffer_size()];
  initial_contents[0] = 0;

  for (;;) {
    const c3_key_t key = line_input.get_line(prompt, padding, initial_contents,
      le_special_keys, NUM_SPECIAL_KEYS);
    switch (key) {
      case KEY_ENTER:
        if (line_input.get_line_length() > 0) {
          add_entry(line_input.get_line_contents(), line_input.get_line_length());
          return std::strcpy(initial_contents, line_input.get_line_contents());
        } else {
          initial_contents[0] = 0;
          search_index = ENTRY_NOT_FOUND;
          history_index = ENTRY_NOT_FOUND;
          beep();
        }
        break;
      case KEY_ESCAPE:
        if (line_input.get_line_length() == 0) {
          beep();
        }
        initial_contents[0] = 0;
        search_index = ENTRY_NOT_FOUND;
        history_index = ENTRY_NOT_FOUND;
        break;
      case KEY_BREAK:
        return nullptr;
      case KEY_TAB:
        if (line_input.get_line_length() > 0 && le_hist_num_entries > 0) {
          if (search_index == ENTRY_NOT_FOUND || // first search OR prefix had been edited
            std::strcmp(get_entry(search_index), line_input.get_line_contents()) != 0) {
            search_index = find_first_entry(line_input.get_line_contents());
          } else {
            search_index = find_next_entry();
          }
        } else {
          search_index = ENTRY_NOT_FOUND;
        }
        if (search_index == ENTRY_NOT_FOUND) {
          std::strcpy(initial_contents, line_input.get_line_contents());
          beep();
        } else {
          std::strcpy(initial_contents, get_entry(search_index));
        }
        history_index = ENTRY_NOT_FOUND;
        break;
      case KEY_CTRL | KEY_ARROW_UP:
      case KEY_ARROW_UP: {
        bool fetched = false;
        if (le_hist_num_entries > 0) {
          if (history_index > le_hist_num_entries) {
            history_index = le_hist_num_entries;
          }
          if (history_index > 0) {
            history_index--;
            std::strcpy(initial_contents, get_entry(history_index));
            fetched = true;
          }
        }
        if (!fetched) {
          std::strcpy(initial_contents, line_input.get_line_contents());
          beep();
        }
        search_index = ENTRY_NOT_FOUND;
        break;
      }
      case KEY_CTRL | KEY_ARROW_DOWN:
      case KEY_ARROW_DOWN: {
        bool fetched = false;
        if (le_hist_num_entries > 0) {
          if (history_index < le_hist_num_entries - 1) {
            history_index++;
            std::strcpy(initial_contents, get_entry(history_index));
            fetched = true;
          }
        }
        if (!fetched) {
          std::strcpy(initial_contents, line_input.get_line_contents());
          beep();
        }
        search_index = ENTRY_NOT_FOUND;
        break;
      }
      default:
        beep();
        c3_assert_failure();
    }
  }
}

} // CyberCache
