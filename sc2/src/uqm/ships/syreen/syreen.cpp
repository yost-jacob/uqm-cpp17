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

#include "../ship.h"
#include "uqm/ship_behavior.hpp"
#include "uqm/ship_builders.hpp"
#include "uqm/ship_tuning.hpp"
#include "uqm/projectile_legacy.hpp"
#include "resinst.h"

#include "libs/mathlib.h"

// Balance values are immutable, loaded from base/ships.json at startup.
static const uqm::combat::ShipTuning &tuning() {
    static const auto &value = uqm::combat::shipTuning("syreen");
    return value;
}
static const uqm::combat::ProjectileStats &missileStats() {
    static const auto &value = tuning().projectile("missile");
    return value;
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto SyreenMaxCrewSize = MAX_CREW_SIZE;
constexpr auto SyreenOffset = 30;
constexpr auto MissileOffset = 3;
constexpr auto AbandonerRange = 208;
constexpr auto MaxAbandoners = 8;

static const RACE_DESC &syreenDefinition() {
    static const RACE_DESC definition = {
        { /* SHIP_INFO */
            FIRES_FORE,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, SyreenMaxCrewSize,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            SYREEN_RACE_STRINGS,
            SYREEN_ICON_MASK_PMAP_ANIM,
            SYREEN_MICON_MASK_PMAP_ANIM,
            NULL, NULL, NULL
        },
        { /* FLEET_STUFF */
            0, /* Initial sphere of influence radius */
            { /* Known location (center of SoI) */
                0, 0,
            },
        },
        {
            tuning().ship.maxThrust,
            tuning().ship.thrustIncrement,
            static_cast<BYTE>(tuning().ship.energyRegeneration),
            tuning().ship.weaponEnergyCost,
            tuning().ship.specialEnergyCost,
            tuning().ship.energyWait,
            tuning().ship.turnWait,
            tuning().ship.thrustWait,
            tuning().ship.weaponWait,
            tuning().ship.specialWait,
            tuning().ship.shipMass,
        },
        {
            {
                SYREEN_BIG_MASK_PMAP_ANIM,
                SYREEN_MED_MASK_PMAP_ANIM,
                SYREEN_SML_MASK_PMAP_ANIM,
            },
            {
                DAGGER_BIG_MASK_PMAP_ANIM,
                DAGGER_MED_MASK_PMAP_ANIM,
                DAGGER_SML_MASK_PMAP_ANIM,
            },
            {
                NULL_RESOURCE,
                NULL_RESOURCE,
                NULL_RESOURCE,
            },
            {
                SYREEN_CAPTAIN_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
            SYREEN_VICTORY_SONG,
            SYREEN_SHIP_SOUNDS,
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            NULL, NULL
        },
        {
            0,
            uqm::combat::weaponRangeHint((missileStats().speed * missileStats().lifetime * 2 / 3)),
            NULL,
        },

        0,
        0, /* ship_owner */
    };
    return definition;
}

static COUNT
initialize_dagger (ELEMENT *ShipPtr, HELEMENT DaggerArray[])
{
	STARSHIP *StarShipPtr;
	MISSILE_BLOCK MissileBlock;

	GetElementStarShip (ShipPtr, &StarShipPtr);
	MissileBlock.cx = ShipPtr->next.location.x;
	MissileBlock.cy = ShipPtr->next.location.y;
	MissileBlock.farray = StarShipPtr->RaceDescPtr->ship_data.weapon;
	MissileBlock.face = MissileBlock.index = StarShipPtr->ShipFacing;
	MissileBlock.sender = ShipPtr->playerNr;
	MissileBlock.flags = IGNORE_SIMILAR;
	MissileBlock.pixoffs = SyreenOffset;
	MissileBlock.speed = missileStats().speed;
	MissileBlock.hit_points = missileStats().hitPoints;
	MissileBlock.damage = missileStats().damage;
	MissileBlock.life = missileStats().lifetime;
	MissileBlock.preprocess_func = NULL;
	MissileBlock.blast_offs = MissileOffset;
	DaggerArray[0] = uqm::combat::publishMissile(MissileBlock, missileStats().flight);

	return (1);
}

static void
spawn_crew (ELEMENT *ElementPtr)
{
	if (ElementPtr->state_flags & PLAYER_SHIP)
	{
		HELEMENT hCrew;

		hCrew = AllocElement ();
		if (hCrew != 0)
		{
			ELEMENT *CrewPtr;

			LockElement (hCrew, &CrewPtr);
			CrewPtr->next.location = ElementPtr->next.location;
			CrewPtr->playerNr = ElementPtr->playerNr;
			CrewPtr->state_flags = APPEARING | NONSOLID | FINITE_LIFE;
			CrewPtr->life_span = 0;
			CrewPtr->death_func = spawn_crew;
			CrewPtr->pParent = ElementPtr->pParent;
			CrewPtr->hTarget = 0;
			UnlockElement (hCrew);

			PutElement (hCrew);
		}
	}
	else
	{
		HELEMENT hElement, hNextElement;

		for (hElement = GetHeadElement ();
				hElement != 0; hElement = hNextElement)
		{
			ELEMENT *ObjPtr;

			LockElement (hElement, &ObjPtr);
			hNextElement = GetSuccElement (ObjPtr);

			if ((ObjPtr->state_flags & PLAYER_SHIP)
					&& !elementsOfSamePlayer (ObjPtr, ElementPtr)
					&& ObjPtr->crew_level > 1)
			{
				SIZE dx, dy;
				DWORD d_squared;

				dx = ObjPtr->next.location.x - ElementPtr->next.location.x;
				if (dx < 0)
					dx = -dx;
				dy = ObjPtr->next.location.y - ElementPtr->next.location.y;
				if (dy < 0)
					dy = -dy;

				dx = WORLD_TO_DISPLAY (dx);
				dy = WORLD_TO_DISPLAY (dy);
				if (dx <= AbandonerRange && dy <= AbandonerRange
						&& (d_squared = (DWORD)((UWORD)dx * (UWORD)dx)
						+ (DWORD)((UWORD)dy * (UWORD)dy)) <=
						(DWORD)((UWORD)AbandonerRange * (UWORD)AbandonerRange))
				{
					COUNT crew_loss;

					crew_loss = ((MaxAbandoners
							* (AbandonerRange - square_root (d_squared)))
							/ AbandonerRange) + 1;
					if (crew_loss >= ObjPtr->crew_level)
						crew_loss = ObjPtr->crew_level - 1;

					AbandonShip (ObjPtr, ElementPtr, crew_loss);
				}
			}

			UnlockElement (hElement);
		}
	}
}

static void
syreen_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
		COUNT ConcernCounter)
{
	EVALUATE_DESC *lpEvalDesc;

	ship_intelligence (ShipPtr,
			ObjectsOfConcern, ConcernCounter);

	lpEvalDesc = &ObjectsOfConcern[ENEMY_SHIP_INDEX];
	if (lpEvalDesc->ObjectPtr != NULL)
	{
		STARSHIP *StarShipPtr;
		STARSHIP *EnemyStarShipPtr;

		GetElementStarShip (ShipPtr, &StarShipPtr);
		GetElementStarShip (lpEvalDesc->ObjectPtr, &EnemyStarShipPtr);
		if (!(EnemyStarShipPtr->RaceDescPtr->ship_info.ship_flags & CREW_IMMUNE)
				&& StarShipPtr->special_counter == 0
				&& lpEvalDesc->ObjectPtr->crew_level > 1
				&& lpEvalDesc->which_turn <= 14)
			StarShipPtr->ship_input_state |= SPECIAL;
		else
			StarShipPtr->ship_input_state &= ~SPECIAL;
	}
}

static void
syreen_postprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if ((StarShipPtr->cur_status_flags & SPECIAL)
			&& StarShipPtr->special_counter == 0
			&& DeltaEnergy (ElementPtr, -tuning().ship.specialEnergyCost))
	{
		ProcessSound (SetAbsSoundIndex (
						/* SYREEN_SONG */
				StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 1), ElementPtr);
		spawn_crew (ElementPtr);

		StarShipPtr->special_counter =
				StarShipPtr->RaceDescPtr->characteristics.special_wait;
	}
}

namespace {
class SyreenShip final : public uqm::combat::Ship {
public:
    explicit SyreenShip(const RACE_DESC &definition) noexcept
        : Ship(definition) {
        legacyData().cyborg_control.intelligence_func = syreen_intelligence;
    }
private:
    void afterMotion(ELEMENT &element) noexcept override {
        syreen_postprocess(&element);
    }
    COUNT createWeapons(ELEMENT &element, uqm::combat::WeaponSlots &weapons) noexcept override {
        return initialize_dagger(&element, weapons.data());
    }
};
} // namespace

std::unique_ptr<uqm::combat::Ship>
uqm::combat::makeSyreenShip()
{

	return std::make_unique<SyreenShip>(syreenDefinition());
}

