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
 * This file should be the very first included from within any extension module.
 */
#ifndef _EXTERNAL_APIS_H
#define _EXTERNAL_APIS_H

#include "php.h"

#ifdef ZTS // thread-safe compilation mode?
#include "TSRM.h"
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#include "c3lib/c3lib.h"

using namespace CyberCache;

#endif // _EXTERNAL_APIS_H
