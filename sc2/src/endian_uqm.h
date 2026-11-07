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

#ifndef ENDIAN_UQM_H_
#define ENDIAN_UQM_H_

#include "types.h"
#include <SDL_endian.h>

/* SDL supplies the platform-aware implementation; UQM keeps typed C-compatible
 * functions so every input is evaluated once. Values are passed by value and
 * inputs are never modified. These operations apply to integer values, not
 * pointers to encoded bytes. */

/* Reverse the byte order of a 16-bit value. */
static inline uint16 UQM_Swap16(const uint16 value) {
    return SDL_Swap16(value);
}

/* Reverse the byte order of a 32-bit value. */
static inline uint32 UQM_Swap32(const uint32 value) {
    return SDL_Swap32(value);
}

/* Reverse the byte order of a 64-bit value. */
static inline uint64 UQM_Swap64(const uint64 value) {
    return SDL_Swap64(value);
}

/* Convert between native and little-endian 16-bit representation. */
static inline uint16 UQM_SwapLE16(const uint16 value) {
    return SDL_SwapLE16(value);
}

/* Convert between native and little-endian 32-bit representation. */
static inline uint32 UQM_SwapLE32(const uint32 value) {
    return SDL_SwapLE32(value);
}

/* Convert between native and little-endian 64-bit representation. */
static inline uint64 UQM_SwapLE64(const uint64 value) {
    return SDL_SwapLE64(value);
}

/* Convert between native and big-endian 16-bit representation. */
static inline uint16 UQM_SwapBE16(const uint16 value) {
    return SDL_SwapBE16(value);
}

/* Convert between native and big-endian 32-bit representation. */
static inline uint32 UQM_SwapBE32(const uint32 value) {
    return SDL_SwapBE32(value);
}

/* Convert between native and big-endian 64-bit representation. */
static inline uint64 UQM_SwapBE64(const uint64 value) {
    return SDL_SwapBE64(value);
}

#endif /* ENDIAN_UQM_H_ */
