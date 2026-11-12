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
class WreckSystemBehavior final : public SystemBehavior {
public:
    using SystemBehavior::SystemBehavior;
    COUNT generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT node, NODE_INFO *info) override;
    bool pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT node) override;
};
}

COUNT
WreckSystemBehavior::generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT whichNode, NODE_INFO *info)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 6, MATCH_PLANET))
	{
		return GenerateDefault_generateArtifact (solarSys, whichNode, info);
	}

	return 0;
}

bool
WreckSystemBehavior::pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT whichNode)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 6, MATCH_PLANET))
	{
		assert (whichNode == 0);

		GenerateDefault_landerReportCycle (solarSys);

		if (!story_.read(StoryFlag::PortalKey))
		{
			SetLanderTakeoff ();

			story_.set(StoryFlag::PortalKey, (1) != 0);
			story_.set(StoryFlag::PortalKeyOnShip, (1) != 0);
		}

		// The Wreck cannot be "picked up". It is always on the surface.
		return false;
	}

	(void) whichNode;
	return false;
}

std::unique_ptr<SystemBehavior> createWreckSystemBehavior(StoryState &story) {
    return std::make_unique<WreckSystemBehavior>(story);
}
} // namespace uqm
