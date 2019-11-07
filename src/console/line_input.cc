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
#include "line_input.h"
#include "key_input.h"

#include <sys/ioctl.h>
#include <unistd.h>

namespace CyberCache {

c3_uint_t LineInput::get_window_width() {
  struct winsize wsize;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &wsize) != 0) {
    c3_set_stdlib_error_message();
    return DEFAULT_WINDOW_WIDTH;
  }
  return wsize.ws_col;
}

bool LineInput::is_word_char(c3_uint_t i) const {
  c3_assert(i < BUFFER_SIZE);
  const char c = li_buffer[i];
  return c == '_' || std::isalnum(c) != 0;
}

void LineInput::print_with_hidden_cursor(const char* text) {
  // cursor OFF, print text, cursor ON
  printf("\e[?25l%s\e[?25h", text);
}

void LineInput::beep() {
  if (li_beeps) {
    std::putc('\a', stdout);
  }
}

c3_key_t LineInput::get_line(const char* prompt, c3_uint_t padding, const char* contents,
  const c3_key_t* keys, c3_uint_t num_keys) {
  c3_assert(prompt && contents && keys && num_keys);

  // 1) Initialize internal buffer

  li_size = (c3_uint_t) std::strlen(contents);
  c3_assert(li_size < BUFFER_SIZE - 1);
  std::strcpy(li_buffer, contents);

  // 2) Calculate constant (for the call) text and window properties
  // ---------------------------------------------------------------

  // position at which text entry starts on screen
  const c3_uint_t screen_offset = (c3_uint_t) std::strlen(prompt) + padding;
  // total width of the console window; shared by prompt, padding, and text being entered
  c3_uint_t window_width = get_window_width();
  // part of window width used for text entry / editing
  c3_uint_t entry_width = MIN_ENTRY_WIDTH;
  if (screen_offset + entry_width < window_width) {
    entry_width = window_width - screen_offset - 1 /* we never draw anything in the very last position */;
  } else { // just to maintain invariant...
    window_width = screen_offset + entry_width + 1;
  }

  // 3) Print out prompt and set initial cursor positions
  // ----------------------------------------------------

  printf("%s%*s", prompt, padding, "");
  // for the 1st iteration, that's where the cursor "is" (just past prompt), not where it "should be"...
  c3_uint_t cursor_screen_pos = screen_offset;
  // ... and we will fix it
  bool set_cursor_screen_pos = true;
  // start of the "window" of up to `entry_width` characters that are actually visible on screen
  c3_uint_t buffer_offset = li_size > entry_width? li_size - entry_width: 0;

  // 4) Enter main entry/editing loop
  // --------------------------------

  KeyInput key_input;
  c3_uint_t new_cursor_screen_pos = cursor_screen_pos;
  for (;;) {

    // 5) Re-draw entire text to the right of the prompt
    // -------------------------------------------------

    const c3_uint_t LINE_SIZE = entry_width * 3 + 1;
    char line[(LINE_SIZE + 3) & ~3 /* alignment padding */];
    c3_uint_t i = 0;
    // 5a) return cursor to initial position (past prompt + padding)
    c3_uint_t pos = cursor_screen_pos;
    while (pos > screen_offset) {
      c3_assert(i < LINE_SIZE);
      line[i++] = '\b';
      pos--;
    }
    // 5b) re-draw visible "window" of the buffer
    for (c3_uint_t j = 0; j < entry_width; j++) {
      c3_assert(i < LINE_SIZE);
      const c3_uint_t buffer_pos = buffer_offset + j;
      line[i++] = buffer_pos < li_size? li_buffer[buffer_pos]: ' ';
    }
    // 5c) return cursor to...
    if (set_cursor_screen_pos) {
      // 5c1) ... either end of the visible portion of the text....
      cursor_screen_pos = screen_offset + entry_width; // this is where the cursor actually is
      while (cursor_screen_pos > screen_offset + (li_size - buffer_offset)) {
        cursor_screen_pos--;
        c3_assert(i < LINE_SIZE);
        line[i++] = '\b';
      }
      new_cursor_screen_pos = cursor_screen_pos;
      set_cursor_screen_pos = false;
    } else {
      // 5c2) ... or where it was before text re-drawing OR the new location
      pos = screen_offset + entry_width;
      while (pos > new_cursor_screen_pos) {
        c3_assert(i < LINE_SIZE);
        line[i++] = '\b';
        pos--;
      }
      cursor_screen_pos = new_cursor_screen_pos;
    }
    // 5d) terminate the line and do re-drawing
    c3_assert(i < LINE_SIZE);
    line[i] = 0;
    print_with_hidden_cursor(line);

    // 6) Get next key and see if we should return
    // -------------------------------------------

    const c3_key_t key = key_input.get_key();
    for (c3_uint_t k = 0; k < num_keys; k++) {
      if (key == keys[k]) {
        carriage_return();
        return key;
      }
    }

    // 7) Modify text buffer and/or screen position
    // --------------------------------------------

    c3_uint_t visible_portion = li_size - buffer_offset;
    if (visible_portion > entry_width) {
      visible_portion = entry_width;
    }
    switch (key) {
      case KEY_ARROW_LEFT:
        if (cursor_screen_pos > screen_offset) {
          new_cursor_screen_pos = cursor_screen_pos - 1;
        } else if (buffer_offset > 0) {
          buffer_offset--;
        } else {
          beep();
        }
        break;
      case KEY_CTRL | KEY_ARROW_LEFT: {
        c3_uint_t at = buffer_offset + (cursor_screen_pos - screen_offset);
        if (at > 0) {
          if (is_word_char(at) && !is_word_char(at - 1)) {
            at--; // we were on the very first letter of a word
          }
          while (at > 0 && !is_word_char(at)) {
            at--; // skip whitespace and punctuation
          }
          while (at > 0 && is_word_char(at - 1)) {
            at--; // go to the very first letter
          }
          if (at < buffer_offset) {
            buffer_offset = at;
          }
          new_cursor_screen_pos = screen_offset + (at - buffer_offset);
        } else {
          beep();
        }
        break;
      }
      case KEY_ARROW_RIGHT:
        if (cursor_screen_pos < screen_offset + visible_portion) {
          new_cursor_screen_pos = cursor_screen_pos + 1;
        } else if (buffer_offset + visible_portion < li_size) {
          buffer_offset++;
        } else {
          beep();
        }
        break;
      case KEY_CTRL | KEY_ARROW_RIGHT: {
        c3_uint_t at = buffer_offset + (cursor_screen_pos - screen_offset);
        if (at < li_size) {
          while (at < li_size && is_word_char(at)) {
            at++;
          }
          while (at < li_size && !is_word_char(at)) {
            at++;
          }
          if (at - buffer_offset >= entry_width) {
            buffer_offset = at - entry_width + 1;
          }
          new_cursor_screen_pos = screen_offset + (at - buffer_offset);
        } else {
          beep();
        }
        break;
      }
      case KEY_HOME:
        if (cursor_screen_pos > screen_offset || buffer_offset > 0) {
          new_cursor_screen_pos = screen_offset;
          buffer_offset = 0;
        } else {
          beep();
        }
        break;
      case KEY_END:
        if (cursor_screen_pos < screen_offset + visible_portion ||
          buffer_offset + visible_portion < li_size) {
          if (li_size >= entry_width) {
            buffer_offset = li_size - entry_width;
            new_cursor_screen_pos = screen_offset + entry_width;
          } else {
            new_cursor_screen_pos = screen_offset + li_size;
          }
        } else {
          beep();
        }
        break;
      case KEY_BACKSPACE:
        if (cursor_screen_pos > screen_offset) {
          const c3_uint_t from = buffer_offset + (cursor_screen_pos - screen_offset);
          const c3_uint_t to = from - 1;
          const c3_uint_t nchars = li_size - from + 1 /* terminating '\0' */ ;
          std::memmove(li_buffer + to, li_buffer + from, nchars);
          c3_assert(li_size);
          li_size--;
          new_cursor_screen_pos = cursor_screen_pos - 1;
        } else {
          beep();
        }
        break;
      case KEY_DELETE:
        if (cursor_screen_pos < screen_offset + visible_portion) {
          const c3_uint_t to = buffer_offset + (cursor_screen_pos - screen_offset);
          const c3_uint_t from = to + 1;
          const c3_uint_t nchars = li_size - from + 1 /* terminating '\0' */ ;
          std::memmove(li_buffer + to, li_buffer + from, nchars);
          c3_assert(li_size);
          li_size--;
        } else {
          beep();
        }
        break;
      case KEY_CTRL | KEY_DELETE:
        if (cursor_screen_pos < screen_offset + visible_portion) {
          const c3_uint_t to = buffer_offset + (cursor_screen_pos - screen_offset);
          bool deleting_space = li_buffer[to] == ' ';
          c3_uint_t from = to + 1;
          for (;;) {
            const char c = li_buffer[from];
            if (c == 0) {
              break; // end of the buffer
            }
            if (c == ' ') {
              if (!deleting_space) {
                deleting_space = true; // now remove whitespace *after* the word
              }
            } else {
              if (deleting_space || std::ispunct(c) != 0) {
                break; // end of the word
              }
            }
            from++;
          }
          const c3_uint_t nchars = li_size - from + 1 /* terminating '\0' */ ;
          std::memmove(li_buffer + to, li_buffer + from, nchars);
          c3_assert(li_size);
          li_size -= from - to;
        } else {
          beep();
        }
        break;
      default:
        // all control keys we know what to do with had already been processed
        if (Key::is_regular_char(key)) {
          char sequence[4];
          const c3_uint_t seq_length = (c3_uint_t) std::snprintf(sequence, sizeof sequence,
            Key::is_printable_char(key)? "%c": "\\%02X", Key::get_char(key));
          c3_assert(seq_length <= 3);
          if (li_size + seq_length + 1 < BUFFER_SIZE) {
            const c3_uint_t from = buffer_offset + (cursor_screen_pos - screen_offset);
            const c3_uint_t to = from + seq_length;
            const c3_uint_t nchars = li_size - from + 1 /* terminating '\0' */ ;
            std::memmove(li_buffer + to, li_buffer + from, nchars);
            std::memcpy(li_buffer + from, sequence, seq_length);
            li_size += seq_length;
            for (c3_uint_t k = 0; k < seq_length; k++) {
              if (new_cursor_screen_pos < screen_offset + entry_width) {
                new_cursor_screen_pos++;
              } else {
                buffer_offset++;
              }
            }
          } else {
            beep();
          }
        } else {
          beep();
        }
    }
  }
}

const char* LineInput::get_password(const char* prompt, c3_uint_t padding) {
  c3_assert(prompt);
  printf("%s%*s", prompt, padding, "");
  li_buffer[0] = 0;

  KeyInput key_input;
  for (;;) {
    const c3_key_t key = key_input.get_key();
    switch (key) {
      case KEY_BACKSPACE:
        if (li_size > 0) {
          li_buffer[li_size--] = 0;
        } else {
          beep();
        }
        break;
      case KEY_ESCAPE:
        if (li_size > 0) {
          li_buffer[0] = 0;
          li_size = 0;
        } else {
          beep();
        }
        break;
      case KEY_ENTER:
        carriage_return();
        return li_size > 0? li_buffer: nullptr;
      default:
        if (Key::is_printable_char(key) && key != ' ' && li_size < BUFFER_SIZE - 1) {
          li_buffer[li_size++] = Key::get_char(key);
          li_buffer[li_size] = 0;
        } else {
          beep();
        }
    }
  }
}

} // CyberCache
