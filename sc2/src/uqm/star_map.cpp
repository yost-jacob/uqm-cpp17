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
#include <unordered_map>
#include <utility>
#include <nlohmann/json.hpp>
#include "star_map.hpp"
#include "gendef.h"
#include "options.h"
#include "libs/log.h"

namespace {
using Json = nlohmann::json;
const std::unordered_map<std::string, int> StarTypes{
    {"DWARF_STAR", DWARF_STAR}, {"GIANT_STAR", GIANT_STAR},
    {"SUPER_GIANT_STAR", SUPER_GIANT_STAR}};
const std::unordered_map<std::string, int> StarColors{
    {"BLUE_BODY", BLUE_BODY}, {"GREEN_BODY", GREEN_BODY},
    {"ORANGE_BODY", ORANGE_BODY}, {"RED_BODY", RED_BODY},
    {"WHITE_BODY", WHITE_BODY}, {"YELLOW_BODY", YELLOW_BODY}};
std::unique_ptr<StarMap> catalogue;

// Read an integral JSON property without accepting float truncation or narrowing.
int integer(const Json &record, const char *key, const int minimum, const int maximum) {
    const auto &value = record.at(key);
    if (!value.is_number_integer())
        throw std::runtime_error(std::string(key) + " must be an integer");
    const auto number = value.get<int64_t>();
    if (number < minimum || number > maximum)
        throw std::runtime_error(std::string(key) + " is out of range");
    return static_cast<int>(number);
}
// UI search buffers hold at most 255 UTF-8 bytes plus the terminator. Control
// characters and embedded NULs are not meaningful display names.
std::string displayText(const Json &record, const char *key) {
    auto result = record.at(key).get<std::string>();
    if (result.size() > 255 || std::any_of(result.begin(), result.end(),
            [](unsigned char c) { return c < 32 || c == 127; }))
        throw std::runtime_error(std::string(key) + " must be display text of at most 255 bytes");
    return result;
}
// Resolve existing enum spellings without introducing another numeric enum.
int enumeration(const Json &record, const char *key,
        const std::unordered_map<std::string, int> &values) {
    const auto name = record.at(key).get<std::string>();
    const auto found = values.find(name);
    if (found == values.end())
        throw std::runtime_error(std::string(key) + ": unknown value '" + name + "'");
    return found->second;
}
StarMap &map() { assert(catalogue); return *catalogue; }
}

StarDesc::StarDesc(STAR_DESC descriptor, std::string prefix, std::string postfix)
    : descriptor_(descriptor), prefix_(std::move(prefix)), postfix_(std::move(postfix)),
      name_(prefix_.empty() ? postfix_ : prefix_ + " " + postfix_) {
    if (name_.size() > 255)
        throw std::runtime_error("combined star name exceeds 255 bytes");
}

StarMap::StarMap(std::istream &input) {
    const auto root = Json::parse(input);
    integer(root, "schema_version", 1, 1);
    quasispaceName_ = displayText(root, "quasispace_name");
    stars_.reserve(STAR_CATALOGUE_SIZE);
    // Keep the legacy record order, including its sentinel gaps. It indexes
    // scan state, visited-star flags, encounter groups and QuasiSpace portals.
    for (const auto &space : {std::pair{"hyperspace", NUM_SOLAR_SYSTEMS},
            {"quasispace", NUM_QUASISPACE_LOCATIONS}}) {
        const auto &records = root.at(space.first);
        if (!records.is_array() || records.size() != static_cast<size_t>(space.second))
            throw std::runtime_error(std::string(space.first) + " has the wrong record count");
        size_t row = 0;
        for (const auto &record : records) {
            try {
                STAR_DESC descriptor{};
                descriptor.catalogue_index = integer(record, "catalogue_index", 0, STAR_CATALOGUE_SIZE - 1);
                if (descriptor.catalogue_index != stars_.size())
                    throw std::runtime_error("catalogue_index must retain its original position");
                descriptor.star_pt.x = integer(record, "x", 0, MAX_X_UNIVERSE);
                descriptor.star_pt.y = integer(record, "y", 0, MAX_Y_UNIVERSE);
                const auto type = enumeration(record, "type", StarTypes);
                const auto color = enumeration(record, "color", StarColors);
                descriptor.Type = MAKE_STAR(type, color, integer(record, "owner", 0, STAR_OWNER_MASK));
                descriptor.Index = integer(record, "special_system", 0, MYCON_TRAP_DEFINED);
                descriptor.Prefix = integer(record, "prefix_id", 0, 14);
                descriptor.Postfix = integer(record, "postfix_id", 0, 132);
                auto prefix = displayText(record, "prefix"), postfix = displayText(record, "postfix");
                if (postfix.empty())
                    throw std::runtime_error("postfix must not be empty");
                stars_.emplace_back(descriptor, std::move(prefix), std::move(postfix));
            } catch (const std::exception &error) {
                throw std::runtime_error(std::string(space.first) + "[" + std::to_string(row)
                        + "]: " + error.what());
            }
            ++row;
        }
        STAR_DESC sentinel{};
        sentinel.star_pt = {MAX_X_UNIVERSE * 2, MAX_Y_UNIVERSE * 2};
        sentinel.catalogue_index = stars_.size();
        stars_.emplace_back(sentinel, "", "");
    }
}

StarDesc &StarMap::at(const size_t index) noexcept {
    assert(index < stars_.size());
    return stars_[index];
}
const StarDesc &StarMap::at(const size_t index) const noexcept {
    assert(index < stars_.size());
    return stars_[index];
}
size_t StarMap::indexOf(const STAR_DESC &descriptor) const noexcept {
    const auto index = descriptor.catalogue_index;
    assert(index < size() && &at(index).descriptor() == &descriptor);
    return index;
}
STAR_DESC *StarMap::find(const STAR_DESC *last, const POINT &position,
        const SIZE xbounds, const SIZE ybounds, const bool quasispace) noexcept {
    const size_t first = quasispace ? QUASISPACE_FIRST : 0;
    const size_t end = first + (quasispace ? NUM_QUASISPACE_LOCATIONS : NUM_SOLAR_SYSTEMS);
    const auto start = last ? indexOf(*last) + 1 : first;
    if (start < first || start >= end)
        return nullptr;
    // A bounded linear scan is simple for 502 stars and also permits coordinate
    // edits without sorting/reordering persistent catalogue identities.
    const auto found = std::find_if(stars_.begin() + start, stars_.begin() + end,
            [&](const StarDesc &star) {
        const auto &point = star.descriptor().star_pt;
        return (xbounds < 0 || std::abs(int(point.x) - position.x) <= xbounds)
                && (ybounds < 0 || std::abs(int(point.y) - position.y) <= ybounds);
    });
    return found == stars_.begin() + end ? nullptr : &found->descriptor();
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
        std::istringstream input(std::move(bytes));
        catalogue = std::make_unique<StarMap>(input);
        CurStarDescPtr = nullptr;
        return TRUE;
    } catch (const std::exception &error) {
        log_add(log_Error, "base/starmap.json: %s", error.what());
        return FALSE;
    } catch (...) { std::terminate(); }
}
void FreeStarMap(void) { CurStarDescPtr = nullptr; catalogue.reset(); }
STAR_DESC *GetStar(const COUNT index) { return &map().at(index).descriptor(); }
COUNT GetStarIndex(const STAR_DESC *star) { assert(star); return map().indexOf(*star); }
const char *GetStarPrefix(const STAR_DESC *star) {
    return map().at(GetStarIndex(star)).prefix().c_str();
}
const char *GetStarPostfix(const STAR_DESC *star) {
    return map().at(GetStarIndex(star)).postfix().c_str();
}
const char *GetQuasiSpaceName(void) { return map().quasispaceName().c_str(); }
STAR_DESC *FindStarInSpace(const STAR_DESC *last, const POINT *position,
        const SIZE xbounds, const SIZE ybounds, const bool quasispace) {
    assert(position);
    return map().find(last, *position, xbounds, ybounds, quasispace);
}
void GetClusterName(const STAR_DESC *star, UNICODE *buf, const size_t capacity) {
    assert(star && (buf || capacity == 0));
    if (capacity)
        utf8StringCopy(buf, capacity, map().at(GetStarIndex(star)).name().c_str());
}
}
