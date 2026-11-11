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

#include "build.h"
#include "ship_factory.hpp"
#include "ship_assets_legacy.hpp"
#include "game_assets.hpp"
#include "legacy_runtime.hpp"
#include "nameref.h"
#include "init.h"
#include "libs/log.h"
#include <exception>
#include <stdexcept>
#include <memory>

namespace {
// Keep the legacy selective-transfer contract: master/fleet callers sometimes
// retain icons after the behavior owner is destroyed. This does not publish state.
void freeAssets(RACE_DESC &descriptor, bool iconData) noexcept {
    // Battle resources are shared owners in Ship, never manually freed here.
    // Icons retain the existing explicit transfer into fleet/menu storage.
    if (iconData) {
        auto &info = descriptor.ship_info;
        DestroyDrawable(ReleaseDrawable(info.melee_icon));
        DestroyDrawable(ReleaseDrawable(info.icons));
        DestroyStringTable(ReleaseStringTable(info.race_strings));
    }
}
FRAME graphic(uqm::combat::Ship &ship, uqm::AssetCache &cache, RESOURCE id) {
    if (!id) throw std::invalid_argument("missing required ship graphic");
    auto asset = cache.find({uqm::AssetKind::Graphic, id});
    const auto frame = dynamic_cast<const uqm::GraphicAsset &>(*asset).frame();
    ship.retainAsset(std::move(asset));
    return frame;
}
void animation(uqm::combat::Ship &ship, uqm::AssetCache &cache, FRAME *frames, const RESOURCE *ids) {
    // Missing smaller views alias the last available view, as in load_animation.
    FRAME previous = nullptr;
    for (unsigned i = 0; i < NUM_VIEWS; ++i) {
        if (i == 0 || ids[i] != NULL_RESOURCE) previous = graphic(ship, cache, ids[i]);
        frames[i] = previous;
    }
}
// Resource functions and required/optional asset policy are unchanged. Only
// construction ownership and rollback move into this native boundary.
void loadAssets(uqm::combat::Ship &ship, uqm::AssetCache &cache, bool battleData) {
    auto &descriptor = ship.legacyData();
	if (descriptor.ship_info.icons_rsc != NULL_RESOURCE)
	{
		descriptor.ship_info.icons = CaptureDrawable (LoadGraphic (
				descriptor.ship_info.icons_rsc));
	}
	
	if (descriptor.ship_info.melee_icon_rsc != NULL_RESOURCE)
	{
		descriptor.ship_info.melee_icon = CaptureDrawable (LoadGraphic (
				descriptor.ship_info.melee_icon_rsc));
	}

	if (descriptor.ship_info.race_strings_rsc != NULL_RESOURCE)
	{
		descriptor.ship_info.race_strings =	CaptureStringTable (LoadStringTable (
				descriptor.ship_info.race_strings_rsc));
	}

	if (battleData)
	{
		DATA_STUFF *RawPtr = &descriptor.ship_data;
        animation(ship, cache, RawPtr->ship, RawPtr->ship_rsc);
        if (RawPtr->weapon_rsc[0] != NULL_RESOURCE)
            animation(ship, cache, RawPtr->weapon, RawPtr->weapon_rsc);
        if (RawPtr->special_rsc[0] != NULL_RESOURCE)
            animation(ship, cache, RawPtr->special, RawPtr->special_rsc);
        if (RawPtr->captain_control.captain_rsc != NULL_RESOURCE)
            RawPtr->captain_control.background = graphic(ship, cache, RawPtr->captain_control.captain_rsc);
        if (RawPtr->victory_ditty_rsc != NULL_RESOURCE) {
            auto asset = cache.find({uqm::AssetKind::Music, RawPtr->victory_ditty_rsc});
            RawPtr->victory_ditty = dynamic_cast<const uqm::MusicAsset &>(*asset).music();
            ship.retainAsset(std::move(asset));
        }
        if (RawPtr->ship_sounds_rsc != NULL_RESOURCE) {
            auto asset = cache.find({uqm::AssetKind::Sound, RawPtr->ship_sounds_rsc});
            RawPtr->ship_sounds = dynamic_cast<const uqm::SoundAsset &>(*asset).sound();
            ship.retainAsset(std::move(asset));
        }
	}

}
struct AssetLoadRollback {
    RACE_DESC &descriptor;
    bool committed = false;
    ~AssetLoadRollback() {
        if (!committed) freeAssets(descriptor, true);
    }
};
}

extern "C" RACE_DESC *load_ship(SPECIES_ID species, BOOLEAN battleData) {
    try {
        auto ship = uqm::combat::createShip(species);
        if (!ship) return nullptr;
        AssetLoadRollback rollback{ship->legacyData()};
        loadAssets(*ship, uqm::detail::sessionAssets(), battleData != FALSE);
        rollback.committed = true;
        // The C gameplay layer temporarily owns this object through its borrowed
        // descriptor. free_ship reconstructs the unique owner at the same boundary.
        auto *owner = ship.release();
        return &owner->legacyData();
    } catch (const std::exception &error) {
        log_add(log_Error, "Could not load ship species %d: %s", static_cast<int>(species), error.what());
    } catch (...) {
        log_add(log_Error, "Could not load ship species %d: unknown exception", static_cast<int>(species));
    }
    return nullptr;
}

extern "C" void free_ship(RACE_DESC *descriptor, BOOLEAN iconData) {
    if (!descriptor) return;
    std::unique_ptr<uqm::combat::Ship> owner(&uqm::combat::shipFor(*descriptor));
    owner->finish(); // Explicit legacy campaign completion precedes asset release.
    freeAssets(owner->legacyData(), iconData != FALSE);
}

void uqm::detail::discardLoadedShip(RACE_DESC &descriptor) noexcept {
    std::unique_ptr<uqm::combat::Ship> owner(&uqm::combat::shipFor(descriptor));
    freeAssets(owner->legacyData(), true);
}
