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
#include "pl_pipeline_commands.h"
#include <new>

namespace CyberCache {

PipelineCommand* PipelineCommand::create(c3_uintptr_t id, domain_t domain, const void* buff, size_t size) {
  c3_assert(C3_OFFSETOF(PipelineCommand, pc_null) == 0); // NULL offset must be zero
  c3_assert(id <= BYTE_MAX_VAL && size <= USHORT_MAX_VAL);
  auto pc = alloc<PipelineCommand>(domain, size + sizeof(PipelineCommand));
  new (pc) PipelineCommand((c3_byte_t) id, domain, (c3_ushort_t) size);
  std::memcpy((c3_byte_t*) pc + sizeof(PipelineCommand), buff, size);
  return pc;
}

}
