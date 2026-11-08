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

#include <istream>
#include <string>
#include <vector>
#include "starmap.h"

// Owns a star's display text independently of the old game-string catalogue.
// The plain descriptor is the temporary C interface; its eight serialized
// fields retain their original meanings. Names are immutable after loading.
class StarDesc {
public:
    StarDesc(STAR_DESC descriptor, std::string prefix, std::string postfix);
    STAR_DESC &descriptor() noexcept { return descriptor_; }
    const STAR_DESC &descriptor() const noexcept { return descriptor_; }
    const std::string &prefix() const noexcept { return prefix_; }
    const std::string &postfix() const noexcept { return postfix_; }
    const std::string &name() const noexcept { return name_; }
private:
    STAR_DESC descriptor_;
    std::string prefix_, postfix_, name_;
};

// Owns the complete ordered catalogue, including the two legacy sentinel slots.
// Loading validates counts, enum ranges, indices and text before publication.
// Storage never resizes afterward: borrowed descriptors/names last until this
// owner is destroyed. It cannot be copied or moved while handles are published.
class StarMap {
public:
    explicit StarMap(std::istream &json);
    StarMap(const StarMap &) = delete;
    StarMap &operator=(const StarMap &) = delete;
    size_t size() const noexcept { return stars_.size(); }
    // index must be within size(); includes sentinel positions 502 and 519.
    StarDesc &at(size_t index) noexcept;
    const StarDesc &at(size_t index) const noexcept;
    // descriptor must be an actual borrowed member of this map, not a copy.
    size_t indexOf(const STAR_DESC &descriptor) const noexcept;
    // Search in catalogue order after last (NULL starts a new search). Bounds
    // are inclusive; zero is exact, negative is unbounded on that axis. last
    // must belong to this map; a handle in another space ends the search.
    STAR_DESC *find(const STAR_DESC *last, const POINT &position,
            SIZE xbounds, SIZE ybounds, bool quasispace) noexcept;
    const std::string &quasispaceName() const noexcept { return quasispaceName_; }
private:
    std::vector<StarDesc> stars_;
    std::string quasispaceName_;
};
#endif
