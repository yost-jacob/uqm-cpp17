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
 
/****************************************************************************
* FILE: random.h
* DESC: definitions and externs for random number generators
*
* HISTORY: Created 6/ 6/1989
* LAST CHANGED:
*
* Copyright (c) 1989, Robert Leyland and Scott Anderson
****************************************************************************/

#ifndef LIBS_MATH_RANDOM_H_
#define LIBS_MATH_RANDOM_H_

#include "libs/compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { RandomDefaultSeed = 12345 };

/* Value-owned random stream, initialized with {RandomDefaultSeed}. Copying the
 * value makes an independent stream at the same position. No heap ownership.
 * Callers serialize access; mutate state only through the functions below. */
typedef struct RandomContext {
    DWORD seed;
} RandomContext;

/* Advance a required initialized context and return its new uint32 state.
 * Retains legacy unsigned arithmetic, not the std::minstd_rand0 sequence. */
DWORD RandomContext_Random(RandomContext *context);
/* Set a required context's state, returning the previous state for restoration.
 * Zero becomes 1; values above 2147483647 have that modulus subtracted ONCE.
 * Thus UINT32_MAX becomes 2147483648. Preserving this is part of the contract. */
DWORD RandomContext_SeedRandom(RandomContext *context, DWORD seed);
/* Read a required initialized context without advancing it. */
DWORD RandomContext_GetSeed(const RandomContext *context);

/* General gameplay uses std::minstd_rand0 in the currently bound mode, or a
 * startup/Super Melee fallback when no mode is active. Seed normalization is
 * modulo 2147483647, with zero becoming one; it differs from RandomContext.
 * SeedRandom returns the old state for restoration; GetRandomSeed only reads.
 * Callers serialize operations on a stream; the C binding is thread-local. */
DWORD TFB_SeedRandom(DWORD seed);
DWORD TFB_Random(void);
DWORD TFB_GetRandomSeed(void);

#ifdef __cplusplus
}
#endif
#endif /* LIBS_MATH_RANDOM_H_ */
