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
 * Regular expression engine tailored for CyberCache extension needs.
 */
#ifndef _REGEX_MATCHER_H
#define _REGEX_MATCHER_H

#include "c3lib/c3lib.h"

#define C3_DEFAULT_BOT_REGEX \
  "^alexa|^blitz\\.io|bot|^browsermob|crawl|^facebookexternalhit|feed|google web preview|^ia_archiver|indexer|^java|jakarta|^libwww-perl|^load impact|^magespeedtest|monitor|^Mozilla$|nagios |^\\.net|^pinterest|postrank|slurp|spider|uptime|^wget|yandex"

void regex_init() C3_FUNC_COLD;
bool regex_compile(const char* pattern) C3_FUNC_COLD;
bool regex_match(const char* text);
void regex_cleanup() C3_FUNC_COLD;

#endif // _REGEX_MATCHER_H
