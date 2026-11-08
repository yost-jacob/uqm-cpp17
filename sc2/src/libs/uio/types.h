/*
 * Copyright (C) 2003  Serge van den Boom
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * Nota bene: later versions of the GNU General Public License do not apply
 * to this program.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef LIBS_UIO_TYPES_H_
#define LIBS_UIO_TYPES_H_

#include "config.h"
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

/* UIO uses the same standard integer and boolean representations as the game.
 * uintptr_t is the unsigned integer type capable of holding a pointer. */
typedef bool uio_bool;
typedef uint8_t uio_uint8;
typedef int8_t uio_sint8;
typedef uint16_t uio_uint16;
typedef int16_t uio_sint16;
typedef uint32_t uio_uint32;
typedef int32_t uio_sint32;
typedef uintptr_t uio_uintptr;

#endif /* LIBS_UIO_TYPES_H_ */
