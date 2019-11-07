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
#include "io_response_handlers.h"
#include "c3lib.h"

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// ResponseAccessor
///////////////////////////////////////////////////////////////////////////////

bool ResponseAccessor::get_header_info(header_info_t &hi) const {
  // 1) Figure out offset
  c3_uint_t offset;
  c3_byte_t desc = get_response_descriptor();
  switch (desc & RESP_HEADER_BITS) {
    case RESP_NO_HEADER:
      return hi.invalidate();
    case RESP_BYTE_HEADER:
      offset = 2;
      break;
    case RESP_WORD_HEADER:
      offset = 3;
      break;
    default:
      offset = 5;
      break;
  }
  switch (desc & RESP_PAYLOAD_BITS) {
    case RESP_NO_PAYLOAD:
      break;
    case RESP_BYTE_PAYLOAD:
      offset += (desc & RESP_PAYLOAD_IS_COMPRESSED) != 0? 3: 1;
      break;
    case RESP_WORD_PAYLOAD:
      offset += (desc & RESP_PAYLOAD_IS_COMPRESSED) != 0? 5: 2;
      break;
    default:
      offset += (desc & RESP_PAYLOAD_IS_COMPRESSED) != 0? 9: 4;
  }
  // 2) Figure out size
  c3_uint_t header_size = get_response_header_size();
  if (offset < header_size) {
    hi.hi_chunks_offset = offset;
    hi.hi_chunks_size = header_size - offset;
    return true;
  } else {
    assert(offset == header_size);
    return hi.invalidate();
  }
}

bool ResponseAccessor::get_payload_info(payload_info_t &pi) const {
  c3_byte_t desc = get_response_descriptor();
  if ((desc & RESP_PAYLOAD_BITS) == RESP_NO_PAYLOAD) {
    return pi.invalidate(false);
  }
  c3_uint_t offset = get_response_header_data_size_length() + 1;
  c3_uint_t payload_size, available_header_size = get_available_header_size();
  switch (desc & RESP_PAYLOAD_BITS) {
    case RESP_BYTE_PAYLOAD:
      if ((desc & RESP_PAYLOAD_IS_COMPRESSED) != 0) {
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
    case RESP_WORD_PAYLOAD:
      if ((desc & RESP_PAYLOAD_IS_COMPRESSED) != 0) {
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
      if ((desc & RESP_PAYLOAD_IS_COMPRESSED) != 0) {
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
  if ((desc & RESP_PAYLOAD_IS_COMPRESSED) != 0) {
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
    // not received yet
    pi.pi_buffer = nullptr;
  }
  return true;

INVALID_HEADER:
  return pi.invalidate(true);
}

///////////////////////////////////////////////////////////////////////////////
// ResponseReader
///////////////////////////////////////////////////////////////////////////////

#if INCLUDE_RESPONSEREADER_CLONE
ResponseReader* ResponseReader::clone(bool full) {
  #if INCLUDE_FILERESPONSEREADER
  if ((rw_flags & IO_FLAG_NETWORK) != 0) {
  #endif // INCLUDE_FILERESPONSEREADER
    auto srr = alloc<SocketResponseReader>(get_sb_memory_object());
    return new (srr) SocketResponseReader(*this, full);
  #if INCLUDE_FILERESPONSEREADER
  } else {
    auto frr = alloc<FileResponseReader>(get_sb_memory_object());
    return new (frr) FileResponseReader(*this, full);
  }
  #endif // INCLUDE_FILERESPONSEREADER
}
#endif // INCLUDE_RESPONSEREADER_CLONE

io_result_t ResponseReader::read(c3_ulong_t& ntotal) {
  ntotal = 0;
  for (;;) {
    c3_uint_t nread;
    io_result_t result;
    switch (rw_state) {
      case IO_STATE_CREATED:
        rw_state = IO_STATE_RESPONSE_READ_DESCRIPTOR;
        // fall through

      case IO_STATE_RESPONSE_READ_DESCRIPTOR:
        result = read_bytes(get_fd(), get_header_bytes(0, 1), 1, nread);
        switch (result) {
          case IO_RESULT_OK:
            c3_assert(nread == 1);
            ntotal++;
            rw_state = IO_STATE_RESPONSE_READ_HEADER_SIZE_BYTES;
            rw_pos = 1;
            rw_remains = get_response_header_data_size_length();
            if (rw_remains == 0) {
              // zero-length header; must be 'OK' response
              if (get_response_type() == RESP_TYPE_OK) {
                rw_state = IO_STATE_RESPONSE_READ_MARKER_BYTE;
                continue; // continue with the next state
              } else {
                c3_assert_failure();
                return set_error_state();
              }
            }
            continue; // continue with the next state

          case IO_RESULT_ERROR:
          case IO_RESULT_EOF:
            rw_state = IO_STATE_ERROR;
            // fall through
          default: // IO_RESULT_RETRY
            return result;
        }

      case IO_STATE_RESPONSE_READ_HEADER_SIZE_BYTES:
        result = read_bytes(get_fd(), get_header_bytes(rw_pos, rw_remains), rw_remains, nread);
        switch (result) {
          case IO_RESULT_OK:
            ntotal += nread;
            rw_pos += nread;
            if (nread == rw_remains) {
              c3_uint_t full_header_size = get_response_header_size();
              c3_assert(full_header_size > rw_pos);
              configure_header(rw_pos, full_header_size);
              rw_remains = full_header_size - rw_pos;
              rw_state = IO_STATE_RESPONSE_READ_HEADER_BYTES;
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

      case IO_STATE_RESPONSE_READ_HEADER_BYTES:
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
                rw_state = IO_STATE_RESPONSE_READ_PAYLOAD_BYTES;
                break; // continue with payload
              } else {
                if (pi.pi_has_errors) {
                  rw_state = IO_STATE_ERROR;
                  return IO_RESULT_ERROR;
                }
                rw_state = IO_STATE_RESPONSE_READ_MARKER_BYTE;
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

      case IO_STATE_RESPONSE_READ_PAYLOAD_BYTES:
        result = read_bytes(get_fd(), get_payload_bytes(rw_pos, rw_remains), rw_remains, nread);
        switch (result) {
          case IO_RESULT_OK:
            ntotal += nread;
            if (nread == rw_remains) {
              rw_state = IO_STATE_RESPONSE_READ_MARKER_BYTE;
              continue; // continue with the next state
            } else {
              rw_pos += nread;
              rw_remains -= nread;
              return IO_RESULT_RETRY;
            }
          case IO_RESULT_ERROR:
          case IO_RESULT_EOF:
            rw_state = IO_STATE_ERROR;
            // fall through
          default: // IO_RESULT_RETRY
            return result;
        }

      case IO_STATE_RESPONSE_READ_MARKER_BYTE:
        if (response_marker_is_present()) {
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
        rw_state = IO_STATE_RESPONSE_READ_DONE;
        rw_pos = UINT_MAX_VAL;
        rw_remains = 0;
        return IO_RESULT_OK; // done!

      default: // any other state
        c3_assert_failure();
        return set_error_state();
    }
  }
}

bool ResponseReader::io_completed() const {
  return rw_state == IO_STATE_RESPONSE_READ_DONE;
}

response_type_t ResponseReader::get_type() const {
  c3_assert(rw_state == IO_STATE_RESPONSE_READ_DONE);
  return get_translated_response_type();
}

///////////////////////////////////////////////////////////////////////////////
// ResponseWriter
///////////////////////////////////////////////////////////////////////////////

#if INCLUDE_RESPONSEWRITER_CLONE
ResponseWriter* ResponseWriter::clone(bool full) {
  #if INCLUDE_FILERESPONSEWRITER
  if ((rw_flags & IO_FLAG_NETWORK) != 0) {
  #endif
    auto srr = alloc<SocketResponseWriter>(get_sb_memory_object());
    return new (srr) SocketResponseWriter(*this, full);
  #if INCLUDE_FILERESPONSEWRITER
  } else {
    auto frr = alloc<FileResponseWriter>(get_sb_memory_object());
    return new (frr) FileResponseWriter(*this, full);
  }
  #endif // INCLUDE_FILERESPONSEWRITER
}
#endif // INCLUDE_RESPONSEWRITER_CLONE

io_result_t ResponseWriter::write(c3_ulong_t& ntotal) {
  ntotal = 0;
  for (;;) {
    c3_uint_t nwritten;
    io_result_t result;
    switch (rw_state) {
      case IO_STATE_RESPONSE_WRITE_READY:
        rw_pos = 0;
        rw_remains = get_response_header_size();
        rw_state = IO_STATE_RESPONSE_WRITE_HEADER;
        c3_begin_data_block(get_fd());
        // fall through

      case IO_STATE_RESPONSE_WRITE_HEADER:
        result = write_bytes(get_fd(), get_header_bytes(rw_pos, rw_remains), rw_remains, nwritten);
        switch (result) {
          case IO_RESULT_OK:
            ntotal += nwritten;
            if (nwritten == rw_remains) {
              rw_remains = get_payload_size();
              if (rw_remains > 0) {
                rw_pos = 0;
                rw_state = IO_STATE_RESPONSE_WRITE_PAYLOAD;
              } else {
                rw_state = IO_STATE_RESPONSE_WRITE_MARKER_BYTE;
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

      case IO_STATE_RESPONSE_WRITE_PAYLOAD:
        result = write_bytes(get_fd(), get_payload_bytes(rw_pos, rw_remains), rw_remains, nwritten);
        switch (result) {
          case IO_RESULT_OK:
            ntotal += nwritten;
            if (nwritten == rw_remains) {
              rw_state = IO_STATE_RESPONSE_WRITE_MARKER_BYTE;
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

      case IO_STATE_RESPONSE_WRITE_MARKER_BYTE:
        if (response_marker_is_present()) {
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
        rw_state = IO_STATE_RESPONSE_WRITE_DONE;
        rw_pos = UINT_MAX_VAL;
        rw_remains = 0;
        c3_end_data_block(get_fd());
        return IO_RESULT_OK; // done!

      case IO_STATE_ERROR:
        // so that to be able to pass separate check before every write() attempt
        return IO_RESULT_ERROR;

      default:
        c3_assert_failure();
        return set_error_state();
    }
  }
}

bool ResponseWriter::io_completed() const {
  return rw_state == IO_STATE_RESPONSE_WRITE_DONE;
}

response_type_t ResponseWriter::get_type() const {
  c3_assert(rw_state >= IO_STATE_RESPONSE_WRITE_READY && rw_state <= IO_STATE_RESPONSE_WRITE_DONE);
  return get_translated_response_type();
}

} // CyberCache
