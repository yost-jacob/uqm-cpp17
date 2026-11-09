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

#include "genall.h"
#include "../../system_builders.hpp"
#include "../../game_state.hpp"
#include "../lander.h"
#include "../planets.h"
#include "../../globdata.h"
#include "../../nameref.h"
#include "../../resinst.h"
#include "libs/mathlib.h"
namespace uqm {
namespace {
class VaultSystemBehavior final : public SystemBehavior {
public:
    using SystemBehavior::SystemBehavior;
    bool generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body) override;
    COUNT generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT node, NODE_INFO *info) override;
    bool pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT node) override;
};
}











bool
VaultSystemBehavior::generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, 0))
	{
		LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
		solarSys->PlanetSideFrame[1] =
				CaptureDrawable (LoadGraphic (VAULT_MASK_PMAP_ANIM));
		solarSys->SysInfo.PlanetInfo.DiscoveryString =
				CaptureStringTable (LoadStringTable (VAULT_STRTAB));
		if (campaign_.read(StoryFlag::ShipVaultUnlocked))
		{
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					SetAbsStringTableIndex (
					solarSys->SysInfo.PlanetInfo.DiscoveryString, 2);
		}
		else if (campaign_.read(StoryFlag::SyreenShuttleOnShip))
		{
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					SetAbsStringTableIndex (
					solarSys->SysInfo.PlanetInfo.DiscoveryString, 1);
		}
	}

	GenerateDefault_generateOrbital (solarSys, world);
	return true;
}

COUNT
VaultSystemBehavior::generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT whichNode, NODE_INFO *info)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, 0))
	{
		return GenerateDefault_generateArtifact (solarSys, whichNode, info);
	}

	return 0;
}

bool
VaultSystemBehavior::pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT whichNode)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, 0))
	{
		assert (whichNode == 0);

		if (campaign_.read(StoryFlag::ShipVaultUnlocked))
		{	// Give the final report, "omg empty" and whatnot
			GenerateDefault_landerReportCycle (solarSys);
		}
		else if (campaign_.read(StoryFlag::SyreenShuttleOnShip))
		{
			GenerateDefault_landerReportCycle (solarSys);
			SetLanderTakeoff ();

			campaign_.set(StoryFlag::ShipVaultUnlocked, (1) != 0);
			campaign_.set(StoryFlag::SyreenShuttleOnShip, (0) != 0);
			campaign_.set(StoryCounter::SyreenHomeVisits, 0);
		}
		else
		{
			GenerateDefault_landerReport (solarSys);

			if (!campaign_.read(StoryFlag::KnowSyreenVault))
			{
				campaign_.set(StoryFlag::KnowSyreenVault, (1) != 0);
			}
		}

		// The Vault cannot be "picked up". It is always on the surface.
		return false;
	}

	(void) whichNode;
	return false;
}

std::unique_ptr<SystemBehavior> createVaultSystemBehavior(CampaignState &campaign) {
    return std::make_unique<VaultSystemBehavior>(campaign);
}
} // namespace uqm
