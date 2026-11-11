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

#include "uqm/globdata.h"

// Balance values are immutable, loaded from base/ships.json at startup.
static const uqm::combat::ShipTuning &tuning() {
    static const auto &value = uqm::combat::shipTuning("thradd");
    return value;
}
static const uqm::combat::ProjectileStats &missileStats() {
    static const auto &value = tuning().projectile("missile");
    return value;
}

static const uqm::combat::ProjectileStats &napalmStats() {
    static const auto &value = tuning().projectile("napalm");
    return value;
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto MissileOffset = 3;
constexpr auto ThraddashOffset = 9;
constexpr auto SpecialThrustIncrement = 12;
constexpr auto SpecialMaxThrust = 72;
constexpr auto NapalmOffset = 0;
constexpr auto NapalmDecayRate = 5;
constexpr auto NumNapalmFades = 6;
constexpr auto StationarySpeed = WORLD_TO_VELOCITY (DISPLAY_TO_WORLD (4));

// NapalmDecayRate is the number of ticks between afterburner decay animation frames.

static const RACE_DESC &thraddashDefinition() {
    static const RACE_DESC definition = {
        { /* SHIP_INFO */
            FIRES_FORE,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, tuning().ship.maxCrew,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            THRADDASH_RACE_STRINGS,
            THRADDASH_ICON_MASK_PMAP_ANIM,
            THRADDASH_MICON_MASK_PMAP_ANIM,
            NULL, NULL, NULL
        },
        { /* FLEET_STUFF */
            833 / SPHERE_RADIUS_INCREMENT * 2, /* Initial SoI radius */
            { /* Known location (center of SoI) */
                2535, 8358,
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
                THRADDASH_BIG_MASK_PMAP_ANIM,
                THRADDASH_MED_MASK_PMAP_ANIM,
                THRADDASH_SML_MASK_PMAP_ANIM,
            },
            {
                HORN_BIG_MASK_PMAP_ANIM,
                HORN_MED_MASK_PMAP_ANIM,
                HORN_SML_MASK_PMAP_ANIM,
            },
            {
                NAPALM_BIG_MASK_PMAP_ANIM,
                NAPALM_MED_MASK_PMAP_ANIM,
                NAPALM_SML_MASK_PMAP_ANIM,
            },
            {
                THRADDASH_CAPTAIN_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
            THRADDASH_VICTORY_SONG,
            THRADDASH_SHIP_SOUNDS,
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            NULL, NULL
        },
        {
            0,
            uqm::combat::weaponRangeHint((missileStats().speed * missileStats().lifetime) >> 1),
            NULL,
        },

        0,
        0, /* ship_owner */
    };
    return definition;
}

static void
thraddash_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
		COUNT ConcernCounter)
{

	STARSHIP *StarShipPtr;
	EVALUATE_DESC *lpEvalDesc;
	
	lpEvalDesc = &ObjectsOfConcern[ENEMY_SHIP_INDEX];
	if (lpEvalDesc->ObjectPtr)
	{
		SIZE dx, dy;

		GetCurrentVelocityComponents (
				&lpEvalDesc->ObjectPtr->velocity, &dx, &dy
				);
		if (lpEvalDesc->which_turn > 8
				|| (long)dx * dx + (long)dy * dy <=
				(long)StationarySpeed * StationarySpeed)
			lpEvalDesc->MoveState = PURSUE;
		else
			lpEvalDesc->MoveState = ENTICE;
	}
	ship_intelligence (ShipPtr, ObjectsOfConcern, ConcernCounter);

	GetElementStarShip (ShipPtr, &StarShipPtr);
	if (StarShipPtr->special_counter == 0)
	{
		StarShipPtr->ship_input_state &= ~SPECIAL;
		if (ObjectsOfConcern[ENEMY_WEAPON_INDEX].ObjectPtr
				&& ObjectsOfConcern[ENEMY_WEAPON_INDEX].MoveState == ENTICE)
		{
			if ((StarShipPtr->ship_input_state & THRUST)
					|| (ShipPtr->turn_wait == 0
					&& !(StarShipPtr->ship_input_state & (LEFT | RIGHT)))
					|| NORMALIZE_FACING (ANGLE_TO_FACING (
					GetVelocityTravelAngle (
					&ObjectsOfConcern[ENEMY_WEAPON_INDEX].ObjectPtr->velocity
					) + HALF_CIRCLE + OCTANT)
					- StarShipPtr->ShipFacing) > ANGLE_TO_FACING (QUADRANT))
				StarShipPtr->ship_input_state |= SPECIAL;
		}
		else if (lpEvalDesc->ObjectPtr)
		{
			if (lpEvalDesc->MoveState == PURSUE)
			{
				if (StarShipPtr->RaceDescPtr->ship_info.energy_level >= tuning().ship.weaponEnergyCost
						+ tuning().ship.specialEnergyCost
						&& ShipPtr->turn_wait == 0
						&& !(StarShipPtr->ship_input_state & (LEFT | RIGHT))
						&& (!(StarShipPtr->cur_status_flags & SPECIAL)
						|| !(StarShipPtr->cur_status_flags
						& (SHIP_AT_MAX_SPEED | SHIP_BEYOND_MAX_SPEED))))
					StarShipPtr->ship_input_state |= SPECIAL;
			}
			else if (lpEvalDesc->MoveState == ENTICE)
			{
				COUNT direction_angle;
				SIZE delta_x, delta_y;

				delta_x = lpEvalDesc->ObjectPtr->next.location.x
						- ShipPtr->next.location.x;
				delta_y = lpEvalDesc->ObjectPtr->next.location.y
						- ShipPtr->next.location.y;
				direction_angle = ARCTAN (delta_x, delta_y);

				if ((lpEvalDesc->which_turn > 24
						&& !(StarShipPtr->ship_input_state & (LEFT | RIGHT)))
						|| (lpEvalDesc->which_turn <= 16
						&& NORMALIZE_ANGLE (direction_angle
						- (FACING_TO_ANGLE (StarShipPtr->ShipFacing) + HALF_CIRCLE)
						+ QUADRANT) <= HALF_CIRCLE
						&& (lpEvalDesc->which_turn < 12
						|| NORMALIZE_ANGLE (direction_angle
						- (GetVelocityTravelAngle (
								&lpEvalDesc->ObjectPtr->velocity
								) + HALF_CIRCLE)
						+ (OCTANT + 2)) <= ((OCTANT + 2) << 1))))
					StarShipPtr->ship_input_state |= SPECIAL;
			}
		}

		if ((StarShipPtr->ship_input_state & SPECIAL)
				&& StarShipPtr->RaceDescPtr->ship_info.energy_level >=
				tuning().ship.specialEnergyCost)
			StarShipPtr->ship_input_state &= ~THRUST;
	}
}

// Per-shot behavior; engine kinematics and collision state remain borrowed.
namespace {
class ThraddashNapalm final : public uqm::combat::Projectile {
public:
    using Projectile::Projectile;
    void update(ELEMENT &element) override {
        auto *ElementPtr = &element;

	ZeroVelocityComponents (&ElementPtr->velocity);

	if (ElementPtr->state_flags & NONSOLID)
	{
		ElementPtr->state_flags &= ~NONSOLID;
		ElementPtr->state_flags |= APPEARING;
		SetPrimType (&(GLOBAL (DisplayArray))[ElementPtr->PrimIndex],
				STAMP_PRIM);

		InitIntersectStartPoint (ElementPtr);
		InitIntersectEndPoint (ElementPtr);
		InitIntersectFrame (ElementPtr);
	}
	/* turn_wait is abused here to store the speed of the decay animation */
	else if (ElementPtr->turn_wait > 0)
		--ElementPtr->turn_wait;
	else
	{
		if (ElementPtr->life_span <= NumNapalmFades * (NapalmDecayRate + 1)
				|| GetFrameIndex (
				ElementPtr->current.image.frame
				) != NumNapalmFades)
			ElementPtr->next.image.frame =
					DecFrameIndex (ElementPtr->current.image.frame);
		else if (ElementPtr->life_span > NumNapalmFades * (NapalmDecayRate + 1))
			ElementPtr->next.image.frame = SetAbsFrameIndex (
					ElementPtr->current.image.frame,
					GetFrameCount (ElementPtr->current.image.frame) - 1
					);

		/* turn_wait is abused here to store the speed of the decay
		 * animation. */
		ElementPtr->turn_wait = NapalmDecayRate;
		ElementPtr->state_flags |= CHANGING;
	}
    }
};
}


static COUNT
initialize_horn (ELEMENT *ShipPtr, HELEMENT HornArray[])
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
	MissileBlock.pixoffs = ThraddashOffset;
	MissileBlock.speed = missileStats().speed;
	MissileBlock.hit_points = missileStats().hitPoints;
	MissileBlock.damage = missileStats().damage;
	MissileBlock.life = missileStats().lifetime;
	MissileBlock.preprocess_func = NULL;
	MissileBlock.blast_offs = MissileOffset;
	HornArray[0] = uqm::combat::publishMissile(MissileBlock, missileStats().flight);

	return (1);
}

static void
thraddash_preprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if (!(StarShipPtr->cur_status_flags & SPECIAL))
	{
		if ((StarShipPtr->old_status_flags & SPECIAL)
				&& (StarShipPtr->cur_status_flags & SHIP_AT_MAX_SPEED))
			StarShipPtr->cur_status_flags |= SHIP_BEYOND_MAX_SPEED;
	}
	else if (DeltaEnergy (ElementPtr, -tuning().ship.specialEnergyCost))
	{
		COUNT max_thrust, thrust_increment;
		STATUS_FLAGS thrust_status;
		HELEMENT hTrailElement;

		if (!(StarShipPtr->old_status_flags & SPECIAL))
			StarShipPtr->cur_status_flags &=
					~(SHIP_AT_MAX_SPEED | SHIP_BEYOND_MAX_SPEED);

		if (ElementPtr->thrust_wait == 0)
			++ElementPtr->thrust_wait;

		thrust_increment =
				StarShipPtr->RaceDescPtr->characteristics.thrust_increment;
		max_thrust = StarShipPtr->RaceDescPtr->characteristics.max_thrust;
		StarShipPtr->RaceDescPtr->characteristics.thrust_increment =
				SpecialThrustIncrement;
		StarShipPtr->RaceDescPtr->characteristics.max_thrust =
				SpecialMaxThrust;

		thrust_status = inertial_thrust (ElementPtr);
		StarShipPtr->cur_status_flags &=
				~(SHIP_AT_MAX_SPEED
				| SHIP_BEYOND_MAX_SPEED
				| SHIP_IN_GRAVITY_WELL);
		StarShipPtr->cur_status_flags |= thrust_status;

		StarShipPtr->RaceDescPtr->characteristics.thrust_increment =
				thrust_increment;
		StarShipPtr->RaceDescPtr->characteristics.max_thrust = max_thrust;

		{
			MISSILE_BLOCK MissileBlock;

			MissileBlock.cx = ElementPtr->next.location.x;
			MissileBlock.cy = ElementPtr->next.location.y;
			MissileBlock.farray = StarShipPtr->RaceDescPtr->ship_data.special;
			MissileBlock.face = 0;
			MissileBlock.index = GetFrameCount (
					StarShipPtr->RaceDescPtr->ship_data.special[0]
					) - 1;
			MissileBlock.sender = ElementPtr->playerNr;
			MissileBlock.flags = IGNORE_SIMILAR;
			MissileBlock.pixoffs = 0;
			MissileBlock.speed = napalmStats().speed;
			MissileBlock.hit_points = napalmStats().hitPoints;
			MissileBlock.damage = napalmStats().damage;
			MissileBlock.life = napalmStats().lifetime;
			MissileBlock.preprocess_func = nullptr;
			MissileBlock.blast_offs = NapalmOffset;

			hTrailElement = uqm::combat::publishMissile<ThraddashNapalm>(MissileBlock);
			if (hTrailElement)
			{
				ELEMENT *TrailElementPtr;

				LockElement (hTrailElement, &TrailElementPtr);
				SetElementStarShip (TrailElementPtr, StarShipPtr);
				TrailElementPtr->hTarget = 0;

				/* turn_wait is abused here to store the speed of the decay
				 * animation */
				TrailElementPtr->turn_wait = NapalmDecayRate;

				TrailElementPtr->state_flags |= NONSOLID;
				SetPrimType (
						&(GLOBAL (DisplayArray))[TrailElementPtr->PrimIndex],
						NO_PRIM
						);

						/* normally done during preprocess, but because
						 * object is being inserted at head rather than
						 * appended after tail it may never get preprocessed.
						 */
				TrailElementPtr->next = TrailElementPtr->current;
				TrailElementPtr->state_flags |= PRE_PROCESS;

				UnlockElement (hTrailElement);
				InsertElement (hTrailElement, GetHeadElement ());

				ProcessSound (SetAbsSoundIndex (
						StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 1), ElementPtr);
			}
		}
	}
}

namespace {
class ThraddashShip final : public uqm::combat::Ship {
public:
    explicit ThraddashShip(const RACE_DESC &definition) noexcept
        : Ship(definition) {
        legacyData().cyborg_control.intelligence_func = thraddash_intelligence;
    }
private:
    void beforeMotion(ELEMENT &element) noexcept override {
        thraddash_preprocess(&element);
    }
    COUNT createWeapons(ELEMENT &element, uqm::combat::WeaponSlots &weapons) noexcept override {
        return initialize_horn(&element, weapons.data());
    }
};
} // namespace

std::unique_ptr<uqm::combat::Ship>
uqm::combat::makeThraddashShip()
{

	return std::make_unique<ThraddashShip>(thraddashDefinition());
}
