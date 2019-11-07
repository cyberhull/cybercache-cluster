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
 * Type to be used by various virtual methods handling payloads.
 */
#ifndef _IO_PAYLOAD_H
#define _IO_PAYLOAD_H

namespace CyberCache {

/**
 * Base class for the hierarchy of hash object.
 *
 * It's sole purpose is to be a placeholder: pointers to it are used in various virtual methods that
 * handle payloads: default implementations of those methods (that exist in the library) do nothing,
 * hence the class is empty, while their implementations found in the server do downcasting to lockable
 * hash objects on which they are supposed to operate.
 *
 * The overall purpose is to untie the library from the hash objects' hierarchy while maintaining some
 * type safety and code clearance by using `Payload*` instead of `void*`.
 */
class Payload {
};

} // CyberCache

#endif // _IO_PAYLOAD_H
