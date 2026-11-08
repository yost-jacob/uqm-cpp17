///Copyright Paul Reiche, Fred Ford. 1992-2002

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

#include <cassert>
#include "random.h"

namespace {
constexpr DWORD Multiplier = 16807;
constexpr DWORD Modulus = 2147483647;
constexpr DWORD Quotient = Modulus / Multiplier;
constexpr DWORD Remainder = Modulus % Multiplier;
RandomContext globalContext{RandomDefaultSeed};
}

DWORD RandomContext_Random(RandomContext *context) {
    assert(context);
    // Preserve the original uint32 wrap and correction. std::minstd_rand0
    // uses the mathematical modulus instead: it differs by two whenever this
    // subtraction wraps, changing planet generation and netplay checksums.
    auto &seed = context->seed;
    seed = Multiplier * (seed % Quotient) - Remainder * (seed / Quotient);
    if (seed > Modulus)
        seed -= Modulus;
    else if (seed == 0)
        seed = 1;
    return seed;
}

DWORD RandomContext_SeedRandom(RandomContext *context, const DWORD seed) {
    assert(context);
    const auto old = context->seed;
    // Deliberately subtract once, not modulo: M and UINT32_MAX have observable
    // legacy states that a standard engine's seed normalization would discard.
    context->seed = seed == 0 ? 1 : seed > Modulus ? seed - Modulus : seed;
    return old;
}

DWORD RandomContext_GetSeed(const RandomContext *context) {
    assert(context);
    return context->seed;
}

DWORD TFB_Random(void) { return RandomContext_Random(&globalContext); }
DWORD TFB_SeedRandom(const DWORD seed) {
    return RandomContext_SeedRandom(&globalContext, seed);
}
