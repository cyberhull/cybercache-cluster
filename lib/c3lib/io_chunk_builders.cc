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
#include "io_net_config.h"
#include "io_chunk_builders.h"
#include "io_response_handlers.h"
#include "c3_profiler_defs.h"

#include <cstdarg>
#include <cstdio>

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// ChunkBuilder
///////////////////////////////////////////////////////////////////////////////

c3_uint_t ChunkBuilder::put_entity(c3_byte_t* p, c3_uint_t n,
  c3_byte_t small_mask, c3_byte_t medium_mask, c3_byte_t large_mask) {
  if (n < CHNK_MEDIUM_BIAS) {
    *p = CHNK_SUBTYPE | small_mask | (c3_byte_t) n;
    return 1;
  } else if (n < CHNK_LARGE_BIAS) {
    *p = medium_mask | (c3_byte_t)(n - CHNK_MEDIUM_BIAS);
    return 1;
  } else {
    n -= CHNK_LARGE_BIAS;
    if ((n & 0xFFFFFF00) == 0) {
      p[0] = CHNK_SUBTYPE | large_mask;
      p[1] = (c3_byte_t) n;
      return 2;
    } else if ((n & 0xFFFF0000) == 0) {
      p[0] = CHNK_SUBTYPE | large_mask | (c3_byte_t) 1;
      #ifdef C3_UNALIGNED_ACCESS
      *(c3_ushort_t*)(p + 1) = (c3_ushort_t) n;
      #else
      p[1] = (c3_byte_t) n;
      p[2] = (c3_byte_t) (n >> 8);
      #endif // C3_UNALIGNED_ACCESS
      return 3;
    } else if ((n & 0xFF000000) == 0) {
      p[0] = CHNK_SUBTYPE | large_mask | (c3_byte_t) 2;
      #ifdef C3_UNALIGNED_ACCESS
      *(c3_ushort_t*)(p + 1) = (c3_ushort_t) n;
      #else
      p[1] = (c3_byte_t) n;
      p[2] = (c3_byte_t) (n >> 8);
      #endif // C3_UNALIGNED_ACCESS
      p[3] = (c3_byte_t) (n >> 16);
      return 4;
    } else {
      p[0] = CHNK_SUBTYPE | large_mask | (c3_byte_t) 3;
      #ifdef C3_UNALIGNED_ACCESS
      *(c3_uint_t*)(p + 1) = n;
      #else
      p[1] = (c3_byte_t) n;
      p[2] = (c3_byte_t) (n >> 8);
      p[3] = (c3_byte_t) (n >> 16);
      p[4] = (c3_byte_t) (n >> 24);
      #endif // C3_UNALIGNED_ACCESS
      return 5;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// ListChunkBuilder
///////////////////////////////////////////////////////////////////////////////

ListChunkBuilder::ListChunkBuilder(ReaderWriter& container, const NetworkConfiguration& net_config):
  ChunkBuilder(container, net_config) {
  lcb_buffer = nullptr;
  lcb_allocated_size = 0;
  lcb_used_size = 0;
  lcb_count = 0;
  // get domain ID while container is valid; the object can be disposed after its "container"
  lcb_domain = cb_container.get_domain();
}

ListChunkBuilder::~ListChunkBuilder() {
  if (lcb_buffer != nullptr) {
    c3_assert(lcb_allocated_size);
    get_memory_object().free(lcb_buffer, lcb_allocated_size);
  }
}

void ListChunkBuilder::put_string(c3_uint_t size, const char* str) {
  c3_uint_t n = size;
  while (n >= 255) {
    lcb_buffer[lcb_used_size++] = 255;
    n -= 255;
  }
  lcb_buffer[lcb_used_size++] = (c3_byte_t) n;
  if (size > 0) {
    std::memcpy(lcb_buffer + lcb_used_size, str, size);
    lcb_used_size += size;
  }
}

///////////////////////////////////////////////////////////////////////////////
// HeaderListChunkBuilder
///////////////////////////////////////////////////////////////////////////////

HeaderListChunkBuilder::HeaderListChunkBuilder(ReaderWriter& container, const NetworkConfiguration& net_config)
  : ListChunkBuilder(container, net_config) {
  flcb_estimated_size = 0;
}

c3_uint_t HeaderListChunkBuilder::estimate(c3_uint_t size) {
  c3_assert(lcb_allocated_size == 0);
  c3_uint_t n = measure_string(size);
  flcb_estimated_size += n;
  lcb_count++;
  return n;
}

c3_uint_t HeaderListChunkBuilder::estimate(const char* str) {
  assert(str);
  c3_assert(lcb_allocated_size == 0);
  c3_uint_t n = measure_cstring(str);
  flcb_estimated_size += n;
  lcb_count++;
  return n;
}

void HeaderListChunkBuilder::configure() {
  c3_assert(lcb_allocated_size == 0);
  // calculate number of bytes for the element count to be stored as list header
  flcb_estimated_size += measure_entity(lcb_count);
  lcb_buffer = (c3_byte_t*) get_memory_object().alloc(flcb_estimated_size);
  lcb_allocated_size = flcb_estimated_size;
  // store list chunk header
  lcb_used_size = put_entity(lcb_buffer, lcb_count, CHNK_SHORT_LIST, CHNK_LIST, CHNK_LONG_LIST);
  c3_assert(lcb_used_size <= lcb_allocated_size);
}

void HeaderListChunkBuilder::add(c3_uint_t size, const char* str) {
  assert(str);
  c3_assert(lcb_buffer != nullptr && lcb_used_size + measure_string(size) <= lcb_allocated_size);
  put_string(size, str);
}

void HeaderListChunkBuilder::add(const char* str) {
  assert(str);
  add((c3_uint_t) std::strlen(str), str);
}

///////////////////////////////////////////////////////////////////////////////
// PayloadListChunkBuilder
///////////////////////////////////////////////////////////////////////////////

PayloadListChunkBuilder::PayloadListChunkBuilder(ReaderWriter& container,
  const NetworkConfiguration& net_config,
  c3_uint_t min_guess, c3_uint_t max_guess, c3_uint_t average_length):
  ListChunkBuilder(container, net_config) {
  /*
   * TODO: ===============================================
   * TODO: adjust guessing algorithm using real-world data
   * TODO: ===============================================
   */
  c3_assert(max_guess == 0 || min_guess <= max_guess);

  // process default values
  if (min_guess == 0) {
    min_guess = 1;
  }
  if (max_guess == 0) {
    max_guess = min_guess == 1? 64: min_guess;
  }
  if (average_length == 0) {
    average_length = 16;
  }
  average_length++; // take into account length byte(s)

  // figure out probable number of strings
  c3_uint_t num = max_guess;
  if (num > min_guess) {
    num = min_guess + (max_guess - min_guess) * 3 / 4;
  }

  // allocate string buffer
  c3_ulong_t size64 = (c3_ulong_t) num * average_length;
  if (size64 > UINT_MAX_VAL) {
    size64 = UINT_MAX_VAL;
  }
  c3_uint_t size = (c3_uint_t) size64;
  lcb_buffer = (c3_byte_t*) get_memory_object().alloc(size);
  lcb_allocated_size = size;
}

bool PayloadListChunkBuilder::add(c3_uint_t size, const char* str) {
  assert(str);
  c3_assert(lcb_buffer);
  c3_uint_t full_length = measure_string(size);
  c3_ulong_t minimum_new_size = (c3_ulong_t) lcb_used_size + full_length;
  if (minimum_new_size > lcb_allocated_size) {
    /*
     * TODO: ==============================================================
     * TODO: adjust guessing algorithm using data from performance counters
     * TODO: ==============================================================
     */
    c3_uint_t average_length = (c3_uint_t)(minimum_new_size / (lcb_count + 1));
    c3_uint_t extra_strings;
    if (lcb_count < 4) {
      extra_strings = 2;
      PERF_INCREMENT_VAR_DOMAIN_COUNTER(lcb_domain, List_Short_Reallocs)
    } else if (lcb_count < 32) {
      extra_strings = 8;
      PERF_INCREMENT_VAR_DOMAIN_COUNTER(lcb_domain, List_Medium_Reallocs)
    } else {
      extra_strings = lcb_count / 4;
      PERF_INCREMENT_VAR_DOMAIN_COUNTER(lcb_domain, List_Long_Reallocs)
    }
    c3_uint_t extra_space = extra_strings * average_length;
    if (extra_space < full_length) { // in case a huge string after lots if tiny ones ruined our stats...
      extra_space = full_length;
      PERF_INCREMENT_VAR_DOMAIN_COUNTER(lcb_domain, List_Miscalculations)
    }
    if ((c3_ulong_t) lcb_allocated_size + extra_space > UINT_MAX_VAL) {
      extra_space = UINT_MAX_VAL - lcb_allocated_size;
      if (extra_space + (lcb_allocated_size - lcb_used_size) < full_length) {
        PERF_INCREMENT_VAR_DOMAIN_COUNTER(lcb_domain, List_Add_Failures)
        // the string won't fit...
        c3_assert_failure();
        return false;
      }
    }
    PERF_INCREMENT_VAR_DOMAIN_COUNTER(lcb_domain, List_Completed_Reallocs)
    c3_uint_t new_size = lcb_allocated_size + extra_space;
    lcb_buffer = (c3_byte_t*) get_memory_object().realloc(lcb_buffer, new_size, lcb_allocated_size);
    lcb_allocated_size = new_size;
  }
  PERF_INCREMENT_VAR_DOMAIN_COUNTER(lcb_domain, List_Added_Strings)
  put_string(size, str);
  lcb_count++;
  return true;
}

bool PayloadListChunkBuilder::add(const char* str) {
  assert(str);
  return add((c3_uint_t) std::strlen(str), str);
}

bool PayloadListChunkBuilder::addf(const char* format, ...) {
  char buffer[MAX_FORMATED_STRING_LENGTH];
  va_list args;
  va_start(args, format);
  int length = std::vsnprintf(buffer, sizeof buffer, format, args);
  va_end(args);
  c3_assert(length > 0);
  return add((c3_uint_t) length, buffer);
}

///////////////////////////////////////////////////////////////////////////////
// PayloadAllocator
///////////////////////////////////////////////////////////////////////////////

c3_byte_t* PayloadAllocator::alloc(c3_uint_t size) {
  c3_assert(size && pa_container.get_payload_size() == 0);
  return pa_container.set_payload_size(size);
}

void PayloadAllocator::free(void* buff, c3_uint_t size) {
  // not supposed to be called
  c3_assert_failure();
}

///////////////////////////////////////////////////////////////////////////////
// PayloadChunkBuilder
///////////////////////////////////////////////////////////////////////////////

PayloadChunkBuilder::PayloadChunkBuilder(ReaderWriter& container, const NetworkConfiguration& net_config):
  ChunkBuilder(container, net_config) {
  pcb_comp_threshold = net_config.get_compression_threshold();
  pcb_compressor = cb_net_config.get_compressor(container.get_domain());
  pcb_usize = 0;
}

void PayloadChunkBuilder::add(const c3_byte_t* buffer, c3_uint_t usize, comp_data_t hint) {
  assert(buffer);
  c3_assert(cb_container.get_payload_size() == 0);
  if (usize > 0) {
    c3_byte_t* result;
    // optionally try to compress the payload
    if (usize >= pcb_comp_threshold) {
      PayloadAllocator allocator(cb_container);
      c3_uint_t size = usize;
      result = global_compressor.pack(pcb_compressor, buffer, usize, size, allocator, CL_FASTEST, hint);
      if (result != nullptr) {
        pcb_usize = usize;
        c3_assert(size && size < usize && size == cb_container.get_payload_size());
        return;
      }
    }
    // payload could not be compressed, so just store it as is
    result = cb_container.set_payload_size(usize);
    c3_assert(result);
    pcb_usize = usize;
    pcb_compressor = CT_NONE;
    std::memcpy(result, buffer, usize);
  } else {
    pcb_usize = 0;
    pcb_compressor = CT_NONE;
  }
}

void PayloadChunkBuilder::add(const PayloadListChunkBuilder &list) {
  add(list.get_buffer(), list.get_size(), CD_TEXT);
}

void PayloadChunkBuilder::add(Payload* payload) {
  assert(payload);
  c3_assert(cb_container.get_payload_size() == 0);
  cb_container.response_writer_attach_payload(payload);
  pcb_usize = cb_container.get_payload_usize();
  pcb_compressor = cb_container.get_payload_compressor();
}

void PayloadChunkBuilder::add() {
  /*
   * Payload object must be attached at this point. We cannot test it by checking
   * payload size as it might as well be zero, but get_payload_compressor() will generate
   * assertion failure if the object has not been attached yet (the shared buffer has
   * to be `SharedObjectBuffers` though).
   */
  pcb_usize = cb_container.get_payload_usize();
  pcb_compressor = cb_container.get_payload_compressor();
}

///////////////////////////////////////////////////////////////////////////////
// HeaderChunkBuilder
///////////////////////////////////////////////////////////////////////////////

bool HeaderChunkBuilder::verify() const {
  return true;
}

c3_uint_t HeaderChunkBuilder::estimate_number(c3_long_t num) {
  c3_assert(cb_container.header_is_not_initialized());
  if (num >= 0 && num <= UINT_MAX_VAL) {
    c3_uint_t size = measure_entity((c3_uint_t) num);
    hcb_estimated_size += size;
    return size;
  }
  c3_assert(num >= INT_MIN_VAL && num < 0);
  if (num > CHNK_BIG_NEGATIVE_BIAS) {
    hcb_estimated_size++;
    return 1;
  }
  c3_uint_t nn = (c3_uint_t)(-num + CHNK_BIG_NEGATIVE_BIAS);
  if ((nn & 0xFFFFFF00) == 0) {
    hcb_estimated_size += 2;
    return 2;
  }
  if ((nn & 0xFFFF0000) == 0) {
    hcb_estimated_size += 3;
    return 3;
  }
  if ((nn & 0xFF000000) == 0) {
    hcb_estimated_size += 4;
    return 4;
  }
  hcb_estimated_size += 5;
  return 5;
}

c3_uint_t HeaderChunkBuilder::estimate_string(c3_uint_t size) {
  c3_assert(cb_container.header_is_not_initialized());
  c3_uint_t n = measure_entity(size) + size;
  hcb_estimated_size += n;
  return n;
}

c3_uint_t HeaderChunkBuilder::estimate_cstring(const char* str) {
  assert(str != nullptr);
  return estimate_string((c3_uint_t) std::strlen(str));
}

c3_uint_t HeaderChunkBuilder::estimate_list(const HeaderListChunkBuilder& list) {
  c3_assert(cb_container.header_is_not_initialized());
  c3_uint_t size = list.get_size();
  hcb_estimated_size += size;
  return size;
}

void HeaderChunkBuilder::configure(const PayloadChunkBuilder* payload) {
  c3_assert(cb_container.header_is_not_initialized() && hcb_used_size == 0);

  // retrieve password hash
  c3_hash_t hash;
  bool auth = get_password_hash(hash);

  // initialize descriptor
  c3_byte_t desc = get_descriptor_initializer();

  // fetch command ID (might be `CMD_INVALID` in case of responses)
  command_t cmd = get_command();

  // assume we do not have payload
  c3_compressor_t payload_compressor = CT_NONE;
  c3_uint_t payload_size = 0;
  c3_uint_t payload_usize = 0;
  c3_uint_t payload_size_bytes = 0;

  // see if we have to store header size
  c3_uint_t header_size = 0;
  if (hcb_estimated_size > 0 || payload != nullptr) {

    // a) header data chunks (estimated so far) + optional command ID
    header_size = hcb_estimated_size;
    if (cmd != CMD_INVALID) {
      header_size++;
    }

    // b) optional password hash
    if (auth) {
      header_size += sizeof hash;
    }

    // c) payload
    if (payload != nullptr) {
      payload_compressor = payload->get_compressor();
      payload_size = cb_container.get_payload_size();
      payload_usize = payload->get_usize();

      payload_size_bytes = estimate_size_bytes(payload_usize);
      if (payload_compressor != CT_NONE) {
        assert(payload_size < payload_usize);
        desc |= DESC_PAYLOAD_IS_COMPRESSED;
        header_size += payload_size_bytes * 2 + 1; // compressor + compressed + uncompressed
      } else {
        assert(payload_size == payload_usize);
        header_size += payload_size_bytes;
      }
    }
  }

  // calculate *full* size of the header
  c3_uint_t header_size_bytes = 0;
  c3_uint_t full_header_size;
  if (header_size > 0) {
    // calculate number of bytes necessary to store header size
    header_size_bytes = estimate_size_bytes(header_size);
    // descriptor + header size bytes + header data
    full_header_size = 1 + header_size_bytes + header_size;
  } else {
    // a "sizeless" header (contains only descriptor, optional command, optional password hash)
    full_header_size = 1; // descriptor...
    if (cmd != CMD_INVALID) {
      full_header_size++; // ... + optional command
    }
    if (auth) {
      full_header_size += sizeof hash;
    }
  }

  // allocate header
  cb_container.initialize_header(full_header_size);
  c3_byte_t& descriptor_ref = *cb_container.get_header_bytes(0, full_header_size);

  // a) store descriptor
  descriptor_ref = desc;
  hcb_used_size = 1;

  // b) optionally store header size
  if (header_size > 0) {
    put_size_bytes(header_size, header_size, descriptor_ref,
      DESC_BYTE_HEADER, DESC_WORD_HEADER, DESC_DWORD_HEADER);
    // the above call increments `hcb_used_size`
  }

  // c) optionally store command ID
  if (cmd != CMD_INVALID) {
    cb_container.set_header_byte_at(hcb_used_size, cmd);
    hcb_used_size++;
  }

  // d) optionally store password hash code
  if (auth) {
    c3_byte_t* p = cb_container.get_header_bytes(hcb_used_size, sizeof hash);
    c3_assert(p);
    std::memcpy(p, &hash, sizeof hash);
    hcb_used_size += sizeof hash;
  }

  // e) optionally store payload size(es)
  if (payload != nullptr) {
    // e1) optional compressor
    if (payload_compressor != CT_NONE) {
      cb_container.set_header_byte_at(hcb_used_size, payload_compressor);
      hcb_used_size++;
    }

    // e2) mandatory size
    put_size_bytes(payload_size, payload_usize, descriptor_ref,
      DESC_BYTE_PAYLOAD, DESC_WORD_PAYLOAD, DESC_DWORD_PAYLOAD);

    // e3) optional uncompressed size
    if (payload_compressor != CT_NONE) {
      put_size_bytes(payload_usize, payload_usize);
    }
  }
  // f) update target for check()
  hcb_estimated_size = full_header_size;
}

void HeaderChunkBuilder::add_number(c3_long_t num) {
  if (num >= 0 && num <= UINT_MAX_VAL) {
    put((c3_uint_t) num, CHNK_SMALL_INTEGER, CHNK_INTEGER, CHNK_BIG_INTEGER);
  } else {
    assert(num >= INT_MIN_VAL && num < 0);
    if (num > CHNK_BIG_NEGATIVE_BIAS) {
      c3_byte_t small = (c3_byte_t)(-num + CHNK_SMALL_NEGATIVE_BIAS);
      cb_container.set_header_byte_at(hcb_used_size, CHNK_SUBTYPE | CHNK_SMALL_NEGATIVE | small);
      hcb_used_size++;
    } else {
      c3_uint_t big = (c3_uint_t)(-num + CHNK_BIG_NEGATIVE_BIAS);
      if ((big & 0xFFFFFF00) == 0) {
        cb_container.set_header_byte_at(hcb_used_size, CHNK_SUBTYPE | CHNK_BIG_NEGATIVE);
        cb_container.set_header_byte_at(hcb_used_size + 1, (c3_byte_t) big);
        hcb_used_size += 2;
      } else if ((big & 0xFFFF0000) == 0) {
        cb_container.set_header_byte_at(hcb_used_size, CHNK_SUBTYPE | CHNK_BIG_NEGATIVE | 1);
        cb_container.set_header_ushort_at(hcb_used_size + 1, (c3_ushort_t) big);
        hcb_used_size += 3;
      } else if ((big & 0xFF000000) == 0) {
        cb_container.set_header_byte_at(hcb_used_size, CHNK_SUBTYPE | CHNK_BIG_NEGATIVE | 2);
        cb_container.set_header_uint3_at(hcb_used_size + 1, big);
        hcb_used_size += 4;
      } else {
        cb_container.set_header_byte_at(hcb_used_size, CHNK_SUBTYPE | CHNK_BIG_NEGATIVE | 3);
        cb_container.set_header_uint_at(hcb_used_size + 1, big);
        hcb_used_size += 5;
      }
    }
  }
}

void HeaderChunkBuilder::add_string(const char* str, c3_uint_t size) {
  assert(str != nullptr);
  c3_assert_def(c3_uint_t n) put(size, CHNK_SHORT_STRING, CHNK_STRING, CHNK_LONG_STRING);
  c3_assert(n);
  if (size > 0) {
    c3_byte_t* buffer = cb_container.get_header_bytes(hcb_used_size, size);
    std::memcpy(buffer, str, size);
    hcb_used_size += size;
  }
}

void HeaderChunkBuilder::add_cstring(const char* str) {
  assert(str);
  add_string(str, (c3_uint_t) std::strlen(str));
}

void HeaderChunkBuilder::add_list(const HeaderListChunkBuilder &list) {
  const c3_byte_t* list_buffer = list.get_buffer();
  c3_uint_t list_size = list.get_size();
  c3_assert(list_buffer && list_size);
  c3_byte_t* buffer = cb_container.get_header_bytes(hcb_used_size, list_size);
  std::memcpy(buffer, list_buffer, list_size);
  hcb_used_size += list_size;
}

///////////////////////////////////////////////////////////////////////////////
// CommandHeaderChunkBuilder
///////////////////////////////////////////////////////////////////////////////

c3_byte_t CommandHeaderChunkBuilder::get_descriptor_initializer() const {
  c3_byte_t desc = chcb_auth? (chcb_admin? DESC_ADMIN_AUTH: DESC_USER_AUTH): DESC_NO_AUTH;
  bool network = cb_container.is_set(IO_FLAG_NETWORK);
  if ((network && cb_net_config.get_command_integrity_check()) ||
    (!network && cb_net_config.get_file_integrity_check())) {
    desc |= DESC_MARKER_IS_PRESENT;
  }
  return desc;
}

command_t CommandHeaderChunkBuilder::get_command() const {
  return chcb_cmd;
}

bool CommandHeaderChunkBuilder::get_password_hash(c3_hash_t& hash) const {
  hash = chcb_hash;
  return chcb_auth;
}

bool CommandHeaderChunkBuilder::verify() const {
  c3_assert(hcb_used_size >= 2); // must have at least a command and a descriptor
  cb_container.command_writer_set_ready_state();
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// CommandHeaderChunkBuilder
///////////////////////////////////////////////////////////////////////////////

CommandHeaderChunkBuilder::CommandHeaderChunkBuilder(ReaderWriter& container,
  const NetworkConfiguration& net_config, command_t cmd, bool admin):
  HeaderChunkBuilder(container, net_config), chcb_cmd(cmd), chcb_admin(admin) {
  c3_assert(cb_container.is_clear(IO_FLAG_IS_RESPONSE));
  chcb_hash = admin? cb_net_config.get_admin_password(): cb_net_config.get_user_password();
  chcb_auth = chcb_hash != INVALID_HASH_VALUE;
}

///////////////////////////////////////////////////////////////////////////////
// ResponseHeaderChunkBuilder
///////////////////////////////////////////////////////////////////////////////

ResponseHeaderChunkBuilder::ResponseHeaderChunkBuilder(ResponseWriter& container,
  const NetworkConfiguration& net_config, c3_byte_t type):
  HeaderChunkBuilder(container, net_config), rhcb_type(type) {
  c3_assert(cb_container.is_set(IO_FLAG_NETWORK));
  c3_assert(cb_container.is_set(IO_FLAG_IS_RESPONSE));
}

c3_byte_t ResponseHeaderChunkBuilder::get_descriptor_initializer() const {
  return cb_net_config.get_response_integrity_check()?
    rhcb_type | RESP_MARKER_IS_PRESENT: rhcb_type;
}

command_t ResponseHeaderChunkBuilder::get_command() const {
  return CMD_INVALID;
}

bool ResponseHeaderChunkBuilder::get_password_hash(c3_hash_t& hash) const {
  hash = 0;
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// OkResponseHeaderChunkBuilder
///////////////////////////////////////////////////////////////////////////////

bool OkResponseHeaderChunkBuilder::verify() const {
  // descriptor OR descriptor + marker
  c3_assert((hcb_used_size == 1 || hcb_used_size == 2) && cb_container.get_payload_size() == 0);
  cb_container.response_writer_set_ready_state();
  return true;
}

OkResponseHeaderChunkBuilder::OkResponseHeaderChunkBuilder(ResponseWriter& container,
  const NetworkConfiguration& net_config):
  ResponseHeaderChunkBuilder(container, net_config, RESP_TYPE_OK) {}

///////////////////////////////////////////////////////////////////////////////
// ErrorResponseHeaderChunkBuilder
///////////////////////////////////////////////////////////////////////////////

bool ErrorResponseHeaderChunkBuilder::verify() const {
  // descriptor + header byte + number + string
  c3_assert(hcb_used_size > 4 && cb_container.get_payload_size() == 0);
  cb_container.response_writer_set_ready_state();
  return true;
}

ErrorResponseHeaderChunkBuilder::ErrorResponseHeaderChunkBuilder(ResponseWriter& container,
  const NetworkConfiguration& net_config):
  ResponseHeaderChunkBuilder(container, net_config, RESP_TYPE_ERROR) {}

///////////////////////////////////////////////////////////////////////////////
// DataResponseHeaderChunkBuilder
///////////////////////////////////////////////////////////////////////////////

bool DataResponseHeaderChunkBuilder::verify() const {
  // descriptor + header byte + (chunk or payload size)
  c3_assert(hcb_used_size >= 3);
  cb_container.response_writer_set_ready_state();
  return true;
}

DataResponseHeaderChunkBuilder::DataResponseHeaderChunkBuilder(ResponseWriter& container,
  const NetworkConfiguration& net_config):
  ResponseHeaderChunkBuilder(container, net_config, RESP_TYPE_DATA) {}

///////////////////////////////////////////////////////////////////////////////
// ListResponseHeaderChunkBuilder
///////////////////////////////////////////////////////////////////////////////

bool ListResponseHeaderChunkBuilder::verify() const {
  // descriptor + header byte + list count (may not have payload)
  c3_assert(hcb_used_size >= 3);
  cb_container.response_writer_set_ready_state();
  return true;
}

ListResponseHeaderChunkBuilder::ListResponseHeaderChunkBuilder(ResponseWriter& container,
  const NetworkConfiguration& net_config):
  ResponseHeaderChunkBuilder(container, net_config, RESP_TYPE_LIST) {}

} // CyberCache
