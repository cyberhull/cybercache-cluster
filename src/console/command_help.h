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
 * Functions collecting (through log interface etc.) console command descriptions.
 */
#ifndef _COMMAND_HELP_H
#define _COMMAND_HELP_H

#include "c3lib/c3lib.h"

namespace CyberCache {

void get_help(Parser& parser);
bool get_help(Parser& parser, const char* command);

} // CyberCache

#endif // _COMMAND_HELP_H
