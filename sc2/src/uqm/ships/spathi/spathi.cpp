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

// Balance values are immutable, loaded from base/ships.json at startup.
static const uqm::combat::ShipTuning &tuning() {
    static const auto &value = uqm::combat::shipTuning("spathi");
    return value;
}
static const uqm::combat::ProjectileStats &discriminatorStats() {
    static const auto &value = tuning().projectile("discriminator");
    return value;
}
static const uqm::combat::ProjectileStats &missileStats() {
    static const auto &value = tuning().projectile("missile");
    return value;
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto SpathiForwardOffset = 16;
constexpr auto MissileOffset = 1;
static auto missileRange() { return (missileStats().speed * missileStats().lifetime); }
constexpr auto SpathiRearOffset = 20;
constexpr auto DiscriminatorOffset = 4;
constexpr auto TrackWait = 1;

// missileRange is an AI hint derived from the configured projectile.

static const RACE_DESC &spathiDefinition() {
    static const RACE_DESC definition = {
        { /* SHIP_INFO */
            FIRES_FORE | FIRES_AFT | SEEKING_SPECIAL | DONT_CHASE,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, tuning().ship.maxCrew,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            SPATHI_RACE_STRINGS,
            SPATHI_ICON_MASK_PMAP_ANIM,
            SPATHI_MICON_MASK_PMAP_ANIM,
            NULL, NULL, NULL
        },
        { /* FLEET_STUFF */
            1000 / SPHERE_RADIUS_INCREMENT * 2, /* Initial SoI radius */
            { /* Known location (center of SoI) */
                2549, 3600,
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
                SPATHI_BIG_MASK_PMAP_ANIM,
                SPATHI_MED_MASK_PMAP_ANIM,
                SPATHI_SML_MASK_PMAP_ANIM,
            },
            {
                MISSILE_BIG_MASK_PMAP_ANIM,
                MISSILE_MED_MASK_PMAP_ANIM,
                MISSILE_SML_MASK_PMAP_ANIM,
            },
            {
                DISCRIM_BIG_MASK_PMAP_ANIM,
                DISCRIM_MED_MASK_PMAP_ANIM,
                DISCRIM_SML_MASK_PMAP_ANIM,
            },
            {
                SPATHI_CAPTAIN_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
            SPATHI_VICTORY_SONG,
            SPATHI_SHIP_SOUNDS,
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
butt_missile_preprocess (ELEMENT *ElementPtr)
{
	if (ElementPtr->turn_wait > 0)
		--ElementPtr->turn_wait;
	else
	{
		COUNT facing;

		facing = GetFrameIndex (ElementPtr->next.image.frame);
		if (TrackShip (ElementPtr, &facing) > 0)
		{
			ElementPtr->next.image.frame =
					SetAbsFrameIndex (ElementPtr->next.image.frame,
					facing);
			ElementPtr->state_flags |= CHANGING;
	
			SetVelocityVector (&ElementPtr->velocity,
					discriminatorStats().speed, facing);
		}

		ElementPtr->turn_wait = TrackWait;
	}
}

static void
spawn_butt_missile (ELEMENT *ShipPtr)
{
	HELEMENT ButtMissile;
	STARSHIP *StarShipPtr;
	MISSILE_BLOCK ButtMissileBlock;

	GetElementStarShip (ShipPtr, &StarShipPtr);
	ButtMissileBlock.cx = ShipPtr->next.location.x;
	ButtMissileBlock.cy = ShipPtr->next.location.y;
	ButtMissileBlock.farray = StarShipPtr->RaceDescPtr->ship_data.special;
	ButtMissileBlock.face = ButtMissileBlock.index =
			NORMALIZE_FACING (StarShipPtr->ShipFacing
			+ ANGLE_TO_FACING (HALF_CIRCLE));
	ButtMissileBlock.sender = ShipPtr->playerNr;
	ButtMissileBlock.flags = 0;
	ButtMissileBlock.pixoffs = SpathiRearOffset;
	ButtMissileBlock.speed = discriminatorStats().speed;
	ButtMissileBlock.hit_points = discriminatorStats().hitPoints;
	ButtMissileBlock.damage = discriminatorStats().damage;
	ButtMissileBlock.life = discriminatorStats().lifetime;
	ButtMissileBlock.preprocess_func = butt_missile_preprocess;
	ButtMissileBlock.blast_offs = DiscriminatorOffset;
	ButtMissile = initialize_missile (&ButtMissileBlock);
	if (ButtMissile)
	{
		ELEMENT *ButtPtr;

		LockElement (ButtMissile, &ButtPtr);
		ButtPtr->turn_wait = TrackWait;
		SetElementStarShip (ButtPtr, StarShipPtr);

		ProcessSound (SetAbsSoundIndex (
					/* LAUNCH_BUTT_MISSILE */
				StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 1), ButtPtr);

		UnlockElement (ButtMissile);
		PutElement (ButtMissile);
	}
}

static void
spathi_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
		COUNT ConcernCounter)
{
	STARSHIP *StarShipPtr;
	EVALUATE_DESC *lpEvalDesc;

	ship_intelligence (ShipPtr, ObjectsOfConcern, ConcernCounter);

	GetElementStarShip (ShipPtr, &StarShipPtr);
	StarShipPtr->ship_input_state &= ~SPECIAL;

	lpEvalDesc = &ObjectsOfConcern[ENEMY_SHIP_INDEX];
	if (StarShipPtr->special_counter == 0
			&& lpEvalDesc->ObjectPtr
			&& lpEvalDesc->which_turn <= 24)
	{
		COUNT travel_facing, direction_facing;
		SIZE delta_x, delta_y;

		travel_facing = NORMALIZE_FACING (
				ANGLE_TO_FACING (GetVelocityTravelAngle (&ShipPtr->velocity)
				+ HALF_CIRCLE)
				);
		delta_x = lpEvalDesc->ObjectPtr->current.location.x
				- ShipPtr->current.location.x;
		delta_y = lpEvalDesc->ObjectPtr->current.location.y
				- ShipPtr->current.location.y;
		direction_facing = NORMALIZE_FACING (
				ANGLE_TO_FACING (ARCTAN (delta_x, delta_y))
				);

		if (NORMALIZE_FACING (direction_facing
				- (StarShipPtr->ShipFacing + ANGLE_TO_FACING (HALF_CIRCLE))
				+ ANGLE_TO_FACING (QUADRANT))
				<= ANGLE_TO_FACING (HALF_CIRCLE)
				&& (lpEvalDesc->which_turn <= 8
				|| NORMALIZE_FACING (direction_facing
				+ ANGLE_TO_FACING (HALF_CIRCLE)
				- ANGLE_TO_FACING (GetVelocityTravelAngle (
						&lpEvalDesc->ObjectPtr->velocity
						))
				+ ANGLE_TO_FACING (QUADRANT))
				<= ANGLE_TO_FACING (HALF_CIRCLE))
				&& (!(StarShipPtr->cur_status_flags &
				(SHIP_BEYOND_MAX_SPEED | SHIP_IN_GRAVITY_WELL))
				|| NORMALIZE_FACING (direction_facing
				- travel_facing + ANGLE_TO_FACING (QUADRANT))
				<= ANGLE_TO_FACING (HALF_CIRCLE)))
			StarShipPtr->ship_input_state |= SPECIAL;
	}
}

static COUNT
initialize_standard_missile (ELEMENT *ShipPtr, HELEMENT MissileArray[])
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
	MissileBlock.pixoffs = SpathiForwardOffset;
	MissileBlock.speed = missileStats().speed;
	MissileBlock.hit_points = missileStats().hitPoints;
	MissileBlock.damage = missileStats().damage;
	MissileBlock.life = missileStats().lifetime;
	MissileBlock.preprocess_func = NULL;
	MissileBlock.blast_offs = MissileOffset;
	MissileArray[0] = initialize_missile (&MissileBlock);

	return (1);
}

static void
spathi_postprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if ((StarShipPtr->cur_status_flags & SPECIAL)
			&& StarShipPtr->special_counter == 0
			&& DeltaEnergy (ElementPtr, -tuning().ship.specialEnergyCost))
	{
		spawn_butt_missile (ElementPtr);

		StarShipPtr->special_counter =
				StarShipPtr->RaceDescPtr->characteristics.special_wait;
	}
}

namespace {
class SpathiShip final : public uqm::combat::Ship {
public:
    explicit SpathiShip(const RACE_DESC &definition) noexcept
        : Ship(definition) {
        legacyData().cyborg_control.intelligence_func = spathi_intelligence;
    }
private:
    void afterMotion(ELEMENT &element) noexcept override {
        spathi_postprocess(&element);
    }
    COUNT createWeapons(ELEMENT &element, uqm::combat::WeaponSlots &weapons) noexcept override {
        return initialize_standard_missile(&element, weapons.data());
    }
};
} // namespace

std::unique_ptr<uqm::combat::Ship>
uqm::combat::makeSpathiShip()
{

	return std::make_unique<SpathiShip>(spathiDefinition());
}
