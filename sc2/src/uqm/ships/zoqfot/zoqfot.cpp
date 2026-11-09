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
#include "resinst.h"

#include "libs/mathlib.h"

// Balance values are immutable, loaded from base/ships.json at startup.
static const uqm::combat::ShipTuning &tuning() {
    static const auto &value = uqm::combat::shipTuning("zoqfot");
    return value;
}
static const uqm::combat::ProjectileStats &missileStats() {
    static const auto &value = tuning().projectile("missile");
    return value;
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto ZoqfotpikOffset = 13;
constexpr auto MissileOffset = 0;
static auto missileRange() { return (missileStats().speed * missileStats().lifetime); }
constexpr auto SpitWait = 2;
constexpr auto TongueSpeed = 0;
constexpr auto TongueHits = 1;
constexpr auto TongueDamage = 12;
constexpr auto TongueOffset = 4;

// missileRange is an AI hint. SpitWait controls weapon color-animation cadence.

static const RACE_DESC &zoqfotpikDefinition() {
    static const RACE_DESC definition = {
        { /* SHIP_INFO */
            FIRES_FORE,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, tuning().ship.maxCrew,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            ZOQFOTPIK_RACE_STRINGS,
            ZOQFOTPIK_ICON_MASK_PMAP_ANIM,
            ZOQFOTPIK_MICON_MASK_PMAP_ANIM,
            NULL, NULL, NULL
        },
        { /* FLEET_STUFF */
            320 / SPHERE_RADIUS_INCREMENT * 2, /* Initial SoI radius */
            { /* Known location (center of SoI) */
                3761, 5333,
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
                ZOQFOTPIK_BIG_MASK_PMAP_ANIM,
                ZOQFOTPIK_MED_MASK_PMAP_ANIM,
                ZOQFOTPIK_SML_MASK_PMAP_ANIM,
            },
            {
                SPIT_BIG_MASK_PMAP_ANIM,
                SPIT_MED_MASK_PMAP_ANIM,
                SPIT_SML_MASK_PMAP_ANIM,
            },
            {
                STINGER_BIG_MASK_PMAP_ANIM,
                STINGER_MED_MASK_PMAP_ANIM,
                STINGER_SML_MASK_PMAP_ANIM,
            },
            {
                ZOQFOTPIK_CAPTAIN_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
            ZOQFOTPIK_VICTORY_SONG,
            ZOQFOTPIK_SHIP_SOUNDS,
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
        0, /* ship_owner */
    };
    return definition;
}

static void
spit_preprocess (ELEMENT *ElementPtr)
{
	/* turn_wait is abused here to control the animation speed. */
	if (ElementPtr->turn_wait > 0)
		--ElementPtr->turn_wait;
	else
	{
		COUNT index, angle, speed;

		ElementPtr->next.image.frame =
				IncFrameIndex (ElementPtr->next.image.frame);
		angle = GetVelocityTravelAngle (&ElementPtr->velocity);
		if ((index = GetFrameIndex (ElementPtr->next.image.frame)) == 1)
			angle = angle + (((COUNT)TFB_Random () % 3) - 1);

		speed = WORLD_TO_VELOCITY (DISPLAY_TO_WORLD (
				GetFrameCount (ElementPtr->next.image.frame) - index) << 1);
		SetVelocityComponents (&ElementPtr->velocity,
				(SIZE)COSINE (angle, speed),
				(SIZE)SINE (angle, speed));

		/* turn_wait is abused here to control the animation speed. */
		ElementPtr->turn_wait = SpitWait;
		ElementPtr->state_flags |= CHANGING;
	}
}

static COUNT
initialize_spit (ELEMENT *ShipPtr, HELEMENT SpitArray[])
{
	STARSHIP *StarShipPtr;
	MISSILE_BLOCK MissileBlock;

	GetElementStarShip (ShipPtr, &StarShipPtr);
	MissileBlock.cx = ShipPtr->next.location.x;
	MissileBlock.cy = ShipPtr->next.location.y;
	MissileBlock.farray = StarShipPtr->RaceDescPtr->ship_data.weapon;
	MissileBlock.face = StarShipPtr->ShipFacing;
	MissileBlock.index = 0;
	MissileBlock.sender = ShipPtr->playerNr;
	MissileBlock.flags = IGNORE_SIMILAR;
	MissileBlock.pixoffs = ZoqfotpikOffset;
	MissileBlock.speed = DISPLAY_TO_WORLD (
			GetFrameCount (StarShipPtr->RaceDescPtr->ship_data.weapon[0])) << 1;
	MissileBlock.hit_points = missileStats().hitPoints;
	MissileBlock.damage = missileStats().damage;
	MissileBlock.life = missileStats().lifetime;
	MissileBlock.preprocess_func = spit_preprocess;
	MissileBlock.blast_offs = MissileOffset;
	SpitArray[0] = initialize_missile (&MissileBlock);

	return (1);
}

static void spawn_tongue (ELEMENT *ElementPtr);

static void
tongue_postprocess (ELEMENT *ElementPtr)
{
	if (ElementPtr->turn_wait)
		spawn_tongue (ElementPtr);
}

static void
tongue_collision (ELEMENT *ElementPtr0, POINT *pPt0,
		ELEMENT *ElementPtr1, POINT *pPt1)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr0, &StarShipPtr);
	if (StarShipPtr->special_counter ==
			StarShipPtr->RaceDescPtr->characteristics.special_wait)
		weapon_collision (ElementPtr0, pPt0, ElementPtr1, pPt1);

	StarShipPtr->special_counter -= ElementPtr0->turn_wait;
	ElementPtr0->turn_wait = 0;
	ElementPtr0->state_flags |= NONSOLID;
}

static void
spawn_tongue (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;
	MISSILE_BLOCK TongueBlock;
	HELEMENT Tongue;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	TongueBlock.cx = ElementPtr->next.location.x;
	TongueBlock.cy = ElementPtr->next.location.y;
	TongueBlock.farray = StarShipPtr->RaceDescPtr->ship_data.special;
	TongueBlock.face = TongueBlock.index = StarShipPtr->ShipFacing;
	TongueBlock.sender = ElementPtr->playerNr;
	TongueBlock.flags = IGNORE_SIMILAR;
	TongueBlock.pixoffs = 0;
	TongueBlock.speed = TongueSpeed;
	TongueBlock.hit_points = TongueHits;
	TongueBlock.damage = TongueDamage;
	TongueBlock.life = 1;
	TongueBlock.preprocess_func = 0;
	TongueBlock.blast_offs = TongueOffset;
	Tongue = initialize_missile (&TongueBlock);
	if (Tongue)
	{
		ELEMENT *TonguePtr;

		LockElement (Tongue, &TonguePtr);
		TonguePtr->postprocess_func = tongue_postprocess;
		TonguePtr->collision_func = tongue_collision;
		if (ElementPtr->state_flags & PLAYER_SHIP)
			TonguePtr->turn_wait = StarShipPtr->special_counter;
		else
		{
			COUNT angle;
			RECT r;
			SIZE x_offs, y_offs;

			TonguePtr->turn_wait = ElementPtr->turn_wait - 1;

			GetFrameRect (TonguePtr->current.image.frame, &r);
			x_offs = DISPLAY_TO_WORLD (r.extent.width >> 1);
			y_offs = DISPLAY_TO_WORLD (r.extent.height >> 1);

			angle = FACING_TO_ANGLE (StarShipPtr->ShipFacing);
			if (angle > HALF_CIRCLE && angle < FULL_CIRCLE)
				TonguePtr->current.location.x -= x_offs;
			else if (angle > 0 && angle < HALF_CIRCLE)
				TonguePtr->current.location.x += x_offs;
			if (angle < QUADRANT || angle > FULL_CIRCLE - QUADRANT)
				TonguePtr->current.location.y -= y_offs;
			else if (angle > QUADRANT && angle < FULL_CIRCLE - QUADRANT)
				TonguePtr->current.location.y += y_offs;
		}

		SetElementStarShip (TonguePtr, StarShipPtr);
		UnlockElement (Tongue);
		PutElement (Tongue);
	}
}

static void
zoqfotpik_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
		COUNT ConcernCounter)
{
	BOOLEAN GiveTongueJob;
	STARSHIP *StarShipPtr;

	GetElementStarShip (ShipPtr, &StarShipPtr);

	GiveTongueJob = FALSE;
	if (StarShipPtr->special_counter == 0)
	{
		EVALUATE_DESC *lpEnemyEvalDesc;

		StarShipPtr->ship_input_state &= ~SPECIAL;

		lpEnemyEvalDesc = &ObjectsOfConcern[ENEMY_SHIP_INDEX];
		if (lpEnemyEvalDesc->ObjectPtr
				&& lpEnemyEvalDesc->which_turn <= 4
#ifdef NEVER
				&& StarShipPtr->RaceDescPtr->ship_info.energy_level >= tuning().ship.specialEnergyCost
#endif /* NEVER */
				)
		{
			SIZE delta_x, delta_y;

			GiveTongueJob = TRUE;

			lpEnemyEvalDesc->MoveState = PURSUE;
			delta_x = lpEnemyEvalDesc->ObjectPtr->next.location.x
					- ShipPtr->next.location.x;
			delta_y = lpEnemyEvalDesc->ObjectPtr->next.location.y
					- ShipPtr->next.location.y;
			if (StarShipPtr->ShipFacing == NORMALIZE_FACING (
					ANGLE_TO_FACING (ARCTAN (delta_x, delta_y))
					))
				StarShipPtr->ship_input_state |= SPECIAL;
		}
	}

	++StarShipPtr->weapon_counter;
	ship_intelligence (ShipPtr, ObjectsOfConcern, ConcernCounter);
	--StarShipPtr->weapon_counter;

	if (StarShipPtr->weapon_counter == 0)
	{
		StarShipPtr->ship_input_state &= ~WEAPON;
		if (!GiveTongueJob)
		{
			ObjectsOfConcern += ConcernCounter;
			while (ConcernCounter--)
			{
				--ObjectsOfConcern;
				if (ObjectsOfConcern->ObjectPtr
						&& (ConcernCounter == ENEMY_SHIP_INDEX
						|| (ConcernCounter == ENEMY_WEAPON_INDEX
						&& ObjectsOfConcern->MoveState != AVOID
#ifdef NEVER
						&& !(StarShipPtr->control & STANDARD_RATING)
#endif /* NEVER */
						))
						&& ship_weapons (ShipPtr,
						ObjectsOfConcern->ObjectPtr, DISPLAY_TO_WORLD (20)))
				{
					StarShipPtr->ship_input_state |= WEAPON;
					break;
				}
			}
		}
	}
}

static void
zoqfotpik_postprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if ((StarShipPtr->cur_status_flags & SPECIAL)
			&& StarShipPtr->special_counter == 0
			&& DeltaEnergy (ElementPtr, -tuning().ship.specialEnergyCost))
	{
		ProcessSound (SetAbsSoundIndex (
					/* STICK_OUT_TONGUE */
				StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 1), ElementPtr);

		StarShipPtr->special_counter =
				StarShipPtr->RaceDescPtr->characteristics.special_wait;
	}

	if (StarShipPtr->special_counter)
		spawn_tongue (ElementPtr);
}

namespace {
class ZoqFotPikShip final : public uqm::combat::Ship {
public:
    explicit ZoqFotPikShip(const RACE_DESC &definition) noexcept
        : Ship(definition) {
        legacyData().cyborg_control.intelligence_func = zoqfotpik_intelligence;
    }
private:
    void afterMotion(ELEMENT &element) noexcept override {
        zoqfotpik_postprocess(&element);
    }
    COUNT createWeapons(ELEMENT &element, uqm::combat::WeaponSlots &weapons) noexcept override {
        return initialize_spit(&element, weapons.data());
    }
};
} // namespace

std::unique_ptr<uqm::combat::Ship>
uqm::combat::makeZoqFotPikShip()
{

	return std::make_unique<ZoqFotPikShip>(zoqfotpikDefinition());
}

