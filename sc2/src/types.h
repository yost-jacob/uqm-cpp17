/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef TYPES_H_
#define TYPES_H_

#include "config.h"
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

/* Fixed-width standard types preserve the serialized integer widths. Keep the
 * existing names while C and C++ callers migrate; no emulated integer types or
 * architecture-specific guesses are needed. */
typedef uint8_t uint8;
typedef int8_t sint8;
typedef uint16_t uint16;
typedef int16_t sint16;
typedef uint32_t uint32;
typedef int32_t sint32;
typedef uint64_t uint64;
typedef int64_t sint64;

#if CHAR_BIT != 8
#error "UQM resource and save formats require 8-bit bytes"
#endif

#ifdef __cplusplus
static_assert(sizeof(uint8) == 1 && sizeof(uint16) == 2 && sizeof(uint32) == 4
        && sizeof(uint64) == 8, "UQM integer widths must match its file formats");
#endif

#endif /* TYPES_H_ */
