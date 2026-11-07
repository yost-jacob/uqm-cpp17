//Copyright Paul Reiche, Fred Ford. 1992-2002

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

#ifndef LIBS_COMPILER_H_
#define LIBS_COMPILER_H_

#include "types.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef uint8             BYTE;
typedef uint8             UBYTE;
typedef sint8             SBYTE;
typedef uint16            UWORD;
typedef sint16            SWORD;
typedef uint32            DWORD;
typedef sint32           SDWORD;

typedef UWORD             COUNT;
typedef SWORD              SIZE;

typedef char            UNICODE;


typedef enum
{
	FALSE = 0,
	TRUE
} BOOLEAN;

typedef void     (*PVOIDFUNC) (void);
typedef BOOLEAN  (*PBOOLFUNC) (void);
typedef BYTE     (*PBYTEFUNC) (void);
typedef UWORD    (*PUWORDFUNC) (void);
typedef SWORD    (*PSWORDFUNC) (void);
typedef DWORD    (*PDWORDFUNC) (void);

/* Each extraction evaluates its argument once. Conversions retain only the
 * indicated low byte/word; signed inputs are converted modulo the input width.
 * The temporary qualifier only bridges C99 inline and C++17 constexpr. */
#ifdef __cplusplus
#define UQM_VALUE_FUNCTION constexpr
#else
#define UQM_VALUE_FUNCTION inline
#endif

/* Return bits 0..3 of value; the input is narrowed to one byte first. */
static UQM_VALUE_FUNCTION BYTE LONIBBLE(const BYTE value) { return value & 0x0f; }
/* Return bits 4..7 of a byte. */
static UQM_VALUE_FUNCTION BYTE HINIBBLE(const BYTE value) { return value >> 4; }
/* Return bits 0..7 of a word. */
static UQM_VALUE_FUNCTION BYTE LOBYTE(const UWORD value) { return (BYTE) value; }
/* Return bits 8..15 of a word. */
static UQM_VALUE_FUNCTION BYTE HIBYTE(const UWORD value) { return (BYTE) (value >> 8); }
/* Return bits 0..15 of a double word. */
static UQM_VALUE_FUNCTION UWORD LOWORD(const DWORD value) { return (UWORD) value; }
/* Return bits 16..31 of a double word. */
static UQM_VALUE_FUNCTION UWORD HIWORD(const DWORD value) { return (UWORD) (value >> 16); }

#undef UQM_VALUE_FUNCTION

#ifdef __cplusplus
/* Combine byte-sized lo and hi at bit offsets 0 and 4, retaining eight bits.
 * Callers normally pass nibbles; higher bits of lo are intentionally retained. */
static constexpr BYTE MAKE_BYTE(const BYTE lo, const BYTE hi) {
    return (BYTE) ((hi << 4) | lo);
}
/* Combine the low and high bytes into a 16-bit word. */
static constexpr UWORD MAKE_WORD(const BYTE lo, const BYTE hi) {
    return (UWORD) ((hi << 8) | lo);
}
/* Combine the low and high words into a 32-bit double word. */
static constexpr DWORD MAKE_DWORD(const UWORD lo, const UWORD hi) {
    return ((DWORD) hi << 16) | lo;
}
#else
/* C99 requires expressions here: these three operations also appear in enum
 * values and static resource initializers. Each argument is evaluated once. */
#define MAKE_BYTE(lo, hi) ((BYTE) (((BYTE) (hi) << 4) | (BYTE) (lo)))
#define MAKE_WORD(lo, hi) ((UWORD) ((BYTE) (hi) << 8) | (BYTE) (lo))
#define MAKE_DWORD(lo, hi) (((DWORD) (hi) << 16) | (UWORD) (lo))
#endif

/* Preserve the maximum-alignment annotation used by shared ship structures.
 * Packing and explicit-alignment macros had no callers and were removed. */
#if defined(__GNUC__) || defined(__ARMCC__)
#define _ALIGNED_ANY __attribute__((aligned))
#else
#define _ALIGNED_ANY
#endif

#if defined(__cplusplus)
}
#endif

#endif /* LIBS_COMPILER_H_ */
