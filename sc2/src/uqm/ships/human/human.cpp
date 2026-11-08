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
#include "uqm/ship_tuning.hpp"
#include "human.h"
#include "resinst.h"

#include "uqm/colors.h"
#include "uqm/globdata.h"

// Balance values are immutable, loaded from base/ships.json at startup.
static const uqm::combat::ShipTuning &tuning() {
    static const auto &value = uqm::combat::shipTuning("human");
    return value;
}
static const uqm::combat::ProjectileStats &missileStats() {
    static const auto &value = tuning().projectile("missile");
    return value;
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto HumanOffset = 42;
constexpr auto NukeOffset = 8;
constexpr auto MaxMissileSpeed = DISPLAY_TO_WORLD (20);
constexpr auto ThrustScale = DISPLAY_TO_WORLD (1);
constexpr auto TrackWait = 3;
constexpr auto LaserRange = (UWORD)100;

static const RACE_DESC &humanDefinition() {
    static const RACE_DESC definition = {
        { /* SHIP_INFO */
            FIRES_FORE | SEEKING_WEAPON | POINT_DEFENSE,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, tuning().ship.maxCrew,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            HUMAN_RACE_STRINGS,
            HUMAN_ICON_MASK_PMAP_ANIM,
            HUMAN_MICON_MASK_PMAP_ANIM,
            NULL, NULL, NULL
        },
        { /* FLEET_STUFF */
            0, /* Initial sphere of influence radius */
            { /* Known location (center of SoI) */
                1752, 1450,
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
                HUMAN_BIG_MASK_PMAP_ANIM,
                HUMAN_MED_MASK_PMAP_ANIM,
                HUMAN_SML_MASK_PMAP_ANIM,
            },
            {
                SATURN_BIG_MASK_PMAP_ANIM,
                SATURN_MED_MASK_PMAP_ANIM,
                SATURN_SML_MASK_PMAP_ANIM,
            },
            {
                NULL_RESOURCE,
                NULL_RESOURCE,
                NULL_RESOURCE,
            },
            {
                HUMAN_CAPTAIN_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
            HUMAN_VICTORY_SONG,
            HUMAN_SHIP_SOUNDS,
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            NULL, NULL
        },
        {
            0,
            uqm::combat::weaponRangeHint(LONG_RANGE_WEAPON),
            NULL,
        },

        0,
        0, /* CodeRef */
    };
    return definition;
}

static void
nuke_preprocess (ELEMENT *ElementPtr)
{
	COUNT facing;

	facing = GetFrameIndex (ElementPtr->next.image.frame);
	if (ElementPtr->turn_wait > 0)
		--ElementPtr->turn_wait;
	else
	{
		if (TrackShip (ElementPtr, &facing) > 0)
		{
			ElementPtr->next.image.frame =
					SetAbsFrameIndex (ElementPtr->next.image.frame,
					facing);
			ElementPtr->state_flags |= CHANGING;
		}

		ElementPtr->turn_wait = TrackWait;
	}

	{
		SIZE speed;

		if ((speed = missileStats().speed +
				((missileStats().lifetime - ElementPtr->life_span) *
				ThrustScale)) > MaxMissileSpeed)
			speed = MaxMissileSpeed;
		SetVelocityVector (&ElementPtr->velocity,
				speed, facing);
	}
}

static void
spawn_point_defense (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if (ElementPtr->state_flags & PLAYER_SHIP)
	{
		HELEMENT hDefense;

		hDefense = AllocElement ();
		if (hDefense)
		{
			ELEMENT *DefensePtr;

			LockElement (hDefense, &DefensePtr);
			DefensePtr->playerNr = ElementPtr->playerNr;
			DefensePtr->state_flags = APPEARING | NONSOLID | FINITE_LIFE;
			DefensePtr->death_func = spawn_point_defense;

			GetElementStarShip (ElementPtr, &StarShipPtr);
			SetElementStarShip (DefensePtr, StarShipPtr);
			UnlockElement (hDefense);

			PutElement (hDefense);
		}
	}
	else
	{
		BOOLEAN PaidFor;
		HELEMENT hObject, hNextObject;
		ELEMENT *ShipPtr;

		PaidFor = FALSE;

		LockElement (StarShipPtr->hShip, &ShipPtr);
		for (hObject = GetTailElement (); hObject; hObject = hNextObject)
		{
			ELEMENT *ObjectPtr;

			LockElement (hObject, &ObjectPtr);
			hNextObject = GetPredElement (ObjectPtr);
			if (ObjectPtr != ShipPtr && CollidingElement (ObjectPtr) &&
					!OBJECT_CLOAKED (ObjectPtr))
			{
				SIZE delta_x, delta_y;

				delta_x = ObjectPtr->next.location.x -
						ShipPtr->next.location.x;
				delta_y = ObjectPtr->next.location.y -
						ShipPtr->next.location.y;
				if (delta_x < 0)
					delta_x = -delta_x;
				if (delta_y < 0)
					delta_y = -delta_y;
				delta_x = WORLD_TO_DISPLAY (delta_x);
				delta_y = WORLD_TO_DISPLAY (delta_y);
				if ((UWORD)delta_x <= LaserRange &&
						(UWORD)delta_y <= LaserRange &&
						(UWORD)delta_x * (UWORD)delta_x +
						(UWORD)delta_y * (UWORD)delta_y <=
						LaserRange * LaserRange)
				{
					HELEMENT hPointDefense;
					LASER_BLOCK LaserBlock;

					if (!PaidFor)
					{
						if (!DeltaEnergy (ShipPtr, -tuning().ship.specialEnergyCost))
							break;

						ProcessSound (SetAbsSoundIndex (
										/* POINT_DEFENSE_LASER */
								StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 1), ElementPtr);
						StarShipPtr->special_counter =
								StarShipPtr->RaceDescPtr->characteristics.special_wait;
						PaidFor = TRUE;
					}

					LaserBlock.cx = ShipPtr->next.location.x;
					LaserBlock.cy = ShipPtr->next.location.y;
					LaserBlock.face = 0;
					LaserBlock.ex = ObjectPtr->next.location.x
							- ShipPtr->next.location.x;
					LaserBlock.ey = ObjectPtr->next.location.y
							- ShipPtr->next.location.y;
					LaserBlock.sender = ShipPtr->playerNr;
					LaserBlock.flags = IGNORE_SIMILAR;
					LaserBlock.pixoffs = 0;
					LaserBlock.color = BUILD_COLOR (MAKE_RGB15 (0x1F, 0x1F, 0x1F), 0x0F);
					hPointDefense = initialize_laser (&LaserBlock);
					if (hPointDefense)
					{
						ELEMENT *PDPtr;

						LockElement (hPointDefense, &PDPtr);
						SetElementStarShip (PDPtr, StarShipPtr);
						PDPtr->hTarget = 0;
						UnlockElement (hPointDefense);

						PutElement (hPointDefense);
					}
				}
			}
			UnlockElement (hObject);
		}
		UnlockElement (StarShipPtr->hShip);
	}
}

static COUNT
initialize_nuke (ELEMENT *ShipPtr, HELEMENT NukeArray[])
{
	STARSHIP *StarShipPtr;
	MISSILE_BLOCK MissileBlock;

	GetElementStarShip (ShipPtr, &StarShipPtr);
	MissileBlock.cx = ShipPtr->next.location.x;
	MissileBlock.cy = ShipPtr->next.location.y;
	MissileBlock.farray = StarShipPtr->RaceDescPtr->ship_data.weapon;
	MissileBlock.face = MissileBlock.index = StarShipPtr->ShipFacing;
	MissileBlock.sender = ShipPtr->playerNr;
	MissileBlock.flags = 0;
	MissileBlock.pixoffs = HumanOffset;
	MissileBlock.speed = missileStats().speed;
	MissileBlock.hit_points = missileStats().hitPoints;
	MissileBlock.damage = missileStats().damage;
	MissileBlock.life = missileStats().lifetime;
	MissileBlock.preprocess_func = nuke_preprocess;
	MissileBlock.blast_offs = NukeOffset;
	NukeArray[0] = initialize_missile (&MissileBlock);

	if (NukeArray[0])
	{
		ELEMENT *NukePtr;

		LockElement (NukeArray[0], &NukePtr);
		NukePtr->turn_wait = TrackWait;
		UnlockElement (NukeArray[0]);
	}

	return (1);
}

static void
human_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
		COUNT ConcernCounter)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ShipPtr, &StarShipPtr);
	if (StarShipPtr->special_counter == 0
			&& ((ObjectsOfConcern[ENEMY_WEAPON_INDEX].ObjectPtr != NULL
			&& ObjectsOfConcern[ENEMY_WEAPON_INDEX].which_turn <= 2)
			|| (ObjectsOfConcern[ENEMY_SHIP_INDEX].ObjectPtr != NULL
			&& ObjectsOfConcern[ENEMY_SHIP_INDEX].which_turn <= 4)))
		StarShipPtr->ship_input_state |= SPECIAL;
	else
		StarShipPtr->ship_input_state &= ~SPECIAL;
	ObjectsOfConcern[ENEMY_WEAPON_INDEX].ObjectPtr = NULL;

	ship_intelligence (ShipPtr,
			ObjectsOfConcern, ConcernCounter);

	if (StarShipPtr->weapon_counter == 0)
	{
		if (ObjectsOfConcern[ENEMY_SHIP_INDEX].ObjectPtr
				&& (!(StarShipPtr->ship_input_state & (LEFT | RIGHT /* | THRUST */))
				|| ObjectsOfConcern[ENEMY_SHIP_INDEX].which_turn <= 12))
			StarShipPtr->ship_input_state |= WEAPON;
	}
}

static void
human_postprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if ((StarShipPtr->cur_status_flags & SPECIAL)
			&& StarShipPtr->special_counter == 0)
	{
		spawn_point_defense (ElementPtr);
	}
}

namespace {
class EarthlingShip final : public uqm::combat::Ship {
public:
    explicit EarthlingShip(const RACE_DESC &definition) noexcept
        : Ship(definition) {
        legacyData().cyborg_control.intelligence_func = human_intelligence;
    }
private:
    void afterMotion(ELEMENT &element) noexcept override {
        human_postprocess(&element);
    }
    COUNT createWeapons(ELEMENT &element, uqm::combat::WeaponSlots &weapons) noexcept override {
        return initialize_nuke(&element, weapons.data());
    }
};
} // namespace

RACE_DESC*
init_human (void) try
{

	return uqm::combat::releaseToLegacy(std::make_unique<EarthlingShip>(humanDefinition()));
} catch (...) {
    uqm::combat::logShipCreationFailure("EarthlingShip");
    return nullptr;
}

