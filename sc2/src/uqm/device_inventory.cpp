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

#include "device_menu.hpp"
#include "game_state.hpp"
#include "shipcont.h"
#include "globdata.h"
#include <algorithm>

// Define DEBUG_DEVICES here to include pictures of every defined device.

std::vector<BYTE> uqm::inventoryDevices(const CampaignState &campaign)
{
	BYTE i;
	std::vector<BYTE> devices;
    devices.reserve(NUM_DEVICES);
	
	for (i = 0; i < NUM_DEVICES; ++i)
	{
		BYTE DeviceState;

		DeviceState = 0;
		switch (i)
		{
			case ROSY_SPHERE_DEVICE:
				DeviceState = campaign.read(uqm::StoryFlag::RosySphereOnShip);
				break;
			case ARTIFACT_2_DEVICE:
				DeviceState = campaign.read(uqm::StoryFlag::Artifact2OnShip);
				break;
			case ARTIFACT_3_DEVICE:
				DeviceState = campaign.read(uqm::StoryFlag::Artifact3OnShip);
				break;
			case SUN_EFFICIENCY_DEVICE:
				DeviceState = campaign.read(uqm::StoryFlag::SunDeviceOnShip);
				break;
			case UTWIG_BOMB_DEVICE:
				DeviceState = campaign.read(uqm::StoryFlag::UtwigBombOnShip);
				break;
			case ULTRON_0_DEVICE:
				DeviceState = (campaign.read(uqm::StoryCounter::UltronCondition) == 1);
				break;
			case ULTRON_1_DEVICE:
				DeviceState = (campaign.read(uqm::StoryCounter::UltronCondition) == 2);
				break;
			case ULTRON_2_DEVICE:
				DeviceState = (campaign.read(uqm::StoryCounter::UltronCondition) == 3);
				break;
			case ULTRON_3_DEVICE:
				DeviceState = (campaign.read(uqm::StoryCounter::UltronCondition) == 4);
				break;
			case MAIDENS_DEVICE:
				DeviceState = campaign.read(uqm::StoryFlag::MaidensOnShip);
				break;
			case TALKING_PET_DEVICE:
				DeviceState = campaign.read(uqm::StoryFlag::TalkingPetOnShip);
				break;
			case AQUA_HELIX_DEVICE:
				DeviceState = campaign.read(uqm::StoryFlag::AquaHelixOnShip);
				break;
			case CLEAR_SPINDLE_DEVICE:
				DeviceState = campaign.read(uqm::StoryFlag::ClearSpindleOnShip);
				break;
			case UMGAH_HYPERWAVE_DEVICE:
				DeviceState = campaign.read(uqm::StoryFlag::UmgahBroadcastersOnShip);
				break;
			case TAALO_PROTECTOR_DEVICE:
				DeviceState = campaign.read(uqm::StoryFlag::TaaloProtectorOnShip);
				break;
			case EGG_CASING0_DEVICE:
				DeviceState = campaign.read(uqm::StoryFlag::EggCase0OnShip);
				break;
			case EGG_CASING1_DEVICE:
				DeviceState = campaign.read(uqm::StoryFlag::EggCase1OnShip);
				break;
			case EGG_CASING2_DEVICE:
				DeviceState = campaign.read(uqm::StoryFlag::EggCase2OnShip);
				break;
			case SYREEN_SHUTTLE_DEVICE:
				DeviceState = campaign.read(uqm::StoryFlag::SyreenShuttleOnShip);
				break;
			case VUX_BEAST_DEVICE:
				DeviceState = campaign.read(uqm::StoryFlag::VuxBeastOnShip);
				break;
			case DESTRUCT_CODE_DEVICE:
				break;
			case PORTAL_SPAWNER_DEVICE:
				DeviceState = campaign.read(uqm::StoryFlag::PortalSpawnerOnShip);
				break;
			case URQUAN_WARP_DEVICE:
				DeviceState = campaign.read(uqm::StoryFlag::PortalKeyOnShip);
				break;
			case BURVIX_HYPERWAVE_DEVICE:
				DeviceState = campaign.read(uqm::StoryFlag::BurvBroadcastersOnShip);
				break;
			case LUNAR_BASE_DEVICE:
				DeviceState = campaign.read(uqm::StoryFlag::MoonbaseOnShip);
				break;
		}

#ifndef DEBUG_DEVICES
		if (DeviceState)
#endif /* DEBUG_DEVICES */
		{
			devices.push_back(i);
		}
	}
	
	return devices;
}

// Legacy save records have a fixed buffer; copy a prefix of the native value.
SIZE InventoryDevices(BYTE *buffer, COUNT capacity) {
    const auto devices = uqm::inventoryDevices(uqm::gameSession().player().campaign());
    const auto count = std::min<std::size_t>(devices.size(), capacity);
    if (count) std::copy_n(devices.begin(), count, buffer);
    return static_cast<SIZE>(count);
}
