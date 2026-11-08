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
#include "uqm/tactrans.h"
#include "libs/mathlib.h"

// Balance values are immutable, loaded from base/ships.json at startup.
static const uqm::combat::ShipTuning &tuning() {
    static const auto &value = uqm::combat::shipTuning("shofixti");
    return value;
}
static const uqm::combat::ProjectileStats &missileStats() {
    static const auto &value = tuning().projectile("missile");
    return value;
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto ShofixtiOffset = 15;
constexpr auto MissileOffset = 1;
constexpr auto DestructRange = 180;
constexpr auto MaxDestruction = (DestructRange / 10);
constexpr auto NumLimpets = 3;
constexpr auto DestructSwitch = ((NUM_EXPLOSION_FRAMES * 3) - 3);
constexpr auto MinThrustIncrement = DISPLAY_TO_WORLD (1);

static const RACE_DESC &shofixtiDefinition() {
    static const RACE_DESC definition = {
        { /* SHIP_INFO */
            FIRES_FORE,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, tuning().ship.maxCrew,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            SHOFIXTI_RACE_STRINGS,
            SHOFIXTI_ICON_MASK_PMAP_ANIM,
            SHOFIXTI_MICON_MASK_PMAP_ANIM,
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
                SHOFIXTI_BIG_MASK_PMAP_ANIM,
                SHOFIXTI_MED_MASK_PMAP_ANIM,
                SHOFIXTI_SML_MASK_PMAP_ANIM,
            },
            {
                DART_BIG_MASK_PMAP_ANIM,
                DART_MED_MASK_PMAP_ANIM,
                DART_SML_MASK_PMAP_ANIM,
            },
            {
                DESTRUCT_BIG_MASK_ANIM,
                DESTRUCT_MED_MASK_ANIM,
                DESTRUCT_SML_MASK_ANIM,
            },
            {
                SHOFIXTI_CAPTAIN_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
                SHOFIXTI_VICTORY_SONG,
            SHOFIXTI_SHIP_SOUNDS,
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            NULL, NULL
        },
        {
            0,
            uqm::combat::weaponRangeHint(missileStats().speed * missileStats().lifetime),
            NULL,
        },

        0,
        0, /* ship_owner */
    };
    return definition;
}

static COUNT
initialize_standard_missile (ELEMENT *ShipPtr, HELEMENT MissileArray[])
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
	MissileBlock.pixoffs = ShofixtiOffset;
	MissileBlock.speed = missileStats().speed;
	MissileBlock.hit_points = missileStats().hitPoints;
	MissileBlock.damage = missileStats().damage;
	MissileBlock.life = missileStats().lifetime;
	MissileBlock.preprocess_func = NULL;
	MissileBlock.blast_offs = MissileOffset;
	MissileArray[0] = initialize_missile (&MissileBlock);

	return (1);
}

static void
destruct_preprocess (ELEMENT *ElementPtr)
{
	PRIMITIVE *lpPrim;

	// ship_death() set the ship element's life_span to
	// (NUM_EXPLOSION_FRAMES * 3)
	lpPrim = &(GLOBAL (DisplayArray))[ElementPtr->PrimIndex];
	ElementPtr->state_flags |= CHANGING;
	if (ElementPtr->life_span > DestructSwitch)
	{
		// First, stamp-fill the ship's own element with changing colors
		// for 3 frames. No explosion element yet.
		SetPrimType (lpPrim, STAMPFILL_PRIM);
		if (ElementPtr->life_span == DestructSwitch + 2)
			SetPrimColor (lpPrim,
					BUILD_COLOR (MAKE_RGB15 (0x1F, 0x1F, 0x0A), 0x0E));
		else
			SetPrimColor (lpPrim,
					BUILD_COLOR (MAKE_RGB15 (0x1F, 0x1F, 0x1F), 0x0F));
	}
	else if (ElementPtr->life_span < DestructSwitch)
	{
		// Stamp-fill the explosion element with cycling colors for the
		// remainder of the glory explosion frames.
		Color color = GetPrimColor (lpPrim);

		ElementPtr->next.image.frame =
				IncFrameIndex (ElementPtr->current.image.frame);
		if (sameColor (color,
				BUILD_COLOR (MAKE_RGB15 (0x1F, 0x1F, 0x1F), 0x0F)))
			SetPrimColor (lpPrim,
					BUILD_COLOR (MAKE_RGB15 (0x1F, 0x1F, 0x0A), 0x0E));
		else if (sameColor (color,
				BUILD_COLOR (MAKE_RGB15 (0x1F, 0x1F, 0x0A), 0x0E)))
			SetPrimColor (lpPrim,
					BUILD_COLOR (MAKE_RGB15 (0x1F, 0x0A, 0x0A), 0x0C));
		else if (sameColor (color,
				BUILD_COLOR (MAKE_RGB15 (0x1F, 0x0A, 0x0A), 0x0C)))
			SetPrimColor (lpPrim,
					BUILD_COLOR (MAKE_RGB15 (0x14, 0x0A, 0x00), 0x06));
		else if (sameColor (color,
				BUILD_COLOR (MAKE_RGB15 (0x14, 0x0A, 0x00), 0x06)))
			SetPrimColor (lpPrim,
					BUILD_COLOR (MAKE_RGB15 (0x14, 0x00, 0x00), 0x04));
	}
	else
	{
		HELEMENT hDestruct;

		SetPrimType (lpPrim, NO_PRIM);
		// The ship's own element will not be drawn anymore but will remain
		// alive all through the glory explosion.
		ElementPtr->preprocess_func = NULL;

		// Spawn a separate glory explosion element.
		// XXX: Why? Why not keep using the ship's element?
		//   Is it because of conflicting state_flags, hit_points or
		//   mass_points?
		hDestruct = AllocElement ();
		if (hDestruct)
		{
			ELEMENT *DestructPtr;
			STARSHIP *StarShipPtr;

			GetElementStarShip (ElementPtr, &StarShipPtr);

			PutElement (hDestruct);
			LockElement (hDestruct, &DestructPtr);
			SetElementStarShip (DestructPtr, StarShipPtr);
			DestructPtr->hit_points = 0;
			DestructPtr->mass_points = 0;
			DestructPtr->playerNr = NEUTRAL_PLAYER_NUM;
			DestructPtr->state_flags = APPEARING | FINITE_LIFE | NONSOLID;
			SetPrimType (&(GLOBAL (DisplayArray))[DestructPtr->PrimIndex],
					STAMPFILL_PRIM);
			SetPrimColor (&(GLOBAL (DisplayArray))[DestructPtr->PrimIndex],
					BUILD_COLOR (MAKE_RGB15 (0x1F, 0x1F, 0x1F), 0x0F));
			DestructPtr->current.image.farray =
					StarShipPtr->RaceDescPtr->ship_data.special;
			DestructPtr->current.image.frame =
					StarShipPtr->RaceDescPtr->ship_data.special[0];
			DestructPtr->life_span = GetFrameCount (
					DestructPtr->current.image.frame);
			DestructPtr->current.location = ElementPtr->current.location;
			DestructPtr->preprocess_func = destruct_preprocess;
			DestructPtr->postprocess_func = NULL;
			DestructPtr->death_func = NULL;
			ZeroVelocityComponents (&DestructPtr->velocity);
			UnlockElement (hDestruct);
		}
	}
}

/* In order to detect any Orz Marines that have boarded the ship
   when it self-destructs, we'll need to see some Orz functions */
#include "../orz/orz.h"
static bool isOrzMarine(const ELEMENT &element) noexcept {
    return element.preprocess_func == intruder_preprocess
        && element.collision_func == marine_collision;
}

static void
self_destruct_kill_objects (ELEMENT *ElementPtr)
{
	// This is called during PostProcessQueue(), close to or at the end,
	// for the temporary destruct element to apply the effects of glory
	// explosion. The effects are not seen until the next frame.
	HELEMENT hElement, hNextElement;

	for (hElement = GetHeadElement (); hElement != 0; hElement = hNextElement)
	{
		ELEMENT *ObjPtr;
		SIZE delta_x, delta_y;
		DWORD dist;

		LockElement (hElement, &ObjPtr);
		hNextElement = GetSuccElement (ObjPtr);

		if (!CollidingElement (ObjPtr) && !isOrzMarine (*ObjPtr))
		{
			UnlockElement (hElement);
			continue;
		}

		delta_x = ObjPtr->next.location.x - ElementPtr->next.location.x;
		if (delta_x < 0)
			delta_x = -delta_x;
		delta_y = ObjPtr->next.location.y - ElementPtr->next.location.y;
		if (delta_y < 0)
			delta_y = -delta_y;
		delta_x = WORLD_TO_DISPLAY (delta_x);
		delta_y = WORLD_TO_DISPLAY (delta_y);
		dist = delta_x * delta_x + delta_y * delta_y;
		if (delta_x <= DestructRange && delta_y <= DestructRange
				&& dist <= DestructRange * DestructRange)
		{
			int destruction = 1 + MaxDestruction *
					(DestructRange - square_root (dist)) / DestructRange;

			// XXX: Why not simply call do_damage()?
			if (ObjPtr->state_flags & PLAYER_SHIP)
			{
				if (!DeltaCrew (ObjPtr, -destruction))
					ObjPtr->life_span = 0;
			}
			else if (!GRAVITY_MASS (ObjPtr->mass_points))
			{
				if (destruction < ObjPtr->hit_points)
					ObjPtr->hit_points -= destruction;
				else
				{
					ObjPtr->hit_points = 0;
					ObjPtr->life_span = 0;
				}
			}
		}

		UnlockElement (hElement);
	}
}

// This function is called when the ship dies via Glory Device.
// The generic ship_death() function is not called for the ship in this case.
static void
shofixti_destruct_death (ELEMENT *ShipPtr)
{
	STARSHIP *StarShip;
	STARSHIP *winner;

	GetElementStarShip (ShipPtr, &StarShip);

	StopAllBattleMusic ();

	StartShipExplosion (ShipPtr, false);
	// We process the explosion ourselves because it is different
	ShipPtr->preprocess_func = destruct_preprocess;
	
	PlaySound (SetAbsSoundIndex (StarShip->RaceDescPtr->ship_data.ship_sounds,
			1), CalcSoundPosition (ShipPtr), ShipPtr, GAME_SOUND_PRIORITY + 1);

	winner = GetWinnerStarShip ();
	if (winner == NULL)
	{	// No winner determined yet
		winner = FindAliveStarShip (ShipPtr);
		if (winner == NULL)
		{	// No ships left alive after the Glory Device thus Shofixti wins
			winner = StarShip;
		}
		SetWinnerStarShip (winner);
	}
	else if (winner == StarShip)
	{	// This ship is the winner
		// It may have self-destructed before the ditty started playing,
		// and in that case, there should be no ditty
		StarShip->cur_status_flags &= ~PLAY_VICTORY_DITTY;
	}
	RecordShipDeath (ShipPtr);
}

static void
self_destruct (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;
	HELEMENT hDestruct;

	GetElementStarShip (ElementPtr, &StarShipPtr);
		
	// Spawn a temporary element, which dies in this same frame, in order
	// to defer the effects of the glory explosion.
	// It will be the last element (or one of the last) for which the
	// death_func() will be called from PostProcessQueue() in this frame.
	// XXX: Why at the end? Why not just do it now?
	hDestruct = AllocElement ();
	if (hDestruct)
	{
		ELEMENT *DestructPtr;

		LockElement (hDestruct, &DestructPtr);
		DestructPtr->playerNr = ElementPtr->playerNr;
		DestructPtr->state_flags = APPEARING | NONSOLID | FINITE_LIFE;
		DestructPtr->next.location = ElementPtr->next.location;
		DestructPtr->life_span = 0;
		DestructPtr->pParent = ElementPtr->pParent;
		DestructPtr->hTarget = 0;

		DestructPtr->death_func = self_destruct_kill_objects;

		UnlockElement (hDestruct);

		PutElement (hDestruct);
	}

	// Must kill off the remaining crew ourselves
	DeltaCrew (ElementPtr, -(int)ElementPtr->crew_level);

	ElementPtr->state_flags |= NONSOLID;
	ElementPtr->life_span = 0;
	// The ship is now dead. It's death_func, i.e. shofixti_destruct_death(),
	// will be called the next frame.
	ElementPtr->death_func = shofixti_destruct_death;
}

static void
shofixti_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
		COUNT ConcernCounter)
{
	STARSHIP *StarShipPtr;

	ship_intelligence (ShipPtr, ObjectsOfConcern, ConcernCounter);

	GetElementStarShip (ShipPtr, &StarShipPtr);
	if (StarShipPtr->special_counter != 0)
		return;

	if (StarShipPtr->ship_input_state & SPECIAL)
		StarShipPtr->ship_input_state &= ~SPECIAL;
	else
	{
		EVALUATE_DESC *lpWeaponEvalDesc;
		EVALUATE_DESC *lpShipEvalDesc;

		lpWeaponEvalDesc = &ObjectsOfConcern[ENEMY_WEAPON_INDEX];
		lpShipEvalDesc = &ObjectsOfConcern[ENEMY_SHIP_INDEX];
		if (StarShipPtr->RaceDescPtr->ship_data.special[0]
				&& (GetFrameCount (StarShipPtr->RaceDescPtr->ship_data.
				captain_control.special)
				- GetFrameIndex (StarShipPtr->RaceDescPtr->ship_data.
				captain_control.special) > 5
				|| (lpShipEvalDesc->ObjectPtr != NULL
				&& lpShipEvalDesc->which_turn <= 4)
				|| (lpWeaponEvalDesc->ObjectPtr != NULL
							/* means IMMEDIATE WEAPON */
				&& (((lpWeaponEvalDesc->ObjectPtr->state_flags & PLAYER_SHIP)
				&& ShipPtr->crew_level == 1)
				|| (PlotIntercept (lpWeaponEvalDesc->ObjectPtr, ShipPtr, 2, 0)
				&& lpWeaponEvalDesc->ObjectPtr->mass_points >=
				ShipPtr->crew_level
				&& (TFB_Random () & 1))))))
			StarShipPtr->ship_input_state |= SPECIAL;
	}
}

static void
shofixti_postprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if ((StarShipPtr->cur_status_flags
			^ StarShipPtr->old_status_flags) & SPECIAL)
	{
		StarShipPtr->RaceDescPtr->ship_data.captain_control.special =
				IncFrameIndex (StarShipPtr->RaceDescPtr->ship_data.
				captain_control.special);
		if (GetFrameCount (StarShipPtr->RaceDescPtr->ship_data.
				captain_control.special)
				- GetFrameIndex (StarShipPtr->RaceDescPtr->ship_data.
				captain_control.special) == 3)
			self_destruct (ElementPtr);
	}
}

namespace {
class ShofixtiShip final : public uqm::combat::Ship {
public:
    explicit ShofixtiShip(const RACE_DESC &definition) noexcept
        : Ship(definition) {
        legacyData().cyborg_control.intelligence_func = shofixti_intelligence;
    }
private:
    void afterMotion(ELEMENT &element) noexcept override {
        shofixti_postprocess(&element);
    }
    COUNT createWeapons(ELEMENT &element, uqm::combat::WeaponSlots &weapons) noexcept override {
        return initialize_standard_missile(&element, weapons.data());
    }
};
} // namespace

std::unique_ptr<uqm::combat::Ship>
uqm::combat::makeShofixtiShip()
{
	// Build a per-instance definition, copied into the native owner.
	RACE_DESC new_shofixti_desc;

	new_shofixti_desc = shofixtiDefinition();
	if (LOBYTE (GLOBAL (CurrentActivity)) == IN_ENCOUNTER
			&& !GET_GAME_STATE (SHOFIXTI_RECRUITED))
	{
		// Tanaka/Katana flies in a damaged ship.
		COUNT i;

		new_shofixti_desc.ship_data.ship_rsc[0] = OLDSHOF_BIG_MASK_PMAP_ANIM;
		new_shofixti_desc.ship_data.ship_rsc[1] = OLDSHOF_MED_MASK_PMAP_ANIM;
		new_shofixti_desc.ship_data.ship_rsc[2] = OLDSHOF_SML_MASK_PMAP_ANIM;
		new_shofixti_desc.ship_data.special_rsc[0] = NULL_RESOURCE;
		new_shofixti_desc.ship_data.special_rsc[1] = NULL_RESOURCE;
		new_shofixti_desc.ship_data.special_rsc[2] = NULL_RESOURCE;
		new_shofixti_desc.ship_data.captain_control.captain_rsc =
				OLDSHOF_CAPTAIN_MASK_PMAP_ANIM;

		/* Weapon doesn't work as well */
		new_shofixti_desc.characteristics.weapon_wait = 10;
		
		/* Simulate VUX limpets */
		for (i = 0; i < NumLimpets; ++i)
		{
			if (++new_shofixti_desc.characteristics.turn_wait == 0)
				--new_shofixti_desc.characteristics.turn_wait;
			if (++new_shofixti_desc.characteristics.thrust_wait == 0)
				--new_shofixti_desc.characteristics.thrust_wait;

/* This should be the same as MIN_THRUST_INCREMENT in vux.c */

			if (new_shofixti_desc.characteristics.thrust_increment <=
					MinThrustIncrement)
			{
				new_shofixti_desc.characteristics.max_thrust =
						new_shofixti_desc.characteristics.thrust_increment << 1;
			}
			else
			{
				COUNT num_thrusts;

				num_thrusts = new_shofixti_desc.characteristics.max_thrust /
						new_shofixti_desc.characteristics.thrust_increment;
				--new_shofixti_desc.characteristics.thrust_increment;
				new_shofixti_desc.characteristics.max_thrust =
						new_shofixti_desc.characteristics.thrust_increment *
						num_thrusts;
			}
		}
	}

	return std::make_unique<ShofixtiShip>(new_shofixti_desc);
}

