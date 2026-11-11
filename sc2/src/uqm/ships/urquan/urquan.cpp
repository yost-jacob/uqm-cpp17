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
#include "resinst.h"
#include "uqm/combat_entity_legacy.hpp"
#include "uqm/animation_effect.hpp"
#include "uqm/entity_handle.hpp"

#include "uqm/globdata.h"

#include <stdlib.h>

// Balance values are immutable, loaded from base/ships.json at startup.
static const uqm::combat::ShipTuning &tuning() {
    static const auto &value = uqm::combat::shipTuning("urquan");
    return value;
}
static const uqm::combat::ProjectileStats &missileStats() {
    static const auto &value = tuning().projectile("missile");
    return value;
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto MissileOffset = 8;
constexpr auto UrquanOffset = 32;
constexpr auto FighterOffset = 4;
constexpr auto FighterSpeed = DISPLAY_TO_WORLD (8);
constexpr auto OneWayFlight = 125;
constexpr auto TrackThreshold = 6;
constexpr auto FighterLife = (OneWayFlight + OneWayFlight + 150);
constexpr auto FighterHits = 1;
constexpr auto FighterMass = 0;
constexpr auto FighterWeaponWait = 8;
constexpr auto FighterLaserRange = DISPLAY_TO_WORLD (40 + FighterOffset);

static const RACE_DESC &urquanDefinition() {
    static const RACE_DESC definition = {
        { /* SHIP_INFO */
            FIRES_FORE | SEEKING_SPECIAL,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, tuning().ship.maxCrew,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            URQUAN_RACE_STRINGS,
            URQUAN_ICON_MASK_PMAP_ANIM,
            URQUAN_MICON_MASK_PMAP_ANIM,
            NULL, NULL, NULL
        },
        { /* FLEET_STUFF */
            2666 / SPHERE_RADIUS_INCREMENT * 2, /* Initial SoI radius */
            { /* Known location (center of SoI) */
                5750, 6000,
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
                URQUAN_BIG_MASK_PMAP_ANIM,
                URQUAN_MED_MASK_PMAP_ANIM,
                URQUAN_SML_MASK_PMAP_ANIM,
            },
            {
                FUSION_BIG_MASK_PMAP_ANIM,
                FUSION_MED_MASK_PMAP_ANIM,
                FUSION_SML_MASK_PMAP_ANIM,
            },
            {
                FIGHTER_BIG_MASK_PMAP_ANIM,
                FIGHTER_MED_MASK_PMAP_ANIM,
                FIGHTER_SML_MASK_PMAP_ANIM,
            },
            {
                URQUAN_CAPTAIN_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
            URQUAN_VICTORY_SONG,
            URQUAN_SHIP_SOUNDS,
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
initialize_fusion (ELEMENT *ShipPtr, HELEMENT FusionArray[])
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
	MissileBlock.pixoffs = UrquanOffset;
	MissileBlock.speed = missileStats().speed;
	MissileBlock.hit_points = missileStats().hitPoints;
	MissileBlock.damage = missileStats().damage;
	MissileBlock.life = missileStats().lifetime;
	MissileBlock.preprocess_func = NULL;
	MissileBlock.blast_offs = MissileOffset;
	FusionArray[0] = uqm::combat::publishMissile(MissileBlock, missileStats().flight);

	return (1);
}


// Native autonomous unit; all engine records are borrowed for each hook.
namespace {
class Fighter final : public uqm::combat::CombatEntity {
public:
    explicit Fighter(uqm::combat::EntityHandle carrier) : carrier_(carrier) {}

    void postprocess(ELEMENT &self) override;
    void update(ELEMENT &self) override;
    void collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) override;
private:
    uqm::combat::EntityHandle carrier_;
};
}
void Fighter::postprocess(ELEMENT &self) {
    auto *ElementPtr = &self;
	HELEMENT Laser;
	STARSHIP *StarShipPtr;
	uqm::combat::BeamLaunch beam{};

	GetElementStarShip (ElementPtr, &StarShipPtr);
	beam.origin.x = ElementPtr->next.location.x;
	beam.origin.y = ElementPtr->next.location.y;
	beam.facing = ElementPtr->thrust_wait;
	beam.displacement.x = COSINE (FACING_TO_ANGLE (beam.facing), FighterLaserRange);
	beam.displacement.y = SINE (FACING_TO_ANGLE (beam.facing), FighterLaserRange);
	beam.player = ElementPtr->playerNr;
	beam.flags = IGNORE_SIMILAR;
	beam.muzzleOffset = FighterOffset;
	beam.color = BUILD_COLOR (MAKE_RGB15 (0x1F, 0x1F, 0x0A), 0x0E);
	Laser = uqm::combat::publishBeam(beam);
	if (Laser)
	{
		ELEMENT *LaserPtr;

		LockElement (Laser, &LaserPtr);
		SetElementStarShip (LaserPtr, StarShipPtr);

		ProcessSound (SetAbsSoundIndex (
						/* FIGHTER_ZAP */
				StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 2), LaserPtr);

		UnlockElement (Laser);
		PutElement (Laser);
	}

	ElementPtr->postprocess_func = 0;
	ElementPtr->thrust_wait = FighterWeaponWait;

}


void Fighter::update(ELEMENT &self) {
    auto *ElementPtr = &self;
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);

	++StarShipPtr->RaceDescPtr->characteristics.special_wait;
	if (FighterLife - ElementPtr->life_span > TrackThreshold
			&& !(ElementPtr->state_flags & CHANGING))
	{
		BOOLEAN Enroute;
		COUNT orig_facing, facing;
		SIZE delta_x, delta_y;
		ELEMENT *eptr;

		Enroute = TRUE;

		delta_x = StarShipPtr->RaceDescPtr->ship_info.crew_level;
		delta_y = ElementPtr->life_span;

		orig_facing = facing =
				GetFrameIndex (ElementPtr->current.image.frame);
		if (((delta_y & 1) || ElementPtr->hTarget
				|| TrackShip (ElementPtr, &facing) >= 0)
				&& (delta_x == 0 || delta_y >= OneWayFlight))
			ElementPtr->state_flags |= IGNORE_SIMILAR;
		else if (delta_x)
		{
            const auto carrier = carrier_.resolve(disp_q);
            if (!carrier) { ElementPtr->life_span = 0; return; }
            eptr = &carrier->get();
			delta_x = eptr->current.location.x
					- ElementPtr->current.location.x;
			delta_y = eptr->current.location.y
					- ElementPtr->current.location.y;

			delta_x = WRAP_DELTA_X (delta_x);
			delta_y = WRAP_DELTA_Y (delta_y);
			facing = NORMALIZE_FACING (
					ANGLE_TO_FACING (ARCTAN (delta_x, delta_y))
					);

#ifdef NEVER
			if (delta_x < 0)
				delta_x = -delta_x;
			if (delta_y < 0)
				delta_y = -delta_y;
			if (delta_x <= LASER_RANGE && delta_y <= LASER_RANGE)
#endif /* NEVER */
				ElementPtr->state_flags &= ~IGNORE_SIMILAR;

			Enroute = FALSE;
		}

		if (ElementPtr->thrust_wait > 0)
			--ElementPtr->thrust_wait;

		if (ElementPtr->hTarget)
		{
			LockElement (ElementPtr->hTarget, &eptr);
			delta_x = eptr->current.location.x
					- ElementPtr->current.location.x;
			delta_y = eptr->current.location.y
					- ElementPtr->current.location.y;
			UnlockElement (ElementPtr->hTarget);
			delta_x = WRAP_DELTA_X (delta_x);
			delta_y = WRAP_DELTA_Y (delta_y);

			if (ElementPtr->thrust_wait == 0
					&& abs (delta_x) < FighterLaserRange * 3 / 4
					&& abs (delta_y) < FighterLaserRange * 3 / 4
					&& delta_x * delta_x + delta_y * delta_y <
					(FighterLaserRange * 3 / 4) * (FighterLaserRange * 3 / 4))
			{
				ElementPtr->thrust_wait =
						(BYTE)NORMALIZE_FACING (
						ANGLE_TO_FACING (ARCTAN (delta_x, delta_y))
						);
				uqm::detail::enableEntityHooks(*ElementPtr, uqm::detail::EntityHooks::Postprocess);
			}

			if (Enroute)
			{
				facing = GetFrameIndex (eptr->current.image.frame);
				if (ElementPtr->turn_wait & LEFT)
				{
					delta_x += COSINE (FACING_TO_ANGLE (facing - 4),
							DISPLAY_TO_WORLD (30));
					delta_y += SINE (FACING_TO_ANGLE (facing - 4),
							DISPLAY_TO_WORLD (30));
				}
				else
				{
					delta_x += COSINE (FACING_TO_ANGLE (facing + 4),
							DISPLAY_TO_WORLD (30));
					delta_y += SINE (FACING_TO_ANGLE (facing + 4),
							DISPLAY_TO_WORLD (30));
				}
				facing = NORMALIZE_FACING (
						ANGLE_TO_FACING (ARCTAN (delta_x, delta_y))
						);
			}
		}
		ElementPtr->state_flags |= CHANGING;

		if (facing != orig_facing)
			ElementPtr->next.image.frame = SetAbsFrameIndex (
					ElementPtr->next.image.frame, facing
					);
		SetVelocityVector (
				&ElementPtr->velocity, FighterSpeed, facing
				);
	}

}


void Fighter::collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) {
    auto *ElementPtr0 = &self;
    auto *pPt0 = &point;
    auto *ElementPtr1 = &other;
    auto *pPt1 = &otherPoint;
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr0, &StarShipPtr);
	if (GRAVITY_MASS (ElementPtr1->mass_points))
	{
		HELEMENT hFighterElement;

		hFighterElement = AllocElement ();
		if (hFighterElement)
		{
			COUNT primIndex, travel_facing;
			SIZE delta_facing;
			ELEMENT *FighterElementPtr;

			LockElement (hFighterElement, &FighterElementPtr);
			primIndex = FighterElementPtr->PrimIndex;
			*FighterElementPtr = *ElementPtr0;
            uqm::detail::attachEntity(*FighterElementPtr, std::make_unique<Fighter>(carrier_), uqm::detail::EntityHooks::Update | uqm::detail::EntityHooks::Collision);
			FighterElementPtr->PrimIndex = primIndex;
			(GLOBAL (DisplayArray))[primIndex] =
					(GLOBAL (DisplayArray))[ElementPtr0->PrimIndex];
			FighterElementPtr->state_flags &= ~PRE_PROCESS;
			FighterElementPtr->state_flags |= CHANGING;
			FighterElementPtr->next = FighterElementPtr->current;
			travel_facing = GetVelocityTravelAngle (
					&FighterElementPtr->velocity
					);
			delta_facing = NORMALIZE_ANGLE (
					ARCTAN (pPt1->x - pPt0->x, pPt1->y - pPt0->y)
					- travel_facing);
			if (delta_facing == 0)
			{
				if (FighterElementPtr->turn_wait & LEFT)
					travel_facing -= QUADRANT;
				else
					travel_facing += QUADRANT;
			}
			else if (delta_facing <= HALF_CIRCLE)
				travel_facing -= QUADRANT;
			else
				travel_facing += QUADRANT;

			travel_facing = NORMALIZE_FACING (ANGLE_TO_FACING (
					NORMALIZE_ANGLE (travel_facing)
					));
			FighterElementPtr->next.image.frame =
					SetAbsFrameIndex (FighterElementPtr->next.image.frame,
					travel_facing);
			SetVelocityVector (&FighterElementPtr->velocity,
					FighterSpeed, travel_facing);
			UnlockElement (hFighterElement);

			PutElement (hFighterElement);
		}

		ElementPtr0->state_flags |= DISAPPEARING | COLLISION;
	}
	else if (ElementPtr0->pParent != ElementPtr1->pParent)
	{
		ElementPtr0->blast_offset = 0;
		weapon_collision (ElementPtr0, pPt0, ElementPtr1, pPt1);
		ElementPtr0->state_flags |= DISAPPEARING | COLLISION;
	}
	else if (ElementPtr1->state_flags & PLAYER_SHIP)
	{
		ProcessSound (SetAbsSoundIndex (
						/* FIGHTERS_RETURN */
				StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 3), ElementPtr1);
		DeltaCrew (ElementPtr1, 1);
		ElementPtr0->state_flags |= DISAPPEARING | COLLISION;
	}

	if (ElementPtr0->state_flags & DISAPPEARING)
	{
		ElementPtr0->state_flags &= ~DISAPPEARING;

		ElementPtr0->hit_points = 0;
		ElementPtr0->life_span = 0;
		ElementPtr0->state_flags |= NONSOLID;

		--StarShipPtr->RaceDescPtr->characteristics.special_wait;
	}

}


static void
spawn_fighters (ELEMENT *ElementPtr)
{
	SIZE i;
	COUNT facing;
	SIZE delta_x, delta_y;
	HELEMENT hFighterElement;
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	facing = StarShipPtr->ShipFacing + ANGLE_TO_FACING (HALF_CIRCLE);
	delta_x = COSINE (FACING_TO_ANGLE (facing), DISPLAY_TO_WORLD (14));
	delta_y = SINE (FACING_TO_ANGLE (facing), DISPLAY_TO_WORLD (14));

	i = ElementPtr->crew_level > 2 ? 2 : 1;
	while (i-- && (hFighterElement = AllocElement ()))
	{
		SIZE sx, sy;
		COUNT fighter_facing;
		ELEMENT *FighterElementPtr;

		DeltaCrew (ElementPtr, -1);

		PutElement (hFighterElement);
		LockElement (hFighterElement, &FighterElementPtr);
		FighterElementPtr->hit_points = FighterHits;
		FighterElementPtr->mass_points = FighterMass;
		FighterElementPtr->thrust_wait = TrackThreshold + 1;
		FighterElementPtr->playerNr = ElementPtr->playerNr;
		FighterElementPtr->state_flags = APPEARING | FINITE_LIFE
				| CREW_OBJECT | IGNORE_SIMILAR;
		FighterElementPtr->life_span = FighterLife;
		SetPrimType (&(GLOBAL (DisplayArray))[FighterElementPtr->PrimIndex],
				STAMP_PRIM);
		{
			uqm::detail::attachEntity(*FighterElementPtr, std::make_unique<Fighter>(uqm::combat::EntityHandle::capture(disp_q, *ElementPtr)), uqm::detail::EntityHooks::Update | uqm::detail::EntityHooks::Collision);
			FighterElementPtr->postprocess_func = 0;

			FighterElementPtr->death_func = NULL;
		}

		FighterElementPtr->current.location = ElementPtr->next.location;
		if (i == 1)
		{
			FighterElementPtr->turn_wait = LEFT;
			fighter_facing = NORMALIZE_FACING (facing + 2);
			FighterElementPtr->current.location.x += delta_x - delta_y;
			FighterElementPtr->current.location.y += delta_y + delta_x;
		}
		else
		{
			FighterElementPtr->turn_wait = RIGHT;
			fighter_facing = NORMALIZE_FACING (facing - 2);
			FighterElementPtr->current.location.x += delta_x + delta_y;
			FighterElementPtr->current.location.y += delta_y - delta_x;
		}
		sx = COSINE (FACING_TO_ANGLE (fighter_facing),
				WORLD_TO_VELOCITY (FighterSpeed));
		sy = SINE (FACING_TO_ANGLE (fighter_facing),
				WORLD_TO_VELOCITY (FighterSpeed));
		SetVelocityComponents (&FighterElementPtr->velocity, sx, sy);
		FighterElementPtr->current.location.x -= VELOCITY_TO_WORLD (sx);
		FighterElementPtr->current.location.y -= VELOCITY_TO_WORLD (sy);

		FighterElementPtr->current.image.farray = StarShipPtr->RaceDescPtr->ship_data.special;
		FighterElementPtr->current.image.frame =
				SetAbsFrameIndex (StarShipPtr->RaceDescPtr->ship_data.special[0],
				fighter_facing);
		SetElementStarShip (FighterElementPtr, StarShipPtr);
		UnlockElement (hFighterElement);
	}
}

static void
urquan_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
		COUNT ConcernCounter)
{
	EVALUATE_DESC *lpEvalDesc;
	STARSHIP *StarShipPtr;

	GetElementStarShip (ShipPtr, &StarShipPtr);

	 ObjectsOfConcern[ENEMY_SHIP_INDEX].MoveState = PURSUE;
	lpEvalDesc = &ObjectsOfConcern[ENEMY_WEAPON_INDEX];
	if (lpEvalDesc->ObjectPtr
			&& lpEvalDesc->MoveState == ENTICE
			&& (!(lpEvalDesc->ObjectPtr->state_flags & CREW_OBJECT)
			|| lpEvalDesc->which_turn <= 8)
			&& (!(lpEvalDesc->ObjectPtr->state_flags & FINITE_LIFE)
			|| (lpEvalDesc->ObjectPtr->mass_points >= 4
			&& lpEvalDesc->which_turn == 2
			&& ObjectsOfConcern[ENEMY_SHIP_INDEX].which_turn > 16)))
		lpEvalDesc->MoveState = PURSUE;

	ship_intelligence (ShipPtr,
			ObjectsOfConcern, ConcernCounter);

	lpEvalDesc = &ObjectsOfConcern[ENEMY_SHIP_INDEX];
	{
		STARSHIP *EnemyStarShipPtr = NULL;

		if (lpEvalDesc->ObjectPtr)
			GetElementStarShip (lpEvalDesc->ObjectPtr, &EnemyStarShipPtr);
		if (StarShipPtr->special_counter == 0
				&& lpEvalDesc->ObjectPtr
				&& StarShipPtr->RaceDescPtr->ship_info.crew_level >
				(StarShipPtr->RaceDescPtr->ship_info.max_crew >> 2)
				&& !(EnemyStarShipPtr->RaceDescPtr->ship_info.ship_flags
				& POINT_DEFENSE)
				&& (StarShipPtr->RaceDescPtr->characteristics.special_wait < 6
				|| (MANEUVERABILITY (
						&EnemyStarShipPtr->RaceDescPtr->cyborg_control
						) <= SLOW_SHIP
				&& !(EnemyStarShipPtr->cur_status_flags & SHIP_BEYOND_MAX_SPEED))
				|| (lpEvalDesc->which_turn <= 12
				&& (StarShipPtr->ship_input_state & (LEFT | RIGHT))
				&& StarShipPtr->RaceDescPtr->ship_info.energy_level >=
				(BYTE)(StarShipPtr->RaceDescPtr->ship_info.max_energy >> 1))))
			StarShipPtr->ship_input_state |= SPECIAL;
		else
			StarShipPtr->ship_input_state &= ~SPECIAL;
	}

	StarShipPtr->RaceDescPtr->characteristics.special_wait = 0;
}

static void
urquan_postprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if ((StarShipPtr->cur_status_flags & SPECIAL)
			&& ElementPtr->crew_level > 1
			&& StarShipPtr->special_counter == 0
			&& DeltaEnergy (ElementPtr, -tuning().ship.specialEnergyCost))
	{
		ProcessSound (SetAbsSoundIndex (
						/* LAUNCH_FIGHTERS */
				StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 1), ElementPtr);
		spawn_fighters (ElementPtr);

		StarShipPtr->special_counter = tuning().ship.specialWait;
	}
}

namespace {
class UrQuanShip final : public uqm::combat::Ship {
public:
    explicit UrQuanShip(const RACE_DESC &definition) noexcept
        : Ship(definition) {
        legacyData().cyborg_control.intelligence_func = urquan_intelligence;
    }
private:
    void afterMotion(ELEMENT &element) noexcept override {
        urquan_postprocess(&element);
    }
    COUNT createWeapons(ELEMENT &element, uqm::combat::WeaponSlots &weapons) noexcept override {
        return initialize_fusion(&element, weapons.data());
    }
};
} // namespace

std::unique_ptr<uqm::combat::Ship>
uqm::combat::makeUrQuanShip()
{

	return std::make_unique<UrQuanShip>(urquanDefinition());
}

