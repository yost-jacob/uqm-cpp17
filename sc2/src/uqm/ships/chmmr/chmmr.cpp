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
#include "resinst.h"
#include "uqm/combat_entity_legacy.hpp"
#include "uqm/animation_effect.hpp"

#include "uqm/colors.h"
#include "uqm/globdata.h"
#include "libs/mathlib.h"

// Balance values are immutable, loaded from base/ships.json for the active session.
static const uqm::combat::ShipTuning &tuning() {
    return uqm::combat::shipTuning("chmmr");
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto ChmmrOffset = 18;
constexpr auto LaserRange = DISPLAY_TO_WORLD (150);
constexpr auto NumCycles = 4;
constexpr auto NumShadows = 5;
constexpr auto NumSatellites = 3;
constexpr auto SatelliteOffset = DISPLAY_TO_WORLD (64);
constexpr auto SatelliteHitpoints = 10;
constexpr auto SatelliteMass = 10;
constexpr auto DefenseRange = (UWORD)64;
constexpr auto DefenseWait = 2;

static RACE_DESC chmmrDefinition() {
    const RACE_DESC definition = {
        { /* SHIP_INFO */
            FIRES_FORE | IMMEDIATE_WEAPON | SEEKING_SPECIAL | POINT_DEFENSE,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, tuning().ship.maxCrew,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            CHMMR_RACE_STRINGS,
            CHMMR_ICON_MASK_PMAP_ANIM,
            CHMMR_MICON_MASK_PMAP_ANIM,
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
                CHMMR_BIG_MASK_PMAP_ANIM,
                CHMMR_MED_MASK_PMAP_ANIM,
                CHMMR_SML_MASK_PMAP_ANIM,
            },
            {
                MUZZLE_BIG_MASK_PMAP_ANIM,
                MUZZLE_MED_MASK_PMAP_ANIM,
                MUZZLE_SML_MASK_PMAP_ANIM,
            },
            {
                SATELLITE_BIG_MASK_PMAP_ANIM,
                SATELLITE_MED_MASK_PMAP_ANIM,
                SATELLITE_SML_MASK_PMAP_ANIM,
            },
            {
                CHMMR_CAPTAIN_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
            CHMMR_VICTORY_SONG,
            CHMMR_SHIP_SOUNDS,
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




// Native autonomous unit; all engine records are borrowed for each hook.
namespace {
class ChmmrBeam final : public uqm::combat::Beam {
public:
    bool hasExpiration() const noexcept override { return true; }
    void expire(ELEMENT &self) override;
};
}
void ChmmrBeam::expire(ELEMENT &self) {
    auto *ElementPtr = &self;
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	StarShipPtr->special_counter = ElementPtr->turn_wait;

	if (StarShipPtr->hShip)
	{
		SIZE dx, dy;
		long dist;
		HELEMENT hIonSpots;
		ELEMENT *ShipPtr;

		LockElement (StarShipPtr->hShip, &ShipPtr);

		dx = ElementPtr->current.location.x
				- ShipPtr->current.location.x;
		dy = ElementPtr->current.location.y
				- ShipPtr->current.location.y;
		if (((BYTE)TFB_Random () & 0x07)
				&& (dist = (long)dx * dx + (long)dy * dy) >=
				(long)DISPLAY_TO_WORLD (ChmmrOffset + 10)
				* DISPLAY_TO_WORLD (ChmmrOffset + 10)
				&& (hIonSpots = AllocElement ()))
		{
			COUNT angle, magnitude;
			ELEMENT *IonSpotsPtr;

			LockElement (hIonSpots, &IonSpotsPtr);
			IonSpotsPtr->playerNr = ElementPtr->playerNr;
			IonSpotsPtr->state_flags = FINITE_LIFE | NONSOLID
					| IGNORE_SIMILAR | APPEARING;
			IonSpotsPtr->turn_wait = IonSpotsPtr->next_turn = 0;
			IonSpotsPtr->life_span = 9;

			angle = ARCTAN (dx, dy);
			magnitude = ((COUNT)TFB_Random ()
					% ((square_root (dist) + 1)
					- DISPLAY_TO_WORLD (ChmmrOffset + 10)))
					+ DISPLAY_TO_WORLD (ChmmrOffset + 10);
			IonSpotsPtr->current.location.x =
					ShipPtr->current.location.x
					+ COSINE (angle, magnitude);
			IonSpotsPtr->current.location.y =
					ShipPtr->current.location.y
					+ SINE (angle, magnitude);
			IonSpotsPtr->current.image.farray =
					StarShipPtr->RaceDescPtr->ship_data.weapon;
			IonSpotsPtr->current.image.frame = SetAbsFrameIndex (
					StarShipPtr->RaceDescPtr->ship_data.weapon[0],
					ANGLE_TO_FACING (FULL_CIRCLE) << 1
					);

			uqm::combat::attachAnimation(*IonSpotsPtr);

			SetElementStarShip (IonSpotsPtr, StarShipPtr);

			SetPrimType (&(GLOBAL (DisplayArray))[
					IonSpotsPtr->PrimIndex
					], STAMP_PRIM);

			UnlockElement (hIonSpots);
			PutElement (hIonSpots);
		}

		UnlockElement (StarShipPtr->hShip);
	}

}


static COUNT
initialize_megawatt_laser (ELEMENT *ShipPtr, HELEMENT LaserArray[])
{
	RECT r;
	STARSHIP *StarShipPtr;
	uqm::combat::BeamLaunch beam{};
	static const Color cycle_array[] =
	{
		BUILD_COLOR (MAKE_RGB15_INIT (0x17, 0x00, 0x00), 0x2B),
		BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x03, 0x00), 0x7F),
		BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x11, 0x00), 0x7B),
		BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x03, 0x00), 0x7F),
	};

	GetElementStarShip (ShipPtr, &StarShipPtr);
	beam.facing = StarShipPtr->ShipFacing;
	GetFrameRect (SetAbsFrameIndex (
			StarShipPtr->RaceDescPtr->ship_data.weapon[0], beam.facing
			), &r);
	beam.origin.x = DISPLAY_ALIGN (ShipPtr->next.location.x)
			+ DISPLAY_TO_WORLD (r.corner.x);
	beam.origin.y = DISPLAY_ALIGN (ShipPtr->next.location.y)
			+ DISPLAY_TO_WORLD (r.corner.y);
	beam.displacement.x = COSINE (FACING_TO_ANGLE (beam.facing), LaserRange);
	beam.displacement.y = SINE (FACING_TO_ANGLE (beam.facing), LaserRange);
	beam.player = ShipPtr->playerNr;
	beam.flags = IGNORE_SIMILAR;
	beam.muzzleOffset = 0;
	beam.color = cycle_array[StarShipPtr->special_counter];
	LaserArray[0] = uqm::combat::publishBeam(beam, std::make_unique<ChmmrBeam>());

	if (LaserArray[0])
	{
		ELEMENT *LaserPtr;

		LockElement (LaserArray[0], &LaserPtr);

		LaserPtr->mass_points = 2;
		LaserPtr->turn_wait = (BYTE)((StarShipPtr->special_counter + 1)
				% NumCycles);

		UnlockElement (LaserArray[0]);
	}

	return (1);
}

static void
chmmr_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
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
			&& !(StarShipPtr->ship_input_state & WEAPON)
			&& lpEvalDesc->which_turn > 12
			&& NORMALIZE_ANGLE (
					GetVelocityTravelAngle (&ShipPtr->velocity)
					- (GetVelocityTravelAngle (&lpEvalDesc->ObjectPtr->velocity)
					+ HALF_CIRCLE) + QUADRANT
					) > HALF_CIRCLE)
		StarShipPtr->ship_input_state |= SPECIAL;
}

static void
chmmr_postprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);

	if (StarShipPtr->cur_status_flags & WEAPON)
	{
		HELEMENT hMuzzleFlash;
		ELEMENT *MuzzleFlashPtr;

		LockElement (GetTailElement (), &MuzzleFlashPtr);
		if (MuzzleFlashPtr != ElementPtr
				&& elementsOfSamePlayer (MuzzleFlashPtr, ElementPtr)
				&& (MuzzleFlashPtr->state_flags & APPEARING)
				&& GetPrimType (&(GLOBAL (DisplayArray))[
						MuzzleFlashPtr->PrimIndex
						]) == LINE_PRIM
				&& !(StarShipPtr->special_counter & 1)
				&& (hMuzzleFlash = AllocElement ()))
		{
			LockElement (hMuzzleFlash, &MuzzleFlashPtr);
			MuzzleFlashPtr->playerNr = ElementPtr->playerNr;
			MuzzleFlashPtr->state_flags = FINITE_LIFE | NONSOLID
					| IGNORE_SIMILAR | APPEARING;
			MuzzleFlashPtr->life_span = 1;

			MuzzleFlashPtr->current = ElementPtr->next;
			MuzzleFlashPtr->current.image.farray =
					StarShipPtr->RaceDescPtr->ship_data.weapon;
			MuzzleFlashPtr->current.image.frame = SetAbsFrameIndex (
					StarShipPtr->RaceDescPtr->ship_data.weapon[0],
					StarShipPtr->ShipFacing + ANGLE_TO_FACING (FULL_CIRCLE)
					);

			SetElementStarShip (MuzzleFlashPtr, StarShipPtr);

			SetPrimType (&(GLOBAL (DisplayArray))[
					MuzzleFlashPtr->PrimIndex
					], STAMP_PRIM);

			UnlockElement (hMuzzleFlash);
			PutElement (hMuzzleFlash);
		}
		UnlockElement (GetTailElement ());
	}

	if ((StarShipPtr->cur_status_flags & SPECIAL)
			&& DeltaEnergy (ElementPtr, -tuning().ship.specialEnergyCost))
	{
		COUNT facing;
		ELEMENT *ShipElementPtr;

		LockElement (ElementPtr->hTarget, &ShipElementPtr);

		ProcessSound (SetAbsSoundIndex (
				StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 1),
				ShipElementPtr);

		UnlockElement (ElementPtr->hTarget);

		facing = 0;
		if (TrackShip (ElementPtr, &facing) >= 0)
		{
			ELEMENT *ShipElementPtr;

			LockElement (ElementPtr->hTarget, &ShipElementPtr);
			if (!GRAVITY_MASS (ShipElementPtr->mass_points + 1))
			{
				SIZE i, dx, dy;
				COUNT angle, magnitude;
				STARSHIP *EnemyStarShipPtr;
				static const SIZE shadow_offs[] =
				{
					DISPLAY_TO_WORLD (8),
					DISPLAY_TO_WORLD (8 + 9),
					DISPLAY_TO_WORLD (8 + 9 + 11),
					DISPLAY_TO_WORLD (8 + 9 + 11 + 14),
					DISPLAY_TO_WORLD (8 + 9 + 11 + 14 + 18),
				};
				static const Color color_tab[] =
				{
					BUILD_COLOR (MAKE_RGB15_INIT (0x00, 0x00, 0x10), 0x53),
					BUILD_COLOR (MAKE_RGB15_INIT (0x00, 0x00, 0x0E), 0x54),
					BUILD_COLOR (MAKE_RGB15_INIT (0x00, 0x00, 0x0C), 0x55),
					BUILD_COLOR (MAKE_RGB15_INIT (0x00, 0x00, 0x09), 0x56),
					BUILD_COLOR (MAKE_RGB15_INIT (0x00, 0x00, 0x07), 0x57),
				};
				DWORD current_speed, max_speed;

				// calculate tractor beam effect
				angle = FACING_TO_ANGLE (StarShipPtr->ShipFacing);
				dx = (ElementPtr->next.location.x
						+ COSINE (angle, (LaserRange / 3)
						+ DISPLAY_TO_WORLD (ChmmrOffset)))
						- ShipElementPtr->next.location.x;
				dy = (ElementPtr->next.location.y
						+ SINE (angle, (LaserRange / 3)
						+ DISPLAY_TO_WORLD (ChmmrOffset)))
						- ShipElementPtr->next.location.y;
				angle = ARCTAN (dx, dy);
				magnitude = WORLD_TO_VELOCITY (12) /
						ShipElementPtr->mass_points;
				DeltaVelocityComponents (&ShipElementPtr->velocity,
						COSINE (angle, magnitude), SINE (angle, magnitude));

				GetCurrentVelocityComponents (&ShipElementPtr->velocity,
						&dx, &dy);
				GetElementStarShip (ShipElementPtr, &EnemyStarShipPtr);

				// set the effected ship's speed flags
				current_speed = VelocitySquared (dx, dy);
				max_speed = VelocitySquared (WORLD_TO_VELOCITY (
						EnemyStarShipPtr->RaceDescPtr->characteristics.max_thrust),
						0);
				EnemyStarShipPtr->cur_status_flags &= ~(SHIP_AT_MAX_SPEED
						| SHIP_BEYOND_MAX_SPEED);
				if (current_speed > max_speed)
					EnemyStarShipPtr->cur_status_flags |= (SHIP_AT_MAX_SPEED
							| SHIP_BEYOND_MAX_SPEED);
				else if (current_speed == max_speed)
					EnemyStarShipPtr->cur_status_flags |= SHIP_AT_MAX_SPEED;

				// add tractor beam graphical effects
				for (i = 0; i < NumShadows; ++i)
				{
					HELEMENT hShadow;

					hShadow = AllocElement ();
					if (hShadow)
					{
						ELEMENT *ShadowElementPtr;

						LockElement (hShadow, &ShadowElementPtr);
						ShadowElementPtr->playerNr = ShipElementPtr->playerNr;
						ShadowElementPtr->state_flags = FINITE_LIFE | NONSOLID
								| IGNORE_SIMILAR | POST_PROCESS;
						ShadowElementPtr->life_span = 1;

						ShadowElementPtr->current = ShipElementPtr->next;
						ShadowElementPtr->current.location.x +=
								COSINE (angle, shadow_offs[i]);
						ShadowElementPtr->current.location.y +=
								SINE (angle, shadow_offs[i]);
						ShadowElementPtr->next = ShadowElementPtr->current;

						SetElementStarShip (ShadowElementPtr, EnemyStarShipPtr);
						SetVelocityComponents (&ShadowElementPtr->velocity,
								dx, dy);

						SetPrimType (&(GLOBAL (DisplayArray))[
								ShadowElementPtr->PrimIndex
								], STAMPFILL_PRIM);
						SetPrimColor (&(GLOBAL (DisplayArray))[
								ShadowElementPtr->PrimIndex
								], color_tab[i]);

						UnlockElement (hShadow);
						InsertElement (hShadow, GetHeadElement ());
					}
				}
			}
			UnlockElement (ElementPtr->hTarget);
		}
	}

	StarShipPtr->special_counter = 0;
}


// Native autonomous unit; all engine records are borrowed for each hook.
namespace {
class Satellite final : public uqm::combat::CombatEntity {
public:
    bool dying_ = false;
    void update(ELEMENT &self) override;
    void postprocess(ELEMENT &self) override;
    void expire(ELEMENT &self) override;
    void collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) override;
};
}
void Satellite::update(ELEMENT &self) {
    auto *ElementPtr = &self;
    if (dying_) { uqm::combat::advanceAnimation(self); return; }
	STARSHIP *StarShipPtr;

	++ElementPtr->life_span;

	ElementPtr->next.image.frame =
			SetAbsFrameIndex (ElementPtr->current.image.frame,
			(GetFrameIndex (ElementPtr->current.image.frame) + 1) & 7);
	ElementPtr->state_flags |= CHANGING;

	ElementPtr->turn_wait = (BYTE)NORMALIZE_ANGLE (
			ElementPtr->turn_wait + 1
			);

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if (StarShipPtr->hShip)
	{
		SIZE dx, dy;
		ELEMENT *ShipPtr;

		StarShipPtr->RaceDescPtr->ship_info.ship_flags |= POINT_DEFENSE;

		LockElement (StarShipPtr->hShip, &ShipPtr);

		dx = (ShipPtr->next.location.x
				+ COSINE (ElementPtr->turn_wait, SatelliteOffset))
				- ElementPtr->current.location.x;
		dy = (ShipPtr->next.location.y
				+ SINE (ElementPtr->turn_wait, SatelliteOffset))
				- ElementPtr->current.location.y;
		dx = WRAP_DELTA_X (dx);
		dy = WRAP_DELTA_Y (dy);
		if ((long)dx * dx + (long)dy * dy
				<= DISPLAY_TO_WORLD (20L) * DISPLAY_TO_WORLD (20L))
			SetVelocityComponents (&ElementPtr->velocity,
					WORLD_TO_VELOCITY (dx),
					WORLD_TO_VELOCITY (dy));
		else
		{
			COUNT angle;

			angle = ARCTAN (dx, dy);
			SetVelocityComponents (&ElementPtr->velocity,
					COSINE (angle, WORLD_TO_VELOCITY (DISPLAY_TO_WORLD (20))),
					SINE (angle, WORLD_TO_VELOCITY (DISPLAY_TO_WORLD (20))));
		}

		UnlockElement (StarShipPtr->hShip);
	}

}



// Native autonomous unit; all engine records are borrowed for each hook.
namespace {
class PointDefense final : public uqm::combat::CombatEntity {
public:
    void update(ELEMENT &) override {}
    void expire(ELEMENT &self) override;
};
}
void PointDefense::expire(ELEMENT &self) {
    auto *ElementPtr = &self;
	BYTE weakest;
	UWORD best_dist;
	STARSHIP *StarShipPtr;
	HELEMENT hObject, hNextObject, hBestObject;
	ELEMENT *ShipPtr;
	ELEMENT *SattPtr;
	ELEMENT *ObjectPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	hBestObject = 0;
	best_dist = DefenseRange + 1;
	weakest = 255;
	LockElement (StarShipPtr->hShip, &ShipPtr);
	LockElement (ElementPtr->hTarget, &SattPtr);
	for (hObject = GetPredElement (ElementPtr);
			hObject; hObject = hNextObject)
	{
		LockElement (hObject, &ObjectPtr);
		hNextObject = GetPredElement (ObjectPtr);
		if (!elementsOfSamePlayer (ObjectPtr, ShipPtr)
				&& ObjectPtr->playerNr != NEUTRAL_PLAYER_NUM
				&& CollisionPossible (ObjectPtr, ShipPtr)
				&& !OBJECT_CLOAKED (ObjectPtr))
		{
			SIZE delta_x, delta_y;
			UWORD dist;

			delta_x = ObjectPtr->next.location.x
					- SattPtr->next.location.x;
			delta_y = ObjectPtr->next.location.y
					- SattPtr->next.location.y;
			if (delta_x < 0)
				delta_x = -delta_x;
			if (delta_y < 0)
				delta_y = -delta_y;
			delta_x = WORLD_TO_DISPLAY (delta_x);
			delta_y = WORLD_TO_DISPLAY (delta_y);
			if ((UWORD)delta_x <= DefenseRange &&
					(UWORD)delta_y <= DefenseRange &&
					(dist =
					(UWORD)delta_x * (UWORD)delta_x
					+ (UWORD)delta_y * (UWORD)delta_y) <=
					DefenseRange * DefenseRange
					&& (ObjectPtr->hit_points < weakest
					|| (ObjectPtr->hit_points == weakest
					&& dist < best_dist)))
			{
				hBestObject = hObject;
				best_dist = dist;
				weakest = ObjectPtr->hit_points;
			}
		}
		UnlockElement (hObject);
	}

	if (hBestObject)
	{
		uqm::combat::BeamLaunch beam{};
		HELEMENT hPointDefense;

		LockElement (hBestObject, &ObjectPtr);

		beam.origin.x = SattPtr->next.location.x;
		beam.origin.y = SattPtr->next.location.y;
		beam.facing = 0;
		beam.displacement.x = ObjectPtr->next.location.x
				- SattPtr->next.location.x;
		beam.displacement.y = ObjectPtr->next.location.y
				- SattPtr->next.location.y;
		beam.player = SattPtr->playerNr;
		beam.flags = IGNORE_SIMILAR;
		beam.muzzleOffset = 0;
		beam.color = BUILD_COLOR (MAKE_RGB15 (0x00, 0x01, 0x1F), 0x4D);
		hPointDefense = uqm::combat::publishBeam(beam);
		if (hPointDefense)
		{
			ELEMENT *PDPtr;

			LockElement (hPointDefense, &PDPtr);
			SetElementStarShip (PDPtr, StarShipPtr);
			PDPtr->hTarget = 0;
			UnlockElement (hPointDefense);

			PutElement (hPointDefense);

			SattPtr->thrust_wait = DefenseWait;
		}

		UnlockElement (hBestObject);
	}

	UnlockElement (ElementPtr->hTarget);
	UnlockElement (StarShipPtr->hShip);

}


void Satellite::postprocess(ELEMENT &self) {
    auto *ElementPtr = &self;
	STARSHIP *StarShipPtr;

	if (ElementPtr->thrust_wait || ElementPtr->life_span == 0)
		--ElementPtr->thrust_wait;
	else
	{
		HELEMENT hDefense;

		hDefense = AllocElement ();
		if (hDefense)
		{
			ELEMENT *DefensePtr;

			PutElement (hDefense);

			LockElement (hDefense, &DefensePtr);
			DefensePtr->playerNr = ElementPtr->playerNr;
			DefensePtr->state_flags = APPEARING | NONSOLID | FINITE_LIFE;

			{
				ELEMENT *SuccPtr;

				LockElement (GetSuccElement (ElementPtr), &SuccPtr);
				DefensePtr->hTarget = GetPredElement (SuccPtr);
				UnlockElement (GetSuccElement (ElementPtr));

				uqm::detail::attachEntity(*DefensePtr, std::make_unique<PointDefense>(), uqm::detail::EntityHooks::Expiration);
			}

			GetElementStarShip (ElementPtr, &StarShipPtr);
			SetElementStarShip (DefensePtr, StarShipPtr);

			UnlockElement (hDefense);
		}
	}

}


void Satellite::collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) {
    auto *ElementPtr0 = &self;
    auto *pPt0 = &point;
    auto *ElementPtr1 = &other;
    auto *pPt1 = &otherPoint;
	(void) ElementPtr0;  /* Satisfying compiler (unused parameter) */
	(void) pPt0;  /* Satisfying compiler (unused parameter) */
	(void) ElementPtr1;  /* Satisfying compiler (unused parameter) */
	(void) pPt1;  /* Satisfying compiler (unused parameter) */

}


void Satellite::expire(ELEMENT &self) {
    auto *ElementPtr = &self;
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	StarShipPtr->RaceDescPtr->ship_info.ship_flags &= ~POINT_DEFENSE;

	ElementPtr->state_flags &= ~DISAPPEARING;
	ElementPtr->state_flags |= NONSOLID | FINITE_LIFE | CHANGING;
	ElementPtr->life_span = 4;
	ElementPtr->turn_wait = 1;
	ElementPtr->next_turn = 0;
	ElementPtr->next.image.frame =
			SetAbsFrameIndex (ElementPtr->current.image.frame, 8);

	dying_ = true;
	ElementPtr->death_func = NULL;
	ElementPtr->postprocess_func = NULL;
	ElementPtr->collision_func = NULL;

}



// Native autonomous unit; all engine records are borrowed for each hook.
namespace {
class SatelliteDeployment final : public uqm::combat::CombatEntity {
public:
    void update(ELEMENT &) override {}
    void expire(ELEMENT &self) override;
};
}
void SatelliteDeployment::expire(ELEMENT &self) {
    auto *ElementPtr = &self;
	COUNT i;
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if (StarShipPtr->hShip)
	{
		LockElement (StarShipPtr->hShip, &ElementPtr);
		for (i = 0; i < NumSatellites; ++i)
		{
			HELEMENT hSatellite;

			hSatellite = AllocElement ();
			if (hSatellite)
			{
				COUNT angle;
				ELEMENT *SattPtr;

				LockElement (hSatellite, &SattPtr);
				SattPtr->playerNr = ElementPtr->playerNr;
				SattPtr->state_flags = IGNORE_SIMILAR | APPEARING
						| FINITE_LIFE;
				SattPtr->life_span = NORMAL_LIFE + 1;
				SattPtr->hit_points = SatelliteHitpoints;
				SattPtr->mass_points = SatelliteMass;

				angle = (i * FULL_CIRCLE + (NumSatellites >> 1))
						/ NumSatellites;
				SattPtr->turn_wait = (BYTE)angle;
				SattPtr->current.location.x = ElementPtr->next.location.x
						+ COSINE (angle, SatelliteOffset);
				SattPtr->current.location.y = ElementPtr->next.location.y
						+ SINE (angle, SatelliteOffset);
				SattPtr->current.image.farray =
						StarShipPtr->RaceDescPtr->ship_data.special;
				SattPtr->current.image.frame = SetAbsFrameIndex (
						StarShipPtr->RaceDescPtr->ship_data.special[0],
						(COUNT)TFB_Random () & 0x07
						);

				uqm::detail::attachEntity(*SattPtr, std::make_unique<Satellite>(), uqm::detail::EntityHooks::Update | uqm::detail::EntityHooks::Postprocess | uqm::detail::EntityHooks::Collision | uqm::detail::EntityHooks::Expiration);




				SetElementStarShip (SattPtr, StarShipPtr);

				SetPrimType (&(GLOBAL (DisplayArray))[
						SattPtr->PrimIndex
						], STAMP_PRIM);

				UnlockElement (hSatellite);
				PutElement (hSatellite);
			}
		}
		UnlockElement (StarShipPtr->hShip);
	}

}


static void
chmmr_preprocess (ELEMENT *ElementPtr)
{
	HELEMENT hSatellite;
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	hSatellite = AllocElement ();
	if (hSatellite)
	{
		ELEMENT *SattPtr;
		STARSHIP *StarShipPtr;

		LockElement (hSatellite, &SattPtr);
		SattPtr->playerNr = ElementPtr->playerNr;
		SattPtr->state_flags = FINITE_LIFE | NONSOLID | IGNORE_SIMILAR
				| APPEARING;
		SattPtr->life_span = HYPERJUMP_LIFE + 1;

		uqm::detail::attachEntity(*SattPtr, std::make_unique<SatelliteDeployment>(), uqm::detail::EntityHooks::Expiration);

		GetElementStarShip (ElementPtr, &StarShipPtr);
		SetElementStarShip (SattPtr, StarShipPtr);

		SetPrimType (&(GLOBAL (DisplayArray))[
				SattPtr->PrimIndex
				], NO_PRIM);

		UnlockElement (hSatellite);
		PutElement (hSatellite);
	}

	uqm::combat::shipFor(*StarShipPtr->RaceDescPtr).enablePreprocessing(false);
}

namespace {
class ChmmrShip final : public uqm::combat::Ship {
public:
    explicit ChmmrShip(const RACE_DESC &definition) noexcept
        : Ship(definition) {
        legacyData().cyborg_control.intelligence_func = chmmr_intelligence;
    }
private:
    void beforeMotion(ELEMENT &element) noexcept override {
        chmmr_preprocess(&element);
    }
    void afterMotion(ELEMENT &element) noexcept override {
        chmmr_postprocess(&element);
    }
    COUNT createWeapons(ELEMENT &element, uqm::combat::WeaponSlots &weapons) noexcept override {
        return initialize_megawatt_laser(&element, weapons.data());
    }
};
} // namespace

std::unique_ptr<uqm::combat::Ship>
uqm::combat::makeChmmrShip()
{

	return std::make_unique<ChmmrShip>(chmmrDefinition());
}

