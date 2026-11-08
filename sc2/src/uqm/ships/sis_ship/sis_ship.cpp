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

#include "uqm/colors.h"
#include "uqm/controls.h"
#include "uqm/globdata.h"
#include "uqm/hyper.h"
#include "libs/mathlib.h"


// Balance values are immutable, loaded from base/ships.json at startup.
static const uqm::combat::ShipTuning &tuning() {
    static const auto &value = uqm::combat::shipTuning("sis_ship");
    return value;
}
static const uqm::combat::ProjectileStats &blasterStats() {
    static const auto &value = tuning().projectile("blaster");
    return value;
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto ShivaEnergyRegenInc = 1;
constexpr auto MinEnergyWait = 4;
constexpr auto DynamoUnitEnergyWaitDec = 2;
constexpr auto MaxTracking = 3;
constexpr auto TrackerEnergyCost = 3;
constexpr auto BlasterOffset = 8;
constexpr auto SisVertOffset = 28;
constexpr auto SisHorzOffset = 20;
constexpr auto AntimissileEnergyInc = 1;
constexpr auto LaserRange = (UWORD)100;
constexpr auto MaxDefense = 8;
constexpr auto MenuDelay = 10;

// Flagship JSON values are base values; installed modules derive final characteristics.
// Crew pods determine capacity (including the captain). Furnaces/dynamos adjust
// regeneration and delay; thrusters/jets adjust movement. Weapon modules multiply
// base blaster damage/hit points and extend lifetime. Point defense adds energy cost.
// SisVertOffset is used by forward/spread/rear guns; SisHorzOffset by side guns.

static const RACE_DESC &sisDefinition() {
    static const RACE_DESC definition = {
        { /* SHIP_INFO */
            0,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, tuning().ship.maxCrew,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            NULL_RESOURCE,
            SIS_ICON_MASK_PMAP_ANIM,
            NULL_RESOURCE,
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
                SIS_BIG_MASK_PMAP_ANIM,
                SIS_MED_MASK_PMAP_ANIM,
                SIS_SML_MASK_PMAP_ANIM,
            },
            {
                BLASTER_BIG_MASK_PMAP_ANIM,
                BLASTER_MED_MASK_PMAP_ANIM,
                BLASTER_SML_MASK_PMAP_ANIM,
            },
            {
                NULL_RESOURCE,
                NULL_RESOURCE,
                NULL_RESOURCE,
            },
            {
                SIS_CAPTAIN_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
            SIS_VICTORY_SONG,
            SIS_SHIP_SOUNDS,
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            NULL, NULL
        },
        {
            0,
            uqm::combat::weaponRangeHint(blasterStats().speed * blasterStats().lifetime),
            NULL,
        },

        0,
        0, /* ship_owner */
    };
    return definition;
}

// Private per-instance SIS data
typedef struct
{
	COUNT num_trackers;
	COUNT num_blasters;
	MISSILE_BLOCK MissileBlock[6];

} SIS_DATA;

static void InitWeaponSlots (RACE_DESC *RaceDescPtr,
		const BYTE *ModuleSlots);
static void InitModuleSlots (RACE_DESC *RaceDescPtr,
		const BYTE *ModuleSlots);
static void InitDriveSlots (RACE_DESC *RaceDescPtr,
		const BYTE *DriveSlots);
static void InitJetSlots (RACE_DESC *RaceDescPtr,
		const BYTE *JetSlots);
static void uninit_sis (RACE_DESC *pRaceDesc);

// Local typedef
typedef SIS_DATA CustomShipData_t;

// Retrieve race-specific ship data from a race desc
static CustomShipData_t *
GetCustomShipData (RACE_DESC *pRaceDesc)
{
	return static_cast<CustomShipData_t *>(pRaceDesc->data);
}

// Updates the concrete ship's optional value; no separate heap allocation.
static void SetCustomShipData(RACE_DESC *descriptor, const CustomShipData_t *data);

static void
sis_hyper_preprocess (ELEMENT *ElementPtr)
{
	SIZE udx = 0, udy = 0;
	SIZE dx = 0, dy = 0;
	SIZE AccelerateDirection;
	STARSHIP *StarShipPtr;

	if (ElementPtr->state_flags & APPEARING)
		ElementPtr->velocity = GLOBAL (velocity);

	AccelerateDirection = 0;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	++StarShipPtr->weapon_counter; /* no shooting in hyperspace! */
	if ((GLOBAL (autopilot)).x == ~0
			|| (GLOBAL (autopilot)).y == ~0
			|| (StarShipPtr->cur_status_flags & (LEFT | RIGHT | THRUST)))
	{
LeaveAutoPilot:
		(GLOBAL (autopilot)).x =
				(GLOBAL (autopilot)).y = ~0;
		if (!(StarShipPtr->cur_status_flags & THRUST)
				|| (GLOBAL_SIS (FuelOnBoard) == 0
				&& GET_GAME_STATE (ARILOU_SPACE_SIDE) <= 1))
		{
			AccelerateDirection = -1;
			GetCurrentVelocityComponents (&ElementPtr->velocity,
					&dx, &dy);
			udx = dx << 4;
			udy = dy << 4;

			StarShipPtr->cur_status_flags &= ~THRUST;
		}
	}
	else
	{
		SIZE facing;
		POINT universe;

		universe.x = LOGX_TO_UNIVERSE (GLOBAL_SIS (log_x));
		universe.y = LOGY_TO_UNIVERSE (GLOBAL_SIS (log_y));
		udx = (GLOBAL (autopilot)).x - universe.x;
		udy = -((GLOBAL (autopilot)).y - universe.y);
		if ((dx = udx) < 0)
			dx = -dx;
		if ((dy = udy) < 0)
			dy = -dy;
		if (dx <= 1 && dy <= 1)
			goto LeaveAutoPilot;

		facing = NORMALIZE_FACING (ANGLE_TO_FACING (ARCTAN (udx, udy)));

		/* This prevents ship from flying backwards on auto-pilot.
		 * It could also theoretically abort autopilot in a bad savegame */
		if ((StarShipPtr->cur_status_flags & SHIP_AT_MAX_SPEED)
				/*|| (ElementPtr->state_flags & APPEARING)*/ )
		{
			if (NORMALIZE_FACING (StarShipPtr->ShipFacing
					+ ANGLE_TO_FACING (QUADRANT)
					- facing) > ANGLE_TO_FACING (HALF_CIRCLE))
				goto LeaveAutoPilot;

			facing = StarShipPtr->ShipFacing;
		}
		else if ((int)facing != (int)StarShipPtr->ShipFacing
				&& ElementPtr->turn_wait == 0)
		{
			if (NORMALIZE_FACING (
					StarShipPtr->ShipFacing - facing
					) >= ANGLE_TO_FACING (HALF_CIRCLE))
			{
				facing = NORMALIZE_FACING (facing - 1);
				StarShipPtr->cur_status_flags |= RIGHT;
			}
			else if ((int)StarShipPtr->ShipFacing != (int)facing)
			{
				facing = NORMALIZE_FACING (facing + 1);
				StarShipPtr->cur_status_flags |= LEFT;
			}

			if ((int)facing == (int)StarShipPtr->ShipFacing)
			{
				ZeroVelocityComponents (&ElementPtr->velocity);
			}
		}

		GetCurrentVelocityComponents (&ElementPtr->velocity, &dx, &dy);
		if ((GLOBAL_SIS (FuelOnBoard)
				|| GET_GAME_STATE (ARILOU_SPACE_SIDE) > 1)
				&& (int)facing == (int)StarShipPtr->ShipFacing)
		{
			StarShipPtr->cur_status_flags |= SHIP_AT_MAX_SPEED;
			AccelerateDirection = 1;
		}
		else
		{
			AccelerateDirection = -1;
			udx = dx << 4;
			udy = dy << 4;
		}
	}

	if (ElementPtr->thrust_wait == 0 && AccelerateDirection)
	{
		COUNT dist;
		SIZE speed, velocity_increment;

		velocity_increment = WORLD_TO_VELOCITY (
				StarShipPtr->RaceDescPtr->characteristics.thrust_increment);

		if ((dist = square_root ((long)udx * udx + (long)udy * udy)) == 0)
			dist = 1; /* prevent divide by zero */

		speed = square_root ((long)dx * dx + (long)dy * dy);
		if (AccelerateDirection < 0)
		{
			dy = (speed / velocity_increment - 1) * velocity_increment;
			if (dy < speed - velocity_increment)
				dy = speed - velocity_increment;
			if ((speed = dy) < 0)
				speed = 0;

			StarShipPtr->cur_status_flags &= ~SHIP_AT_MAX_SPEED;
		}
		else
		{
			SIZE max_velocity;

			AccelerateDirection = 0;

			max_velocity = WORLD_TO_VELOCITY (
					StarShipPtr->RaceDescPtr->characteristics.max_thrust);

			dy = (speed / velocity_increment + 1)
					* velocity_increment;
			if (dy < speed + velocity_increment)
				dy = speed + velocity_increment;
			if ((speed = dy) > max_velocity)
			{
				speed = max_velocity;
				StarShipPtr->cur_status_flags |= SHIP_AT_MAX_SPEED;
			}
		}

		dx = (SIZE)((long)udx * speed / (long)dist);
		dy = (SIZE)((long)udy * speed / (long)dist);
		SetVelocityComponents (&ElementPtr->velocity, dx, dy);

		ElementPtr->thrust_wait =
				StarShipPtr->RaceDescPtr->characteristics.thrust_wait;
	}
}

static void
sis_hyper_postprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GLOBAL (velocity) = ElementPtr->velocity;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if (((StarShipPtr->cur_status_flags & WEAPON) ||
			PulsedInputState.menu[KEY_MENU_CANCEL])
			&& StarShipPtr->special_counter == 0)
	{
		HyperspaceMenu ();
		StarShipPtr->cur_status_flags &= ~SHIP_AT_MAX_SPEED;
		StarShipPtr->special_counter = MenuDelay;
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
		Color LaserColor;
		static const Color ColorRange[] =
		{
			BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x03, 0x00), 0x7F),
			BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x07, 0x00), 0x7E),
			BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x0A, 0x00), 0x7D),
			BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x0E, 0x00), 0x7C),
			BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x11, 0x00), 0x7B),
			BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x15, 0x00), 0x7A),
			BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x18, 0x00), 0x79),
			BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x1C, 0x00), 0x78),
		};

		PaidFor = FALSE;

		LaserColor = ColorRange[
				StarShipPtr->RaceDescPtr->characteristics.special_energy_cost
				];
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
						if (!DeltaEnergy (ShipPtr,
								-(StarShipPtr->RaceDescPtr->characteristics.special_energy_cost
								<< 2)))
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
					LaserBlock.color = LaserColor;
					hPointDefense = initialize_laser (&LaserBlock);
					if (hPointDefense)
					{
						ELEMENT *PDPtr;

						LockElement (hPointDefense, &PDPtr);
						PDPtr->mass_points =
								StarShipPtr->RaceDescPtr->characteristics.special_energy_cost;
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

static void
sis_battle_preprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if (StarShipPtr->RaceDescPtr->characteristics.special_energy_cost == 0)
	{
		StarShipPtr->cur_status_flags &= ~SPECIAL;
		StarShipPtr->special_counter = 2;
	}
	if (!(StarShipPtr->RaceDescPtr->ship_info.ship_flags
			& (FIRES_FORE | FIRES_RIGHT | FIRES_AFT | FIRES_LEFT)))
	{
		StarShipPtr->cur_status_flags &= ~WEAPON;
		StarShipPtr->weapon_counter = 2;
	}
}

static void
sis_battle_postprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if ((StarShipPtr->cur_status_flags & SPECIAL)
			&& StarShipPtr->special_counter == 0
			&& StarShipPtr->RaceDescPtr->characteristics.special_energy_cost)
	{
		spawn_point_defense (ElementPtr);
	}
}

// Legacy sprites identify the gun tier through its damage multiple. Keep that
// relationship explicit while allowing the base damage to come from tuning.
static unsigned blasterTier(BYTE damage) noexcept {
    const auto base = blasterStats().damage; // Validated positive for the flagship.
    return damage % base == 0 ? damage / base : 0;
}

static void
blaster_collision (ELEMENT *ElementPtr0, POINT *pPt0,
		ELEMENT *ElementPtr1, POINT *pPt1)
{
	HELEMENT hBlastElement;

	hBlastElement = weapon_collision (ElementPtr0, pPt0, ElementPtr1, pPt1);
	if (hBlastElement)
	{
		ELEMENT *BlastElementPtr;

		LockElement (hBlastElement, &BlastElementPtr);
		switch (blasterTier(ElementPtr0->mass_points))
		{
			case 1:
				BlastElementPtr->life_span = 2;
				BlastElementPtr->current.image.frame =
						SetAbsFrameIndex (ElementPtr0->current.image.frame, 0);
				BlastElementPtr->preprocess_func = NULL;
				break;
			case 2:
				BlastElementPtr->life_span = 6;
				BlastElementPtr->current.image.frame =
						IncFrameIndex (ElementPtr0->current.image.frame);
				break;
			case 3:
				BlastElementPtr->life_span = 7;
				BlastElementPtr->current.image.frame =
						SetAbsFrameIndex (ElementPtr0->current.image.frame, 20);
				break;
		}
		UnlockElement (hBlastElement);
	}
}

static void
blaster_preprocess (ELEMENT *ElementPtr)
{
	BYTE wait;

	switch (blasterTier(ElementPtr->mass_points))
	{
		case 1:
			if (GetFrameIndex (ElementPtr->current.image.frame) < 8)
			{
				ElementPtr->next.image.frame =
						IncFrameIndex (ElementPtr->current.image.frame);
				ElementPtr->state_flags |= CHANGING;
			}
			break;
		case 3:
			if (GetFrameIndex (ElementPtr->current.image.frame) < 19)
				ElementPtr->next.image.frame =
						IncFrameIndex (ElementPtr->current.image.frame);
			else
				ElementPtr->next.image.frame =
						SetAbsFrameIndex (ElementPtr->current.image.frame, 16);
			ElementPtr->state_flags |= CHANGING;
			break;
	}

	if (LONIBBLE (ElementPtr->turn_wait))
		--ElementPtr->turn_wait;
	else if ((wait = HINIBBLE (ElementPtr->turn_wait)))
	{
		COUNT facing;

		facing = NORMALIZE_FACING (ANGLE_TO_FACING (
				GetVelocityTravelAngle (&ElementPtr->velocity)));
		if (TrackShip (ElementPtr, &facing) > 0)
			SetVelocityVector (&ElementPtr->velocity, blasterStats().speed, facing);

		ElementPtr->turn_wait = MAKE_BYTE (wait, wait);
	}
}

static COUNT
initialize_blasters (ELEMENT *ShipPtr, HELEMENT BlasterArray[])
{
	BYTE nt;
	COUNT i;
	STARSHIP *StarShipPtr;
	SIS_DATA *SisData;

	GetElementStarShip (ShipPtr, &StarShipPtr);
	SisData = GetCustomShipData (StarShipPtr->RaceDescPtr);

	nt = (BYTE)((4 - SisData->num_trackers) & 3);

	for (i = 0; i < SisData->num_blasters; ++i)
	{
		MISSILE_BLOCK MissileBlock = SisData->MissileBlock[i];

		MissileBlock.cx = ShipPtr->next.location.x;
		MissileBlock.cy = ShipPtr->next.location.y;
		MissileBlock.farray = StarShipPtr->RaceDescPtr->ship_data.weapon;
		MissileBlock.sender = ShipPtr->playerNr;
		MissileBlock.face = NORMALIZE_FACING (StarShipPtr->ShipFacing
				+ MissileBlock.face);

		BlasterArray[i] = initialize_missile (&MissileBlock);
		if (BlasterArray[i])
		{
			ELEMENT *BlasterPtr;

			LockElement (BlasterArray[i], &BlasterPtr);
			BlasterPtr->collision_func = blaster_collision;
			BlasterPtr->turn_wait = MAKE_BYTE (nt, nt);
			UnlockElement (BlasterArray[i]);
		}
	
	}

	return SisData->num_blasters;
}

static void
sis_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
		COUNT ConcernCounter)
{
	EVALUATE_DESC *lpEvalDesc;
	STARSHIP *StarShipPtr;
	SIS_DATA *SisData;

	GetElementStarShip (ShipPtr, &StarShipPtr);
	SisData = GetCustomShipData (StarShipPtr->RaceDescPtr);

	lpEvalDesc = &ObjectsOfConcern[ENEMY_WEAPON_INDEX];
	if (lpEvalDesc->ObjectPtr)
	{
		if (StarShipPtr->RaceDescPtr->characteristics.special_energy_cost)
		{
			if (StarShipPtr->special_counter == 0
					&& ((lpEvalDesc->ObjectPtr
					&& lpEvalDesc->which_turn <= 2)
					|| (ObjectsOfConcern[ENEMY_SHIP_INDEX].ObjectPtr != NULL
					&& ObjectsOfConcern[ENEMY_SHIP_INDEX].which_turn <= 4)))
				StarShipPtr->ship_input_state |= SPECIAL;
			else
				StarShipPtr->ship_input_state &= ~SPECIAL;
			lpEvalDesc->ObjectPtr = NULL;
		}
		else if (MANEUVERABILITY (&StarShipPtr->RaceDescPtr->cyborg_control)
				< MEDIUM_SHIP
				&& lpEvalDesc->MoveState == ENTICE
				&& (!(lpEvalDesc->ObjectPtr->state_flags & CREW_OBJECT)
				|| lpEvalDesc->which_turn <= 8)
				&& (!(lpEvalDesc->ObjectPtr->state_flags & FINITE_LIFE)
				|| (lpEvalDesc->ObjectPtr->mass_points >= 4
				&& lpEvalDesc->which_turn == 2
				&& ObjectsOfConcern[ENEMY_SHIP_INDEX].which_turn > 16)))
			lpEvalDesc->MoveState = PURSUE;
	}

	ship_intelligence (ShipPtr, ObjectsOfConcern, ConcernCounter);

	lpEvalDesc = &ObjectsOfConcern[ENEMY_SHIP_INDEX];
	if (SisData->num_trackers
			&& StarShipPtr->weapon_counter == 0
			&& !(StarShipPtr->ship_input_state & WEAPON)
			&& lpEvalDesc->ObjectPtr
			&& lpEvalDesc->which_turn <= 16)
	{
		COUNT direction_facing;
		SIZE delta_x, delta_y;
		UWORD fire_flags, ship_flags;
		COUNT facing;

		delta_x = lpEvalDesc->ObjectPtr->current.location.x
				- ShipPtr->current.location.x;
		delta_y = lpEvalDesc->ObjectPtr->current.location.y
				- ShipPtr->current.location.y;
		direction_facing = NORMALIZE_FACING (
				ANGLE_TO_FACING (ARCTAN (delta_x, delta_y)));

		ship_flags = StarShipPtr->RaceDescPtr->ship_info.ship_flags;
		for (fire_flags = FIRES_FORE, facing = StarShipPtr->ShipFacing;
				fire_flags <= FIRES_LEFT;
				fire_flags <<= 1, facing += QUADRANT)
		{
			if ((ship_flags & fire_flags) && NORMALIZE_FACING (
					direction_facing - facing + ANGLE_TO_FACING (OCTANT)
					) <= ANGLE_TO_FACING (QUADRANT))
			{
				StarShipPtr->ship_input_state |= WEAPON;
				break;
			}
		}
	}
}

static void
InitWeaponSlots (RACE_DESC *RaceDescPtr, const BYTE *ModuleSlots)
{
	COUNT i;
	SIS_DATA *SisData = GetCustomShipData (RaceDescPtr);
	MISSILE_BLOCK *lpMB = SisData->MissileBlock;

	SisData->num_blasters = 0;
	for (i = 0; i < NUM_MODULE_SLOTS; ++i)
	{
		COUNT which_gun;

		if (i == 3)
			i = NUM_MODULE_SLOTS - 1;
		
		which_gun = ModuleSlots[(NUM_MODULE_SLOTS - 1) - i];
		
		if (which_gun < GUN_WEAPON || which_gun > CANNON_WEAPON)
			continue; /* not a gun */

		which_gun -= GUN_WEAPON - 1;
		RaceDescPtr->characteristics.weapon_energy_cost +=
				which_gun * 2;
		
		lpMB->flags = IGNORE_SIMILAR;
		lpMB->blast_offs = BlasterOffset;
		lpMB->speed = blasterStats().speed;
		lpMB->preprocess_func = blaster_preprocess;
		lpMB->hit_points = blasterStats().hitPoints * which_gun;
		lpMB->damage = blasterStats().damage * which_gun;
		lpMB->life = blasterStats().lifetime + ((blasterStats().lifetime >> 2) * (which_gun - 1));

		if (which_gun == 1)
			lpMB->index = 0;
		else if (which_gun == 2)
			lpMB->index = 9;
		else
			lpMB->index = 16;

		switch (i)
		{
			case 0: /* NOSE WEAPON */
				RaceDescPtr->ship_info.ship_flags |= FIRES_FORE;
				lpMB->pixoffs = SisVertOffset;
				lpMB->face = 0;
				break;
			case 1: /* SPREAD WEAPON */
				RaceDescPtr->ship_info.ship_flags |= FIRES_FORE;
				lpMB->pixoffs = SisVertOffset;
				lpMB->face = +1;
				/* copy it because there are two */
				lpMB[1] = lpMB[0];
				++lpMB;
				++SisData->num_blasters;
				lpMB->face = NORMALIZE_FACING (-1);
				break;
			case 2: /* SIDE WEAPON */
				RaceDescPtr->ship_info.ship_flags |=
						FIRES_LEFT | FIRES_RIGHT;
				lpMB->pixoffs = SisHorzOffset;
				lpMB->face = ANGLE_TO_FACING (QUADRANT);
				/* copy it because there are two */
				lpMB[1] = lpMB[0];
				++lpMB;
				++SisData->num_blasters;
				lpMB->face = NORMALIZE_FACING (-ANGLE_TO_FACING (QUADRANT));
				break;
			case NUM_MODULE_SLOTS - 1: /* TAIL WEAPON */
				RaceDescPtr->ship_info.ship_flags |= FIRES_AFT;
				lpMB->pixoffs = SisVertOffset;
				lpMB->face = ANGLE_TO_FACING (HALF_CIRCLE);
				break;
		}

		++lpMB;
		++SisData->num_blasters;
	}
}

static void
InitModuleSlots (RACE_DESC *RaceDescPtr, const BYTE *ModuleSlots)
{
	COUNT i;
	COUNT num_trackers;
	SIS_DATA *SisData = GetCustomShipData (RaceDescPtr);

	RaceDescPtr->ship_info.max_crew = 0;
	num_trackers = 0;
	for (i = 0; i < NUM_MODULE_SLOTS; ++i)
	{
		BYTE which_mod;

		which_mod = ModuleSlots[(NUM_MODULE_SLOTS - 1) - i];
		switch (which_mod)
		{
			case CREW_POD:
				RaceDescPtr->ship_info.max_crew += CREW_POD_CAPACITY;
				break;
			case TRACKING_SYSTEM:
				++num_trackers;
				break;
			case ANTIMISSILE_DEFENSE:
				RaceDescPtr->characteristics.special_energy_cost +=
						AntimissileEnergyInc;
				break;
			case SHIVA_FURNACE:
				RaceDescPtr->characteristics.energy_regeneration +=
						ShivaEnergyRegenInc;
				break;
			case DYNAMO_UNIT:
				RaceDescPtr->characteristics.energy_wait -=
						DynamoUnitEnergyWaitDec;
				if (RaceDescPtr->characteristics.energy_wait < MinEnergyWait)
					RaceDescPtr->characteristics.energy_wait = MinEnergyWait;
				break;
		}
	}

	if (num_trackers > MaxTracking)
		num_trackers = MaxTracking;
	RaceDescPtr->characteristics.weapon_energy_cost +=
			num_trackers * TrackerEnergyCost;
	SisData->num_trackers = num_trackers;
	if (RaceDescPtr->characteristics.special_energy_cost)
	{
		RaceDescPtr->ship_info.ship_flags |= POINT_DEFENSE;
		if (RaceDescPtr->characteristics.special_energy_cost > MaxDefense)
			RaceDescPtr->characteristics.special_energy_cost = MaxDefense;
	}
}

static void
InitDriveSlots (RACE_DESC *RaceDescPtr, const BYTE *DriveSlots)
{
	COUNT i;

	// NB. RaceDescPtr->characteristics.max_thrust is already initialised.
	RaceDescPtr->characteristics.thrust_wait = 0;
	for (i = 0; i < NUM_DRIVE_SLOTS; ++i)
	{
		switch (DriveSlots[i])
		{
			case FUSION_THRUSTER:
				RaceDescPtr->characteristics.max_thrust += 2;
				++RaceDescPtr->characteristics.thrust_wait;
				break;
		}
	}
	RaceDescPtr->characteristics.thrust_wait = (BYTE)(
			tuning().ship.thrustWait - (RaceDescPtr->characteristics.thrust_wait >> 1));
	RaceDescPtr->characteristics.max_thrust =
			((RaceDescPtr->characteristics.max_thrust /
			RaceDescPtr->characteristics.thrust_increment) + 1)
			* RaceDescPtr->characteristics.thrust_increment;
}

static void
InitJetSlots (RACE_DESC *RaceDescPtr, const BYTE *JetSlots)
{
	COUNT i;

	for (i = 0; i < NUM_JET_SLOTS; ++i)
	{
		switch (JetSlots[i])
		{
			case TURNING_JETS:
				RaceDescPtr->characteristics.turn_wait -= 2;
				break;
		}
	}
}

namespace {
class Flagship final : public uqm::combat::Ship {
public:
    explicit Flagship(const RACE_DESC &definition, bool hyperspace) noexcept
        : Ship(definition), hyperspace_(hyperspace) {
        if (!hyperspace_) legacyData().cyborg_control.intelligence_func = sis_intelligence;
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
        if (hyperspace_) sis_hyper_preprocess(&element);
        else sis_battle_preprocess(&element);
    }
    void afterMotion(ELEMENT &element) noexcept override {
        if (hyperspace_) sis_hyper_postprocess(&element);
        else sis_battle_postprocess(&element);
    }
    COUNT createWeapons(ELEMENT &element, uqm::combat::WeaponSlots &weapons) noexcept override {
        return hyperspace_ ? 0 : initialize_blasters(&element, weapons.data());
    }
    void onFinish() noexcept override {
        uninit_sis(&legacyData());
    }
    bool hyperspace_;
    std::optional<CustomShipData_t> privateData_;
};
} // namespace

static void SetCustomShipData(RACE_DESC *descriptor, const CustomShipData_t *data)
{
    static_cast<Flagship &>(uqm::combat::shipFor(*descriptor)).setData(data);
}

std::unique_ptr<uqm::combat::Ship>
uqm::combat::makeFlagship()
{
	COUNT i;
	// Build a per-instance definition, copied into the native owner.
	RACE_DESC new_sis_desc;
	SIS_DATA empty_data{};

	/* copy initial ship settings to new_sis_desc */
	new_sis_desc = sisDefinition();

	if (inHQSpace ())
	{
		for (i = 0; i < NUM_VIEWS; ++i)
		{
			new_sis_desc.ship_data.ship_rsc[i] = NULL_RESOURCE;
			new_sis_desc.ship_data.weapon_rsc[i] = NULL_RESOURCE;
			new_sis_desc.ship_data.special_rsc[i] = NULL_RESOURCE;
		}
		new_sis_desc.ship_info.icons_rsc = NULL_RESOURCE;
		new_sis_desc.ship_data.captain_control.captain_rsc = NULL_RESOURCE;
		new_sis_desc.ship_data.victory_ditty_rsc = NULL_RESOURCE;
		new_sis_desc.ship_data.ship_sounds_rsc = NULL_RESOURCE;

		new_sis_desc.ship_data.ship_rsc[0] = SIS_HYPER_MASK_PMAP_ANIM;

		new_sis_desc.characteristics.max_thrust -= 4;
	}
	else
	{

		if (GET_GAME_STATE (CHMMR_BOMB_STATE) == 3)
			SET_GAME_STATE (BOMB_CARRIER, 1);
	}

	new_sis_desc.data = &empty_data;
	InitModuleSlots (&new_sis_desc, GLOBAL_SIS (ModuleSlots));
	InitWeaponSlots (&new_sis_desc, GLOBAL_SIS (ModuleSlots));
	InitDriveSlots (&new_sis_desc, GLOBAL_SIS (DriveSlots));
	InitJetSlots (&new_sis_desc, GLOBAL_SIS (JetSlots));
	
	if (LOBYTE (GLOBAL (CurrentActivity)) == SUPER_MELEE)
	{
		new_sis_desc.ship_info.crew_level = new_sis_desc.ship_info.max_crew;
	}
	else
	{
		// Count the captain too.
		new_sis_desc.ship_info.max_crew++;
		new_sis_desc.ship_info.crew_level = GLOBAL_SIS (CrewEnlisted) + 1;
		new_sis_desc.ship_info.ship_flags |= PLAYER_CAPTAIN;
	}
	
	new_sis_desc.ship_info.energy_level = new_sis_desc.ship_info.max_energy;

	return std::make_unique<Flagship>(new_sis_desc, inHQSpace());
}

static void
uninit_sis (RACE_DESC *pRaceDesc)
{
	if (!inHQSpace ())
	{
		GLOBAL_SIS (CrewEnlisted) = pRaceDesc->ship_info.crew_level;
		if (pRaceDesc->ship_info.ship_flags & PLAYER_CAPTAIN)
			GLOBAL_SIS (CrewEnlisted)--;
	}

	SetCustomShipData (pRaceDesc, NULL);
}

