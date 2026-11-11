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
#include "nameref.h"
#include "init.h"
#include "libs/log.h"
#include <exception>
#include <memory>

namespace {
// Keep the legacy selective-transfer contract: master/fleet callers sometimes
// retain icons after the behavior owner is destroyed. This does not publish state.
void freeAssets(RACE_DESC &descriptor, bool iconData, bool battleData) noexcept {
	if (battleData)
	{
		DATA_STUFF *shipData = &descriptor.ship_data;

		free_image (shipData->special);
		free_image (shipData->weapon);
		free_image (shipData->ship);

		DestroyDrawable (
				ReleaseDrawable (shipData->captain_control.background));
		DestroyMusic (shipData->victory_ditty);
		DestroySound (ReleaseSound (shipData->ship_sounds));
	}

	if (iconData)
	{
		SHIP_INFO *shipInfo = &descriptor.ship_info;

		DestroyDrawable (ReleaseDrawable (shipInfo->melee_icon));
		DestroyDrawable (ReleaseDrawable (shipInfo->icons));
		DestroyStringTable (ReleaseStringTable (shipInfo->race_strings));
	}
}
// Resource functions and required/optional asset policy are unchanged. Only
// construction ownership and rollback move into this native boundary.
bool loadAssets(RACE_DESC &descriptor, bool battleData) {
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
		if (!load_animation (RawPtr->ship,
				RawPtr->ship_rsc[0],
				RawPtr->ship_rsc[1],
				RawPtr->ship_rsc[2]))
			return false;

		if (RawPtr->weapon_rsc[0] != NULL_RESOURCE)
		{
			if (!load_animation (RawPtr->weapon,
					RawPtr->weapon_rsc[0],
					RawPtr->weapon_rsc[1],
					RawPtr->weapon_rsc[2]))
				return false;
		}

		if (RawPtr->special_rsc[0] != NULL_RESOURCE)
		{
			if (!load_animation (RawPtr->special,
					RawPtr->special_rsc[0],
					RawPtr->special_rsc[1],
					RawPtr->special_rsc[2]))
				return false;
		}

		if (RawPtr->captain_control.captain_rsc != NULL_RESOURCE)
		{
			RawPtr->captain_control.background = CaptureDrawable (LoadGraphic (
					RawPtr->captain_control.captain_rsc));
			if (!RawPtr->captain_control.background)
				return false;
		}

		if (RawPtr->victory_ditty_rsc != NULL_RESOURCE)
		{
			RawPtr->victory_ditty =
					LoadMusic (RawPtr->victory_ditty_rsc);
			if (!RawPtr->victory_ditty)
				return false;
		}

		if (RawPtr->ship_sounds_rsc != NULL_RESOURCE)
		{
			RawPtr->ship_sounds = CaptureSound (
					LoadSound (RawPtr->ship_sounds_rsc));
			if (!RawPtr->ship_sounds)
				return false;
		}
	}

    return true;
}
struct AssetLoadRollback {
    RACE_DESC &descriptor;
    bool committed = false;
    ~AssetLoadRollback() {
        if (!committed) freeAssets(descriptor, true, true);
    }
};
}

extern "C" RACE_DESC *load_ship(SPECIES_ID species, BOOLEAN battleData) {
    try {
        auto ship = uqm::combat::createShip(species);
        if (!ship) return nullptr;
        AssetLoadRollback rollback{ship->legacyData()};
        if (!loadAssets(ship->legacyData(), battleData != FALSE)) return nullptr;
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

extern "C" void free_ship(RACE_DESC *descriptor, BOOLEAN iconData, BOOLEAN battleData) {
    if (!descriptor) return;
    std::unique_ptr<uqm::combat::Ship> owner(&uqm::combat::shipFor(*descriptor));
    owner->finish(); // Explicit legacy campaign completion precedes asset release.
    freeAssets(owner->legacyData(), iconData != FALSE, battleData != FALSE);
}

void uqm::detail::discardLoadedShip(RACE_DESC &descriptor) noexcept {
    std::unique_ptr<uqm::combat::Ship> owner(&uqm::combat::shipFor(descriptor));
    freeAssets(owner->legacyData(), true, true);
}
