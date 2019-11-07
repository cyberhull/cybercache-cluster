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
#include "key_input.h"

#include <termios.h>
#include <cstdio>

namespace CyberCache {

// VT52 emulation not included (uses single ESC prefixes)
const KeyInput::key_sequence_t KeyInput::ki_sequences[] = {
  {{ 'A', 0,   0,   0   }, 1, KST_CSI, KEY_ARROW_UP               }, // regular mode / Cygwin
  {{ 'A', 0,   0,   0   }, 1, KST_SS3, KEY_ARROW_UP               }, // DECCKM mode
  {{ 'B', 0,   0,   0   }, 1, KST_CSI, KEY_ARROW_DOWN             }, // regular mode / Cygwin
  {{ 'B', 0,   0,   0   }, 1, KST_SS3, KEY_ARROW_DOWN             }, // DECCKM mode
  {{ 'C', 0,   0,   0   }, 1, KST_CSI, KEY_ARROW_RIGHT            }, // regular mode / Cygwin
  {{ 'C', 0,   0,   0   }, 1, KST_SS3, KEY_ARROW_RIGHT            }, // DECCKM mode
  {{ 'D', 0,   0,   0   }, 1, KST_CSI, KEY_ARROW_LEFT             }, // regular mode / Cygwin
  {{ 'D', 0,   0,   0   }, 1, KST_SS3, KEY_ARROW_LEFT             }, // DECCKM mode
  {{ 'H', 0,   0,   0   }, 1, KST_CSI, KEY_HOME                   }, // regular mode
  {{ 'H', 0,   0,   0   }, 1, KST_SS3, KEY_HOME                   }, // DECCKM mode
  {{ '1', '~', 0,   0   }, 2, KST_CSI, KEY_HOME                   }, // VT220 / Cygwin
  {{ 'F', 0,   0,   0   }, 1, KST_CSI, KEY_END                    }, // regular mode
  {{ 'F', 0,   0,   0   }, 1, KST_SS3, KEY_END                    }, // DECCKM mode
  {{ '4', '~', 0,   0   }, 2, KST_CSI, KEY_END                    }, // VT220 / Cygwin
  {{ 'M', 0,   0,   0   }, 1, KST_SS3, KEY_ENTER                  }, // keypad, DECCKM mode
  {{ 'j', 0,   0,   0   }, 1, KST_SS3, '*'                        }, // keypad, DECCKM mode
  {{ 'k', 0,   0,   0   }, 1, KST_SS3, '+'                        }, // keypad, DECCKM mode
  {{ 'm', 0,   0,   0   }, 1, KST_SS3, '-'                        }, // keypad, DECCKM mode
  {{ '3', '~', 0,   0   }, 2, KST_CSI, KEY_DELETE                 }, // keypad, DECCKM mode / Cygwin
  {{ 'n', 0,   0,   0   }, 1, KST_SS3, '.'                        }, // VT102/VT220
  {{ 'o', 0,   0,   0   }, 1, KST_SS3, '/'                        }, // keypad, DECCKM mode
  {{ '2', '~', 0,   0   }, 2, KST_CSI, KEY_INSERT                 }, // keypad, DECCKM mode / Cygwin
  {{ '6', '~', 0,   0   }, 2, KST_CSI, KEY_PAGE_DOWN              }, // keypad, DECCKM mode / Cygwin
  {{ 'E', 0,   0,   0   }, 1, KST_CSI, KEY_BEGIN                  }, // keypad, DECCKM mode
  {{ 'G', 0,   0,   0   }, 1, KST_CSI, KEY_BEGIN                  }, // Cygwin
  {{ 'p', 0,   0,   0   }, 1, KST_SS3, '0'                        }, // VT102/VT220, keypad
  {{ 'q', 0,   0,   0   }, 1, KST_SS3, '1'                        }, // VT102/VT220, keypad
  {{ 'r', 0,   0,   0   }, 1, KST_SS3, '2'                        }, // VT102/VT220, keypad
  {{ 's', 0,   0,   0   }, 1, KST_SS3, '3'                        }, // VT102/VT220, keypad
  {{ 't', 0,   0,   0   }, 1, KST_SS3, '4'                        }, // VT102/VT220, keypad
  {{ 'u', 0,   0,   0   }, 1, KST_SS3, '5'                        }, // VT102/VT220, keypad
  {{ 'v', 0,   0,   0   }, 1, KST_SS3, '6'                        }, // VT102/VT220, keypad
  {{ 'w', 0,   0,   0   }, 1, KST_SS3, '7'                        }, // VT102/VT220, keypad
  {{ 'x', 0,   0,   0   }, 1, KST_SS3, '8'                        }, // VT102/VT220, keypad
  {{ 'y', 0,   0,   0   }, 1, KST_SS3, '9'                        }, // VT102/VT220, keypad
  {{ '5', '~', 0,   0   }, 2, KST_CSI, KEY_PAGE_UP                }, // keypad, DECCKM mode / Cygwin
  {{ 'P', 0,   0,   0   }, 1, KST_SS3, KEY_F1                     }, // xterm
  {{ '1', '1', '~', 0   }, 3, KST_CSI, KEY_F1                     }, // xterm, older version
  {{ '[', 'A', 0,   0   }, 2, KST_CSI, KEY_F1                     }, // Cygwin
  {{ 'Q', 0,   0,   0   }, 1, KST_SS3, KEY_F2                     }, // xterm
  {{ '1', '2', '~', 0   }, 3, KST_CSI, KEY_F2                     }, // xterm, older version
  {{ '[', 'B', 0,   0   }, 2, KST_CSI, KEY_F2                     }, // Cygwin
  {{ 'R', 0,   0,   0   }, 1, KST_SS3, KEY_F3                     }, // xterm
  {{ '1', '3', '~', 0   }, 3, KST_CSI, KEY_F3                     }, // xterm, older version
  {{ '[', 'C', 0,   0   }, 2, KST_CSI, KEY_F3                     }, // Cygwin
  {{ 'S', 0,   0,   0   }, 1, KST_SS3, KEY_F4                     }, // xterm
  {{ '1', '4', '~', 0   }, 3, KST_CSI, KEY_F4                     }, // xterm, older version
  {{ '[', 'D', 0,   0   }, 2, KST_CSI, KEY_F4                     }, // Cygwin
  {{ '1', '5', '~', 0   }, 3, KST_CSI, KEY_F5                     }, // xterm
  {{ '[', 'E', 0,   0   }, 2, KST_CSI, KEY_F5                     }, // Cygwin
  {{ '1', '7', '~', 0   }, 3, KST_CSI, KEY_F6                     }, // xterm / Cygwin
  {{ '1', '8', '~', 0   }, 3, KST_CSI, KEY_F7                     }, // xterm / Cygwin
  {{ '1', '9', '~', 0   }, 3, KST_CSI, KEY_F8                     }, // xterm / Cygwin
  {{ '2', '0', '~', 0   }, 3, KST_CSI, KEY_F9                     }, // xterm / Cygwin
  {{ '2', '1', '~', 0   }, 3, KST_CSI, KEY_F10                    }, // xterm / Cygwin
  {{ '2', '3', '~', 0   }, 3, KST_CSI, KEY_F11                    }, // xterm / Cygwin
  {{ '2', '4', '~', 0   }, 3, KST_CSI, KEY_F12                    }, // xterm / Cygwin
  {{ '1', ';', '5', 'A' }, 4, KST_CSI, KEY_CTRL | KEY_ARROW_UP    }, // Cygwin
  {{ '1', ';', '5', 'B' }, 4, KST_CSI, KEY_CTRL | KEY_ARROW_DOWN  }, // Cygwin
  {{ '1', ';', '5', 'C' }, 4, KST_CSI, KEY_CTRL | KEY_ARROW_RIGHT }, // Cygwin
  {{ '1', ';', '5', 'D' }, 4, KST_CSI, KEY_CTRL | KEY_ARROW_LEFT  }, // Cygwin
  {{ '3', ';', '5', '~' }, 4, KST_CSI, KEY_CTRL | KEY_DELETE      }, // Cygwin
};

int KeyInput::get_raw_key() {
  struct termios old_attr;
  tcgetattr(STDIN_FILENO, &old_attr);
  struct termios new_attr = old_attr;
  new_attr.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &new_attr);
  int c = getchar();
  tcsetattr(STDIN_FILENO, TCSANOW, &old_attr);
  return c;
}

c3_key_t KeyInput::store_and_check_char(c3_byte_t c) {
  c3_assert(ki_length >= 2 && ki_seq_type != KST_INVALID);
  if (ki_length < MAX_SEQUENCE_LENGTH) {
    store_char(c);
    const c3_uint_t NUM_ESCAPE_SEQUENCES = sizeof ki_sequences / sizeof(key_sequence_t);
    bool longer_sequence_found = false;
    c3_byte_t subsequence_length = (c3_byte_t)(ki_length - 2);
    for (c3_uint_t i = 0; i < NUM_ESCAPE_SEQUENCES; i++) {
      const key_sequence_t& sequence = ki_sequences[i];
      if (sequence.ks_type == ki_seq_type) {
        if (sequence.ks_length == subsequence_length) {
          if (std::memcmp(ki_chars + 2, sequence.ks_sequence, subsequence_length) == 0) {
            reset_sequence();
            return sequence.ks_key;
          }
        } else if (sequence.ks_length > subsequence_length) {
          longer_sequence_found = true;
        }
      }
    }
    if (longer_sequence_found) {
      return 0; // will try again later...
    }
  }
  reset_sequence();
  return c;
}

c3_key_t KeyInput::get_key() {
  for (;;) {
    c3_key_t key;
    const int raw_key = get_raw_key();
    // printf("    --> 0x%02X (%3d) '%c'\n", raw_key, raw_key, raw_key);
    switch (raw_key) {
      case -1: // Ctrl-C / Ctrl-Break (will only receive this if `SIGINT` is blocked)
        reset_sequence();
        return KEY_BREAK;
      case 0x00: // Ctrl-SPACE
        reset_sequence();
        // processing it separately because `store_and_check_char()` wouldn't catch it
        return 0;
      case 0x0A: // Enter
        reset_sequence();
        return KEY_ENTER;
      case 0x09: // Tab
        reset_sequence();
        return KEY_TAB;
      case 0x7F: // Backspace
        reset_sequence();
        return KEY_BACKSPACE;
      case 0x1B: // Escape
        switch (ki_length) {
          default: // some unrecognized sequence; discard it and start over...
            reset_sequence();
            // fall through
          case 0:
            c3_assert(ki_seq_type == KST_INVALID);
            store_char(0x1B);
            continue;
          case 1: // second press
            c3_assert(ki_seq_type == KST_INVALID);
            ki_length = 0;
            return KEY_ESCAPE;
        }
      case 0x5B: // "["
        switch (ki_length) {
          case 0:
            c3_assert(ki_seq_type == KST_INVALID);
            return '[';
          case 1:
            c3_assert(ki_seq_type == KST_INVALID);
            store_char('[');
            ki_seq_type = KST_CSI;
            continue;
          default:
            key = store_and_check_char('[');
            if (key != 0) {
              return key;
            }
            continue;
        }
      case 0x4F: // "O"
        switch (ki_length) {
          case 0:
            c3_assert(ki_seq_type == KST_INVALID);
            return 'O';
          case 1:
            c3_assert(ki_seq_type == KST_INVALID);
            store_char('O');
            ki_seq_type = KST_SS3;
            continue;
          default:
            key = store_and_check_char('O');
            if (key != 0) {
              return key;
            }
            continue;
        }
      default:
        switch (ki_length) {
          case 1: // invalid escape sequence, discard it and return the key
            c3_assert(ki_seq_type == KST_INVALID);
            reset_sequence();
            // fall through
          case 0:
            c3_assert(ki_seq_type == KST_INVALID);
            return (c3_byte_t) raw_key;
          default:
            key = store_and_check_char((c3_byte_t) raw_key);
            if (key != 0) {
              return key;
            }
        }
    }
  }
}

} // CyberCache
