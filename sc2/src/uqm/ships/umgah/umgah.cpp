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
#include <optional>
#include "resinst.h"

#include "libs/mathlib.h"

// Balance values are immutable, loaded from base/ships.json at startup.
static const uqm::combat::ShipTuning &tuning() {
    static const auto &value = uqm::combat::shipTuning("umgah");
    return value;
}
static const uqm::combat::ProjectileStats &coneStats() {
    static const auto &value = tuning().projectile("cone");
    return value;
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto UmgahOffset = 0;
constexpr auto ConeOffset = 0;
constexpr auto JumpDist = DISPLAY_TO_WORLD (40);

static const RACE_DESC &umgahDefinition() {
    static const RACE_DESC definition = {
        { /* SHIP_INFO */
            FIRES_FORE | IMMEDIATE_WEAPON,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, tuning().ship.maxCrew,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            UMGAH_RACE_STRINGS,
            UMGAH_ICON_MASK_PMAP_ANIM,
            UMGAH_MICON_MASK_PMAP_ANIM,
            NULL, NULL, NULL
        },
        { /* FLEET_STUFF */
            833 / SPHERE_RADIUS_INCREMENT * 2, /* Initial SoI radius */
            { /* Known location (center of SoI) */
                1798, 6000,
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
                UMGAH_BIG_MASK_PMAP_ANIM,
                UMGAH_MED_MASK_PMAP_ANIM,
                UMGAH_SML_MASK_PMAP_ANIM,
            },
            {
                SPRITZ_MASK_PMAP_ANIM,
                NULL_RESOURCE,
                NULL_RESOURCE,
            },
            {
                CONE_BIG_MASK_ANIM,
                CONE_MED_MASK_ANIM,
                CONE_SML_MASK_ANIM,
            },
            {
                UMGAH_CAPTAIN_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
            UMGAH_VICTORY_SONG,
            UMGAH_SHIP_SOUNDS,
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            NULL, NULL
        },
        {
            0,
            uqm::combat::weaponRangeHint((LONG_RANGE_WEAPON << 2)),
            NULL,
        },

        0,
        0, /* ship_owner */
    };
    return definition;
}

// Private per-instance ship data
typedef struct
{
	UWORD prevFacing;
} UMGAH_DATA;

// Local typedef
typedef UMGAH_DATA CustomShipData_t;

// Retrieve race-specific ship data from a race desc
static CustomShipData_t *
GetCustomShipData (RACE_DESC *pRaceDesc)
{
	return static_cast<CustomShipData_t *>(pRaceDesc->data);
}

// Updates the concrete ship's optional value; no separate heap allocation.
static void SetCustomShipData(RACE_DESC *descriptor, const CustomShipData_t *data);

static void
cone_preprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	StarShipPtr->RaceDescPtr->ship_data.special[0] =
			SetRelFrameIndex (StarShipPtr->RaceDescPtr->ship_data.special[0],
			ANGLE_TO_FACING (FULL_CIRCLE));

	ElementPtr->state_flags |= APPEARING;
}

static void
cone_collision (ELEMENT *ElementPtr0, POINT *pPt0,
		ELEMENT *ElementPtr1, POINT *pPt1)
{
	HELEMENT hBlastElement;

	hBlastElement = weapon_collision (ElementPtr0, pPt0, ElementPtr1, pPt1);
	if (hBlastElement)
	{
		RemoveElement (hBlastElement);
		FreeElement (hBlastElement);

		ElementPtr0->state_flags &= ~DISAPPEARING;
	}
}

static void
umgah_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
		COUNT ConcernCounter)
{
	EVALUATE_DESC *lpEvalDesc;
	STARSHIP *StarShipPtr;
	STARSHIP *EnemyStarShipPtr;

	GetElementStarShip (ShipPtr, &StarShipPtr);
	lpEvalDesc = &ObjectsOfConcern[ENEMY_WEAPON_INDEX];
	if (lpEvalDesc->ObjectPtr && lpEvalDesc->MoveState == ENTICE)
	{
		if (lpEvalDesc->which_turn > 3
				|| (StarShipPtr->old_status_flags & SPECIAL))
			lpEvalDesc->ObjectPtr = 0;
		else if ((lpEvalDesc->ObjectPtr->state_flags & FINITE_LIFE)
				&& !(lpEvalDesc->ObjectPtr->state_flags & CREW_OBJECT))
			lpEvalDesc->MoveState = AVOID;
		else
			lpEvalDesc->MoveState = PURSUE;
	}

	lpEvalDesc = &ObjectsOfConcern[ENEMY_SHIP_INDEX];
	if (StarShipPtr->special_counter
			|| ObjectsOfConcern[GRAVITY_MASS_INDEX].ObjectPtr
			|| lpEvalDesc->ObjectPtr == 0)
	{
		StarShipPtr->RaceDescPtr->cyborg_control.WeaponRange = CLOSE_RANGE_WEAPON;
		ship_intelligence (ShipPtr, ObjectsOfConcern, ConcernCounter);

		if (lpEvalDesc->which_turn < 16)
			StarShipPtr->ship_input_state |= WEAPON;
		StarShipPtr->ship_input_state &= ~SPECIAL;
	}
	else
	{
		BYTE this_turn;
		SIZE delta_x, delta_y;
		BOOLEAN EnemyBehind, EnoughJuice;

		if (lpEvalDesc->which_turn >= 0xFF + 1)
			this_turn = 0xFF;
		else
			this_turn = (BYTE)lpEvalDesc->which_turn;

		EnoughJuice = (BOOLEAN)(WORLD_TO_TURN (
				JumpDist * StarShipPtr->RaceDescPtr->ship_info.energy_level
				/ tuning().ship.specialEnergyCost
				) > this_turn);
		delta_x = lpEvalDesc->ObjectPtr->next.location.x -
				ShipPtr->next.location.x;
		delta_y = lpEvalDesc->ObjectPtr->next.location.y -
				ShipPtr->next.location.y;
		EnemyBehind = (BOOLEAN)(NORMALIZE_ANGLE (
				ARCTAN (delta_x, delta_y)
				- (FACING_TO_ANGLE (StarShipPtr->ShipFacing)
				+ HALF_CIRCLE) + (OCTANT + (OCTANT >> 2))
				) <= ((OCTANT + (OCTANT >> 2)) << 1));
		
		GetElementStarShip (lpEvalDesc->ObjectPtr, &EnemyStarShipPtr);
		if (EnoughJuice
				&& ((StarShipPtr->old_status_flags & SPECIAL)
				|| EnemyBehind
				|| (this_turn > 6
				&& MANEUVERABILITY (
				&EnemyStarShipPtr->RaceDescPtr->cyborg_control
				) <= SLOW_SHIP)
				|| (this_turn >= 16 && this_turn <= 24)))
			StarShipPtr->RaceDescPtr->cyborg_control.WeaponRange = (LONG_RANGE_WEAPON << 3);
		else
			StarShipPtr->RaceDescPtr->cyborg_control.WeaponRange = CLOSE_RANGE_WEAPON;

		ship_intelligence (ShipPtr, ObjectsOfConcern, ConcernCounter);

		if (StarShipPtr->RaceDescPtr->cyborg_control.WeaponRange == CLOSE_RANGE_WEAPON)
			StarShipPtr->ship_input_state &= ~SPECIAL;
		else
		{
			BOOLEAN LinedUp;

			StarShipPtr->ship_input_state &= ~THRUST;
			LinedUp = (BOOLEAN)(ShipPtr->turn_wait == 0
					&& !(StarShipPtr->old_status_flags & (LEFT | RIGHT)));
			if (((StarShipPtr->old_status_flags & SPECIAL)
					&& this_turn <= StarShipPtr->RaceDescPtr->characteristics.special_wait)
					|| (!(StarShipPtr->old_status_flags & SPECIAL)
					&& EnemyBehind && (LinedUp || this_turn < 16)))
			{
				StarShipPtr->ship_input_state |= SPECIAL;
				StarShipPtr->RaceDescPtr->characteristics.special_wait = this_turn;

				/* don't want him backing straight into ship */
				if (this_turn <= 8 && LinedUp)
				{
					if (TFB_Random () & 1)
						StarShipPtr->ship_input_state |= LEFT;
					else
						StarShipPtr->ship_input_state |= RIGHT;
				}
			}
			else if (StarShipPtr->old_status_flags & SPECIAL)
			{
				StarShipPtr->ship_input_state &= ~(SPECIAL | LEFT | RIGHT);
				StarShipPtr->ship_input_state |= THRUST;
			}
		}

		if (this_turn < 16 && !EnemyBehind)
			StarShipPtr->ship_input_state |= WEAPON;
	}

	if (!(StarShipPtr->ship_input_state & SPECIAL))
		StarShipPtr->RaceDescPtr->characteristics.special_wait = 0xFF;
}

static COUNT
initialize_cone (ELEMENT *ShipPtr, HELEMENT ConeArray[])
{
	STARSHIP *StarShipPtr;
	UMGAH_DATA* UmgahData;
	MISSILE_BLOCK MissileBlock;

	GetElementStarShip (ShipPtr, &StarShipPtr);
	MissileBlock.cx = ShipPtr->next.location.x;
	MissileBlock.cy = ShipPtr->next.location.y;
	MissileBlock.farray = StarShipPtr->RaceDescPtr->ship_data.special;
	MissileBlock.face = StarShipPtr->ShipFacing;
	MissileBlock.sender = ShipPtr->playerNr;
	MissileBlock.flags = IGNORE_SIMILAR;
	MissileBlock.pixoffs = UmgahOffset;
	MissileBlock.speed = coneStats().speed;
	MissileBlock.hit_points = coneStats().hitPoints;
	MissileBlock.damage = coneStats().damage;
	MissileBlock.life = coneStats().lifetime;
	MissileBlock.preprocess_func = cone_preprocess;
	MissileBlock.blast_offs = ConeOffset;

	// This func is called every frame while the player is holding down WEAPON
	// Don't reset the cone FRAME to the first image every time
	UmgahData = GetCustomShipData (StarShipPtr->RaceDescPtr);
	if (!UmgahData || StarShipPtr->ShipFacing != UmgahData->prevFacing)
	{
		const UMGAH_DATA shipData = {StarShipPtr->ShipFacing};

		SetCustomShipData (StarShipPtr->RaceDescPtr, &shipData);

		StarShipPtr->RaceDescPtr->ship_data.special[0] =
				SetAbsFrameIndex (
				StarShipPtr->RaceDescPtr->ship_data.special[0],
				StarShipPtr->ShipFacing);
	}
	
	MissileBlock.index = GetFrameIndex (StarShipPtr->RaceDescPtr->ship_data.special[0]);
	ConeArray[0] = initialize_missile (&MissileBlock);

	if (ConeArray[0])
	{
		ELEMENT *ConePtr;

		LockElement (ConeArray[0], &ConePtr);
		ConePtr->collision_func = cone_collision;
		ConePtr->state_flags &= ~APPEARING;
		ConePtr->next = ConePtr->current;
		InitIntersectStartPoint (ConePtr);
		InitIntersectEndPoint (ConePtr);
		ConePtr->IntersectControl.IntersectStamp.frame =
				StarShipPtr->RaceDescPtr->ship_data.special[0];
		UnlockElement (ConeArray[0]);
	}

	return (1);
}

static void
umgah_postprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if (StarShipPtr->special_counter > 0)
	{
		StarShipPtr->special_counter = 0;

		ZeroVelocityComponents (&ElementPtr->velocity);
	}
}

static void
umgah_preprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	
	if (ElementPtr->state_flags & APPEARING)
	{
		// Reset the value just in case
		SetCustomShipData (StarShipPtr->RaceDescPtr, NULL);
	}
	else
	{
		if (ElementPtr->thrust_wait == 0
				&& (StarShipPtr->cur_status_flags & SPECIAL)
				&& DeltaEnergy (ElementPtr, -tuning().ship.specialEnergyCost))
		{
			COUNT facing;

			ProcessSound (SetAbsSoundIndex (
							/* ZIP_BACKWARDS */
					StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 1), ElementPtr);
			facing = FACING_TO_ANGLE (StarShipPtr->ShipFacing) + HALF_CIRCLE;
			DeltaVelocityComponents (&ElementPtr->velocity,
					COSINE (facing, WORLD_TO_VELOCITY (JumpDist)),
					SINE (facing, WORLD_TO_VELOCITY (JumpDist)));
			StarShipPtr->cur_status_flags &=
					~(SHIP_AT_MAX_SPEED | SHIP_BEYOND_MAX_SPEED);

			StarShipPtr->special_counter = tuning().ship.specialWait;
		}
	}
}

static void
uninit_umgah (RACE_DESC *pRaceDesc)
{
	SetCustomShipData (pRaceDesc, NULL);
}

namespace {
class UmgahShip final : public uqm::combat::Ship {
public:
    explicit UmgahShip(const RACE_DESC &definition) noexcept
        : Ship(definition) {
        legacyData().cyborg_control.intelligence_func = umgah_intelligence;
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
        umgah_preprocess(&element);
    }
    void afterMotion(ELEMENT &element) noexcept override {
        umgah_postprocess(&element);
    }
    COUNT createWeapons(ELEMENT &element, uqm::combat::WeaponSlots &weapons) noexcept override {
        return initialize_cone(&element, weapons.data());
    }
    void onFinish() noexcept override {
        uninit_umgah(&legacyData());
    }
    std::optional<CustomShipData_t> privateData_;
};
} // namespace

static void SetCustomShipData(RACE_DESC *descriptor, const CustomShipData_t *data)
{
    static_cast<UmgahShip &>(uqm::combat::shipFor(*descriptor)).setData(data);
}

std::unique_ptr<uqm::combat::Ship>
uqm::combat::makeUmgahShip()
{

	return std::make_unique<UmgahShip>(umgahDefinition());
}

