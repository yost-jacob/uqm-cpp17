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

#include "uqm/globdata.h"

// Balance values are immutable, loaded from base/ships.json at startup.
static const uqm::combat::ShipTuning &tuning() {
    static const auto &value = uqm::combat::shipTuning("blackurq");
    return value;
}
static const uqm::combat::ProjectileStats &missileStats() {
    static const auto &value = tuning().projectile("missile");
    return value;
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto MissileOffset = 9;
constexpr auto KohrAhOffset = 28;
constexpr auto SawRate = 0;
constexpr auto MaxSaws = 8;
constexpr auto ActivateRange = 224;
constexpr auto TrackWait = 4;
static auto fragmentSpeed() { return missileStats().speed; }
constexpr auto FragmentLife = 10;
static auto fragmentRange() { return (FragmentLife * fragmentSpeed()); }
constexpr auto GasOffset = 2;
constexpr auto GasSpeed = 16;
constexpr auto GasRate = 2;
constexpr auto GasHits = 100;
constexpr auto GasDamage = 3;
constexpr auto GasAltDamage = 50;
constexpr auto NumGasClouds = 16;
constexpr auto LastSpinIndex = 1;

// Discs remain controlled while fire is held; their lifetime is extended by behavior.
// ActivateRange is the stationary-disc homing threshold, independent of screen width.

static const RACE_DESC &black_urquanDefinition() {
    static const RACE_DESC definition = {
        { /* SHIP_INFO */
            FIRES_FORE,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, tuning().ship.maxCrew,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            KOHR_AH_RACE_STRINGS,
            KOHR_AH_ICON_MASK_PMAP_ANIM,
            KOHR_AH_MICON_MASK_PMAP_ANIM,
            NULL, NULL, NULL
        },
        { /* FLEET_STUFF */
            2666 / SPHERE_RADIUS_INCREMENT * 2, /* Initial SoI radius */
            { /* Known location (center of SoI) */
                6000, 6250,
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
                KOHR_AH_BIG_MASK_PMAP_ANIM,
                KOHR_AH_MED_MASK_PMAP_ANIM,
                KOHR_AH_SML_MASK_PMAP_ANIM,
            },
            {
                BUZZSAW_BIG_MASK_PMAP_ANIM,
                BUZZSAW_MED_MASK_PMAP_ANIM,
                BUZZSAW_SML_MASK_PMAP_ANIM,
            },
            {
                GAS_BIG_MASK_PMAP_ANIM,
                GAS_MED_MASK_PMAP_ANIM,
                GAS_SML_MASK_PMAP_ANIM,
            },
            {
                KOHR_AH_CAPTAIN_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
            KOHR_AH_VICTORY_SONG,
            KOHR_AH_SHIP_SOUNDS,
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

static void
spin_preprocess (ELEMENT *ElementPtr)
{
	ELEMENT *ShipPtr;
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	LockElement (StarShipPtr->hShip, &ShipPtr);
	if (ShipPtr->crew_level
			&& ++StarShipPtr->RaceDescPtr->characteristics.special_wait > MaxSaws)
	{
		ElementPtr->life_span = 1;
		ElementPtr->state_flags |= DISAPPEARING;
	}
	else
	{
		++ElementPtr->life_span;
		if (ElementPtr->turn_wait)
			--ElementPtr->turn_wait;
		else
		{
			if (GetFrameIndex (
					ElementPtr->current.image.frame
					) < LastSpinIndex)
				ElementPtr->next.image.frame =
						IncFrameIndex (ElementPtr->current.image.frame);
			else
				ElementPtr->next.image.frame =
						SetAbsFrameIndex (ElementPtr->current.image.frame, 0);
			ElementPtr->state_flags |= CHANGING;

			ElementPtr->turn_wait = SawRate;
		}
	}
	UnlockElement (StarShipPtr->hShip);
}

static void
buzztrack_preprocess (ELEMENT *ElementPtr)
{
	if (ElementPtr->thrust_wait)
		--ElementPtr->thrust_wait;
	else
	{
		COUNT facing = 0;

		if (ElementPtr->hTarget == 0
				&& TrackShip (ElementPtr, &facing) < 0)
		{
			ZeroVelocityComponents (&ElementPtr->velocity);
		}
		else
		{
			SIZE delta_x, delta_y;
			ELEMENT *eptr;

			LockElement (ElementPtr->hTarget, &eptr);
			delta_x = eptr->current.location.x
					- ElementPtr->current.location.x;
			delta_y = eptr->current.location.y
					- ElementPtr->current.location.y;
			UnlockElement (ElementPtr->hTarget);
			delta_x = WRAP_DELTA_X (delta_x);
			delta_y = WRAP_DELTA_Y (delta_y);
			facing = NORMALIZE_FACING (
					ANGLE_TO_FACING (ARCTAN (delta_x, delta_y))
					);

			if (delta_x < 0)
				delta_x = -delta_x;
			if (delta_y < 0)
				delta_y = -delta_y;
			delta_x = WORLD_TO_DISPLAY (delta_x);
			delta_y = WORLD_TO_DISPLAY (delta_y);
			if (delta_x >= ActivateRange
					|| delta_y >= ActivateRange
					|| (DWORD)((UWORD)delta_x * delta_x)
					+ (DWORD)((UWORD)delta_y * delta_y) >=
					(DWORD)ActivateRange * ActivateRange)
			{
				ZeroVelocityComponents (&ElementPtr->velocity);
			}
			else
			{
				ElementPtr->thrust_wait = TrackWait;
				SetVelocityVector (&ElementPtr->velocity,
						DISPLAY_TO_WORLD (2), facing);
			}
		}
	}

	spin_preprocess (ElementPtr);
}

static void
decelerate_preprocess (ELEMENT *ElementPtr)
{
	SIZE dx, dy;

	GetCurrentVelocityComponents (&ElementPtr->velocity, &dx, &dy);
	dx /= 2;
	dy /= 2;
	SetVelocityComponents (&ElementPtr->velocity, dx, dy);
	if (dx == 0 && dy == 0)
	{
		ElementPtr->preprocess_func = buzztrack_preprocess;
	}

	spin_preprocess (ElementPtr);
}

static void
splinter_preprocess (ELEMENT *ElementPtr)
{
	ElementPtr->next.image.frame =
			IncFrameIndex (ElementPtr->current.image.frame);
	ElementPtr->state_flags |= CHANGING;
}

static void
buzzsaw_collision (ELEMENT *ElementPtr0, POINT *pPt0,
		ELEMENT *ElementPtr1, POINT *pPt1)
{
	weapon_collision (ElementPtr0, pPt0, ElementPtr1, pPt1);

	if (ElementPtr0->state_flags & DISAPPEARING)
	{
		ElementPtr0->state_flags &= ~DISAPPEARING;
		ElementPtr0->state_flags |= NONSOLID | CHANGING;
		ElementPtr0->life_span = 5;
		ElementPtr0->next.image.frame =
				SetAbsFrameIndex (ElementPtr0->current.image.frame, 2);

		ElementPtr0->preprocess_func = splinter_preprocess;
	}
}

static void
buzzsaw_preprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if (!(StarShipPtr->cur_status_flags & WEAPON))
	{
		ElementPtr->life_span >>= 1;
		ElementPtr->preprocess_func = decelerate_preprocess;
	}

	spin_preprocess (ElementPtr);
}

static void
buzzsaw_postprocess (ELEMENT *ElementPtr)
{
	HELEMENT hElement;

	ElementPtr->postprocess_func = 0;
	hElement = AllocElement ();
	if (hElement)
	{
		COUNT primIndex;
		ELEMENT *ListElementPtr;
		STARSHIP *StarShipPtr;

		LockElement (hElement, &ListElementPtr);
		primIndex = ListElementPtr->PrimIndex;
		*ListElementPtr = *ElementPtr;
		ListElementPtr->PrimIndex = primIndex;
		(GLOBAL (DisplayArray))[primIndex] =
				(GLOBAL (DisplayArray))[ElementPtr->PrimIndex];
		ListElementPtr->current = ListElementPtr->next;
		InitIntersectStartPoint (ListElementPtr);
		InitIntersectEndPoint (ListElementPtr);
		ListElementPtr->state_flags = (ListElementPtr->state_flags
				& ~(PRE_PROCESS | CHANGING | APPEARING))
				| POST_PROCESS;
		UnlockElement (hElement);

		GetElementStarShip (ElementPtr, &StarShipPtr);
		LockElement (StarShipPtr->hShip, &ListElementPtr);
		InsertElement (hElement, GetSuccElement (ListElementPtr));
		UnlockElement (StarShipPtr->hShip);

		ElementPtr->life_span = 0;
	}
}

static COUNT
initialize_buzzsaw (ELEMENT *ShipPtr, HELEMENT SawArray[])
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
	MissileBlock.pixoffs = KohrAhOffset;
	MissileBlock.speed = missileStats().speed;
	MissileBlock.hit_points = missileStats().hitPoints;
	MissileBlock.damage = missileStats().damage;
	MissileBlock.life = missileStats().lifetime;
	MissileBlock.preprocess_func = buzzsaw_preprocess;
	MissileBlock.blast_offs = MissileOffset;
	SawArray[0] = initialize_missile (&MissileBlock);

	if (SawArray[0])
	{
		ELEMENT *SawPtr;

		LockElement (SawArray[0], &SawPtr);
		SawPtr->turn_wait = SawRate;
		SawPtr->thrust_wait = 0;
		SawPtr->postprocess_func = buzzsaw_postprocess;
		SawPtr->collision_func = buzzsaw_collision;
		UnlockElement (SawArray[0]);
	}

	return (1);
}

static void
black_urquan_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
		COUNT ConcernCounter)
{
	EVALUATE_DESC *lpEvalDesc;
	STARSHIP *StarShipPtr;

	lpEvalDesc = &ObjectsOfConcern[ENEMY_WEAPON_INDEX];
	if (lpEvalDesc->ObjectPtr
			&& lpEvalDesc->MoveState == ENTICE
			&& (lpEvalDesc->ObjectPtr->state_flags & CREW_OBJECT)
			&& lpEvalDesc->which_turn <= 8)
		lpEvalDesc->MoveState = PURSUE;

	ship_intelligence (ShipPtr,
			ObjectsOfConcern, ConcernCounter);

	GetElementStarShip (ShipPtr, &StarShipPtr);
	StarShipPtr->ship_input_state &= ~SPECIAL;

	if (StarShipPtr->special_counter == 0
			&& StarShipPtr->RaceDescPtr->ship_info.energy_level >= tuning().ship.specialEnergyCost
			&& lpEvalDesc->ObjectPtr
			&& lpEvalDesc->which_turn <= 8)
		StarShipPtr->ship_input_state |= SPECIAL;

	lpEvalDesc = &ObjectsOfConcern[ENEMY_SHIP_INDEX];
	if (lpEvalDesc->ObjectPtr)
	{
		HELEMENT h, hNext;
		ELEMENT *BuzzSawPtr;

		h = (StarShipPtr->old_status_flags & WEAPON) ?
				GetSuccElement (ShipPtr) : (HELEMENT)0;
		for (; h; h = hNext)
		{
			LockElement (h, &BuzzSawPtr);
			hNext = GetSuccElement (BuzzSawPtr);
			if (!(BuzzSawPtr->state_flags & NONSOLID)
					&& BuzzSawPtr->next.image.farray == StarShipPtr->RaceDescPtr->ship_data.weapon
					&& BuzzSawPtr->life_span > missileStats().lifetime * 3 / 4
					&& elementsOfSamePlayer (BuzzSawPtr, ShipPtr))
			{
				{
					//COUNT which_turn;

					if (!PlotIntercept (BuzzSawPtr,
							lpEvalDesc->ObjectPtr, BuzzSawPtr->life_span,
							fragmentRange() / 2))
						StarShipPtr->ship_input_state &= ~WEAPON;
					else if (StarShipPtr->weapon_counter == 0)
						StarShipPtr->ship_input_state |= WEAPON;

					UnlockElement (h);
					break;
				}
				hNext = 0;
			}
			UnlockElement (h);
		}

		if (h == 0)
		{
			if (StarShipPtr->old_status_flags & WEAPON)
				StarShipPtr->ship_input_state &= ~WEAPON;
			else if (StarShipPtr->weapon_counter == 0
					&& ship_weapons (ShipPtr, lpEvalDesc->ObjectPtr, fragmentRange() / 2))
				StarShipPtr->ship_input_state |= WEAPON;

			if (StarShipPtr->special_counter == 0
					&& !(StarShipPtr->ship_input_state & WEAPON)
					&& lpEvalDesc->which_turn <= 8
					&& (StarShipPtr->ship_input_state & (LEFT | RIGHT))
					&& StarShipPtr->RaceDescPtr->ship_info.energy_level >=
					tuning().ship.specialEnergyCost)
				StarShipPtr->ship_input_state |= SPECIAL;
		}
	}
}

static void
gas_cloud_preprocess (ELEMENT *ElementPtr)
{
	if (ElementPtr->turn_wait)
		--ElementPtr->turn_wait;
	else
	{
		ElementPtr->next.image.frame =
				IncFrameIndex (ElementPtr->current.image.frame);
		ElementPtr->state_flags |= CHANGING;

		ElementPtr->turn_wait = GasRate;
	}
}

static void
gas_cloud_collision (ELEMENT *ElementPtr0, POINT *pPt0,
		ELEMENT *ElementPtr1, POINT *pPt1)
{
	if (ElementPtr1->state_flags & PLAYER_SHIP)
		ElementPtr0->mass_points = GasDamage;
	else
		ElementPtr0->mass_points = GasAltDamage;

	weapon_collision (ElementPtr0, pPt0, ElementPtr1, pPt1);
}

static void
spawn_gas_cloud (ELEMENT *ElementPtr)
{
	SIZE dx, dy;
	STARSHIP *StarShipPtr;
	MISSILE_BLOCK MissileBlock;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	MissileBlock.cx = ElementPtr->next.location.x;
	MissileBlock.cy = ElementPtr->next.location.y;
	MissileBlock.farray = StarShipPtr->RaceDescPtr->ship_data.special;
	MissileBlock.index = 0;
	MissileBlock.sender = ElementPtr->playerNr;
	MissileBlock.flags = IGNORE_SIMILAR;
	MissileBlock.pixoffs = 20;
	MissileBlock.speed = GasSpeed;
	MissileBlock.hit_points = GasHits;
	MissileBlock.damage = GasDamage;
	MissileBlock.life =
			GetFrameCount (MissileBlock.farray[0]) * (GasRate + 1) - 1;
	MissileBlock.preprocess_func = gas_cloud_preprocess;
	MissileBlock.blast_offs = GasOffset;

	GetCurrentVelocityComponents (&ElementPtr->velocity, &dx, &dy);
	for (MissileBlock.face = 0;
			MissileBlock.face < ANGLE_TO_FACING (FULL_CIRCLE);
			MissileBlock.face +=
			(ANGLE_TO_FACING (FULL_CIRCLE) / NumGasClouds))
	{
		HELEMENT hGasCloud;

		hGasCloud = initialize_missile (&MissileBlock);
		if (hGasCloud)
		{
			ELEMENT *GasCloudPtr;

			LockElement (hGasCloud, &GasCloudPtr);
			SetElementStarShip (GasCloudPtr, StarShipPtr);
			GasCloudPtr->hTarget = 0;
			GasCloudPtr->turn_wait = GasRate - 1;
			GasCloudPtr->collision_func = gas_cloud_collision;
			DeltaVelocityComponents (&GasCloudPtr->velocity, dx, dy);
			UnlockElement (hGasCloud);
			PutElement (hGasCloud);
		}
	}
}

static void
black_urquan_postprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if ((StarShipPtr->cur_status_flags & SPECIAL)
			&& StarShipPtr->special_counter == 0
			&& DeltaEnergy (ElementPtr, -tuning().ship.specialEnergyCost))
	{
		spawn_gas_cloud (ElementPtr);

		ProcessSound (SetAbsSoundIndex (
				StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 1), ElementPtr);

		StarShipPtr->special_counter = tuning().ship.specialWait;
	}
}

static void
black_urquan_preprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
			/* no spinning disks */
	StarShipPtr->RaceDescPtr->characteristics.special_wait = 0;
	if (StarShipPtr->weapon_counter == 0
			&& (StarShipPtr->cur_status_flags
			& StarShipPtr->old_status_flags & WEAPON))
		++StarShipPtr->weapon_counter;
}

namespace {
class KohrAhShip final : public uqm::combat::Ship {
public:
    explicit KohrAhShip(const RACE_DESC &definition) noexcept
        : Ship(definition) {
        legacyData().cyborg_control.intelligence_func = black_urquan_intelligence;
    }
private:
    void beforeMotion(ELEMENT &element) noexcept override {
        black_urquan_preprocess(&element);
    }
    void afterMotion(ELEMENT &element) noexcept override {
        black_urquan_postprocess(&element);
    }
    COUNT createWeapons(ELEMENT &element, uqm::combat::WeaponSlots &weapons) noexcept override {
        return initialize_buzzsaw(&element, weapons.data());
    }
};
} // namespace

std::unique_ptr<uqm::combat::Ship>
uqm::combat::makeKohrAhShip()
{

	return std::make_unique<KohrAhShip>(black_urquanDefinition());
}
