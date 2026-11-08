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
#include "arilou.h"
#include "resinst.h"

#include "libs/mathlib.h"

// Balance values are immutable, loaded from base/ships.json at startup.
static const uqm::combat::ShipTuning &tuning() {
    static const auto &value = uqm::combat::shipTuning("arilou");
    return value;
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto ArilouOffset = 9;
constexpr auto LaserRange = DISPLAY_TO_WORLD (100 + ArilouOffset);
constexpr auto HyperLife = 5;

static const RACE_DESC &arilouDefinition() {
    static const RACE_DESC definition = {
        { /* SHIP_INFO */
            /* FIRES_FORE | */ IMMEDIATE_WEAPON,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, tuning().ship.maxCrew,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            ARILOU_RACE_STRINGS,
            ARILOU_ICON_MASK_PMAP_ANIM,
            ARILOU_MICON_MASK_PMAP_ANIM,
            NULL, NULL, NULL
        },
        { /* FLEET_STUFF */
            250 / SPHERE_RADIUS_INCREMENT * 2, /* Initial SoI radius */
            { /* Known location (center of SoI) */
                438, 6372,
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
                ARILOU_BIG_MASK_PMAP_ANIM,
                ARILOU_MED_MASK_PMAP_ANIM,
                ARILOU_SML_MASK_PMAP_ANIM,
            },
            {
                NULL_RESOURCE,
                NULL_RESOURCE,
                NULL_RESOURCE,
            },
            {
                WARP_BIG_MASK_PMAP_ANIM,
                WARP_MED_MASK_PMAP_ANIM,
                WARP_SML_MASK_PMAP_ANIM,
            },
            {
                ARILOU_CAPTAIN_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
            ARILOU_VICTORY_SONG,
            ARILOU_SHIP_SOUNDS,
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            NULL, NULL
        },
        {
            0,
            uqm::combat::weaponRangeHint(LaserRange >> 1),
            NULL,
        },

        0,
        0, /* CodeRef */
    };
    return definition;
}

static COUNT
initialize_autoaim_laser (ELEMENT *ShipPtr, HELEMENT LaserArray[])
{
	COUNT orig_facing;
	SIZE delta_facing;
	STARSHIP *StarShipPtr;
	LASER_BLOCK LaserBlock;

	GetElementStarShip (ShipPtr, &StarShipPtr);
	LaserBlock.face = orig_facing = StarShipPtr->ShipFacing;
	if ((delta_facing = TrackShip (ShipPtr, &LaserBlock.face)) > 0)
		LaserBlock.face = NORMALIZE_FACING (orig_facing + delta_facing);
	ShipPtr->hTarget = 0;

	LaserBlock.cx = ShipPtr->next.location.x;
	LaserBlock.cy = ShipPtr->next.location.y;
	LaserBlock.ex = COSINE (FACING_TO_ANGLE (LaserBlock.face), LaserRange);
	LaserBlock.ey = SINE (FACING_TO_ANGLE (LaserBlock.face), LaserRange);
	LaserBlock.sender = ShipPtr->playerNr;
	LaserBlock.flags = IGNORE_SIMILAR;
	LaserBlock.pixoffs = ArilouOffset;
	LaserBlock.color = BUILD_COLOR (MAKE_RGB15 (0x1F, 0x1F, 0x0A), 0x0E);
	LaserArray[0] = initialize_laser (&LaserBlock);

	return (1);
}

static void
arilou_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
		COUNT ConcernCounter)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ShipPtr, &StarShipPtr);
	StarShipPtr->ship_input_state |= THRUST;

	 ObjectsOfConcern[ENEMY_SHIP_INDEX].MoveState = ENTICE;
	ship_intelligence (ShipPtr, ObjectsOfConcern, ConcernCounter);

	if (StarShipPtr->special_counter == 0)
	{
		EVALUATE_DESC *lpEvalDesc;

		StarShipPtr->ship_input_state &= ~SPECIAL;

		lpEvalDesc = &ObjectsOfConcern[ENEMY_WEAPON_INDEX];
		if (lpEvalDesc->ObjectPtr && lpEvalDesc->which_turn <= 6)
		{
			BOOLEAN IsTrackingWeapon;
			STARSHIP *EnemyStarShipPtr;

			GetElementStarShip (lpEvalDesc->ObjectPtr, &EnemyStarShipPtr);
			if (((EnemyStarShipPtr->RaceDescPtr->ship_info.ship_flags
					& SEEKING_WEAPON) &&
					lpEvalDesc->ObjectPtr->next.image.farray ==
					EnemyStarShipPtr->RaceDescPtr->ship_data.weapon) ||
					((EnemyStarShipPtr->RaceDescPtr->ship_info.ship_flags
					& SEEKING_SPECIAL) &&
					lpEvalDesc->ObjectPtr->next.image.farray ==
					EnemyStarShipPtr->RaceDescPtr->ship_data.special))
				IsTrackingWeapon = TRUE;
			else
				IsTrackingWeapon = FALSE;

			if (((lpEvalDesc->ObjectPtr->state_flags & PLAYER_SHIP) /* means IMMEDIATE WEAPON */
					|| (IsTrackingWeapon && (lpEvalDesc->which_turn == 1
					|| (lpEvalDesc->ObjectPtr->state_flags & CREW_OBJECT))) /* FIGHTERS!!! */
					|| PlotIntercept (lpEvalDesc->ObjectPtr, ShipPtr, 3, 0))
					&& !(TFB_Random () & 3))
			{
				StarShipPtr->ship_input_state &= ~(LEFT | RIGHT | THRUST | WEAPON);
				StarShipPtr->ship_input_state |= SPECIAL;
			}
		}
	}
	if (StarShipPtr->RaceDescPtr->ship_info.energy_level <= tuning().ship.specialEnergyCost << 1)
		StarShipPtr->ship_input_state &= ~WEAPON;
}

static void
arilou_preprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if (!(ElementPtr->state_flags & NONSOLID))
	{
		if (ElementPtr->thrust_wait == 0)
		{
			ZeroVelocityComponents (&ElementPtr->velocity);
			StarShipPtr->cur_status_flags &= ~SHIP_AT_MAX_SPEED;
		}

		if ((StarShipPtr->cur_status_flags & SPECIAL)
				&& StarShipPtr->special_counter == 0
				&& DeltaEnergy (ElementPtr, -tuning().ship.specialEnergyCost))
		{
			/* Special key is pressed; start teleport */
			ZeroVelocityComponents (&ElementPtr->velocity);
			StarShipPtr->cur_status_flags &=
					~(SHIP_AT_MAX_SPEED | LEFT | RIGHT | THRUST | WEAPON);

			ElementPtr->state_flags |= NONSOLID | FINITE_LIFE | CHANGING;
			ElementPtr->life_span = HyperLife;

			ElementPtr->next.image.farray =
					StarShipPtr->RaceDescPtr->ship_data.special;
			ElementPtr->next.image.frame =
					StarShipPtr->RaceDescPtr->ship_data.special[0];

			ProcessSound (SetAbsSoundIndex (
							/* HYPERJUMP */
					StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 1), ElementPtr);
			StarShipPtr->special_counter =
					StarShipPtr->RaceDescPtr->characteristics.special_wait;
		}
	}
	else if (ElementPtr->next.image.farray == StarShipPtr->RaceDescPtr->ship_data.special)
	{
		COUNT life_span;

		StarShipPtr->cur_status_flags =
				(StarShipPtr->cur_status_flags
				& ~(LEFT | RIGHT | THRUST | WEAPON | SPECIAL))
				| (StarShipPtr->old_status_flags
				& (LEFT | RIGHT | THRUST | WEAPON | SPECIAL));
		++StarShipPtr->weapon_counter;
		++StarShipPtr->special_counter;
		++StarShipPtr->energy_counter;
		++ElementPtr->turn_wait;
		++ElementPtr->thrust_wait;

		if ((life_span = ElementPtr->life_span) == NORMAL_LIFE)
		{
			/* Ending teleport */
			ElementPtr->state_flags &= ~(NONSOLID | FINITE_LIFE);
			ElementPtr->state_flags |= APPEARING;
			ElementPtr->current.image.farray =
					ElementPtr->next.image.farray =
					StarShipPtr->RaceDescPtr->ship_data.ship;
			ElementPtr->current.image.frame =
					ElementPtr->next.image.frame =
					SetAbsFrameIndex (StarShipPtr->RaceDescPtr->ship_data.ship[0],
					StarShipPtr->ShipFacing);
			InitIntersectStartPoint (ElementPtr);
		}
		else
		{
			/* Teleporting in progress */
			--life_span;
			if (life_span != 2)
			{
				if (life_span < 2)
					ElementPtr->next.image.frame =
							DecFrameIndex (ElementPtr->next.image.frame);
				else
					ElementPtr->next.image.frame =
							IncFrameIndex (ElementPtr->next.image.frame);
			}
			else
			{
				ElementPtr->next.location.x =
						WRAP_X (DISPLAY_ALIGN_X (TFB_Random ()));
				ElementPtr->next.location.y =
						WRAP_Y (DISPLAY_ALIGN_Y (TFB_Random ()));
			}
		}

		ElementPtr->state_flags |= CHANGING;
	}
}

namespace {
class ArilouShip final : public uqm::combat::Ship {
public:
    explicit ArilouShip(const RACE_DESC &definition) noexcept
        : Ship(definition) {
        legacyData().cyborg_control.intelligence_func = arilou_intelligence;
    }
private:
    void beforeMotion(ELEMENT &element) noexcept override {
        arilou_preprocess(&element);
    }
    COUNT createWeapons(ELEMENT &element, uqm::combat::WeaponSlots &weapons) noexcept override {
        return initialize_autoaim_laser(&element, weapons.data());
    }
};
} // namespace

RACE_DESC*
init_arilou (void) try
{

	return uqm::combat::releaseToLegacy(std::make_unique<ArilouShip>(arilouDefinition()));
} catch (...) {
    uqm::combat::logShipCreationFailure("ArilouShip");
    return nullptr;
}

