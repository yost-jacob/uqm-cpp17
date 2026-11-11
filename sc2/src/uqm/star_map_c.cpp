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
#include "game_definitions_legacy.hpp"
#include "game_state.hpp"
#include "starmap.h"
#include "options.h"
#include "libs/log.h"

namespace {
static_assert(int(StarType::Dwarf) == DWARF_STAR && int(StarType::Giant) == GIANT_STAR
        && int(StarType::SuperGiant) == SUPER_GIANT_STAR);
static_assert(int(StarColor::Blue) == BLUE_BODY && int(StarColor::Green) == GREEN_BODY
        && int(StarColor::Orange) == ORANGE_BODY && int(StarColor::Red) == RED_BODY
        && int(StarColor::White) == WHITE_BODY && int(StarColor::Yellow) == YELLOW_BODY);
const std::string EmptyName;
uqm::GameDefinitions &definitions() { return uqm::detail::activeDefinitions(); }
std::optional<StarId> &currentStar() {
    if (auto *runtime = uqm::detail::boundRuntime(); runtime && runtime->currentStar)
        return *runtime->currentStar;
    return uqm::gameSession().player().navigationState().currentStar;
}
// Sentinels exist only in the C buffer and have no native star or display text.
const StarDesc *nativeStar(const STAR_DESC *descriptor) {
    const auto index = GetStarIndex(descriptor);
    if (index == NUM_SOLAR_SYSTEMS || index == STAR_CATALOGUE_SIZE - 1)
        return nullptr;
    return &definitions().stars().at(StarId{index});
}
}

extern "C" {
STAR_DESC *GetCurrentStar(void) try {
    const auto id = currentStar();
    if (!id) return nullptr;
    definitions().stars().at(*id); // Validate native identity before C narrowing.
    return GetStar(static_cast<COUNT>(id->value));
} catch (...) { std::terminate(); }
void SetCurrentStar(STAR_DESC *star) try {
    if (!star) { currentStar().reset(); return; }
    const StarId id{GetStarIndex(star)};
    definitions().stars().at(id); // Selection never denotes a sentinel.
    currentStar() = id;
} catch (...) { std::terminate(); }
BOOLEAN LoadStarMap(void) {
    try {
        std::istringstream input(uqm::detail::readDefinitionFile("base/starmap.json"));
        uqm::detail::DefinitionsAccess::loadStars(definitions(), input);
        SetCurrentStar(nullptr);
        return TRUE;
    } catch (const std::exception &error) {
        log_add(log_Error, "base/starmap.json: %s", error.what());
        return FALSE;
    } catch (...) { std::terminate(); }
}
void FreeStarMap(void) try {
    SetCurrentStar(nullptr);
    uqm::detail::DefinitionsAccess::clearStars(definitions());
} catch (...) { std::terminate(); }
STAR_DESC *GetStar(const COUNT index) try {
    return uqm::detail::DefinitionsAccess::descriptor(definitions(), index);
} catch (...) { std::terminate(); }
COUNT GetStarIndex(const STAR_DESC *star) try {
    return uqm::detail::DefinitionsAccess::index(definitions(), star);
} catch (...) { std::terminate(); }
const char *GetStarPrefix(const STAR_DESC *star) try {
    const auto native = nativeStar(star);
    return native ? native->prefix().c_str() : "";
} catch (...) { std::terminate(); }
const char *GetStarPostfix(const STAR_DESC *star) try {
    const auto native = nativeStar(star);
    return native ? native->postfix().c_str() : "";
} catch (...) { std::terminate(); }
const char *GetQuasiSpaceName(void) try { return definitions().stars().quasispaceName().c_str(); }
catch (...) { std::terminate(); }
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
    const auto matches = definitions().stars().find(search);
    const auto next = std::find_if(matches.begin(), matches.end(),
            [after](StarId id) { return int(id.value) > after; });
    return next == matches.end() ? nullptr : GetStar(next->value);
} catch (...) { std::terminate(); }
void GetClusterName(const STAR_DESC *star, UNICODE *buf, const size_t capacity) try {
    assert(star && (buf || capacity == 0));
    if (capacity) {
        const auto native = nativeStar(star);
        // String ownership/references remain C++; only this boundary copies bytes.
        const std::string &name = native ? native->name() : EmptyName;
        utf8StringCopy(buf, capacity, name.c_str());
    }
} catch (...) { std::terminate(); }
}
