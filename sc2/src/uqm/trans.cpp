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

#include <cmath>
#include "units.h"

namespace {
// C++17 has no std::numbers; this is 2*pi / the game's 64 angle steps.
constexpr double RadiansPerAngle = 6.2831853071795864769 / FULL_CIRCLE;

// Return the screen Y unit component for a normalized angle (0..63). Exact
// cardinals prevent floating-point residue from moving objects off an axis.
double sineUnit(const COUNT angle) {
    switch (angle) {
        case 0: return -1.0;
        case QUADRANT: case 3 * QUADRANT: return 0.0;
        case HALF_CIRCLE: return 1.0;
        default: return -std::cos(angle * RadiansPerAngle);
    }
}
}

SIZE SINE(const SDWORD angle, const SDWORD magnitude) {
    return static_cast<SIZE>(std::lround(sineUnit(NORMALIZE_ANGLE(angle)) * magnitude));
}

SIZE COSINE(const SDWORD angle, const SDWORD magnitude) {
    const auto shifted = NORMALIZE_ANGLE(NORMALIZE_ANGLE(angle) + QUADRANT);
    return static_cast<SIZE>(std::lround(sineUnit(shifted) * magnitude));
}

COUNT ARCTAN(const SIZE delta_x, const SIZE delta_y) {
    if (delta_x == 0 && delta_y == 0)
        return FULL_CIRCLE;
    const auto radians = std::atan2(static_cast<double>(delta_x),
            -static_cast<double>(delta_y));
    return NORMALIZE_ANGLE(static_cast<SDWORD>(std::lround(radians / RadiansPerAngle)));
}
