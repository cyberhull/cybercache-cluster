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
#include "io_data_buffer.h"

namespace CyberCache {

void DataBuffer::empty(Memory& memory) {
  validate();
  if (db_buffer != nullptr) {
    memory.free(db_buffer, db_size);
    db_buffer = nullptr;
  }
  db_size = 0;
}

void DataBuffer::reset_buffer_transferred_to_another_object() {
  validate();
  db_buffer = nullptr;
  db_size = 0;
}

c3_byte_t* DataBuffer::set_size(Memory& memory, c3_uint_t size) {
  assert(size);
  validate();
  if (db_buffer != nullptr) {
    if (db_size != size) { // realloc() requires that new and old sizes differ
      db_buffer = (c3_byte_t*) memory.realloc(db_buffer, size, db_size);
      db_size = size;
    }
  } else {
    db_buffer = (c3_byte_t*) memory.alloc(size);
    db_size = size;
  }
  validate();
  return db_buffer;
}

}
