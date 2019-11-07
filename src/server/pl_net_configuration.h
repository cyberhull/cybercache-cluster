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
 * Network configuration object to be used by server pipelines.
 */
#ifndef _PL_NET_CONFIGURATION_H
#define _PL_NET_CONFIGURATION_H

#include "c3lib/c3lib.h"

namespace CyberCache {

/// Object to be used for setting and retrieval of I/O options throughout the server.
extern NetworkConfiguration server_net_config;

} // CyberCache

#endif // _PL_NET_CONFIGURATION_H
