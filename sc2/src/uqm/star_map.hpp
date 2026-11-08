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

#ifndef UQM_STAR_MAP_HPP_
#define UQM_STAR_MAP_HPP_

#include <cstddef>
#include <istream>
#include <optional>
#include <string>
#include <vector>

// Catalogue identity, not a vector offset. IDs stay stable across data edits;
// StarMap::at validates them before returning a star.
struct StarId {
    std::size_t value;
    constexpr bool operator==(StarId other) const noexcept { return value == other.value; }
    constexpr bool operator!=(StarId other) const noexcept { return !(*this == other); }
};
enum class StarSpace { HyperSpace, QuasiSpace };
enum class StarType { Dwarf, Giant, SuperGiant };
enum class StarColor { Blue, Green, Orange, Red, White, Yellow };
struct StarPosition { int x, y; };
struct StarProperties {
    StarType type;
    StarColor color;
    unsigned owner, specialSystem, prefixOrder, postfixId;
};
// Inclusive radii: zero means an exact coordinate; nullopt leaves that axis
// unrestricted. Negative radii are rejected. Space is explicit, never a bool.
struct StarSearch {
    StarSpace space;
    StarPosition center;
    std::optional<int> xRadius;
    std::optional<int> yRadius;
};

// Immutable star value with owned display text and unpacked numeric properties.
// Construction copies/moves values; it never borrows a parser or C descriptor.
class StarDesc {
public:
    StarDesc(StarId id, StarSpace space, StarPosition position,
            StarProperties properties, std::string prefix, std::string postfix);
    StarId id() const noexcept { return id_; }
    StarSpace space() const noexcept { return space_; }
    const StarPosition &position() const noexcept { return position_; }
    const StarProperties &properties() const noexcept { return properties_; }
    const std::string &prefix() const noexcept { return prefix_; }
    const std::string &postfix() const noexcept { return postfix_; }
    const std::string &name() const noexcept { return name_; }
private:
    StarId id_;
    StarSpace space_;
    StarPosition position_;
    StarProperties properties_;
    std::string prefix_, postfix_, name_;
};

// Owns real stars only. Legacy sentinel records and descriptor buffers belong
// to the C adapter. Loading validates the existing catalogue IDs and counts.
// References last until destruction; this owner cannot be copied or moved.
class StarMap {
public:
    explicit StarMap(std::istream &json);
    StarMap(const StarMap &) = delete;
    StarMap &operator=(const StarMap &) = delete;
    const std::vector<StarDesc> &stars() const noexcept { return stars_; }
    // Checked, read-only lookup by identity; unknown IDs throw out_of_range.
    const StarDesc &at(StarId id) const;
    // Return all matching IDs in catalogue order; an empty vector means no
    // match. Results own their IDs and do not borrow pointers or iterators.
    std::vector<StarId> find(const StarSearch &search) const;
    const std::string &quasispaceName() const noexcept { return quasispaceName_; }
private:
    std::vector<StarDesc> stars_;
    std::string quasispaceName_;
};
#endif
