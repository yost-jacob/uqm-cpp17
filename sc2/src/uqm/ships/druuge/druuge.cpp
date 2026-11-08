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
#include "druuge.h"
#include "resinst.h"

// Balance values are immutable, loaded from base/ships.json at startup.
static const uqm::combat::ShipTuning &tuning() {
    static const auto &value = uqm::combat::shipTuning("druuge");
    return value;
}
static const uqm::combat::ProjectileStats &missileStats() {
    static const auto &value = tuning().projectile("missile");
    return value;
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto DruugeOffset = 24;
constexpr auto MissileOffset = 6;
static auto missileRange() { return (missileStats().speed * missileStats().lifetime); }
constexpr auto RecoilVelocity = WORLD_TO_VELOCITY (DISPLAY_TO_WORLD (6));
constexpr auto MaxRecoilVelocity = (RecoilVelocity * 4);

static const RACE_DESC &druugeDefinition() {
    static const RACE_DESC definition = {
        { /* SHIP_INFO */
            FIRES_FORE,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, tuning().ship.maxCrew,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            DRUUGE_RACE_STRINGS,
            DRUUGE_ICON_MASK_PMAP_ANIM,
            DRUUGE_MICON_MASK_PMAP_ANIM,
            NULL, NULL, NULL
        },
        { /* FLEET_STUFF */
            1400 / SPHERE_RADIUS_INCREMENT * 2, /* Initial SoI radius */
            { /* Known location (center of SoI) */
                9500, 2792,
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
                DRUUGE_BIG_MASK_PMAP_ANIM,
                DRUUGE_MED_MASK_PMAP_ANIM,
                DRUUGE_SML_MASK_PMAP_ANIM,
            },
            {
                DRUUGE_CANNON_BIG_MASK_PMAP_ANIM,
                DRUUGE_CANNON_MED_MASK_PMAP_ANIM,
                DRUUGE_CANNON_SML_MASK_PMAP_ANIM,
            },
            {
                NULL_RESOURCE,
                NULL_RESOURCE,
                NULL_RESOURCE,
            },
            {
                DRUUGE_CAPT_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
            DRUUGE_VICTORY_SONG,
            DRUUGE_SHIP_SOUNDS,
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            NULL, NULL
        },
        {
            0,
            uqm::combat::weaponRangeHint(missileRange()),
            NULL,
        },

        0,
        0, /* CodeRef */
    };
    return definition;
}

static void
cannon_collision (ELEMENT *ElementPtr0, POINT *pPt0,
		ELEMENT *ElementPtr1, POINT *pPt1)
{
	weapon_collision (ElementPtr0, pPt0, ElementPtr1, pPt1);

	if ((ElementPtr1->state_flags & PLAYER_SHIP)
			&& ElementPtr1->crew_level
			&& !GRAVITY_MASS (ElementPtr1->mass_points + 1))
	{
		COUNT angle;
		SIZE cur_delta_x, cur_delta_y;
		STARSHIP *StarShipPtr;

		GetElementStarShip (ElementPtr1, &StarShipPtr);
		StarShipPtr->cur_status_flags &=
				~(SHIP_AT_MAX_SPEED | SHIP_BEYOND_MAX_SPEED);

		angle = FACING_TO_ANGLE (
				GetFrameIndex (ElementPtr0->next.image.frame)
				);
		DeltaVelocityComponents (&ElementPtr1->velocity,
				COSINE (angle, RecoilVelocity),
				SINE (angle, RecoilVelocity));
		GetCurrentVelocityComponents (&ElementPtr1->velocity,
				&cur_delta_x, &cur_delta_y);
		if ((long)cur_delta_x * (long)cur_delta_x
				+ (long)cur_delta_y * (long)cur_delta_y
				> (long)MaxRecoilVelocity * (long)MaxRecoilVelocity)
		{
			angle = ARCTAN (cur_delta_x, cur_delta_y);
			SetVelocityComponents (&ElementPtr1->velocity,
					COSINE (angle, MaxRecoilVelocity),
					SINE (angle, MaxRecoilVelocity));
		}
	}
}

static COUNT
initialize_cannon (ELEMENT *ShipPtr, HELEMENT CannonArray[])
{
	STARSHIP *StarShipPtr;
	MISSILE_BLOCK MissileBlock;

	GetElementStarShip (ShipPtr, &StarShipPtr);
	MissileBlock.cx = ShipPtr->next.location.x;
	MissileBlock.cy = ShipPtr->next.location.y;
	MissileBlock.farray = StarShipPtr->RaceDescPtr->ship_data.weapon;
	MissileBlock.face = StarShipPtr->ShipFacing;
	MissileBlock.index = MissileBlock.face;
	MissileBlock.sender = ShipPtr->playerNr;
	MissileBlock.flags = IGNORE_SIMILAR;
	MissileBlock.pixoffs = DruugeOffset;
	MissileBlock.speed = missileStats().speed;
	MissileBlock.hit_points = missileStats().hitPoints;
	MissileBlock.damage = missileStats().damage;
	MissileBlock.life = missileStats().lifetime;
	MissileBlock.preprocess_func = NULL;
	MissileBlock.blast_offs = MissileOffset;
	CannonArray[0] = initialize_missile (&MissileBlock);

	if (CannonArray[0])
	{
		ELEMENT *CannonPtr;

		LockElement (CannonArray[0], &CannonPtr);
		CannonPtr->collision_func = cannon_collision;
		UnlockElement (CannonArray[0]);
	}

	return (1);
}

static void
druuge_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
		COUNT ConcernCounter)
{
	UWORD ship_flags = 0;
	STARSHIP *StarShipPtr;
	STARSHIP *EnemyStarShipPtr = NULL;
	EVALUATE_DESC *lpEvalDesc;

	GetElementStarShip (ShipPtr, &StarShipPtr);

	lpEvalDesc = &ObjectsOfConcern[ENEMY_SHIP_INDEX];
	if (StarShipPtr->cur_status_flags & SHIP_BEYOND_MAX_SPEED)
		lpEvalDesc->MoveState = ENTICE;
	else if (lpEvalDesc->ObjectPtr
			&& lpEvalDesc->which_turn <= WORLD_TO_TURN (missileRange() * 3 / 4))
	{
		GetElementStarShip (lpEvalDesc->ObjectPtr, &EnemyStarShipPtr);
		ship_flags = EnemyStarShipPtr->RaceDescPtr->ship_info.ship_flags;
		EnemyStarShipPtr->RaceDescPtr->ship_info.ship_flags &=
				~(FIRES_FORE | FIRES_RIGHT | FIRES_AFT | FIRES_LEFT);

		lpEvalDesc->MoveState = PURSUE;
		if (ShipPtr->thrust_wait == 0)
			++ShipPtr->thrust_wait;
	}
	ship_intelligence (ShipPtr, ObjectsOfConcern, ConcernCounter);
	if (EnemyStarShipPtr)
	{
		EnemyStarShipPtr->RaceDescPtr->ship_info.ship_flags = ship_flags;
	}

	if (!(StarShipPtr->cur_status_flags & SHIP_BEYOND_MAX_SPEED)
			&& (lpEvalDesc->which_turn <= 12
			|| (
			ObjectsOfConcern[ENEMY_WEAPON_INDEX].ObjectPtr
			&& ObjectsOfConcern[ENEMY_WEAPON_INDEX].which_turn <= 6
			)))
	{
		 StarShipPtr->ship_input_state |= WEAPON;
		 if (ShipPtr->thrust_wait < tuning().ship.weaponWait + 1)
			ShipPtr->thrust_wait = tuning().ship.weaponWait + 1;
	}

	if ((StarShipPtr->ship_input_state & WEAPON)
			&& StarShipPtr->RaceDescPtr->ship_info.energy_level < tuning().ship.weaponEnergyCost
			&& ShipPtr->crew_level > 1)
		StarShipPtr->ship_input_state |= SPECIAL;
	else
		StarShipPtr->ship_input_state &= ~SPECIAL;
}

static void
druuge_postprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
			/* if just fired cannon */
	if ((StarShipPtr->cur_status_flags & WEAPON)
			&& StarShipPtr->weapon_counter ==
			StarShipPtr->RaceDescPtr->characteristics.weapon_wait)
	{
		COUNT angle;
		SIZE cur_delta_x, cur_delta_y;

		StarShipPtr->cur_status_flags &= ~SHIP_AT_MAX_SPEED;

		angle = FACING_TO_ANGLE (StarShipPtr->ShipFacing) + HALF_CIRCLE;
		DeltaVelocityComponents (&ElementPtr->velocity,
				COSINE (angle, RecoilVelocity),
				SINE (angle, RecoilVelocity));
		GetCurrentVelocityComponents (&ElementPtr->velocity,
				&cur_delta_x, &cur_delta_y);
		if ((long)cur_delta_x * (long)cur_delta_x
				+ (long)cur_delta_y * (long)cur_delta_y
				> (long)MaxRecoilVelocity * (long)MaxRecoilVelocity)
		{
			angle = ARCTAN (cur_delta_x, cur_delta_y);
			SetVelocityComponents (&ElementPtr->velocity,
					COSINE (angle, MaxRecoilVelocity),
					SINE (angle, MaxRecoilVelocity));
		}
	}
}

static void
druuge_preprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if (StarShipPtr->cur_status_flags & SPECIAL)
	{
		if (StarShipPtr->special_counter
				|| ElementPtr->crew_level == 1
				|| StarShipPtr->RaceDescPtr->ship_info.energy_level
				== StarShipPtr->RaceDescPtr->ship_info.max_energy)
			StarShipPtr->cur_status_flags &= ~SPECIAL;
		else
		{
			ProcessSound (SetAbsSoundIndex (
							/* BURN UP CREW */
					StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 1), ElementPtr);

			DeltaCrew (ElementPtr, -1);
			DeltaEnergy (ElementPtr, tuning().ship.specialEnergyCost);

			StarShipPtr->special_counter =
					StarShipPtr->RaceDescPtr->characteristics.special_wait;
		}
	}
}

namespace {
class DruugeShip final : public uqm::combat::Ship {
public:
    explicit DruugeShip(const RACE_DESC &definition) noexcept
        : Ship(definition) {
        legacyData().cyborg_control.intelligence_func = druuge_intelligence;
    }
private:
    void beforeMotion(ELEMENT &element) noexcept override {
        druuge_preprocess(&element);
    }
    void afterMotion(ELEMENT &element) noexcept override {
        druuge_postprocess(&element);
    }
    COUNT createWeapons(ELEMENT &element, uqm::combat::WeaponSlots &weapons) noexcept override {
        return initialize_cannon(&element, weapons.data());
    }
};
} // namespace

RACE_DESC*
init_druuge (void) try
{

	return uqm::combat::releaseToLegacy(std::make_unique<DruugeShip>(druugeDefinition()));
} catch (...) {
    uqm::combat::logShipCreationFailure("DruugeShip");
    return nullptr;
}

