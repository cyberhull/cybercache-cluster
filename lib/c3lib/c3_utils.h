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
 * Utility functions.
 */
#ifndef _C3_UTILS_H
#define _C3_UTILS_H

#include "c3_types.h"

namespace CyberCache {

/**
 * Returns power of 2 that is greater than or equal to the provided number (e.g. 50 yields 64)
 *
 * @param num Positive integer number
 * @return Number that is greater or equal to the argument, and that is a power of 2
 */
c3_uint_t get_next_power_of_2(c3_uint_t num);

/**
 * Checks if a number is a power of 2; does *not* handle `0` correctly, so it should only be used for
 * positive numbers.
 *
 * @param num Positive integer
 * @return `true` if the number is a power of 2
 */
constexpr bool is_power_of_2(c3_uint_t num) { return (num & (num - 1)) == 0; }

/**
 * Returns signular/plural suffix for nouns that do not represent special cases (e.g. do not end with
 * "y", do not represent exceptions etc.).
 *
 * @param num Number of items (e.g. errors)
 * @return Empty string if number of items is `1`, "s" otherwise (this includes `0` items)
 */
constexpr const char* plural(c3_uint_t num) { return num == 1? "": "s"; }

template <class T> T min(T n1, T n2) { return n1 < n2? n1: n2; }
template <class T> T max(T n1, T n2) { return n1 > n2? n1: n2; }

}

#endif // _C3_UTILS_H
