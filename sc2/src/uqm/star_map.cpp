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
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <nlohmann/json.hpp>
#include "star_map.hpp"
#include "gendef.h"

namespace {
using Json = nlohmann::json;
const std::unordered_map<std::string, int> StarTypes{
    {"DWARF_STAR", DWARF_STAR}, {"GIANT_STAR", GIANT_STAR},
    {"SUPER_GIANT_STAR", SUPER_GIANT_STAR}};
const std::unordered_map<std::string, int> StarColors{
    {"BLUE_BODY", BLUE_BODY}, {"GREEN_BODY", GREEN_BODY},
    {"ORANGE_BODY", ORANGE_BODY}, {"RED_BODY", RED_BODY},
    {"WHITE_BODY", WHITE_BODY}, {"YELLOW_BODY", YELLOW_BODY}};

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
}

StarDesc::StarDesc(StarId id, StarSpace space, StarPosition position,
        StarProperties properties, std::string prefix, std::string postfix)
    : id_(id), space_(space), position_(position), properties_(properties),
      prefix_(std::move(prefix)), postfix_(std::move(postfix)),
      name_(prefix_.empty() ? postfix_ : prefix_ + " " + postfix_) {
    if (name_.size() > 255)
        throw std::runtime_error("combined star name exceeds 255 bytes");
}

StarMap::StarMap(std::istream &input) {
    const auto root = Json::parse(input);
    integer(root, "schema_version", 1, 1);
    quasispaceName_ = displayText(root, "quasispace_name");
    stars_.reserve(502 + 16);
    // IDs are data identities, independent of vector positions. Preserve the
    // established ranges so save records and portal destinations still agree.
    struct Section { const char *key; std::size_t count, firstId; StarSpace space; };
    for (const auto &section : {Section{"hyperspace", 502, 0, StarSpace::HyperSpace},
            {"quasispace", 16, 503, StarSpace::QuasiSpace}}) {
        const auto &records = root.at(section.key);
        if (!records.is_array() || records.size() != section.count)
            throw std::runtime_error(std::string(section.key) + " has the wrong record count");
        size_t row = 0;
        for (const auto &record : records) {
            try {
                const StarId id{static_cast<size_t>(integer(record, "catalogue_index", 0, 518))};
                if (id.value != section.firstId + row)
                    throw std::runtime_error("catalogue_index must retain its original position");
                const StarPosition position{integer(record, "x", 0, 9999),
                        integer(record, "y", 0, 9999)};
                const StarProperties properties{
                    static_cast<StarType>(enumeration(record, "type", StarTypes)),
                    static_cast<StarColor>(enumeration(record, "color", StarColors)),
                    static_cast<unsigned>(integer(record, "owner", 0, 7)),
                    static_cast<unsigned>(integer(record, "special_system", 0, MYCON_TRAP_DEFINED)),
                    static_cast<unsigned>(integer(record, "prefix_id", 0, 14)),
                    static_cast<unsigned>(integer(record, "postfix_id", 0, 132))};
                auto prefix = displayText(record, "prefix"), postfix = displayText(record, "postfix");
                if (postfix.empty())
                    throw std::runtime_error("postfix must not be empty");
                stars_.emplace_back(id, section.space, position, properties,
                        std::move(prefix), std::move(postfix));
            } catch (const std::exception &error) {
                throw std::runtime_error(std::string(section.key) + "[" + std::to_string(row)
                        + "]: " + error.what());
            }
            ++row;
        }

    }
}

const StarDesc &StarMap::at(const StarId id) const {
    const auto found = std::lower_bound(stars_.begin(), stars_.end(), id.value,
            [](const StarDesc &star, std::size_t value) { return star.id().value < value; });
    if (found == stars_.end() || found->id() != id)
        throw std::out_of_range("unknown star ID: " + std::to_string(id.value));
    return *found;
}
std::vector<StarId> StarMap::find(const StarSearch &search) const {
    if ((search.xRadius && *search.xRadius < 0) || (search.yRadius && *search.yRadius < 0))
        throw std::invalid_argument("star search radii must be nonnegative or nullopt");
    std::vector<StarId> matches;
    for (const auto &star : stars_) {
        const auto &position = star.position();
        if (star.space() == search.space
                && (!search.xRadius || std::abs(int64_t(position.x) - search.center.x) <= *search.xRadius)
                && (!search.yRadius || std::abs(int64_t(position.y) - search.center.y) <= *search.yRadius))
            matches.push_back(star.id());
    }
    return matches;
}
