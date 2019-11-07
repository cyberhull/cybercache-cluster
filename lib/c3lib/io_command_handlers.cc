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
#include "io_command_handlers.h"
#include "c3lib.h"

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// CommandAccessor
///////////////////////////////////////////////////////////////////////////////

command_password_type_t CommandAccessor::get_command_pwd_hash(c3_hash_t& hash) const {
  auto type = (command_password_type_t) get_auth_type();
  if (type != CPT_NO_PASSWORD) {
    hash = get_header_ulong_at(get_command_header_data_size_length() + 2);
  }
  return type;
}

bool CommandAccessor::set_command_pwd_hash(command_password_type_t type, c3_hash_t hash) {
  c3_byte_t desc = get_command_descriptor();
  c3_byte_t auth = desc & DESC_AUTH_BITS;
  if (auth != type) {
    if (auth == DESC_NO_AUTH || type == CPT_NO_PASSWORD) {
      // cannot shrink or expand header
      return false;
    }
    set_header_byte_at(0, (desc & ~DESC_AUTH_BITS) | type);
  }
  if (type != CPT_NO_PASSWORD) {
    c3_assert(auth);
    set_header_ulong_at(get_command_header_data_size_length() + 2, hash);
  }
  return true;
}

bool CommandAccessor::get_header_info(header_info_t &hi) const {

  // 1) Figure out offset to the data chunks
  c3_byte_t desc = get_command_descriptor();
  // initialize offset with combined size of command descriptor, command ID, and optional password hash
  c3_uint_t offset = (desc & DESC_AUTH_BITS) != 0? sizeof(c3_hash_t) + 2: 2;
  switch (desc & DESC_HEADER_BITS) {
    case DESC_NO_HEADER:
      return hi.invalidate();
    case DESC_BYTE_HEADER:
      offset++;
      break;
    case DESC_WORD_HEADER:
      offset += 2;
      break;
    default:
      offset += 4;
  }
  switch (desc & DESC_PAYLOAD_BITS) {
    case DESC_NO_PAYLOAD:
      break;
    case DESC_BYTE_PAYLOAD:
      offset += (desc & DESC_PAYLOAD_IS_COMPRESSED) != 0? 3: 1;
      break;
    case DESC_WORD_PAYLOAD:
      offset += (desc & DESC_PAYLOAD_IS_COMPRESSED) != 0? 5: 2;
      break;
    default:
      offset += (desc & DESC_PAYLOAD_IS_COMPRESSED) != 0? 9: 4;
  }
  // 2) Figure out size
  c3_uint_t header_size = get_command_header_size();
  if (offset < header_size) {
    hi.hi_chunks_offset = offset;
    hi.hi_chunks_size = header_size - offset;
    return true;
  } else {
    assert(offset == header_size);
    return hi.invalidate();
  }
}

bool CommandAccessor::get_payload_info(payload_info_t &pi) const {
  c3_byte_t desc = get_command_descriptor();
  if ((desc & DESC_PAYLOAD_BITS) == DESC_NO_PAYLOAD) {
    return pi.invalidate(false);
  }
  c3_uint_t offset = get_command_header_data_size_length() + 2;
  if ((desc & DESC_AUTH_BITS) != 0) {
    offset += sizeof(c3_hash_t);
  }
  c3_uint_t payload_size, available_header_size = get_available_header_size();
  switch (desc & DESC_PAYLOAD_BITS) {
    case DESC_BYTE_PAYLOAD:
      if ((desc & DESC_PAYLOAD_IS_COMPRESSED) != 0) {
        if (available_header_size < offset + 3) {
          goto INVALID_HEADER;
        }
        pi.pi_size = get_header_byte_at(offset + 1);
        pi.pi_usize = get_header_byte_at(offset + 2);
      } else {
        if (available_header_size < offset + 1) {
          goto INVALID_HEADER;
        }
        pi.pi_size = get_header_byte_at(offset);
      }
      break;
    case DESC_WORD_PAYLOAD:
      if ((desc & DESC_PAYLOAD_IS_COMPRESSED) != 0) {
        if (available_header_size < offset + 5) {
          goto INVALID_HEADER;
        }
        pi.pi_size = get_header_ushort_at(offset + 1);
        pi.pi_usize = get_header_ushort_at(offset + 3);
      } else {
        if (available_header_size < offset + 2) {
          goto INVALID_HEADER;
        }
        pi.pi_size = get_header_ushort_at(offset);
      }
      break;
    default:
      if ((desc & DESC_PAYLOAD_IS_COMPRESSED) != 0) {
        if (available_header_size < offset + 9) {
          goto INVALID_HEADER;
        }
        pi.pi_size = get_header_uint_at(offset + 1);
        pi.pi_usize = get_header_uint_at(offset + 5);
      } else {
        if (available_header_size < offset + 4) {
          goto INVALID_HEADER;
        }
        pi.pi_size = get_header_uint_at(offset);
      }
  }
  if ((desc & DESC_PAYLOAD_IS_COMPRESSED) != 0) {
    pi.pi_compressor = (c3_compressor_t) get_header_byte_at(offset);
    if (pi.pi_compressor == CT_NONE || pi.pi_compressor >= CT_NUMBER_OF_ELEMENTS ||
      pi.pi_size == 0 || pi.pi_size >= pi.pi_usize) {
      goto INVALID_HEADER;
    }
  } else {
    pi.pi_compressor = CT_NONE;
    pi.pi_usize = pi.pi_size;
  }
  payload_size = get_payload_size();
  if (payload_size > 0) {
    if (payload_size == pi.pi_size) {
      pi.pi_buffer = get_payload_bytes(0, payload_size);
    } else {
      goto INVALID_HEADER;
    }
  } else {
    // not received yet, or zero-length
    pi.pi_buffer = nullptr;
  }
  return true;

INVALID_HEADER:
  return pi.invalidate(true);
}

///////////////////////////////////////////////////////////////////////////////
// CommandReader
///////////////////////////////////////////////////////////////////////////////

CommandReader* CommandReader::clone(bool full) const {
  if ((rw_flags & IO_FLAG_NETWORK) != 0) {
    auto scr = alloc<SocketCommandReader>(get_sb_memory_object());
    return new (scr) SocketCommandReader(*this, full);
  } else {
    auto fcr = alloc<FileCommandReader>(get_sb_memory_object());
    return new (fcr) FileCommandReader(*this, full);
  }
}

io_result_t CommandReader::read(c3_ulong_t& ntotal) {
  ntotal = 0;
  for (;;) {
    c3_uint_t nread;
    io_result_t result;
    switch (rw_state) {
      case IO_STATE_CREATED:
        rw_state = IO_STATE_COMMAND_READ_DESCRIPTOR;
        // fall through

      case IO_STATE_COMMAND_READ_DESCRIPTOR:
        result = read_bytes(get_fd(), get_header_bytes(0, 1), 1, nread);
        switch (result) {
          case IO_RESULT_OK:
            c3_assert(nread == 1);
            ntotal++;
            rw_pos = 1;
            rw_remains = get_command_header_data_size_length();
            if (rw_remains == 0) { // "sizeless" header?
              c3_uint_t full_command_size = get_command_sizeless_header_size();
              configure_header(rw_pos, full_command_size);
              rw_remains = full_command_size - rw_pos;
              rw_state = IO_STATE_COMMAND_READ_HEADER_BYTES;
              continue; // continue with the next state
            }
            rw_state = IO_STATE_COMMAND_READ_HEADER_SIZE_BYTES;
            continue; // continue with the next state

          case IO_RESULT_ERROR:
          case IO_RESULT_EOF:
            rw_state = IO_STATE_ERROR;
            // fall through
          default: // IO_RESULT_RETRY
            return result;
        }

      case IO_STATE_COMMAND_READ_HEADER_SIZE_BYTES:
        result = read_bytes(get_fd(), get_header_bytes(rw_pos, rw_remains), rw_remains, nread);
        switch (result) {
          case IO_RESULT_OK:
            ntotal += nread;
            rw_pos += nread;
            if (nread == rw_remains) {
              c3_uint_t full_header_size = get_command_header_size();
              c3_assert(full_header_size > rw_pos);
              configure_header(rw_pos, full_header_size);
              rw_remains = full_header_size - rw_pos;
              rw_state = IO_STATE_COMMAND_READ_HEADER_BYTES;
              continue; // continue with the next state
            } else {
              rw_remains -= nread;
              continue; // try again until explicitly told to retry later
            }
          case IO_RESULT_ERROR:
          case IO_RESULT_EOF:
            rw_state = IO_STATE_ERROR;
            // fall through
          default: // IO_RESULT_RETRY
            return result;
        }

      case IO_STATE_COMMAND_READ_HEADER_BYTES:
        result = read_bytes(get_fd(), get_header_bytes(rw_pos, rw_remains), rw_remains, nread);
        switch (result) {
          case IO_RESULT_OK:
            ntotal += nread;
            if (nread == rw_remains) {
              payload_info_t pi;
              if (get_payload_info(pi) && pi.pi_size > 0) {
                c3_assert(!pi.pi_has_errors);
                set_payload_size(pi.pi_size);
                rw_pos = 0;
                rw_remains = pi.pi_size;
                rw_state = IO_STATE_COMMAND_READ_PAYLOAD_BYTES;
                break; // continue with payload
              } else {
                if (pi.pi_has_errors) {
                  rw_state = IO_STATE_ERROR;
                  return IO_RESULT_ERROR;
                }
                rw_state = IO_STATE_COMMAND_READ_MARKER_BYTE;
                continue; // continue with the next state
              }
            } else {
              rw_pos += nread;
              rw_remains -= nread;
              continue; // try again until explicitly told to retry later
            }
          case IO_RESULT_ERROR:
          case IO_RESULT_EOF:
            rw_state = IO_STATE_ERROR;
            // fall through
          default: // IO_RESULT_RETRY
            return result;
        }

      case IO_STATE_COMMAND_READ_PAYLOAD_BYTES:
        result = read_bytes(get_fd(), get_payload_bytes(rw_pos, rw_remains), rw_remains, nread);
        switch (result) {
          case IO_RESULT_OK:
            ntotal += nread;
            if (nread == rw_remains) {
              rw_state = IO_STATE_COMMAND_READ_MARKER_BYTE;
              continue; // continue with the next state
            } else {
              rw_pos += nread;
              rw_remains -= nread;
              continue; // try again until explicitly told to retry later
            }
          case IO_RESULT_ERROR:
          case IO_RESULT_EOF:
            rw_state = IO_STATE_ERROR;
            // fall through
          default: // IO_RESULT_RETRY
            return result;
        }

      case IO_STATE_COMMAND_READ_MARKER_BYTE:
        if (command_marker_is_present()) {
          c3_byte_t marker = 0;
          result = read_bytes(get_fd(), &marker, 1, nread);
          switch (result) {
            case IO_RESULT_OK:
              ntotal++;
              if (marker == C3_INTEGRITY_MARKER) {
                break; // done!
              }
              result = IO_RESULT_ERROR;
              // fall through
            case IO_RESULT_ERROR:
            case IO_RESULT_EOF:
              rw_state = IO_STATE_ERROR;
              // fall through
            case IO_RESULT_RETRY:
              return result;
          }
        }
        rw_state = IO_STATE_COMMAND_READ_DONE;
        rw_pos = UINT_MAX_VAL;
        rw_remains = 0;
        return IO_RESULT_OK; // done!

      default: // any other state
        c3_assert_failure();
        return set_error_state();
    }
  }
}

bool CommandReader::io_completed() const {
  return rw_state == IO_STATE_COMMAND_READ_DONE;
}

command_t CommandReader::get_command_id() const {
  c3_assert(rw_state == IO_STATE_COMMAND_READ_DONE);
  // right after descriptor and optional header size
    return (command_t) get_command_id_byte();
}

///////////////////////////////////////////////////////////////////////////////
// CommandWriter
///////////////////////////////////////////////////////////////////////////////

#if INCLUDE_COMMANDWRITER_CLONE
CommandWriter* CommandWriter::clone(bool full) {
  if ((rw_flags & IO_FLAG_NETWORK) != 0) {
    auto scw = alloc<SocketCommandWriter>(get_sb_memory_object());
    return new (scw) SocketCommandWriter(*this, full);
  } else {
    auto fcw = alloc<FileCommandWriter>(get_sb_memory_object());
    return new (fcw) FileCommandWriter(*this, full);
  }
}
#endif // INCLUDE_COMMANDWRITER_CLONE

io_result_t CommandWriter::write(c3_ulong_t& ntotal) {
  ntotal = 0;
  for (;;) {
    c3_uint_t nwritten;
    io_result_t result;
    switch (rw_state) {
      case IO_STATE_COMMAND_WRITE_READY:
        rw_pos = 0;
        rw_remains = get_command_header_size();
        rw_state = IO_STATE_COMMAND_WRITE_HEADER;
        if ((rw_flags & IO_FLAG_NETWORK) != 0) { // not a (file) binlog writer?
          c3_begin_data_block(get_fd());
        }
        // fall through

      case IO_STATE_COMMAND_WRITE_HEADER:
        result = write_bytes(get_fd(), get_header_bytes(rw_pos, rw_remains), rw_remains, nwritten);
        switch (result) {
          case IO_RESULT_OK:
            ntotal += nwritten;
            if (nwritten == rw_remains) {
              rw_remains = get_payload_size();
              if (rw_remains > 0) {
                rw_pos = 0;
                rw_state = IO_STATE_COMMAND_WRITE_PAYLOAD;
              } else {
                rw_state = IO_STATE_COMMAND_WRITE_MARKER_BYTE;
              }
              continue; // continue with the next state
            } else {
              rw_pos += nwritten;
              rw_remains -= nwritten;
              continue; // try again unless we are explicitly told to retry
            }
          case IO_RESULT_ERROR:
          case IO_RESULT_EOF:
            rw_state = IO_STATE_ERROR;
            // fall through
          default: // IO_RESULT_RETRY
            return result;
        }

      case IO_STATE_COMMAND_WRITE_PAYLOAD:
        result = write_bytes(get_fd(), get_payload_bytes(rw_pos, rw_remains), rw_remains, nwritten);
        switch (result) {
          case IO_RESULT_OK:
            ntotal += nwritten;
            if (nwritten == rw_remains) {
              rw_state = IO_STATE_COMMAND_WRITE_MARKER_BYTE;
              continue; // continue with the next state
            } else {
              rw_pos += nwritten;
              rw_remains -= nwritten;
              continue; // try again unless we are explicitly told to retry
            }
          case IO_RESULT_ERROR:
          case IO_RESULT_EOF:
            rw_state = IO_STATE_ERROR;
            // fall through
          default: // IO_RESULT_RETRY
            return result;
        }

      case IO_STATE_COMMAND_WRITE_MARKER_BYTE:
        if (command_marker_is_present()) {
          c3_byte_t marker = C3_INTEGRITY_MARKER;
          result = write_bytes(get_fd(), &marker, 1, nwritten);
          switch (result) {
            case IO_RESULT_OK:
              ntotal++;
              break; // done!

            case IO_RESULT_ERROR:
            case IO_RESULT_EOF:
              rw_state = IO_STATE_ERROR;
              // fall through
            default: // IO_RESULT_RETRY
              return result;
          }
        }
        rw_state = IO_STATE_COMMAND_WRITE_DONE;
        rw_pos = UINT_MAX_VAL;
        rw_remains = 0;
        if ((rw_flags & IO_FLAG_NETWORK) != 0) { // not a (file) binlog writer?
          c3_end_data_block(get_fd());
        }
        return IO_RESULT_OK; // done!

      default:
        c3_assert_failure();
        return set_error_state();
    }
  }
}

bool CommandWriter::io_completed() const {
  return rw_state == IO_STATE_COMMAND_WRITE_DONE;
}

void CommandWriter::io_rewind(int fd, c3_ipv4_t ipv4) {
  c3_assert(is_valid() && fd > 0);
  configure_descriptor(fd, ipv4);
  // I/O finite state automaton will set position, remaining bytes etc. itself
  rw_state = IO_STATE_COMMAND_WRITE_READY;
}

#if INCLUDE_COMMANDWRITER_GET_COMMAND_ID
command_t CommandWriter::get_command_id() const {
  c3_assert(rw_state >= IO_STATE_COMMAND_WRITE_READY && rw_state <= IO_STATE_COMMAND_WRITE_DONE);
  // right after descriptor and optional header size
  return (command_t) get_command_id_byte();
}
#endif // INCLUDE_COMMANDWRITER_GET_COMMAND_ID

} // CyberCache
