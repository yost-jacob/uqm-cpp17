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
#include <optional>
#include "androsyn.h"
#include "resinst.h"

#include "libs/mathlib.h"

// Balance values are immutable, loaded from base/ships.json at startup.
static const uqm::combat::ShipTuning &tuning() {
    static const auto &value = uqm::combat::shipTuning("androsyn");
    return value;
}
static const uqm::combat::ProjectileStats &missileStats() {
    static const auto &value = tuning().projectile("missile");
    return value;
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto AndrosynthOffset = 14;
constexpr auto MissileOffset = 3;
constexpr auto TrackWait = 2;
constexpr auto BlazerDegeneration = (-1);
constexpr auto BlazerOffset = 10;
constexpr auto BlazerThrust = 60;
constexpr auto BlazerTurnWait = 1;
constexpr auto BlazerDamage = 3;
constexpr auto BlazerMass = 1;

static const RACE_DESC &androsynthDefinition() {
    static const RACE_DESC definition = {
        { /* SHIP_INFO */
            FIRES_FORE | SEEKING_WEAPON,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, tuning().ship.maxCrew,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            ANDROSYNTH_RACE_STRINGS,
            ANDROSYNTH_ICON_MASK_PMAP_ANIM,
            ANDROSYNTH_MICON_MASK_PMAP_ANIM,
            NULL, NULL, NULL
        },
        { /* FLEET_STUFF */
            INFINITE_RADIUS, /* Initial sphere of influence radius */
                    // XXX: Why infinite radius? Bug?
            { /* Known location (center of SoI) */
                MAX_X_UNIVERSE >> 1, MAX_Y_UNIVERSE >> 1,
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
                ANDROSYNTH_BIG_MASK_PMAP_ANIM,
                ANDROSYNTH_MED_MASK_PMAP_ANIM,
                ANDROSYNTH_SML_MASK_PMAP_ANIM,
            },
            {
                BUBBLE_BIG_MASK_PMAP_ANIM,
                BUBBLE_MED_MASK_PMAP_ANIM,
                BUBBLE_SML_MASK_PMAP_ANIM,
            },
            {
                BLAZER_BIG_MASK_PMAP_ANIM,
                BLAZER_MED_MASK_PMAP_ANIM,
                BLAZER_SML_MASK_PMAP_ANIM,
            },
            {
                ANDROSYNTH_CAPT_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
            ANDROSYNTH_VICTORY_SONG,
            ANDROSYNTH_SHIP_SOUNDS,
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            NULL, NULL
        },
        {
            0,
            uqm::combat::weaponRangeHint(LONG_RANGE_WEAPON >> 2),
            NULL,
        },

        0,
        0, /* CodeRef */
    };
    return definition;
}

// Private per-instance ship data
typedef struct
{
	ElementCollisionFunc* collision_func;
} ANDROSYNTH_DATA;

// Local typedef
typedef ANDROSYNTH_DATA CustomShipData_t;

// Retrieve race-specific ship data from a race desc
static CustomShipData_t *
GetCustomShipData (RACE_DESC *pRaceDesc)
{
	return static_cast<CustomShipData_t *>(pRaceDesc->data);
}

// Updates the concrete ship's optional value; no separate heap allocation.
static void SetCustomShipData(RACE_DESC *descriptor, const CustomShipData_t *data);

static void
blazer_collision (ELEMENT *ElementPtr0, POINT *pPt0,
		ELEMENT *ElementPtr1, POINT *pPt1)
{
	BYTE old_offs;
	COUNT old_crew_level;
	COUNT old_life;

	old_crew_level = ElementPtr0->crew_level;
	old_life = ElementPtr0->life_span;
	old_offs = ElementPtr0->blast_offset;
	ElementPtr0->blast_offset = BlazerOffset;
	ElementPtr0->mass_points = BlazerDamage;
	weapon_collision (ElementPtr0, pPt0, ElementPtr1, pPt1);
	ElementPtr0->mass_points = BlazerMass;
	ElementPtr0->blast_offset = old_offs;
	ElementPtr0->life_span = old_life;
	ElementPtr0->crew_level = old_crew_level;

	ElementPtr0->state_flags &= ~(DISAPPEARING | NONSOLID);
	collision (ElementPtr0, pPt0, ElementPtr1, pPt1);
}

static void
bubble_preprocess (ELEMENT *ElementPtr)
{
	BYTE thrust_wait, turn_wait;

	thrust_wait = HINIBBLE (ElementPtr->turn_wait);
	turn_wait = LONIBBLE (ElementPtr->turn_wait);
	if (thrust_wait > 0)
		--thrust_wait;
	else
	{
		ElementPtr->next.image.frame =
				IncFrameIndex (ElementPtr->current.image.frame);
		ElementPtr->state_flags |= CHANGING;

		thrust_wait = (BYTE)((COUNT)TFB_Random () & 3);
	}

	if (turn_wait > 0)
		--turn_wait;
	else
	{
		COUNT facing;
		SIZE delta_facing;

		facing = NORMALIZE_FACING (ANGLE_TO_FACING (
				GetVelocityTravelAngle (&ElementPtr->velocity)));
		if ((delta_facing = TrackShip (ElementPtr, &facing)) == -1)
			facing = (COUNT)TFB_Random ();
		else if (delta_facing <= ANGLE_TO_FACING (HALF_CIRCLE))
			facing += (COUNT)TFB_Random () & (ANGLE_TO_FACING (HALF_CIRCLE) - 1);
		else
			facing -= (COUNT)TFB_Random () & (ANGLE_TO_FACING (HALF_CIRCLE) - 1);
		SetVelocityVector (&ElementPtr->velocity,
				missileStats().speed, facing);
		turn_wait = TrackWait;
	}

	ElementPtr->turn_wait = MAKE_BYTE (turn_wait, thrust_wait);
}

static COUNT
initialize_bubble (ELEMENT *ShipPtr, HELEMENT BubbleArray[])
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
	MissileBlock.pixoffs = AndrosynthOffset;
	MissileBlock.speed = missileStats().speed;
	MissileBlock.hit_points = missileStats().hitPoints;
	MissileBlock.damage = missileStats().damage;
	MissileBlock.life = missileStats().lifetime;
	MissileBlock.preprocess_func = bubble_preprocess;
	MissileBlock.blast_offs = MissileOffset;
	BubbleArray[0] = initialize_missile (&MissileBlock);

	if (BubbleArray[0])
	{
		ELEMENT *BubblePtr;

		LockElement (BubbleArray[0], &BubblePtr);
		BubblePtr->turn_wait = 0;
		UnlockElement (BubbleArray[0]);
	}

	return (1);
}

static void
androsynth_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
		COUNT ConcernCounter)
{
	EVALUATE_DESC *lpEvalDesc;
	STARSHIP *StarShipPtr;

	GetElementStarShip (ShipPtr, &StarShipPtr);

	lpEvalDesc = &ObjectsOfConcern[ENEMY_WEAPON_INDEX];
				/* in blazer form */
	if (ShipPtr->next.image.farray == StarShipPtr->RaceDescPtr->ship_data.special)
	{
		ObjectsOfConcern[CREW_OBJECT_INDEX].ObjectPtr = 0;
		if (lpEvalDesc->ObjectPtr && lpEvalDesc->MoveState == ENTICE)
		{
			if ((lpEvalDesc->ObjectPtr->state_flags & FINITE_LIFE)
					&& !(lpEvalDesc->ObjectPtr->state_flags & CREW_OBJECT))
				lpEvalDesc->MoveState = AVOID;
			else
				lpEvalDesc->ObjectPtr = 0;
		}

		ship_intelligence (ShipPtr, ObjectsOfConcern, ConcernCounter);
	}
	else
	{
		STARSHIP *pEnemyStarShip = NULL;

		lpEvalDesc = &ObjectsOfConcern[ENEMY_SHIP_INDEX];
		if (lpEvalDesc->ObjectPtr)
		{
			GetElementStarShip (lpEvalDesc->ObjectPtr, &pEnemyStarShip);
			if (lpEvalDesc->which_turn <= 16
					&& (StarShipPtr->special_counter > 0
					|| StarShipPtr->RaceDescPtr->ship_info.energy_level < tuning().ship.maxEnergy / 3
					|| ((WEAPON_RANGE (&pEnemyStarShip->RaceDescPtr->cyborg_control) <= CLOSE_RANGE_WEAPON
					&& lpEvalDesc->ObjectPtr->crew_level > BlazerDamage)
					|| (lpEvalDesc->ObjectPtr->crew_level > (BlazerDamage * 3)
					&& MANEUVERABILITY (&pEnemyStarShip->RaceDescPtr->cyborg_control) > SLOW_SHIP))))
				lpEvalDesc->MoveState = ENTICE;
		}

		ship_intelligence (ShipPtr, ObjectsOfConcern, ConcernCounter);

		if (StarShipPtr->special_counter == 0)
		{
			StarShipPtr->ship_input_state &= ~SPECIAL;
			if ((ObjectsOfConcern[ENEMY_WEAPON_INDEX].ObjectPtr
					&& ObjectsOfConcern[ENEMY_WEAPON_INDEX].which_turn <= 4)
					|| (lpEvalDesc->ObjectPtr
					&& StarShipPtr->RaceDescPtr->ship_info.energy_level >= tuning().ship.maxEnergy / 3
					&& (WEAPON_RANGE (&pEnemyStarShip->RaceDescPtr->cyborg_control) >=
					WEAPON_RANGE (&StarShipPtr->RaceDescPtr->cyborg_control) << 1
					|| (lpEvalDesc->which_turn < 16
					&& (WEAPON_RANGE (&pEnemyStarShip->RaceDescPtr->cyborg_control) > CLOSE_RANGE_WEAPON
					|| lpEvalDesc->ObjectPtr->crew_level <= BlazerDamage)
					&& (lpEvalDesc->ObjectPtr->crew_level <= (BlazerDamage * 3)
					|| MANEUVERABILITY (&pEnemyStarShip->RaceDescPtr->cyborg_control) <=
					SLOW_SHIP)))))
				StarShipPtr->ship_input_state |= SPECIAL;
		}

		if (!(StarShipPtr->ship_input_state & SPECIAL)
				&& StarShipPtr->weapon_counter == 0
				&& lpEvalDesc->ObjectPtr)
		{
			if (lpEvalDesc->which_turn <= 4)
				StarShipPtr->ship_input_state |= WEAPON;
			else if (lpEvalDesc->MoveState != PURSUE
					&& lpEvalDesc->which_turn <= 12)
			{
				COUNT travel_facing, direction_facing;
				SIZE delta_x, delta_y,
							ship_delta_x, ship_delta_y,
							other_delta_x, other_delta_y;

				GetCurrentVelocityComponents (&ShipPtr->velocity,
						&ship_delta_x, &ship_delta_y);
				GetCurrentVelocityComponents (&lpEvalDesc->ObjectPtr->velocity,
						&other_delta_x, &other_delta_y);
				delta_x = ship_delta_x - other_delta_x;
				delta_y = ship_delta_y - other_delta_y;
				travel_facing = ARCTAN (delta_x, delta_y);

				delta_x =
						lpEvalDesc->ObjectPtr->next.location.x -
						ShipPtr->next.location.x;
				delta_y =
						lpEvalDesc->ObjectPtr->next.location.y -
						ShipPtr->next.location.y;
				direction_facing = ARCTAN (delta_x, delta_y);

				if (NORMALIZE_ANGLE (travel_facing
						- direction_facing + OCTANT) <= QUADRANT)
					StarShipPtr->ship_input_state |= WEAPON;
			}
		}
	}
}

static void
androsynth_postprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
			/* take care of blazer effect */
	if (ElementPtr->next.image.farray == StarShipPtr->RaceDescPtr->ship_data.special)
	{
		if ((StarShipPtr->cur_status_flags & SPECIAL)
				|| StarShipPtr->RaceDescPtr->ship_info.energy_level == 0)
		{
			StarShipPtr->RaceDescPtr->characteristics.energy_regeneration =
					(BYTE)BlazerDegeneration;
			StarShipPtr->energy_counter = tuning().ship.energyWait;

			if (StarShipPtr->cur_status_flags & SPECIAL)
			{
				ProcessSound (SetAbsSoundIndex (
						StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 1),
						ElementPtr);  /* COMET_ON */
				ElementPtr->turn_wait = 0;
				ElementPtr->thrust_wait = 0;
				StarShipPtr->RaceDescPtr->characteristics.special_wait =
						StarShipPtr->RaceDescPtr->characteristics.turn_wait;
				ElementPtr->mass_points = BlazerMass;
				StarShipPtr->RaceDescPtr->characteristics.turn_wait
						= BlazerTurnWait;

				/* Save the current collision func because we were not the
				 * ones who set it */
				{
					const ANDROSYNTH_DATA shipData = { ElementPtr->collision_func };
					SetCustomShipData (StarShipPtr->RaceDescPtr, &shipData);
					ElementPtr->collision_func = blazer_collision;
				}
			}
		}

		if (StarShipPtr->RaceDescPtr->ship_info.energy_level == 0)
				/* if blazer wasn't able to change back into ship
				 * give it a little more juice to try to get out
				 * of its predicament.
				 */
		{
			DeltaEnergy (ElementPtr, -BlazerDegeneration);
			StarShipPtr->energy_counter = 1;
		}
	}
}

static void
androsynth_preprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;
	STATUS_FLAGS cur_status_flags;

	GetElementStarShip (ElementPtr, &StarShipPtr);

	cur_status_flags = StarShipPtr->cur_status_flags;
	if (ElementPtr->next.image.farray == StarShipPtr->RaceDescPtr->ship_data.ship)
	{
		if (cur_status_flags & SPECIAL)
		{
			if (StarShipPtr->RaceDescPtr->ship_info.energy_level < tuning().ship.specialEnergyCost)
				DeltaEnergy (ElementPtr, -tuning().ship.specialEnergyCost); /* so text will flash */
			else
			{
				cur_status_flags &= ~WEAPON;

				ElementPtr->next.image.farray =
						StarShipPtr->RaceDescPtr->ship_data.special;
				ElementPtr->next.image.frame =
						SetEquFrameIndex (StarShipPtr->RaceDescPtr->ship_data.special[0],
						ElementPtr->next.image.frame);
				ElementPtr->state_flags |= CHANGING;
			}
		}
	}
	else
	{
		cur_status_flags &= ~(THRUST | WEAPON | SPECIAL);

					/* protection against vux */
		if (StarShipPtr->RaceDescPtr->characteristics.turn_wait > BlazerTurnWait)
		{
			StarShipPtr->RaceDescPtr->characteristics.special_wait +=
					StarShipPtr->RaceDescPtr->characteristics.turn_wait
					- BlazerTurnWait;
			StarShipPtr->RaceDescPtr->characteristics.turn_wait = BlazerTurnWait;
		}

		if (StarShipPtr->RaceDescPtr->ship_info.energy_level == 0)
		{
			ZeroVelocityComponents (&ElementPtr->velocity);
			cur_status_flags &= ~(LEFT | RIGHT
					| SHIP_AT_MAX_SPEED | SHIP_BEYOND_MAX_SPEED);

			StarShipPtr->RaceDescPtr->characteristics.turn_wait =
					StarShipPtr->RaceDescPtr->characteristics.special_wait;
			StarShipPtr->RaceDescPtr->characteristics.energy_regeneration = tuning().ship.energyRegeneration;
			ElementPtr->mass_points = tuning().ship.shipMass;
			ElementPtr->collision_func = 
					GetCustomShipData (StarShipPtr->RaceDescPtr)->collision_func;
			ElementPtr->next.image.farray =
					StarShipPtr->RaceDescPtr->ship_data.ship;
			ElementPtr->next.image.frame =
					SetEquFrameIndex (StarShipPtr->RaceDescPtr->ship_data.ship[0],
					ElementPtr->next.image.frame);
			ElementPtr->state_flags |= CHANGING;
		}
		else
		{
			if (ElementPtr->thrust_wait)
				--ElementPtr->thrust_wait;
			else
			{
				COUNT facing;

				facing = StarShipPtr->ShipFacing;
				if (ElementPtr->turn_wait == 0
						&& (cur_status_flags & (LEFT | RIGHT)))
				{
					if (cur_status_flags & LEFT)
						--facing;
					else
						++facing;
				}

				SetVelocityVector (&ElementPtr->velocity,
						BlazerThrust, NORMALIZE_FACING (facing));
				cur_status_flags |= SHIP_AT_MAX_SPEED | SHIP_BEYOND_MAX_SPEED;
			}
		}
	}
	StarShipPtr->cur_status_flags = cur_status_flags;
}

static void
uninit_androsynth (RACE_DESC *pRaceDesc)
{
	SetCustomShipData (pRaceDesc, NULL);
}

namespace {
class AndrosynthShip final : public uqm::combat::Ship {
public:
    explicit AndrosynthShip(const RACE_DESC &definition) noexcept
        : Ship(definition) {
        legacyData().cyborg_control.intelligence_func = androsynth_intelligence;
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
        androsynth_preprocess(&element);
    }
    void afterMotion(ELEMENT &element) noexcept override {
        androsynth_postprocess(&element);
    }
    COUNT createWeapons(ELEMENT &element, uqm::combat::WeaponSlots &weapons) noexcept override {
        return initialize_bubble(&element, weapons.data());
    }
    void onFinish() noexcept override {
        uninit_androsynth(&legacyData());
    }
    std::optional<CustomShipData_t> privateData_;
};
} // namespace

static void SetCustomShipData(RACE_DESC *descriptor, const CustomShipData_t *data)
{
    static_cast<AndrosynthShip &>(uqm::combat::shipFor(*descriptor)).setData(data);
}

RACE_DESC*
init_androsynth (void) try
{

	return uqm::combat::releaseToLegacy(std::make_unique<AndrosynthShip>(androsynthDefinition()));
} catch (...) {
    uqm::combat::logShipCreationFailure("AndrosynthShip");
    return nullptr;
}

