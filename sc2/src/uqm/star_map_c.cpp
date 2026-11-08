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

#include <algorithm>
#include <cassert>
#include <exception>
#include <memory>
#include <sstream>
#include <stdexcept>
#include "star_map.hpp"
#include "starmap.h"
#include "options.h"
#include "libs/log.h"

namespace {
// C++ consumers never see this mutable descriptor buffer or its sentinels.
// The game only reads catalogue descriptors; all names live in the native map.
struct LegacyStarMap {
    StarMap map;
    std::vector<STAR_DESC> descriptors;
    explicit LegacyStarMap(std::istream &input) : map(input), descriptors(STAR_CATALOGUE_SIZE) {
        for (const auto &star : map.stars()) {
            const auto &p = star.properties();
            auto &out = descriptors.at(star.id().value);
            out.star_pt = {static_cast<COORD>(star.position().x), static_cast<COORD>(star.position().y)};
            out.Type = MAKE_STAR(static_cast<int>(p.type), static_cast<int>(p.color), p.owner);
            out.Index = p.specialSystem;
            out.Prefix = p.prefixOrder;
            out.Postfix = p.postfixId;
        }
        for (const auto index : {int(NUM_SOLAR_SYSTEMS), STAR_CATALOGUE_SIZE - 1})
            descriptors[index].star_pt = {MAX_X_UNIVERSE * 2, MAX_Y_UNIVERSE * 2};
        for (size_t i = 0; i < descriptors.size(); ++i)
            descriptors[i].catalogue_index = i;
    }
};
static_assert(int(StarType::Dwarf) == DWARF_STAR && int(StarType::Giant) == GIANT_STAR
        && int(StarType::SuperGiant) == SUPER_GIANT_STAR);
static_assert(int(StarColor::Blue) == BLUE_BODY && int(StarColor::Green) == GREEN_BODY
        && int(StarColor::Orange) == ORANGE_BODY && int(StarColor::Red) == RED_BODY
        && int(StarColor::White) == WHITE_BODY && int(StarColor::Yellow) == YELLOW_BODY);
const std::string EmptyName;
std::unique_ptr<LegacyStarMap> catalogue;
LegacyStarMap &legacy() { assert(catalogue); return *catalogue; }
// Sentinels exist only in the C buffer and have no native star or display text.
const StarDesc *nativeStar(const STAR_DESC *descriptor) {
    const auto index = GetStarIndex(descriptor);
    if (index == NUM_SOLAR_SYSTEMS || index == STAR_CATALOGUE_SIZE - 1)
        return nullptr;
    return &legacy().map.at(StarId{index});
}
}

extern "C" {
STAR_DESC *CurStarDescPtr = nullptr;
BOOLEAN LoadStarMap(void) {
    assert(!catalogue);
    try {
        using Stream = std::unique_ptr<uio_Stream, decltype(&res_CloseResFile)>;
        const Stream file(res_OpenResFile(contentDir, "base/starmap.json", "rb"), res_CloseResFile);
        if (!file)
            throw std::runtime_error("cannot open catalogue");
        std::string bytes(LengthResFile(file.get()), '\0');
        if (ReadResFile(bytes.data(), 1, bytes.size(), file.get()) != bytes.size())
            throw std::runtime_error("could not read complete catalogue");
        std::istringstream input(bytes);
        catalogue = std::make_unique<LegacyStarMap>(input);
        CurStarDescPtr = nullptr;
        return TRUE;
    } catch (const std::exception &error) {
        log_add(log_Error, "base/starmap.json: %s", error.what());
        return FALSE;
    } catch (...) { std::terminate(); }
}
void FreeStarMap(void) { CurStarDescPtr = nullptr; catalogue.reset(); }
STAR_DESC *GetStar(const COUNT index) {
    auto &buffer = legacy().descriptors;
    return index < buffer.size() ? &buffer[index] : nullptr;
}
COUNT GetStarIndex(const STAR_DESC *star) {
    assert(star);
    const auto index = star->catalogue_index;
    assert(index < legacy().descriptors.size() && &legacy().descriptors[index] == star);
    return index;
}
const char *GetStarPrefix(const STAR_DESC *star) {
    const auto native = nativeStar(star);
    return native ? native->prefix().c_str() : "";
}
const char *GetStarPostfix(const STAR_DESC *star) {
    const auto native = nativeStar(star);
    return native ? native->postfix().c_str() : "";
}
const char *GetQuasiSpaceName(void) { return legacy().map.quasispaceName().c_str(); }
STAR_DESC *FindStarInSpace(const STAR_DESC *last, const POINT *position,
        const SIZE xbounds, const SIZE ybounds, const bool quasispace) try {
    assert(position);
    const auto first = quasispace ? QUASISPACE_FIRST : 0;
    const auto end = first + (quasispace ? NUM_QUASISPACE_LOCATIONS : NUM_SOLAR_SYSTEMS);
    const auto after = last ? GetStarIndex(last) : -1;
    if (last && (after < first || after >= end))
        return nullptr;
    const StarSearch search{quasispace ? StarSpace::QuasiSpace : StarSpace::HyperSpace,
            {position->x, position->y}, xbounds < 0 ? std::nullopt : std::optional<int>{xbounds},
            ybounds < 0 ? std::nullopt : std::optional<int>{ybounds}};
    const auto matches = legacy().map.find(search);
    const auto next = std::find_if(matches.begin(), matches.end(),
            [after](StarId id) { return int(id.value) > after; });
    return next == matches.end() ? nullptr : GetStar(next->value);
} catch (...) { std::terminate(); }
void GetClusterName(const STAR_DESC *star, UNICODE *buf, const size_t capacity) {
    assert(star && (buf || capacity == 0));
    if (capacity) {
        const auto native = nativeStar(star);
        // String ownership/references remain C++; only this boundary copies bytes.
        const std::string &name = native ? native->name() : EmptyName;
        utf8StringCopy(buf, capacity, name.c_str());
    }
}
}
