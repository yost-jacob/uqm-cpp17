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
#include <optional>
#include "resinst.h"

#include "uqm/globdata.h"
#include "uqm/tactrans.h"
#include "libs/mathlib.h"

// Balance values are immutable, loaded from base/ships.json at startup.
static const uqm::combat::ShipTuning &tuning() {
    static const auto &value = uqm::combat::shipTuning("pkunk");
    return value;
}
static const uqm::combat::ProjectileStats &missileStats() {
    static const auto &value = tuning().projectile("missile");
    return value;
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto PkunkOffset = 15;
constexpr auto MissileOffset = 1;
constexpr auto PhoenixLife = 12;
constexpr auto StartPhoenixColor = BUILD_COLOR (MAKE_RGB15 (0x1F, 0x15, 0x00), 0x7A);
constexpr auto TransitionLife = 1;
constexpr auto TransitionSpeed = DISPLAY_TO_WORLD (20);

static const RACE_DESC &pkunkDefinition() {
    static const RACE_DESC definition = {
        { /* SHIP_INFO */
            FIRES_FORE | FIRES_LEFT | FIRES_RIGHT,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, tuning().ship.maxCrew,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            PKUNK_RACE_STRINGS,
            PKUNK_ICON_MASK_PMAP_ANIM,
            PKUNK_MICON_MASK_PMAP_ANIM,
            NULL, NULL, NULL
        },
        { /* FLEET_STUFF */
            666 / SPHERE_RADIUS_INCREMENT * 2, /* Initial SoI radius */
            { /* Known location (center of SoI) */
                502, 401,
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
                PKUNK_BIG_MASK_PMAP_ANIM,
                PKUNK_MED_MASK_PMAP_ANIM,
                PKUNK_SML_MASK_PMAP_ANIM,
            },
            {
                BUG_BIG_MASK_PMAP_ANIM,
                BUG_MED_MASK_PMAP_ANIM,
                BUG_SML_MASK_PMAP_ANIM,
            },
            {
                NULL_RESOURCE,
                NULL_RESOURCE,
                NULL_RESOURCE,
            },
            {
                PKUNK_CAPTAIN_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
            PKUNK_VICTORY_SONG,
            PKUNK_SHIP_SOUNDS,
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            NULL, NULL
        },
        {
            0,
            uqm::combat::weaponRangeHint(CLOSE_RANGE_WEAPON + 1),
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
	HELEMENT hPhoenix;
	ElementProcessFunc *saved_preprocess_func;
	ElementProcessFunc *saved_postprocess_func;
	ElementProcessFunc *saved_death_func;
	
} PKUNK_DATA;

// Local typedef
typedef PKUNK_DATA CustomShipData_t;

// Retrieve race-specific ship data from a race desc
static CustomShipData_t *
GetCustomShipData (RACE_DESC *pRaceDesc)
{
	return static_cast<CustomShipData_t *>(pRaceDesc->data);
}

// Updates the concrete ship's optional value; no separate heap allocation.
static void SetCustomShipData(RACE_DESC *descriptor, const CustomShipData_t *data);

// Per-shot behavior; engine kinematics and collision state remain borrowed.
namespace {
class PkunkShot final : public uqm::combat::Projectile {
public:
    using Projectile::Projectile;
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


static COUNT
initialize_bug_missile (ELEMENT *ShipPtr, HELEMENT MissileArray[])
{
	COUNT i;
	STARSHIP *StarShipPtr;
	MISSILE_BLOCK MissileBlock;

	GetElementStarShip (ShipPtr, &StarShipPtr);
	MissileBlock.cx = ShipPtr->next.location.x;
	MissileBlock.cy = ShipPtr->next.location.y;
	MissileBlock.farray = StarShipPtr->RaceDescPtr->ship_data.weapon;
	MissileBlock.index = 0;
	MissileBlock.sender = ShipPtr->playerNr;
	MissileBlock.flags = IGNORE_SIMILAR;
	MissileBlock.pixoffs = PkunkOffset;
	MissileBlock.speed = missileStats().speed;
	MissileBlock.hit_points = missileStats().hitPoints;
	MissileBlock.damage = missileStats().damage;
	MissileBlock.life = missileStats().lifetime;
	MissileBlock.preprocess_func = NULL;
	MissileBlock.blast_offs = MissileOffset;

	for (i = 0; i < 3; ++i)
	{
		MissileBlock.face =
				StarShipPtr->ShipFacing
				+ (ANGLE_TO_FACING (QUADRANT) * i);
		if (i == 2)
			MissileBlock.face += ANGLE_TO_FACING (QUADRANT);
		MissileBlock.face = NORMALIZE_FACING (MissileBlock.face);

		if ((MissileArray[i] = uqm::combat::publishMissile<PkunkShot>(MissileBlock)))
		{
			SIZE dx, dy;
			ELEMENT *MissilePtr;

			LockElement (MissileArray[i], &MissilePtr);
			GetCurrentVelocityComponents (&ShipPtr->velocity, &dx, &dy);
			DeltaVelocityComponents (&MissilePtr->velocity, dx, dy);
			MissilePtr->current.location.x -= VELOCITY_TO_WORLD (dx);
			MissilePtr->current.location.y -= VELOCITY_TO_WORLD (dy);

			UnlockElement (MissileArray[i]);
		}
	}

	return (3);
}

static void
pkunk_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
		COUNT ConcernCounter)
{
	STARSHIP *StarShipPtr;
	PKUNK_DATA *PkunkData;

	GetElementStarShip (ShipPtr, &StarShipPtr);
	PkunkData = GetCustomShipData (StarShipPtr->RaceDescPtr);
	if (PkunkData->hPhoenix && (StarShipPtr->control & STANDARD_RATING))
	{
		RemoveElement (PkunkData->hPhoenix);
		FreeElement (PkunkData->hPhoenix);
		PkunkData->hPhoenix = 0;
	}

	if (StarShipPtr->RaceDescPtr->ship_info.energy_level <
			StarShipPtr->RaceDescPtr->ship_info.max_energy
			&& (StarShipPtr->special_counter == 0
			|| (BYTE)TFB_Random () < 20))
		StarShipPtr->ship_input_state |= SPECIAL;
	else
		StarShipPtr->ship_input_state &= ~SPECIAL;
	ship_intelligence (ShipPtr, ObjectsOfConcern, ConcernCounter);
}

static void pkunk_preprocess (ELEMENT *ElementPtr);

static void
new_pkunk (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;
	PKUNK_DATA *PkunkData;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	PkunkData = GetCustomShipData (StarShipPtr->RaceDescPtr);

	ElementPtr->state_flags = APPEARING | PLAYER_SHIP | IGNORE_SIMILAR;
	ElementPtr->mass_points = tuning().ship.shipMass;
	// Restore the element processing callbacks after the explosion.
	// The callbacks were changed for the explosion sequence
	ElementPtr->preprocess_func = PkunkData->saved_preprocess_func;
	ElementPtr->postprocess_func = PkunkData->saved_postprocess_func;
	ElementPtr->death_func = PkunkData->saved_death_func;
	// preprocess_func() is called during the phoenix transition and
	// then cleared, so we need to restore it
	uqm::combat::shipFor(*StarShipPtr->RaceDescPtr).enablePreprocessing(true);
	StarShipPtr->RaceDescPtr->ship_info.crew_level = tuning().ship.maxCrew;
	StarShipPtr->RaceDescPtr->ship_info.energy_level = tuning().ship.maxEnergy;
				/* fix vux impairment */
	StarShipPtr->RaceDescPtr->characteristics.max_thrust = tuning().ship.maxThrust;
	StarShipPtr->RaceDescPtr->characteristics.thrust_increment = tuning().ship.thrustIncrement;
	StarShipPtr->RaceDescPtr->characteristics.turn_wait = tuning().ship.turnWait;
	StarShipPtr->RaceDescPtr->characteristics.thrust_wait = tuning().ship.thrustWait;
	StarShipPtr->RaceDescPtr->characteristics.special_wait = 0;

	StarShipPtr->ship_input_state = 0;
	// Pkunk wins in a simultaneous destruction if it reincarnates
	StarShipPtr->cur_status_flags &= PLAY_VICTORY_DITTY;
	StarShipPtr->old_status_flags = 0;
	StarShipPtr->energy_counter = 0;
	StarShipPtr->weapon_counter = 0;
	StarShipPtr->special_counter = 0;
	ElementPtr->crew_level = 0;
	ElementPtr->turn_wait = 0;
	ElementPtr->thrust_wait = 0;
	ElementPtr->life_span = NORMAL_LIFE;

	StarShipPtr->ShipFacing = NORMALIZE_FACING (TFB_Random ());
	ElementPtr->current.image.farray = StarShipPtr->RaceDescPtr->ship_data.ship;
	ElementPtr->current.image.frame = SetAbsFrameIndex (
			StarShipPtr->RaceDescPtr->ship_data.ship[0],
			StarShipPtr->ShipFacing);
	SetPrimType (&(GLOBAL (DisplayArray))[ElementPtr->PrimIndex], STAMP_PRIM);

	do
	{
		ElementPtr->current.location.x =
				WRAP_X (DISPLAY_ALIGN_X (TFB_Random ()));
		ElementPtr->current.location.y =
				WRAP_Y (DISPLAY_ALIGN_Y (TFB_Random ()));
	} while (CalculateGravity (ElementPtr)
			|| TimeSpaceMatterConflict (ElementPtr));

	// XXX: Hack: Set hTarget!=0 so that ship_preprocess() does not
	//   call ship_transition() for us.
	ElementPtr->hTarget = StarShipPtr->hShip;
}

// This function is called when the ship dies but reincarnates.
// The generic ship_death() function is not called for the ship in this case.
static void
pkunk_reincarnation_death (ELEMENT *ShipPtr)
{
	// Simulate ship death
	StopAllBattleMusic ();
	StartShipExplosion (ShipPtr, true);
	// Once the explosion ends, we will get a brand new ship
	ShipPtr->death_func = new_pkunk;
}

static void
intercept_pkunk_death (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;
	PKUNK_DATA *PkunkData;
	ELEMENT *ShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	PkunkData = GetCustomShipData (StarShipPtr->RaceDescPtr);
	
	if (StarShipPtr->RaceDescPtr->ship_info.crew_level != 0)
	{	// Ship not dead yet.
		// Keep the Phoenix element alive.
		ElementPtr->state_flags &= ~DISAPPEARING;
		ElementPtr->life_span = 1;
		return;
	}

	LockElement (StarShipPtr->hShip, &ShipPtr);
	// GRAVITY_MASS() indicates a warp-out here. If Pkunk dies while warping
	// out, there is no reincarnation.
	if (!GRAVITY_MASS (ShipPtr->mass_points + 1))
	{
		// XXX: Hack: Set mass_points to indicate a reincarnation to
		//   FindAliveStarShip()
		ShipPtr->mass_points = MAX_SHIP_MASS + 1;
		// Save the various element processing callbacks before the
		// explosion happens, because we were not the ones who set
		// these callbacks and they are about to be changed.
		PkunkData->saved_preprocess_func = ShipPtr->preprocess_func;
		PkunkData->saved_postprocess_func = ShipPtr->postprocess_func;
		PkunkData->saved_death_func = ShipPtr->death_func;

		ShipPtr->death_func = pkunk_reincarnation_death;
	}
	UnlockElement (StarShipPtr->hShip);
}

static void
spawn_phoenix_trail (ELEMENT *ElementPtr)
{
	static const Color colorTable[] =
	{
		BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x15, 0x00), 0x7a),
		BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x11, 0x00), 0x7b),
		BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x0E, 0x00), 0x7c),
		BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x0A, 0x00), 0x7d),
		BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x07, 0x00), 0x7e),
		BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x03, 0x00), 0x7f),
		BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x00, 0x00), 0x2a),
		BUILD_COLOR (MAKE_RGB15_INIT (0x1B, 0x00, 0x00), 0x2b),
		BUILD_COLOR (MAKE_RGB15_INIT (0x17, 0x00, 0x00), 0x2c),
		BUILD_COLOR (MAKE_RGB15_INIT (0x13, 0x00, 0x00), 0x2d),
		BUILD_COLOR (MAKE_RGB15_INIT (0x0F, 0x00, 0x00), 0x2e),
		BUILD_COLOR (MAKE_RGB15_INIT (0x0B, 0x00, 0x00), 0x2f),
	};
	const size_t colorTableCount = sizeof colorTable / sizeof colorTable[0];
	
	ElementPtr->colorCycleIndex++;
	if (ElementPtr->colorCycleIndex != colorTableCount)
	{
		ElementPtr->life_span = TransitionLife;

		SetPrimColor (&(GLOBAL (DisplayArray))[ElementPtr->PrimIndex],
				colorTable[ElementPtr->colorCycleIndex]);

		ElementPtr->state_flags &= ~DISAPPEARING;
		ElementPtr->state_flags |= CHANGING;
	} // else, the element disappears.
}

static void
phoenix_transition (ELEMENT *ElementPtr)
{
	HELEMENT hShipImage;
	ELEMENT *ShipImagePtr;
	STARSHIP *StarShipPtr;
	
	GetElementStarShip (ElementPtr, &StarShipPtr);
	LockElement (StarShipPtr->hShip, &ShipImagePtr);

	if (!(ShipImagePtr->state_flags & NONSOLID))
	{
		ElementPtr->preprocess_func = NULL;
	}
	else if ((hShipImage = AllocElement ()))
	{
		COUNT angle;

		PutElement (hShipImage);

		LockElement (hShipImage, &ShipImagePtr);
		ShipImagePtr->playerNr = NEUTRAL_PLAYER_NUM;
		ShipImagePtr->state_flags = APPEARING | FINITE_LIFE | NONSOLID;
		ShipImagePtr->life_span = TransitionLife;
		SetPrimType (&(GLOBAL (DisplayArray))[ShipImagePtr->PrimIndex],
				STAMPFILL_PRIM);
		SetPrimColor (
				&(GLOBAL (DisplayArray))[ShipImagePtr->PrimIndex],
				StartPhoenixColor);
		ShipImagePtr->colorCycleIndex = 0;
		ShipImagePtr->current.image = ElementPtr->current.image;
		ShipImagePtr->current.location = ElementPtr->current.location;
		if (!(ElementPtr->state_flags & PLAYER_SHIP))
		{
			angle = ElementPtr->mass_points;

			ShipImagePtr->current.location.x +=
					COSINE (angle, TransitionSpeed);
			ShipImagePtr->current.location.y +=
					SINE (angle, TransitionSpeed);
			ElementPtr->preprocess_func = NULL;
		}
		else
		{
			angle = FACING_TO_ANGLE (StarShipPtr->ShipFacing);

			ShipImagePtr->current.location.x -=
					COSINE (angle, TransitionSpeed)
					* (ElementPtr->life_span - 1);
			ShipImagePtr->current.location.y -=
					SINE (angle, TransitionSpeed)
					* (ElementPtr->life_span - 1);

			ShipImagePtr->current.location.x =
					WRAP_X (ShipImagePtr->current.location.x);
			ShipImagePtr->current.location.y =
					WRAP_Y (ShipImagePtr->current.location.y);
		}

		ShipImagePtr->mass_points = (BYTE)angle;
		ShipImagePtr->preprocess_func = phoenix_transition;
		ShipImagePtr->death_func = spawn_phoenix_trail;
		SetElementStarShip (ShipImagePtr, StarShipPtr);

		UnlockElement (hShipImage);
	}

	UnlockElement (StarShipPtr->hShip);
}

static void
pkunk_preprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;
	PKUNK_DATA *PkunkData;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	PkunkData = GetCustomShipData (StarShipPtr->RaceDescPtr);
	if (ElementPtr->state_flags & APPEARING)
	{
		HELEMENT hPhoenix = 0;

		if (TFB_Random () & 1)
			hPhoenix = AllocElement ();

		// The hPhoenix element is created and placed at the head of the
		// queue so that it is preprocessed before any of the ships' elements
		// are, and so before death_func() is called for the dead Pkunk.
		// hPhoenix detects when the Pkunk ship dies and tweaks the ship,
		// starting the death + reincarnation sequence.
		if (hPhoenix)
		{
			ELEMENT *PhoenixPtr;

			LockElement (hPhoenix, &PhoenixPtr);
			PhoenixPtr->playerNr = ElementPtr->playerNr;
			PhoenixPtr->state_flags = FINITE_LIFE | NONSOLID | IGNORE_SIMILAR;
			PhoenixPtr->life_span = 1;

			PhoenixPtr->death_func = intercept_pkunk_death;

			SetElementStarShip (PhoenixPtr, StarShipPtr);

			UnlockElement (hPhoenix);
			InsertElement (hPhoenix, GetHeadElement ());
		}
		PkunkData->hPhoenix = hPhoenix;

		// XXX: Hack: new_pkunk() sets hTarget!=0 which indicates a
		//   reincarnation to us.
		if (ElementPtr->hTarget == 0)
		{
			// A brand new ship is preprocessed only once
			uqm::combat::shipFor(*StarShipPtr->RaceDescPtr).enablePreprocessing(false);
		}
		else
		{	// Start the reincarnation sequence
			COUNT angle, facing;

			ProcessSound (SetAbsSoundIndex (
					StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 1
					), ElementPtr);

			ElementPtr->life_span = PhoenixLife;
			SetPrimType (&(GLOBAL (DisplayArray))[ElementPtr->PrimIndex],
					NO_PRIM);
			ElementPtr->state_flags |= NONSOLID | FINITE_LIFE | CHANGING;

			facing = StarShipPtr->ShipFacing;
			for (angle = OCTANT; angle < FULL_CIRCLE; angle += QUADRANT)
			{
				StarShipPtr->ShipFacing = NORMALIZE_FACING (
						facing + ANGLE_TO_FACING (angle)
						);
				phoenix_transition (ElementPtr);
			}
			StarShipPtr->ShipFacing = facing;
		}
	}

	if (uqm::combat::shipFor(*StarShipPtr->RaceDescPtr).preprocessingEnabled())
	{
		StarShipPtr->cur_status_flags &=
				~(LEFT | RIGHT | THRUST | WEAPON | SPECIAL);

		if (ElementPtr->life_span == NORMAL_LIFE)
		{
			ElementPtr->current.image.frame =
					ElementPtr->next.image.frame =
					SetEquFrameIndex (
					ElementPtr->current.image.farray[0],
					ElementPtr->current.image.frame);
			SetPrimType (&(GLOBAL (DisplayArray))[ElementPtr->PrimIndex],
					STAMP_PRIM);
			InitIntersectStartPoint (ElementPtr);
			InitIntersectEndPoint (ElementPtr);
			InitIntersectFrame (ElementPtr);
			ZeroVelocityComponents (&ElementPtr->velocity);
			ElementPtr->state_flags &= ~(NONSOLID | FINITE_LIFE);
			ElementPtr->state_flags |= CHANGING;

			uqm::combat::shipFor(*StarShipPtr->RaceDescPtr).enablePreprocessing(false);
		}
	}
}
		
static COUNT LastSound = 0;

static void
pkunk_postprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if (StarShipPtr->RaceDescPtr->characteristics.special_wait)
		--StarShipPtr->RaceDescPtr->characteristics.special_wait;
	else if ((StarShipPtr->cur_status_flags & SPECIAL)
			&& StarShipPtr->RaceDescPtr->ship_info.energy_level <
			StarShipPtr->RaceDescPtr->ship_info.max_energy)
	{
		COUNT CurSound;

		do
		{
			CurSound =
					2 + ((COUNT)TFB_Random ()
					% (GetSoundCount (StarShipPtr->RaceDescPtr->ship_data.ship_sounds) - 2));
		} while (CurSound == LastSound);
		ProcessSound (SetAbsSoundIndex (
				StarShipPtr->RaceDescPtr->ship_data.ship_sounds, CurSound
				), ElementPtr);
		LastSound = CurSound;

		DeltaEnergy (ElementPtr, tuning().ship.specialEnergyCost);

		StarShipPtr->RaceDescPtr->characteristics.special_wait = tuning().ship.specialWait;
	}
}

static void
uninit_pkunk (RACE_DESC *pRaceDesc)
{
	SetCustomShipData (pRaceDesc, NULL);
}

namespace {
class PkunkShip final : public uqm::combat::Ship {
public:
    explicit PkunkShip(const RACE_DESC &definition) noexcept
        : Ship(definition) {
        legacyData().cyborg_control.intelligence_func = pkunk_intelligence;
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
        pkunk_preprocess(&element);
    }
    void afterMotion(ELEMENT &element) noexcept override {
        pkunk_postprocess(&element);
    }
    COUNT createWeapons(ELEMENT &element, uqm::combat::WeaponSlots &weapons) noexcept override {
        return initialize_bug_missile(&element, weapons.data());
    }
    void onFinish() noexcept override {
        uninit_pkunk(&legacyData());
    }
    std::optional<CustomShipData_t> privateData_;
};
} // namespace

static void SetCustomShipData(RACE_DESC *descriptor, const CustomShipData_t *data)
{
    static_cast<PkunkShip &>(uqm::combat::shipFor(*descriptor)).setData(data);
}

std::unique_ptr<uqm::combat::Ship>
uqm::combat::makePkunkShip()
{
	// Build a per-instance definition, copied into the native owner.
	RACE_DESC new_pkunk_desc;
	PKUNK_DATA empty_data{};

	/* copy initial ship settings to the new descriptor */
	new_pkunk_desc = pkunkDefinition();
	new_pkunk_desc.data = &empty_data;

	LastSound = 0;
			// We need to reinitialise it at least each battle, to ensure
			// that NetPlay is synchronised if one player played another
			// game before playing against a networked opponent.

	return std::make_unique<PkunkShip>(new_pkunk_desc);
}
