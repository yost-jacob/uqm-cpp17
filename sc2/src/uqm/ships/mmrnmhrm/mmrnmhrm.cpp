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

#include "../../beam.hpp"
#include "../ship.h"
#include "uqm/ship_behavior.hpp"
#include "uqm/ship_builders.hpp"
#include "uqm/ship_tuning.hpp"
#include "uqm/projectile_legacy.hpp"
#include <optional>
#include "resinst.h"

// Balance values are immutable, loaded from base/ships.json at startup.
static const uqm::combat::ShipTuning &tuning() {
    static const auto &value = uqm::combat::shipTuning("mmrnmhrm");
    return value;
}
static const uqm::combat::ProjectileStats &missileStats() {
    static const auto &value = tuning().projectile("missile");
    return value;
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto YwingEnergyRegeneration = 1;
static auto ywingSpecialEnergyCost() { return tuning().ship.maxEnergy; }
constexpr auto YwingEnergyWait = 6;
constexpr auto YwingMaxThrust = 50;
constexpr auto YwingThrustIncrement = 10;
constexpr auto YwingThrustWait = 0;
constexpr auto YwingTurnWait = 14;
constexpr auto MmrnmhrmOffset = 16;
constexpr auto CenterOffs = DISPLAY_TO_WORLD (4);
constexpr auto WingOffs = DISPLAY_TO_WORLD (10);
constexpr auto LaserRange = DISPLAY_TO_WORLD (125 + MmrnmhrmOffset);
constexpr auto YwingWeaponEnergyCost = 1;
constexpr auto YwingWeaponWait = 20;
constexpr auto LaunchOffs = DISPLAY_TO_WORLD (4);
constexpr auto MissileOffset = 0;
constexpr auto YwingSpecialWait = 0;

static const RACE_DESC &mmrnmhrmDefinition() {
    static const RACE_DESC definition = {
        { /* SHIP_INFO */
            FIRES_FORE | IMMEDIATE_WEAPON,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, tuning().ship.maxCrew,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            MMRNMHRM_RACE_STRINGS,
            MMRNMHRM_ICON_MASK_PMAP_ANIM,
            MMRNMHRM_MICON_MASK_PMAP_ANIM,
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
                MMRNMHRM_BIG_MASK_PMAP_ANIM,
                MMRNMHRM_MED_MASK_PMAP_ANIM,
                MMRNMHRM_SML_MASK_PMAP_ANIM,
            },
            {
                TORP_BIG_MASK_PMAP_ANIM,
                TORP_MED_MASK_PMAP_ANIM,
                TORP_SML_MASK_PMAP_ANIM,
            },
            {
                YWING_BIG_MASK_PMAP_ANIM,
                YWING_MED_MASK_PMAP_ANIM,
                YWING_SML_MASK_PMAP_ANIM,
            },
            {
                MMRNMHRM_CAPTAIN_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
            MMRNMHRM_VICTORY_SONG,
            MMRNMHRM_SHIP_SOUNDS,
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            NULL, NULL
        },
        {
            0,
            uqm::combat::weaponRangeHint(CLOSE_RANGE_WEAPON),
            NULL,
        },

        0,
        0, /* ship_owner */
    };
    return definition;
}

// Private per-instance ship data
typedef CHARACTERISTIC_STUFF MMRNMHRM_DATA;

// Local typedef
typedef MMRNMHRM_DATA CustomShipData_t;

// Retrieve race-specific ship data from a race desc
static CustomShipData_t *
GetCustomShipData (RACE_DESC *pRaceDesc)
{
	return static_cast<CustomShipData_t *>(pRaceDesc->data);
}

// Updates the concrete ship's optional value; no separate heap allocation.
static void SetCustomShipData(RACE_DESC *descriptor, const CustomShipData_t *data);

static void
mmrnmhrm_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
		COUNT ConcernCounter)
{
	BOOLEAN CanTransform;
	EVALUATE_DESC *lpEvalDesc;
	STARSHIP *StarShipPtr;
	STARSHIP *EnemyStarShipPtr = NULL;

	GetElementStarShip (ShipPtr, &StarShipPtr);
	CanTransform = (BOOLEAN)(StarShipPtr->special_counter == 0
			&& StarShipPtr->RaceDescPtr->ship_info.energy_level >=
			StarShipPtr->RaceDescPtr->characteristics.special_energy_cost);

	lpEvalDesc = &ObjectsOfConcern[ENEMY_SHIP_INDEX];
	if (lpEvalDesc->ObjectPtr)
	{
		GetElementStarShip (lpEvalDesc->ObjectPtr, &EnemyStarShipPtr);
	}

	ship_intelligence (ShipPtr, ObjectsOfConcern, ConcernCounter);

	StarShipPtr->ship_input_state &= ~SPECIAL;
	if (CanTransform
			&& lpEvalDesc->ObjectPtr
			&& !(StarShipPtr->ship_input_state & WEAPON))
	{
		SIZE delta_x, delta_y;
		COUNT travel_angle, direction_angle;

		GetCurrentVelocityComponents (&lpEvalDesc->ObjectPtr->velocity,
				&delta_x, &delta_y);
		if (delta_x == 0 && delta_y == 0)
			direction_angle = travel_angle = 0;
		else
		{
			delta_x = lpEvalDesc->ObjectPtr->current.location.x
					- ShipPtr->current.location.x;
			delta_y = lpEvalDesc->ObjectPtr->current.location.y
					- ShipPtr->current.location.y;
			direction_angle = ARCTAN (-delta_x, -delta_y);
			travel_angle = GetVelocityTravelAngle (
					&lpEvalDesc->ObjectPtr->velocity
					);
		}

		if (ShipPtr->next.image.farray == StarShipPtr->RaceDescPtr->ship_data.ship)
		{
			if (lpEvalDesc->which_turn > 8)
			{
				if (MANEUVERABILITY (&EnemyStarShipPtr->RaceDescPtr->cyborg_control) <= SLOW_SHIP
						|| NORMALIZE_ANGLE (
								direction_angle - travel_angle + QUADRANT
								) > HALF_CIRCLE)
					StarShipPtr->ship_input_state |= SPECIAL;
			}
		}
		else
		{
			SIZE ship_delta_x, ship_delta_y;

			GetCurrentVelocityComponents (&ShipPtr->velocity,
					&ship_delta_x, &ship_delta_y);
			delta_x -= ship_delta_x;
			delta_y -= ship_delta_y;
			travel_angle = ARCTAN (delta_x, delta_y);
			if (lpEvalDesc->which_turn < 16)
			{
				if (lpEvalDesc->which_turn <= 8
						|| NORMALIZE_ANGLE (
								direction_angle - travel_angle + OCTANT
								) <= QUADRANT)
					StarShipPtr->ship_input_state |= SPECIAL;
			}
			else if (lpEvalDesc->which_turn > 32
					&& NORMALIZE_ANGLE (
							direction_angle - travel_angle + QUADRANT
							) > HALF_CIRCLE)
				StarShipPtr->ship_input_state |= SPECIAL;
		}
	}

	if (ShipPtr->current.image.farray == StarShipPtr->RaceDescPtr->ship_data.special)
	{
		if (!(StarShipPtr->ship_input_state & SPECIAL)
				&& lpEvalDesc->ObjectPtr)
			StarShipPtr->ship_input_state |= WEAPON;
		else
			StarShipPtr->ship_input_state &= ~WEAPON;
	}
}

static void
twin_laser_collision (ELEMENT *ElementPtr0, POINT *pPt0,
		ELEMENT *ElementPtr1, POINT *pPt1)
{
	if (!(ElementPtr1->state_flags & PLAYER_SHIP)
			|| !elementsOfSamePlayer (ElementPtr0, ElementPtr1))
		weapon_collision (ElementPtr0, pPt0, ElementPtr1, pPt1);
}

static COUNT
initialize_dual_weapons (ELEMENT *ShipPtr, HELEMENT WeaponArray[])
{
	COORD cx, cy;
	COUNT facing, angle;
	SIZE offs_x, offs_y;
	STARSHIP *StarShipPtr;

	GetElementStarShip (ShipPtr, &StarShipPtr);
	facing = StarShipPtr->ShipFacing;
	angle = FACING_TO_ANGLE (facing);
	cx = ShipPtr->next.location.x + COSINE (angle, CenterOffs);
	cy = ShipPtr->next.location.y + SINE (angle, CenterOffs);

	if (ShipPtr->next.image.farray == StarShipPtr->RaceDescPtr->ship_data.ship)
	{
		COORD ex, ey;
		uqm::combat::BeamLaunch beam{};
		ELEMENT *LaserPtr;

		beam.player = ShipPtr->playerNr;
		beam.flags = 0;
		beam.muzzleOffset = 0;
		beam.color = BUILD_COLOR (MAKE_RGB15 (0x1F, 0x0A, 0x0A), 0x0C);
		beam.facing = facing;

		ex = cx + COSINE (angle, LaserRange);
		ey = cy + SINE (angle, LaserRange);
		offs_x = -SINE (angle, WingOffs);
		offs_y = COSINE (angle, WingOffs);

		beam.origin.x = cx + offs_x;
		beam.origin.y = cy + offs_y;
		beam.displacement.x = ex - beam.origin.x;
		beam.displacement.y = ey - beam.origin.y;
		if ((WeaponArray[0] = uqm::combat::publishBeam(beam)))
		{
			LockElement (WeaponArray[0], &LaserPtr);
			LaserPtr->collision_func = twin_laser_collision;
			UnlockElement (WeaponArray[0]);
		}

		beam.origin.x = cx - offs_x;
		beam.origin.y = cy - offs_y;
		beam.displacement.x = ex - beam.origin.x;
		beam.displacement.y = ey - beam.origin.y;
		if ((WeaponArray[1] = uqm::combat::publishBeam(beam)))
		{
			LockElement (WeaponArray[1], &LaserPtr);
			LaserPtr->collision_func = twin_laser_collision;
			UnlockElement (WeaponArray[1]);
		}
	}
	else
	{
		MISSILE_BLOCK TorpBlock;
		ELEMENT *TorpPtr;

		TorpBlock.farray = StarShipPtr->RaceDescPtr->ship_data.weapon;
		TorpBlock.sender = ShipPtr->playerNr;
		TorpBlock.flags = IGNORE_SIMILAR;
		TorpBlock.pixoffs = 0;
		TorpBlock.speed = missileStats().speed;
		TorpBlock.hit_points = missileStats().hitPoints;
		TorpBlock.damage = missileStats().damage;
		TorpBlock.life = missileStats().lifetime;
		TorpBlock.preprocess_func = nullptr;
		TorpBlock.blast_offs = MissileOffset;

		TorpBlock.face = TorpBlock.index = NORMALIZE_FACING (facing - 1);
		offs_x = -SINE (FACING_TO_ANGLE (TorpBlock.face), LaunchOffs);
		offs_y = COSINE (FACING_TO_ANGLE (TorpBlock.face), LaunchOffs);

		TorpBlock.cx = cx + offs_x;
		TorpBlock.cy = cy + offs_y;
		if ((WeaponArray[0] = uqm::combat::publishMissile(TorpBlock, missileStats().flight)))
		{
			LockElement (WeaponArray[0], &TorpPtr);

			UnlockElement (WeaponArray[0]);
		}

		TorpBlock.face = TorpBlock.index = NORMALIZE_FACING (facing + 1);

		TorpBlock.cx = cx - offs_x;
		TorpBlock.cy = cy - offs_y;
		if ((WeaponArray[1] = uqm::combat::publishMissile(TorpBlock, missileStats().flight)))
		{
			LockElement (WeaponArray[1], &TorpPtr);

			UnlockElement (WeaponArray[1]);
		}
	}

	return (2);
}

static void
mmrnmhrm_postprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
			/* take care of transform effect */
	if (ElementPtr->next.image.farray != ElementPtr->current.image.farray)
	{
		MMRNMHRM_DATA tempShipData;
		MMRNMHRM_DATA *otherwing_desc;

		ProcessSound (SetAbsSoundIndex (
						/* TRANSFORM */
				StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 1), ElementPtr);

		StarShipPtr->weapon_counter = 0;

		/* Swap characteristics descriptors around */
		otherwing_desc = GetCustomShipData (StarShipPtr->RaceDescPtr);
		if (!otherwing_desc)
			return;  // No ship data (?!)

		tempShipData = *otherwing_desc;
		SetCustomShipData (StarShipPtr->RaceDescPtr, &StarShipPtr->RaceDescPtr->characteristics);
		StarShipPtr->RaceDescPtr->characteristics = tempShipData;
		StarShipPtr->RaceDescPtr->cyborg_control.ManeuverabilityIndex = 0;

		if (ElementPtr->next.image.farray == StarShipPtr->RaceDescPtr->ship_data.special)
		{
			StarShipPtr->RaceDescPtr->cyborg_control.WeaponRange = LONG_RANGE_WEAPON - 1;
			StarShipPtr->RaceDescPtr->ship_info.ship_flags &= ~IMMEDIATE_WEAPON;
			StarShipPtr->RaceDescPtr->ship_info.ship_flags |= SEEKING_WEAPON;
			StarShipPtr->RaceDescPtr->ship_data.ship_sounds =
					SetAbsSoundIndex (StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 2);

			StarShipPtr->cur_status_flags &=
					~(SHIP_AT_MAX_SPEED | SHIP_BEYOND_MAX_SPEED);
		}
		else
		{
			StarShipPtr->RaceDescPtr->cyborg_control.WeaponRange = CLOSE_RANGE_WEAPON;
			StarShipPtr->RaceDescPtr->ship_info.ship_flags &= ~SEEKING_WEAPON;
			StarShipPtr->RaceDescPtr->ship_info.ship_flags |= IMMEDIATE_WEAPON;
			StarShipPtr->RaceDescPtr->ship_data.ship_sounds =
					SetAbsSoundIndex (StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 0);

			if (StarShipPtr->cur_status_flags
					& (SHIP_AT_MAX_SPEED | SHIP_BEYOND_MAX_SPEED))
				StarShipPtr->cur_status_flags |=
						SHIP_AT_MAX_SPEED | SHIP_BEYOND_MAX_SPEED;
		}
	}
}

static void
mmrnmhrm_preprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);

	if (!(ElementPtr->state_flags & APPEARING))
	{
		if ((StarShipPtr->cur_status_flags & SPECIAL)
				&& StarShipPtr->special_counter == 0)
		{
			/* Either we transform or text will flash */
			if (DeltaEnergy (ElementPtr,
					-StarShipPtr->RaceDescPtr->characteristics.special_energy_cost))
			{
				if (ElementPtr->next.image.farray == StarShipPtr->RaceDescPtr->ship_data.ship)
					ElementPtr->next.image.farray =
							StarShipPtr->RaceDescPtr->ship_data.special;
				else
					ElementPtr->next.image.farray =
							StarShipPtr->RaceDescPtr->ship_data.ship;
				ElementPtr->next.image.frame =
						SetEquFrameIndex (ElementPtr->next.image.farray[0],
						ElementPtr->next.image.frame);
				ElementPtr->state_flags |= CHANGING;
	
				StarShipPtr->special_counter =
						StarShipPtr->RaceDescPtr->characteristics.special_wait;
			}
		}
	}
}

static void
uninit_mmrnmhrm (RACE_DESC *pRaceDesc)
{
	SetCustomShipData (pRaceDesc, NULL);
}

namespace {
class MmrnmhrmShip final : public uqm::combat::Ship {
public:
    explicit MmrnmhrmShip(const RACE_DESC &definition) noexcept
        : Ship(definition) {
        legacyData().cyborg_control.intelligence_func = mmrnmhrm_intelligence;
        legacyData().data = nullptr;
        if (definition.data) setData(static_cast<const CustomShipData_t *>(definition.data));
    }
    void setData(const CustomShipData_t *data) noexcept {
        if (data == legacyData().data) return;
        if (data) {
            privateData_ = *data;
            legacyData().data = &*privateData_;
        } else {
            privateData_.reset();
            legacyData().data = nullptr;
        }
    }
private:
    void beforeMotion(ELEMENT &element) noexcept override {
        mmrnmhrm_preprocess(&element);
    }
    void afterMotion(ELEMENT &element) noexcept override {
        mmrnmhrm_postprocess(&element);
    }
    COUNT createWeapons(ELEMENT &element, uqm::combat::WeaponSlots &weapons) noexcept override {
        return initialize_dual_weapons(&element, weapons.data());
    }
    void onFinish() noexcept override {
        uninit_mmrnmhrm(&legacyData());
    }
    std::optional<CustomShipData_t> privateData_;
};
} // namespace

static void SetCustomShipData(RACE_DESC *descriptor, const CustomShipData_t *data)
{
    static_cast<MmrnmhrmShip &>(uqm::combat::shipFor(*descriptor)).setData(data);
}

std::unique_ptr<uqm::combat::Ship>
uqm::combat::makeMmrnmhrmShip()
{
	// Build a per-instance definition, copied into the native owner.
	RACE_DESC new_mmrnmhrm_desc;
	MMRNMHRM_DATA otherwing_desc;

	new_mmrnmhrm_desc = mmrnmhrmDefinition();

	otherwing_desc.max_thrust = YwingMaxThrust;
	otherwing_desc.thrust_increment = YwingThrustIncrement;
	otherwing_desc.energy_regeneration = YwingEnergyRegeneration;
	otherwing_desc.weapon_energy_cost = YwingWeaponEnergyCost;
	otherwing_desc.special_energy_cost = ywingSpecialEnergyCost();
	otherwing_desc.energy_wait = YwingEnergyWait;
	otherwing_desc.turn_wait = YwingTurnWait;
	otherwing_desc.thrust_wait = YwingThrustWait;
	otherwing_desc.weapon_wait = YwingWeaponWait;
	otherwing_desc.special_wait = YwingSpecialWait;
	otherwing_desc.ship_mass = tuning().ship.shipMass;

	new_mmrnmhrm_desc.data = &otherwing_desc;

	return std::make_unique<MmrnmhrmShip>(new_mmrnmhrm_desc);
}

