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

#include "uqm/colors.h"
#include "uqm/globdata.h"
#include "uqm/battle.h"
		// For BATTLE_FRAME_RATE
#include "libs/mathlib.h"
#include "libs/timelib.h"

// Balance values are immutable, loaded from base/ships.json at startup.
static const uqm::combat::ShipTuning &tuning() {
    static const auto &value = uqm::combat::shipTuning("lastbat");
    return value;
}
static const uqm::combat::ProjectileStats &cometStats() {
    static const auto &value = tuning().projectile("comet");
    return value;
}
static const uqm::combat::ProjectileStats &sentinelStats() {
    static const auto &value = tuning().projectile("sentinel");
    return value;
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto TurretWait = 0;
constexpr auto CometOffset = 0;
constexpr auto CometTurnWait = 3;
constexpr auto MaxComets = 3;
constexpr auto SentinelOffset = 0;
constexpr auto TrackWait = 1;
constexpr auto AnimationWait = 1;
constexpr auto RecoilVelocity = WORLD_TO_VELOCITY (DISPLAY_TO_WORLD (10));
constexpr auto MaxRecoilVelocity = (RecoilVelocity * 4);
constexpr auto MaxSentinels = 4;
constexpr auto GateDamage = 1;
constexpr auto GateHits = 100;
constexpr auto GeneratorHits = 15;
constexpr auto MaxGenerators = 8;

// The boss initializes energy costs for compatibility; its special routines do not use them.

static const RACE_DESC &samatraDefinition() {
    static const RACE_DESC definition = {
        { /* SHIP_INFO */
            /* FIRES_FORE | */ IMMEDIATE_WEAPON | CREW_IMMUNE,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, tuning().ship.maxCrew,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            NULL_RESOURCE,
            NULL_RESOURCE,
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
                SAMATRA_BIG_MASK_ANIM,
                SAMATRA_MED_MASK_PMAP_ANIM,
                SAMATRA_SML_MASK_PMAP_ANIM,
            },
            {
                SENTINEL_BIG_MASK_ANIM,
                SENTINEL_MED_MASK_PMAP_ANIM,
                SENTINEL_SML_MASK_PMAP_ANIM,
            },
            {
                GENERATOR_BIG_MASK_ANIM,
                GENERATOR_MED_MASK_PMAP_ANIM,
                GENERATOR_SML_MASK_PMAP_ANIM,
            },
            {
                SAMATRA_CAPTAIN_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
            NULL_RESOURCE,
            SAMATRA_SHIP_SOUNDS,
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            NULL, NULL
        },
        {
            0,
            uqm::combat::weaponRangeHint(0),
            NULL,
        },

        0,
        0, /* ship_owner */
    };
    return definition;
}

static HELEMENT spawn_comet (ELEMENT *ElementPtr);

static void
comet_preprocess (ELEMENT *ElementPtr)
{
	COUNT frame_index;

	frame_index = GetFrameIndex (ElementPtr->current.image.frame) + 1;
	if (frame_index < 29)
	{
		if (frame_index == 25)
		{
			SIZE cur_delta_x, cur_delta_y;
			STARSHIP *StarShipPtr;

			GetElementStarShip (ElementPtr, &StarShipPtr);
			++StarShipPtr->RaceDescPtr->characteristics.weapon_wait;
			spawn_comet (ElementPtr);
			ElementPtr->state_flags |= NONSOLID;

			GetCurrentVelocityComponents (&ElementPtr->velocity,
					&cur_delta_x, &cur_delta_y);
			SetVelocityComponents (&ElementPtr->velocity,
					cur_delta_x / 2, cur_delta_y / 2);
		}
		++ElementPtr->life_span;
	}

	ElementPtr->next.image.frame =
			SetAbsFrameIndex (
			ElementPtr->current.image.frame, frame_index
			);
	ElementPtr->state_flags |= CHANGING;
}

static void
comet_collision (ELEMENT *ElementPtr0, POINT *pPt0,
		ELEMENT *ElementPtr1, POINT *pPt1)
{
	if (ElementPtr1->playerNr == RPG_PLAYER_NUM)
	{
		BYTE old_hits;
		COUNT old_life;
		HELEMENT hBlastElement;

		if (ElementPtr1->state_flags & PLAYER_SHIP)
			ElementPtr0->mass_points = cometStats().damage;
		else
			ElementPtr0->mass_points = 50;

		old_hits = ElementPtr0->hit_points;
		old_life = ElementPtr0->life_span;
		hBlastElement = weapon_collision (ElementPtr0, pPt0, ElementPtr1, pPt1);
		if (ElementPtr1->state_flags & PLAYER_SHIP)
		{
			ElementPtr0->hit_points = old_hits;
			ElementPtr0->life_span = old_life;
			ElementPtr0->state_flags &= ~(DISAPPEARING | NONSOLID | COLLISION);

			if (hBlastElement)
			{
				RemoveElement (hBlastElement);
				FreeElement (hBlastElement);
			}
		}

		if (ElementPtr0->state_flags & DISAPPEARING)
		{
			STARSHIP *StarShipPtr;

			GetElementStarShip (ElementPtr0, &StarShipPtr);
			--StarShipPtr->RaceDescPtr->characteristics.weapon_wait;
		}
	}
}

static HELEMENT
spawn_comet (ELEMENT *ElementPtr)
{
	MISSILE_BLOCK MissileBlock;
	HELEMENT hComet;
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	MissileBlock.cx = ElementPtr->next.location.x;
	MissileBlock.cy = ElementPtr->next.location.y;
	MissileBlock.farray = StarShipPtr->RaceDescPtr->ship_data.weapon;
	MissileBlock.face = 0;
	MissileBlock.index = 24;
	MissileBlock.sender = ElementPtr->playerNr;
	MissileBlock.flags = IGNORE_SIMILAR;
	MissileBlock.pixoffs = 0;
	MissileBlock.speed = 0;
	MissileBlock.hit_points = cometStats().hitPoints;
	MissileBlock.damage = cometStats().damage;
	MissileBlock.life = cometStats().lifetime;
	MissileBlock.preprocess_func = comet_preprocess;
	MissileBlock.blast_offs = CometOffset;
	hComet = initialize_missile (&MissileBlock);

	if (hComet)
	{
		ELEMENT *CometPtr;

		PutElement (hComet);

		LockElement (hComet, &CometPtr);
		CometPtr->collision_func = comet_collision;
		SetElementStarShip (CometPtr, StarShipPtr);
		{
			COUNT facing;

			CometPtr->turn_wait = ElementPtr->turn_wait;
			CometPtr->hTarget = ElementPtr->hTarget;
			if (ElementPtr->state_flags & PLAYER_SHIP)
			{
				CometPtr->turn_wait = 0;
				facing = (COUNT)TFB_Random ();
				SetVelocityVector (&CometPtr->velocity,
						cometStats().speed, facing);
			}
			else
			{
				CometPtr->velocity = ElementPtr->velocity;
				CometPtr->hit_points = ElementPtr->hit_points;
				facing = ANGLE_TO_FACING (
						GetVelocityTravelAngle (&CometPtr->velocity)
						);
			}

			if (CometPtr->turn_wait)
				--CometPtr->turn_wait;
			else
			{
				facing = NORMALIZE_FACING (facing);
				if (TrackShip (CometPtr, &facing) > 0)
					SetVelocityVector (&CometPtr->velocity,
							cometStats().speed, facing);
				CometPtr->turn_wait = CometTurnWait;
			}
		}
		UnlockElement (hComet);
	}

	return (hComet);
}

static void
turret_preprocess (ELEMENT *ElementPtr)
{
	if (ElementPtr->turn_wait > 0)
		--ElementPtr->turn_wait;
	else
	{
		ElementPtr->next.image.frame =
				SetAbsFrameIndex (ElementPtr->current.image.frame,
				(GetFrameIndex (ElementPtr->current.image.frame) % 10) + 1);
		ElementPtr->state_flags |= CHANGING;

		ElementPtr->turn_wait = TurretWait;
	}
}

static void
gate_collision (ELEMENT *ElementPtr0, POINT *pPt0,
		ELEMENT *ElementPtr1, POINT *pPt1)
{
	if (ElementPtr1->playerNr == RPG_PLAYER_NUM)
	{
		STARSHIP *StarShipPtr;

		GetElementStarShip (ElementPtr0, &StarShipPtr);
		if (StarShipPtr->RaceDescPtr->characteristics.max_thrust == 0)
		{
			if (!(ElementPtr1->state_flags & FINITE_LIFE))
				ElementPtr0->state_flags |= COLLISION;

			if ((ElementPtr1->state_flags & PLAYER_SHIP)
					&& GetPrimType (
					&GLOBAL (DisplayArray[ElementPtr0->PrimIndex])
					) == STAMPFILL_PRIM
					&& GET_GAME_STATE (BOMB_CARRIER))
			{
				GLOBAL (CurrentActivity) &= ~IN_BATTLE;
			}
		}
		else
		{
			HELEMENT hBlastElement;

			if (ElementPtr1->state_flags & PLAYER_SHIP)
				ElementPtr0->mass_points = GateDamage;
			else
				ElementPtr0->mass_points = 50;
				
			ElementPtr0->hit_points = GateHits;
			hBlastElement = weapon_collision (ElementPtr0, pPt0, ElementPtr1, pPt1);
			ElementPtr0->state_flags &= ~(DISAPPEARING | NONSOLID | COLLISION);
			ElementPtr0->life_span = NORMAL_LIFE;

			if (hBlastElement)
			{
				RemoveElement (hBlastElement);
				FreeElement (hBlastElement);
			}
		}
	}
}

static void
gate_preprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if (StarShipPtr->RaceDescPtr->characteristics.max_thrust == 0)
	{
		ElementPtr->mass_points = tuning().ship.shipMass;
		ElementPtr->state_flags &= ~FINITE_LIFE;
		ElementPtr->life_span = NORMAL_LIFE + 1;
		ElementPtr->preprocess_func = 0;
		SetPrimColor (
				&GLOBAL (DisplayArray[ElementPtr->PrimIndex]),
				BLACK_COLOR
				);
		SetPrimType (
				&GLOBAL (DisplayArray[ElementPtr->PrimIndex]),
				STAMPFILL_PRIM
				);
	}
	else
	{
		++ElementPtr->life_span;
		ElementPtr->next.image.frame =
				IncFrameIndex (ElementPtr->current.image.frame);
		if (GetFrameIndex (ElementPtr->next.image.frame) == 0)
			ElementPtr->next.image.frame =
					SetAbsFrameIndex (
					ElementPtr->next.image.frame, 11
					);

		ElementPtr->state_flags |= CHANGING;
	}
}

static void
generator_death (ELEMENT *ElementPtr)
{
	if (!(ElementPtr->state_flags & FINITE_LIFE))
	{
		STARSHIP *StarShipPtr;

		GetElementStarShip (ElementPtr, &StarShipPtr);
		--StarShipPtr->RaceDescPtr->characteristics.max_thrust;
		ElementPtr->state_flags |= FINITE_LIFE | NONSOLID;
		ElementPtr->preprocess_func = 0;
		ElementPtr->turn_wait = 12;
		ElementPtr->thrust_wait = 0;

		ElementPtr->current.image.frame =
				SetAbsFrameIndex (ElementPtr->current.image.frame, 10 - 1);
	}

	if (ElementPtr->thrust_wait)
	{
		--ElementPtr->thrust_wait;
		ElementPtr->state_flags &= ~DISAPPEARING;
		ElementPtr->state_flags |= CHANGING;
		++ElementPtr->life_span;
	}
	else if (ElementPtr->turn_wait--)
	{
		ElementPtr->state_flags &= ~DISAPPEARING;
		ElementPtr->state_flags |= CHANGING;
		++ElementPtr->life_span;

		ElementPtr->next.image.frame = IncFrameIndex (
				ElementPtr->current.image.frame
				);

		ElementPtr->thrust_wait = 1;
	}
}

static void
generator_preprocess (ELEMENT *ElementPtr)
{
	if (ElementPtr->turn_wait > 0)
		--ElementPtr->turn_wait;
	else if ((ElementPtr->turn_wait =
			(BYTE)((GeneratorHits
			- ElementPtr->hit_points) / 5)) < 3)
	{
		ElementPtr->next.image.frame =
				SetAbsFrameIndex (ElementPtr->current.image.frame,
				(GetFrameIndex (ElementPtr->current.image.frame) + 1) % 10);
		ElementPtr->state_flags |= CHANGING;
	}
}

static void
generator_collision (ELEMENT *ElementPtr0, POINT *pPt0,
		ELEMENT *ElementPtr1, POINT *pPt1)
{
	if (!(ElementPtr1->state_flags & FINITE_LIFE))
	{
		ElementPtr0->state_flags |= COLLISION;
	}
	(void) pPt0;  /* Satisfying compiler (unused parameter) */
	(void) pPt1;  /* Satisfying compiler (unused parameter) */
}

static void
sentinel_preprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	++StarShipPtr->RaceDescPtr->characteristics.special_wait;
	++ElementPtr->life_span;

	if (ElementPtr->thrust_wait)
		--ElementPtr->thrust_wait;
	else
	{
		ElementPtr->next.image.frame =
				SetAbsFrameIndex (ElementPtr->current.image.frame,
				(GetFrameIndex (ElementPtr->current.image.frame) + 1) % 6);
		ElementPtr->state_flags |= CHANGING;

		ElementPtr->thrust_wait = AnimationWait;
	}

	if (ElementPtr->turn_wait > 0)
		--ElementPtr->turn_wait;
	else
	{
		COUNT facing;
		HELEMENT hTarget;

		if (!(ElementPtr->state_flags & NONSOLID))
			facing = ANGLE_TO_FACING (
					GetVelocityTravelAngle (&ElementPtr->velocity)
					);
		else
		{
			ElementPtr->state_flags &= ~NONSOLID;
			facing = (COUNT)TFB_Random ();
			SetVelocityVector (&ElementPtr->velocity,
					sentinelStats().speed, facing);
		}
		facing = NORMALIZE_FACING (facing);
		if (ElementPtr->hTarget == 0)
		{
			COUNT f;

			f = facing;
			TrackShip (ElementPtr, &f);
		}

		if (ElementPtr->hTarget == 0)
			hTarget = StarShipPtr->hShip;
		else if (StarShipPtr->hShip == 0)
			hTarget = ElementPtr->hTarget;
		else
		{
			SIZE delta_x0, delta_y0, delta_x1, delta_y1;
			ELEMENT *ShipPtr;
			ELEMENT *EnemyShipPtr;

			LockElement (ElementPtr->hTarget, &EnemyShipPtr);

			LockElement (StarShipPtr->hShip, &ShipPtr);
			delta_x0 = ShipPtr->current.location.x
					- ElementPtr->current.location.x;
			delta_y0 = ShipPtr->current.location.y
					- ElementPtr->current.location.y;

			delta_x1 = ShipPtr->current.location.x
					- EnemyShipPtr->current.location.x;
			delta_y1 = ShipPtr->current.location.y
					- EnemyShipPtr->current.location.y;
			UnlockElement (StarShipPtr->hShip);

			if ((long)delta_x0 * delta_x0
					+ (long)delta_y0 * delta_y0 >
					(long)delta_x1 * delta_x1
					+ (long)delta_y1 * delta_y1)
				hTarget = StarShipPtr->hShip;
			else
				hTarget = ElementPtr->hTarget;

			UnlockElement (ElementPtr->hTarget);
		}

		if (hTarget)
		{
			COUNT num_frames;
			SIZE delta_x, delta_y;
			ELEMENT *TargetPtr;
			VELOCITY_DESC TargetVelocity;

			LockElement (hTarget, &TargetPtr);

			delta_x = TargetPtr->current.location.x
					- ElementPtr->current.location.x;
			delta_x = WRAP_DELTA_X (delta_x);
			delta_y = TargetPtr->current.location.y
					- ElementPtr->current.location.y;
			delta_y = WRAP_DELTA_Y (delta_y);

			if ((num_frames = WORLD_TO_TURN (
					square_root ((long)delta_x * delta_x
					+ (long)delta_y * delta_y)
					)) == 0)
				num_frames = 1;

			TargetVelocity = TargetPtr->velocity;
			GetNextVelocityComponents (&TargetVelocity,
					&delta_x, &delta_y, num_frames);

			delta_x = (TargetPtr->current.location.x + delta_x)
					- ElementPtr->current.location.x;
			delta_x = WRAP_DELTA_X (delta_x);
			delta_y = (TargetPtr->current.location.y + delta_y)
					- ElementPtr->current.location.y;
			delta_y = WRAP_DELTA_Y (delta_y);

			UnlockElement (hTarget);

			delta_x = NORMALIZE_FACING (
					ANGLE_TO_FACING (ARCTAN (delta_x, delta_y)) - facing
					);

			if (delta_x > 0)
			{
				if (delta_x <= ANGLE_TO_FACING (HALF_CIRCLE))
					++facing;
				else
					--facing;
			}

			SetVelocityVector (&ElementPtr->velocity,
					sentinelStats().speed, facing);
		}

		ElementPtr->turn_wait = TrackWait;
	}
}

static void
sentinel_collision (ELEMENT *ElementPtr0, POINT *pPt0,
		ELEMENT *ElementPtr1, POINT *pPt1)
{
	COUNT angle;
	STARSHIP *StarShipPtr;

	if (ElementPtr1->playerNr == NPC_PLAYER_NUM)
	{
		if (ElementPtr0->preprocess_func == ElementPtr1->preprocess_func
				&& !(ElementPtr0->state_flags & DEFY_PHYSICS)
				&& (pPt0->x != ElementPtr0->IntersectControl.IntersectStamp.origin.x
				|| pPt0->y != ElementPtr0->IntersectControl.IntersectStamp.origin.y))
		{
			angle = ARCTAN (pPt0->x - pPt1->x, pPt0->y - pPt1->y);

			SetVelocityComponents (&ElementPtr0->velocity,
					COSINE (angle, WORLD_TO_VELOCITY (sentinelStats().speed)),
					SINE (angle, WORLD_TO_VELOCITY (sentinelStats().speed)));
			ElementPtr0->turn_wait = TrackWait;
			ElementPtr0->state_flags |= COLLISION | DEFY_PHYSICS;
		}
	}
	else
	{
		BYTE old_hits;
		COUNT old_life;
		HELEMENT hBlastElement;

		old_hits = ElementPtr0->hit_points;
		old_life = ElementPtr0->life_span;
		ElementPtr0->blast_offset = 0;
		hBlastElement = weapon_collision (ElementPtr0, pPt0, ElementPtr1, pPt1);
		ElementPtr0->thrust_wait = 0;

		if ((ElementPtr1->state_flags & PLAYER_SHIP)
				&& ElementPtr1->crew_level
				&& !GRAVITY_MASS (ElementPtr1->mass_points + 1))
		{
			SIZE cur_delta_x, cur_delta_y;

			ElementPtr0->life_span = old_life;
			ElementPtr0->hit_points = old_hits;
			ElementPtr0->state_flags &= ~DISAPPEARING;
			ElementPtr0->state_flags |= DEFY_PHYSICS;
			ElementPtr0->turn_wait = (ONE_SECOND / BATTLE_FRAME_RATE) >> 1;

			GetElementStarShip (ElementPtr1, &StarShipPtr);
			StarShipPtr->cur_status_flags &=
					~(SHIP_AT_MAX_SPEED | SHIP_BEYOND_MAX_SPEED);
			if (ElementPtr1->turn_wait < COLLISION_TURN_WAIT)
				ElementPtr1->turn_wait += COLLISION_TURN_WAIT;
			if (ElementPtr1->thrust_wait < COLLISION_THRUST_WAIT)
				ElementPtr1->thrust_wait += COLLISION_THRUST_WAIT;

			angle = GetVelocityTravelAngle (&ElementPtr0->velocity);
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

			ZeroVelocityComponents (&ElementPtr0->velocity);
		}

		if (ElementPtr0->state_flags & DISAPPEARING)
		{
			GetElementStarShip (ElementPtr0, &StarShipPtr);
			--StarShipPtr->RaceDescPtr->characteristics.special_wait;
			if (hBlastElement)
			{
				ELEMENT *BlastElementPtr;

				LockElement (hBlastElement, &BlastElementPtr);
				BlastElementPtr->life_span = 6;
				BlastElementPtr->current.image.frame =
						SetAbsFrameIndex (
						BlastElementPtr->current.image.farray[0], 6
						);
				UnlockElement (hBlastElement);
			}
		}
	}
}

static void
samatra_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
		COUNT ConcernCounter)
{
	ship_intelligence (ShipPtr, ObjectsOfConcern, ConcernCounter);
}

static void
samatra_postprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if (StarShipPtr->RaceDescPtr->characteristics.max_thrust)
	{
		if (StarShipPtr->weapon_counter == 0
				&& StarShipPtr->RaceDescPtr->characteristics.weapon_wait < MaxComets
				&& spawn_comet (ElementPtr))
		{
			StarShipPtr->weapon_counter = tuning().ship.weaponWait;
		}

		if (StarShipPtr->special_counter == 0
				&& StarShipPtr->RaceDescPtr->characteristics.special_wait < MaxSentinels)
		{
			MISSILE_BLOCK MissileBlock;
			HELEMENT hSentinel;

			MissileBlock.cx = ElementPtr->next.location.x;
			MissileBlock.cy = ElementPtr->next.location.y;
			MissileBlock.farray = StarShipPtr->RaceDescPtr->ship_data.weapon;
			MissileBlock.face = 0;
			MissileBlock.index = 0;
			MissileBlock.sender = ElementPtr->playerNr;
			MissileBlock.flags = 0;
			MissileBlock.pixoffs = 0;
			MissileBlock.speed = sentinelStats().speed;
			MissileBlock.hit_points = sentinelStats().hitPoints;
			MissileBlock.damage = sentinelStats().damage;
			MissileBlock.life = sentinelStats().lifetime;
			MissileBlock.preprocess_func = sentinel_preprocess;
			MissileBlock.blast_offs = SentinelOffset;
			hSentinel = initialize_missile (&MissileBlock);

			if (hSentinel)
			{
				ELEMENT *SentinelPtr;

				LockElement (hSentinel, &SentinelPtr);
				SentinelPtr->collision_func = sentinel_collision;
				SentinelPtr->turn_wait = TrackWait + 2;
				SetElementStarShip (SentinelPtr, StarShipPtr);
				UnlockElement (hSentinel);

				StarShipPtr->special_counter = tuning().ship.specialWait;

				PutElement (hSentinel);
			}
		}
	}
}

static void
samatra_preprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	StarShipPtr->RaceDescPtr->characteristics.weapon_wait = 0;
	StarShipPtr->RaceDescPtr->characteristics.special_wait = 0;
	if (!(ElementPtr->state_flags & APPEARING))
	{
		++ElementPtr->turn_wait;
		++ElementPtr->thrust_wait;
	}
	else
	{
		POINT offs[] =
		{
			{-127-9,  -53+18},
			{ -38-9,  -88+18},
			{  44-9,  -85+18},
			{ 127-9,  -60+18},
			{ 124-9,   28+18},
			{  73-9,   61+18},
			{ -87-9,   58+18},
			{-136-9,   29+18},
		};

		for (StarShipPtr->RaceDescPtr->characteristics.max_thrust = 0;
				StarShipPtr->RaceDescPtr->characteristics.max_thrust < MaxGenerators;
				++StarShipPtr->RaceDescPtr->characteristics.max_thrust)
		{
			HELEMENT hGenerator;

			hGenerator = AllocElement ();
			if (hGenerator)
			{
				ELEMENT *GeneratorPtr;

				LockElement (hGenerator, &GeneratorPtr);
				GeneratorPtr->hit_points = GeneratorHits;
				GeneratorPtr->mass_points = MAX_SHIP_MASS * 10;
				GeneratorPtr->life_span = NORMAL_LIFE;
				GeneratorPtr->playerNr = ElementPtr->playerNr;
				GeneratorPtr->state_flags = APPEARING | IGNORE_SIMILAR;
				SetPrimType (
						&GLOBAL (DisplayArray[GeneratorPtr->PrimIndex]),
						STAMP_PRIM
						);
				GeneratorPtr->current.location.x =
						((LOG_SPACE_WIDTH >> 1)
						+ DISPLAY_TO_WORLD (offs[StarShipPtr->RaceDescPtr->characteristics.max_thrust].x))
						& ~((SCALED_ONE << MAX_VIS_REDUCTION) - 1);
				GeneratorPtr->current.location.y =
						((LOG_SPACE_HEIGHT >> 1)
						+ DISPLAY_TO_WORLD (offs[StarShipPtr->RaceDescPtr->characteristics.max_thrust].y))
						& ~((SCALED_ONE << MAX_VIS_REDUCTION) - 1);
				GeneratorPtr->current.image.farray =
						StarShipPtr->RaceDescPtr->ship_data.special;
				GeneratorPtr->current.image.frame =
						SetAbsFrameIndex (
								StarShipPtr->RaceDescPtr->ship_data.special[0],
								(BYTE)TFB_Random () % 10
								);

				GeneratorPtr->preprocess_func = generator_preprocess;
				GeneratorPtr->collision_func = generator_collision;
				GeneratorPtr->death_func = generator_death;

				SetElementStarShip (GeneratorPtr, StarShipPtr);
				UnlockElement (hGenerator);

				InsertElement (hGenerator, GetHeadElement ());
			}
		}

		{
			HELEMENT hTurret;

			hTurret = AllocElement ();
			if (hTurret)
			{
				ELEMENT *TurretPtr;

				LockElement (hTurret, &TurretPtr);
				TurretPtr->hit_points = 1;
				TurretPtr->life_span = NORMAL_LIFE;
				TurretPtr->playerNr = ElementPtr->playerNr;
				TurretPtr->state_flags = APPEARING | IGNORE_SIMILAR | NONSOLID;
				SetPrimType (
						&GLOBAL (DisplayArray[TurretPtr->PrimIndex]),
						STAMP_PRIM
						);
				TurretPtr->current.location.x = LOG_SPACE_WIDTH >> 1;
				TurretPtr->current.location.y = LOG_SPACE_HEIGHT >> 1;
				TurretPtr->current.image.farray =
						StarShipPtr->RaceDescPtr->ship_data.ship;
				TurretPtr->current.image.frame =
						SetAbsFrameIndex (
						StarShipPtr->RaceDescPtr->ship_data.ship[0], 1
						);

				TurretPtr->preprocess_func = turret_preprocess;

				SetElementStarShip (TurretPtr, StarShipPtr);
				UnlockElement (hTurret);

				InsertElement (hTurret, GetSuccElement (ElementPtr));
			}
		}

		{
			HELEMENT hGate;

			hGate = AllocElement ();
			if (hGate)
			{
				ELEMENT *GatePtr;

				LockElement (hGate, &GatePtr);
				GatePtr->hit_points = GateHits;
				GatePtr->mass_points = GateDamage;
				GatePtr->life_span = 2;
				GatePtr->playerNr = ElementPtr->playerNr;
				GatePtr->state_flags = APPEARING | FINITE_LIFE
						| IGNORE_SIMILAR;
				SetPrimType (
						&GLOBAL (DisplayArray[GatePtr->PrimIndex]),
						STAMP_PRIM
						);
				GatePtr->current.location.x = LOG_SPACE_WIDTH >> 1;
				GatePtr->current.location.y = LOG_SPACE_HEIGHT >> 1;
				GatePtr->current.image.farray =
						StarShipPtr->RaceDescPtr->ship_data.ship;
				GatePtr->current.image.frame =
						SetAbsFrameIndex (
						StarShipPtr->RaceDescPtr->ship_data.ship[0], 11
						);

				GatePtr->preprocess_func = gate_preprocess;
				GatePtr->collision_func = gate_collision;

				SetElementStarShip (GatePtr, StarShipPtr);
				UnlockElement (hGate);

				InsertElement (hGate, GetSuccElement (ElementPtr));
			}
		}

		StarShipPtr->weapon_counter = tuning().ship.weaponWait >> 1;
		StarShipPtr->special_counter = tuning().ship.specialWait >> 1;
	}
}

namespace {
class SaMatraShip final : public uqm::combat::Ship {
public:
    explicit SaMatraShip(const RACE_DESC &definition) noexcept
        : Ship(definition) {
        legacyData().cyborg_control.intelligence_func = samatra_intelligence;
    }
private:
    // This special ship has no ordinary primary-weapon initializer.
    COUNT createWeapons(ELEMENT &, uqm::combat::WeaponSlots &) noexcept override { return 0; }

    void beforeMotion(ELEMENT &element) noexcept override {
        samatra_preprocess(&element);
    }
    void afterMotion(ELEMENT &element) noexcept override {
        samatra_postprocess(&element);
    }
};
} // namespace

std::unique_ptr<uqm::combat::Ship>
uqm::combat::makeSaMatraShip()
{

	return std::make_unique<SaMatraShip>(samatraDefinition());
}

