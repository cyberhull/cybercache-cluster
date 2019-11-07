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
#ifndef _KEY_DEFS_H
#define _KEY_DEFS_H

#include "c3lib/c3lib.h"
#include <cctype>

namespace CyberCache {

typedef c3_ushort_t c3_key_t;

/*
 * Key codes returned by key input method of the `KeyInput` class in addition to the regular characters.
 *
 * These special codes are either translated from regular chars (e.g. '\b' or '\n'),  or from escape
 * sequences (e.g. arrow keys).
 */
constexpr c3_key_t KEY_ESCAPE      = 0x0100; // `Escape` key pressed *twice*
constexpr c3_key_t KEY_ENTER       = 0x0200; // '\n'
constexpr c3_key_t KEY_TAB         = 0x0300; // '\t'
constexpr c3_key_t KEY_BACKSPACE   = 0x0400; // '\b'
constexpr c3_key_t KEY_INSERT      = 0x0500;
constexpr c3_key_t KEY_DELETE      = 0x0600;
constexpr c3_key_t KEY_ARROW_UP    = 0x0700;
constexpr c3_key_t KEY_ARROW_DOWN  = 0x0800;
constexpr c3_key_t KEY_ARROW_LEFT  = 0x0900;
constexpr c3_key_t KEY_ARROW_RIGHT = 0x0A00;
constexpr c3_key_t KEY_HOME        = 0x0B00;
constexpr c3_key_t KEY_END         = 0x0C00;
constexpr c3_key_t KEY_BEGIN       = 0x0D00; // '5' on keypad with NumLock OFF
constexpr c3_key_t KEY_PAGE_UP     = 0x0E00;
constexpr c3_key_t KEY_PAGE_DOWN   = 0x0F00;
constexpr c3_key_t KEY_F1          = 0x1000;
constexpr c3_key_t KEY_F2          = 0x1100;
constexpr c3_key_t KEY_F3          = 0x1200;
constexpr c3_key_t KEY_F4          = 0x1300;
constexpr c3_key_t KEY_F5          = 0x1400;
constexpr c3_key_t KEY_F6          = 0x1500;
constexpr c3_key_t KEY_F7          = 0x1600;
constexpr c3_key_t KEY_F8          = 0x1700;
constexpr c3_key_t KEY_F9          = 0x1800;
constexpr c3_key_t KEY_F10         = 0x1900;
constexpr c3_key_t KEY_F11         = 0x1A00;
constexpr c3_key_t KEY_F12         = 0x1B00;
constexpr c3_key_t KEY_CTRL        = 0x2000; // Ctrl is down
constexpr c3_key_t KEY_ALT         = 0x4000; // Ctrl is down
constexpr c3_key_t KEY_SHIFT       = 0x8000; // Ctrl is down
constexpr c3_key_t KEY_BREAK       = 0xFFFF; // Ctl-C or Ctrl-Break

class Key {
public:
  static bool is_control_key(c3_key_t key) { return (key & 0xFF00) != 0; }
  static bool is_regular_char(c3_key_t key) { return (key & 0xFF00) == 0; }
  static bool is_printable_char(c3_key_t key) { return (key & 0xFF00) == 0 && isprint((c3_byte_t) key) != 0; }

  static c3_byte_t get_char(c3_key_t key) { return (c3_byte_t) key; }
};

} // CyberCache

#endif // _KEY_DEFS_H
