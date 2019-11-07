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
 * Implementation of the `SharedObjectBuffers` class.
 */
#ifndef _HT_SHARED_BUFFERS_H
#define _HT_SHARED_BUFFERS_H

#include "c3lib/c3lib.h"
#include "ht_objects.h"
#include "mt_spinlock.h"

namespace CyberCache {

/// Class adding payload hash object support to the shared buffers container.
class SharedObjectBuffers: public SharedBuffers {
  constexpr static c3_byte_t ZERO_LENGTH_BUFFER[] = "SOB_ZeroLengthBuffer";

  PayloadHashObject* sob_object; // pointer to object with data; NULL if `sb_payload` is not empty
  mutable SpinLock   sob_lock;   // mutex preventing modifications during buffer access

  explicit SharedObjectBuffers(Memory& memory);
  ~SharedObjectBuffers() override;

  c3_uint_t get_object_size() const override;

  static bool is_usable(const PayloadHashObject* pho) {
    return pho != nullptr && pho->is_locked() && pho->flags_are_clear(HOF_BEING_DELETED);
  }

  void clone_payload(SharedBuffers* cloned_sb) const override;

public:
  SharedObjectBuffers(const SharedObjectBuffers&) = delete;
  SharedObjectBuffers(SharedObjectBuffers&&) = delete;

  SharedObjectBuffers& operator=(const SharedObjectBuffers&) = delete;
  SharedObjectBuffers& operator=(SharedObjectBuffers&&) = delete;

  /**
   * Creates instance of the SharedBuffers class; ctor is private, so this method is the only way to
   * obtain an instance.
   *
   * @param memory Memory domain that should be used to allocate all buffers
   * @return Instance of the SharedObjectBuffers class
   */
  static SharedObjectBuffers* create_object(Memory& memory);

  c3_uint_t get_payload_size() const override;

  c3_uint_t get_payload_usize() const override;

  c3_compressor_t get_payload_compressor() const override;

  c3_byte_t* get_payload_bytes(c3_uint_t offset, c3_uint_t size) const override;

  c3_byte_t* set_payload_size(c3_uint_t size) override;

  void attach_payload(Payload* payload) override;

  void transfer_payload(Payload* payload, domain_t domain, c3_uint_t usize, c3_compressor_t compressor) override;
};

}

#endif // _HT_SHARED_BUFFERS_H
