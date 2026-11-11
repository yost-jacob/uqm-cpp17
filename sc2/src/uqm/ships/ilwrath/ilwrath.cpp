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

#include "uqm/colors.h"
#include "uqm/globdata.h"

// Balance values are immutable, loaded from base/ships.json for the active session.
static const uqm::combat::ShipTuning &tuning() {
    return uqm::combat::shipTuning("ilwrath");
}
static const uqm::combat::ProjectileStats &missileStats() {
    return tuning().projectile("missile");
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto LookAhead = 4;
constexpr auto IlwrathOffset = 29;
constexpr auto MissileOffset = 0;

// LookAhead controls how far auto-turn leads its target.

static RACE_DESC ilwrathDefinition() {
    const RACE_DESC definition = {
        { /* SHIP_INFO */
            FIRES_FORE,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, tuning().ship.maxCrew,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            ILWRATH_RACE_STRINGS,
            ILWRATH_ICON_MASK_PMAP_ANIM,
            ILWRATH_MICON_MASK_PMAP_ANIM,
            NULL, NULL, NULL
        },
        { /* FLEET_STUFF */
            1410 / SPHERE_RADIUS_INCREMENT * 2, /* Initial SoI radius */
            { /* Known location (center of SoI) */
                48, 1700,
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
                ILWRATH_BIG_MASK_PMAP_ANIM,
                ILWRATH_MED_MASK_PMAP_ANIM,
                ILWRATH_SML_MASK_PMAP_ANIM,
            },
            {
                FIRE_BIG_MASK_PMAP_ANIM,
                FIRE_MED_MASK_PMAP_ANIM,
                FIRE_SML_MASK_PMAP_ANIM,
            },
            {
                NULL_RESOURCE,
                NULL_RESOURCE,
                NULL_RESOURCE,
            },
            {
                ILWRATH_CAPTAIN_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
            ILWRATH_VICTORY_SONG,
            ILWRATH_SHIP_SOUNDS,
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

// Per-shot behavior; engine kinematics and collision state remain borrowed.
namespace {
class IlwrathFlame final : public uqm::combat::Projectile {
public:
    using Projectile::Projectile;
    void collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) override;
    void update(ELEMENT &element) override {
        auto *ElementPtr = &element;

	if (ElementPtr->turn_wait > 0)
		--ElementPtr->turn_wait;
	else
	{
		ElementPtr->next.image.frame =
				IncFrameIndex (ElementPtr->current.image.frame);
		ElementPtr->state_flags |= CHANGING;

		ElementPtr->turn_wait = ElementPtr->next_turn;
	}
    }
};
}


void IlwrathFlame::collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) {
    auto *ElementPtr0 = &self;
    auto *pPt0 = &point;
    auto *ElementPtr1 = &other;
    auto *pPt1 = &otherPoint;

	weapon_collision (ElementPtr0, pPt0, ElementPtr1, pPt1);
	ElementPtr0->state_flags &= ~DISAPPEARING;
	ElementPtr0->state_flags |= NONSOLID;
}


static void
ilwrath_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
		COUNT ConcernCounter)
{
	EVALUATE_DESC *lpEvalDesc;
	STARSHIP *StarShipPtr;

	lpEvalDesc = &ObjectsOfConcern[ENEMY_SHIP_INDEX];
	 lpEvalDesc->MoveState = PURSUE;
	if (lpEvalDesc->ObjectPtr && lpEvalDesc->which_turn <= 10)
				/* don't want to dodge when you could be flaming */
		ObjectsOfConcern[ENEMY_WEAPON_INDEX].ObjectPtr = 0;

	ship_intelligence (ShipPtr, ObjectsOfConcern, ConcernCounter);

	GetElementStarShip (ShipPtr, &StarShipPtr);
	if (lpEvalDesc->ObjectPtr
			&& (lpEvalDesc->which_turn <= 6
			|| (lpEvalDesc->which_turn <= 10
			&& ObjectsOfConcern[ENEMY_WEAPON_INDEX].which_turn <= 10)))
	{
		StarShipPtr->ship_input_state &= ~SPECIAL;
		if (OBJECT_CLOAKED (ShipPtr))
		{
			StarShipPtr->ship_input_state &= ~LEFT | RIGHT;
			StarShipPtr->ship_input_state |= THRUST;
		}
		StarShipPtr->ship_input_state |= WEAPON;
	}
	else if (StarShipPtr->special_counter == 0
			&& (LOBYTE (GLOBAL (CurrentActivity)) != IN_ENCOUNTER
			|| !GET_GAME_STATE (PROBE_ILWRATH_ENCOUNTER)))
	{
		StarShipPtr->ship_input_state &= ~SPECIAL;
		if (!OBJECT_CLOAKED (ShipPtr)
				&& !(StarShipPtr->ship_input_state & WEAPON))
			StarShipPtr->ship_input_state |= SPECIAL;
	}
}

static COUNT
initialize_flame (ELEMENT *ShipPtr, HELEMENT FlameArray[])
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
	MissileBlock.pixoffs = IlwrathOffset;
	MissileBlock.speed = missileStats().speed;
	MissileBlock.hit_points = missileStats().hitPoints;
	MissileBlock.damage = missileStats().damage;
	MissileBlock.life = missileStats().lifetime;
	MissileBlock.preprocess_func = nullptr;
	MissileBlock.blast_offs = MissileOffset;
	FlameArray[0] = uqm::combat::publishMissile<IlwrathFlame>(MissileBlock);

	if (FlameArray[0])
	{
		SIZE dx, dy;
		ELEMENT *FlamePtr;

		LockElement (FlameArray[0], &FlamePtr);
		GetCurrentVelocityComponents (&ShipPtr->velocity, &dx, &dy);
		DeltaVelocityComponents (&FlamePtr->velocity, dx, dy);
		FlamePtr->current.location.x -= VELOCITY_TO_WORLD (dx);
		FlamePtr->current.location.y -= VELOCITY_TO_WORLD (dy);

		FlamePtr->turn_wait = 0;
		UnlockElement (FlameArray[0]);
	}

	return (1);
}

static void
ilwrath_preprocess (ELEMENT *ElementPtr)
{
	STATUS_FLAGS status_flags;
	STARSHIP *StarShipPtr;
	PRIMITIVE *lpPrim;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	status_flags = StarShipPtr->cur_status_flags;
	lpPrim = &(GLOBAL (DisplayArray))[ElementPtr->PrimIndex];
	if (GetPrimType (lpPrim) == STAMPFILL_PRIM)
	{
		Color color;
		bool weapon_discharge;

		color = GetPrimColor (lpPrim);
		weapon_discharge = ((status_flags & WEAPON)
				&& StarShipPtr->RaceDescPtr->ship_info.energy_level >= tuning().ship.weaponEnergyCost);
		if (weapon_discharge
				|| (StarShipPtr->special_counter == 0
				&& ((status_flags & SPECIAL) ||
				!sameColor (color, BLACK_COLOR))))
		{
			if (sameColor (color,
					BUILD_COLOR (MAKE_RGB15 (0x1F, 0x1F, 0x1F), 0x0F)))
				SetPrimType (lpPrim, STAMP_PRIM);
			else if (sameColor (color,
					BUILD_COLOR (MAKE_RGB15 (0x0A, 0x1F, 0x1F), 0x0B)))
				SetPrimColor (lpPrim,
						BUILD_COLOR (MAKE_RGB15 (0x1F, 0x1F, 0x1F), 0x0F));
			else if (sameColor (color,
					BUILD_COLOR (MAKE_RGB15 (0x00, 0x14, 0x14), 0x03)))
				SetPrimColor (lpPrim,
						BUILD_COLOR (MAKE_RGB15 (0x0A, 0x1F, 0x1F), 0x0B));
			else if (sameColor (color,
					BUILD_COLOR (MAKE_RGB15 (0x0A, 0x0A, 0x1F), 0x09)))
				SetPrimColor (lpPrim,
						BUILD_COLOR (MAKE_RGB15 (0x00, 0x14, 0x14), 0x03));
			else if (sameColor (color,
					BUILD_COLOR (MAKE_RGB15 (0x00, 0x00, 0x14), 0x01)))
				SetPrimColor (lpPrim,
						BUILD_COLOR (MAKE_RGB15 (0x0A, 0x0A, 0x1F), 0x09));
			else
			{
				ProcessSound (SetAbsSoundIndex (
								/* CLOAKING_OFF */
						StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 2), ElementPtr);
				SetPrimColor (lpPrim,
						BUILD_COLOR (MAKE_RGB15 (0x00, 0x00, 0x14), 0x01));
				if (weapon_discharge)
				{
					COUNT facing;

					facing = StarShipPtr->ShipFacing;
					if (TrackShip (ElementPtr, &facing) >= 0)
					{
						ELEMENT *eptr;
						SIZE dx0, dy0, dx1, dy1;
						VELOCITY_DESC v;

						LockElement (ElementPtr->hTarget, &eptr);
						v = eptr->velocity;
						GetNextVelocityComponents (&v, &dx0, &dy0, LookAhead);
						v = ElementPtr->velocity;
						GetNextVelocityComponents (&v, &dx1, &dy1, LookAhead);
						dx0 = (eptr->current.location.x + dx0)
								- (ElementPtr->current.location.x + dx1);
						dy0 = (eptr->current.location.y + dy0)
								- (ElementPtr->current.location.y + dy1);
						UnlockElement (ElementPtr->hTarget);

						StarShipPtr->ShipFacing =
								NORMALIZE_FACING (
								ANGLE_TO_FACING (ARCTAN (dx0, dy0))
								);
#ifdef NOTYET
						if (ElementPtr->thrust_wait == 0
								&& (StarShipPtr->cur_status_flags & THRUST))
						{
							COUNT last_facing;

							do
							{
								VELOCITY_DESC temp_v;

								last_facing = StarShipPtr->ShipFacing;
								inertial_thrust (ElementPtr);
								temp_v = ElementPtr->velocity;
								ElementPtr->velocity = v;

								dx0 += dx1;
								dy0 += dy1;
								GetNextVelocityComponents (&temp_v,
										&dx1, &dy1, LookAhead);
								dx0 -= dx1;
								dy0 -= dy1;
								StarShipPtr->ShipFacing =
										NORMALIZE_FACING (
										ANGLE_TO_FACING (ARCTAN (dx0, dy0))
										);
							} while (StarShipPtr->ShipFacing != last_facing);
						}
#endif /* NOTYET */
						if (ElementPtr->turn_wait == 0)
							++ElementPtr->turn_wait;
						ElementPtr->next.image.frame =
								SetAbsFrameIndex (ElementPtr->next.image.frame,
								StarShipPtr->ShipFacing);
					}
					ElementPtr->hTarget = 0;
				}
			}

			ElementPtr->state_flags |= CHANGING;
			status_flags &= ~SPECIAL;
			StarShipPtr->special_counter = 0;
		}
		else if (!sameColor (color, BLACK_COLOR))
		{
			if (sameColor (color,
					BUILD_COLOR (MAKE_RGB15 (0x00, 0x00, 0x14), 0x01)))
			{
				SetPrimColor (lpPrim, BLACK_COLOR);
				Untarget (ElementPtr);
			}
			else if (sameColor (color,
					BUILD_COLOR (MAKE_RGB15 (0x0A, 0x0A, 0x1F), 0x09)))
				SetPrimColor (lpPrim,
						BUILD_COLOR (MAKE_RGB15 (0x00, 0x00, 0x14), 0x01));
			else if (sameColor (color,
					BUILD_COLOR (MAKE_RGB15 (0x00, 0x14, 0x14), 0x03)))
				SetPrimColor (lpPrim,
						BUILD_COLOR (MAKE_RGB15 (0x0A, 0x0A, 0x1F), 0x09));
			else if (sameColor (color,
					BUILD_COLOR (MAKE_RGB15 (0x0A, 0x1F, 0x1F), 0x0B)))
				SetPrimColor (lpPrim,
						BUILD_COLOR (MAKE_RGB15 (0x00, 0x14, 0x14), 0x03));
			else
				SetPrimColor (lpPrim,
						BUILD_COLOR (MAKE_RGB15 (0x0A, 0x1F, 0x1F), 0x0B));

			ElementPtr->state_flags |= CHANGING;
		}
	}

	if ((status_flags & SPECIAL)
			&& StarShipPtr->special_counter == 0
			&& DeltaEnergy (ElementPtr, -tuning().ship.specialEnergyCost))
	{
		SetPrimColor (lpPrim,
				BUILD_COLOR (MAKE_RGB15 (0x1F, 0x1F, 0x1F), 0x0F));
		SetPrimType (lpPrim, STAMPFILL_PRIM);

		ProcessSound (SetAbsSoundIndex (
						/* CLOAKING_ON */
				StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 1),
				ElementPtr);
		StarShipPtr->special_counter =
				StarShipPtr->RaceDescPtr->characteristics.special_wait;

		ElementPtr->state_flags |= CHANGING;
	}
}

namespace {
class IlwrathShip final : public uqm::combat::Ship {
public:
    explicit IlwrathShip(const RACE_DESC &definition) noexcept
        : Ship(definition) {
        legacyData().cyborg_control.intelligence_func = ilwrath_intelligence;
    }
private:
    void beforeMotion(ELEMENT &element) noexcept override {
        ilwrath_preprocess(&element);
    }
    COUNT createWeapons(ELEMENT &element, uqm::combat::WeaponSlots &weapons) noexcept override {
        return initialize_flame(&element, weapons.data());
    }
};
} // namespace

std::unique_ptr<uqm::combat::Ship>
uqm::combat::makeIlwrathShip()
{

	return std::make_unique<IlwrathShip>(ilwrathDefinition());
}

