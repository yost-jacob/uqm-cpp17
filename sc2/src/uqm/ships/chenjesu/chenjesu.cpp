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
#include "uqm/combat_entity_legacy.hpp"
#include "uqm/animation_effect.hpp"

#include "uqm/globdata.h"
#include "libs/mathlib.h"

// Balance values are immutable, loaded from base/ships.json at startup.
static const uqm::combat::ShipTuning &tuning() {
    static const auto &value = uqm::combat::shipTuning("chenjesu");
    return value;
}
static const uqm::combat::ProjectileStats &fragmentStats() {
    static const auto &value = tuning().projectile("fragment");
    return value;
}
static const uqm::combat::ProjectileStats &missileStats() {
    static const auto &value = tuning().projectile("missile");
    return value;
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto ChenjesuOffset = 16;
constexpr auto MissileOffset = 0;
constexpr auto NumSparkles = 8;
constexpr auto FragmentOffset = 2;
constexpr auto NumFragments = 8;
static auto fragmentRange() { return (fragmentStats().lifetime * fragmentStats().speed); }
constexpr auto DoggyOffset = 18;
constexpr auto DoggySpeed = DISPLAY_TO_WORLD (8);
constexpr auto EnergyDrain = 10;
constexpr auto MaxDoggies = 4;
constexpr auto DoggyHits = 3;
constexpr auto DoggyMass = 4;

// The crystal extends its lifetime while fire is held. fragmentRange is an AI hint.

static const RACE_DESC &chenjesuDefinition() {
    static const RACE_DESC definition = {
        { /* SHIP_INFO */
            FIRES_FORE | SEEKING_SPECIAL | SEEKING_WEAPON,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, tuning().ship.maxCrew,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            CHENJESU_RACE_STRINGS,
            CHENJESU_ICON_MASK_PMAP_ANIM,
            CHENJESU_MICON_MASK_PMAP_ANIM,
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
                CHENJESU_BIG_MASK_PMAP_ANIM,
                CHENJESU_MED_MASK_PMAP_ANIM,
                CHENJESU_SML_MASK_PMAP_ANIM,
            },
            {
                SPARK_BIG_MASK_PMAP_ANIM,
                SPARK_MED_MASK_PMAP_ANIM,
                SPARK_SML_MASK_PMAP_ANIM,
            },
            {
                DOGGY_BIG_MASK_PMAP_ANIM,
                DOGGY_MED_MASK_PMAP_ANIM,
                DOGGY_SML_MASK_PMAP_ANIM,
            },
            {
                CHENJESU_CAPTAIN_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
            CHENJESU_VICTORY_SONG,
            CHENJESU_SHIP_SOUNDS,
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            NULL, NULL
        },
        {
            0,
            uqm::combat::weaponRangeHint(LONG_RANGE_WEAPON),
            NULL,
        },

        0,
        0, /* ship_owner */
    };
    return definition;
}

// Per-shot behavior; engine kinematics and collision state remain borrowed.
namespace {
class ChenjesuCrystal final : public uqm::combat::Projectile {
public:
    using Projectile::Projectile;
    void collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) override;
    void postprocess(ELEMENT &element) override;
    void update(ELEMENT &element) override {
        auto *ElementPtr = &element;

	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if (StarShipPtr->cur_status_flags & WEAPON)
		++ElementPtr->life_span; /* keep it going while key pressed */
	else
	{
		ElementPtr->life_span = 1;

		uqm::detail::enableProjectilePostprocess(*ElementPtr);
	}
    }
};
}


void ChenjesuCrystal::postprocess(ELEMENT &element) {
    auto *ElementPtr = &element;

	STARSHIP *StarShipPtr;
	MISSILE_BLOCK MissileBlock;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	MissileBlock.cx = ElementPtr->next.location.x;
	MissileBlock.cy = ElementPtr->next.location.y;
	MissileBlock.farray = StarShipPtr->RaceDescPtr->ship_data.weapon;
	MissileBlock.index = 1;
	MissileBlock.sender = ElementPtr->playerNr;
	MissileBlock.flags = IGNORE_SIMILAR;
	MissileBlock.pixoffs = 0;
	MissileBlock.speed = fragmentStats().speed;
	MissileBlock.hit_points = fragmentStats().hitPoints;
	MissileBlock.damage = fragmentStats().damage;
	MissileBlock.life = fragmentStats().lifetime;
	MissileBlock.preprocess_func = NULL;
	MissileBlock.blast_offs = FragmentOffset;

	for (MissileBlock.face = 0;
			MissileBlock.face < ANGLE_TO_FACING (FULL_CIRCLE);
			MissileBlock.face +=
			(ANGLE_TO_FACING (FULL_CIRCLE) / NumFragments))
	{
		HELEMENT hFragment;

		hFragment = uqm::combat::publishMissile(MissileBlock, fragmentStats().flight);
		if (hFragment)
		{
			ELEMENT *FragPtr;

			LockElement (hFragment, &FragPtr);
			SetElementStarShip (FragPtr, StarShipPtr);
			UnlockElement (hFragment);
			PutElement (hFragment);
		}
	}

	ProcessSound (SetAbsSoundIndex (
					/* CRYSTAL_FRAGMENTS */
			StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 1), ElementPtr);
}



void ChenjesuCrystal::collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) {
    auto *ElementPtr0 = &self;
    auto *pPt0 = &point;
    auto *ElementPtr1 = &other;
    auto *pPt1 = &otherPoint;

	HELEMENT hBlastElement;

	hBlastElement =
			weapon_collision (ElementPtr0, pPt0, ElementPtr1, pPt1);
	if (hBlastElement)
	{
		ELEMENT *BlastElementPtr;

		LockElement (hBlastElement, &BlastElementPtr);
		BlastElementPtr->current.location = ElementPtr1->current.location;

		BlastElementPtr->life_span = NumSparkles;
		BlastElementPtr->turn_wait = BlastElementPtr->next_turn = 0;
		{
			uqm::combat::attachAnimation(*BlastElementPtr);
		}

		BlastElementPtr->current.image.farray = ElementPtr0->next.image.farray;
		BlastElementPtr->current.image.frame =
				SetAbsFrameIndex (BlastElementPtr->current.image.farray[0],
				2); /* skip stones */

		UnlockElement (hBlastElement);
	}
}



// Native autonomous unit; all engine records are borrowed for each hook.
namespace {
class Dogi final : public uqm::combat::CombatEntity {
public:
    bool dying_ = false;
    void update(ELEMENT &self) override;
    void expire(ELEMENT &self) override;
    void collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) override;
};
}
void Dogi::update(ELEMENT &self) {
    auto *ElementPtr = &self;
    if (dying_) { uqm::combat::advanceAnimation(self); return; }
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	++StarShipPtr->special_counter;
	if (ElementPtr->thrust_wait > 0) /* could be non-zero after a collision */
		--ElementPtr->thrust_wait;
	else
	{
		COUNT facing, orig_facing;
		SIZE delta_facing;

		facing = orig_facing =
				NORMALIZE_FACING (ANGLE_TO_FACING (
				GetVelocityTravelAngle (&ElementPtr->velocity)
				));
		if ((delta_facing = TrackShip (ElementPtr, &facing)) < 0)
			facing = NORMALIZE_FACING (TFB_Random ());
		else
		{
			ELEMENT *ShipPtr;

			LockElement (ElementPtr->hTarget, &ShipPtr);
			facing = NORMALIZE_FACING (ANGLE_TO_FACING (
					ARCTAN (ShipPtr->current.location.x -
					ElementPtr->current.location.x,
					ShipPtr->current.location.y -
					ElementPtr->current.location.y)
					));
			delta_facing = NORMALIZE_FACING (facing -
					GetFrameIndex (ShipPtr->current.image.frame));
			UnlockElement (ElementPtr->hTarget);

			if (delta_facing > ANGLE_TO_FACING (HALF_CIRCLE - OCTANT) &&
					delta_facing < ANGLE_TO_FACING (HALF_CIRCLE + OCTANT))
			{
				if (delta_facing >= ANGLE_TO_FACING (HALF_CIRCLE))
					facing -= ANGLE_TO_FACING (QUADRANT);
				else
					facing += ANGLE_TO_FACING (QUADRANT);
			}

			facing = NORMALIZE_FACING (facing);
		}

		if (facing != orig_facing)
			SetVelocityVector (&ElementPtr->velocity,
					DoggySpeed, facing);
	}

}


void Dogi::expire(ELEMENT &self) {
    auto *ElementPtr = &self;
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	ProcessSound (SetAbsSoundIndex (
					/* DOGGY_DIES */
			StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 3), ElementPtr);

	ElementPtr->state_flags &= ~DISAPPEARING;
	ElementPtr->state_flags |= NONSOLID | FINITE_LIFE;
	ElementPtr->life_span = 6;
	{
		dying_ = true;
	}
	ElementPtr->death_func = NULL;
	ElementPtr->collision_func = NULL;
	ZeroVelocityComponents (&ElementPtr->velocity);

	ElementPtr->turn_wait = ElementPtr->next_turn = 0;

}


void Dogi::collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) {
    auto *ElementPtr0 = &self;
    auto *pPt0 = &point;
    auto *ElementPtr1 = &other;
    auto *pPt1 = &otherPoint;
	collision (ElementPtr0, pPt0, ElementPtr1, pPt1);
	if ((ElementPtr1->state_flags & PLAYER_SHIP)
			&& !elementsOfSamePlayer (ElementPtr0, ElementPtr1))
	{
		STARSHIP *StarShipPtr;

		GetElementStarShip (ElementPtr0, &StarShipPtr);
		ProcessSound (SetAbsSoundIndex (
						/* DOGGY_STEALS_ENERGY */
				StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 2), ElementPtr0);
		GetElementStarShip (ElementPtr1, &StarShipPtr);
		if (StarShipPtr->RaceDescPtr->ship_info.energy_level < EnergyDrain)
			DeltaEnergy (ElementPtr1, -StarShipPtr->RaceDescPtr->ship_info.energy_level);
		else
			DeltaEnergy (ElementPtr1, -EnergyDrain);
	}
	if (ElementPtr0->thrust_wait <= COLLISION_THRUST_WAIT)
		ElementPtr0->thrust_wait += COLLISION_THRUST_WAIT << 1;

}


static void
spawn_doggy (ELEMENT *ElementPtr)
{
	HELEMENT hDoggyElement;

	if ((hDoggyElement = AllocElement ()) != 0)
	{
		COUNT angle;
		ELEMENT *DoggyElementPtr;
		STARSHIP *StarShipPtr;

		ElementPtr->state_flags |= DEFY_PHYSICS;

		PutElement (hDoggyElement);
		LockElement (hDoggyElement, &DoggyElementPtr);
		DoggyElementPtr->hit_points = DoggyHits;
		DoggyElementPtr->mass_points = DoggyMass;
		DoggyElementPtr->thrust_wait = 0;
		DoggyElementPtr->playerNr = ElementPtr->playerNr;
		DoggyElementPtr->state_flags = APPEARING;
		DoggyElementPtr->life_span = NORMAL_LIFE;
		SetPrimType (&(GLOBAL (DisplayArray))[DoggyElementPtr->PrimIndex],
				STAMP_PRIM);
		{
			uqm::detail::attachEntity(*DoggyElementPtr, std::make_unique<Dogi>(), uqm::detail::EntityHooks::Update | uqm::detail::EntityHooks::Collision | uqm::detail::EntityHooks::Expiration);
			DoggyElementPtr->postprocess_func = NULL;


		}

		GetElementStarShip (ElementPtr, &StarShipPtr);
		angle = FACING_TO_ANGLE (StarShipPtr->ShipFacing) + HALF_CIRCLE;
		DoggyElementPtr->current.location.x = ElementPtr->next.location.x
				+ COSINE (angle, DISPLAY_TO_WORLD (ChenjesuOffset + DoggyOffset));
		DoggyElementPtr->current.location.y = ElementPtr->next.location.y
				+ SINE (angle, DISPLAY_TO_WORLD (ChenjesuOffset + DoggyOffset));
		DoggyElementPtr->current.image.farray = StarShipPtr->RaceDescPtr->ship_data.special;
		DoggyElementPtr->current.image.frame = StarShipPtr->RaceDescPtr->ship_data.special[0];

		SetVelocityVector (&DoggyElementPtr->velocity,
				DoggySpeed, NORMALIZE_FACING (ANGLE_TO_FACING (angle)));

		SetElementStarShip (DoggyElementPtr, StarShipPtr);

		ProcessSound (SetAbsSoundIndex (
						/* RELEASE_DOGGY */
				StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 4), DoggyElementPtr);

		UnlockElement (hDoggyElement);
	}
}

static void
chenjesu_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
		COUNT ConcernCounter)
{
	EVALUATE_DESC *lpEvalDesc;
	STARSHIP *StarShipPtr;

	GetElementStarShip (ShipPtr, &StarShipPtr);
	StarShipPtr->ship_input_state &= ~SPECIAL;

	lpEvalDesc = &ObjectsOfConcern[ENEMY_SHIP_INDEX];
	if (lpEvalDesc->ObjectPtr)
	{
		STARSHIP *EnemyStarShipPtr;

		GetElementStarShip (lpEvalDesc->ObjectPtr, &EnemyStarShipPtr);
		if ((lpEvalDesc->which_turn <= 16
				&& MANEUVERABILITY (
				&EnemyStarShipPtr->RaceDescPtr->cyborg_control
				) >= MEDIUM_SHIP)
				|| (MANEUVERABILITY (
				&EnemyStarShipPtr->RaceDescPtr->cyborg_control
				) <= SLOW_SHIP
				&& WEAPON_RANGE (
				&EnemyStarShipPtr->RaceDescPtr->cyborg_control
				) >= LONG_RANGE_WEAPON * 3 / 4
				&& (EnemyStarShipPtr->RaceDescPtr->ship_info.ship_flags & SEEKING_WEAPON)))
			lpEvalDesc->MoveState = PURSUE;
	}

	if (StarShipPtr->special_counter == 1
			&& ObjectsOfConcern[ENEMY_WEAPON_INDEX].ObjectPtr
			&& ObjectsOfConcern[ENEMY_WEAPON_INDEX].MoveState == ENTICE
			&& ObjectsOfConcern[ENEMY_WEAPON_INDEX].which_turn <= 8)
	{
		lpEvalDesc = &ObjectsOfConcern[ENEMY_WEAPON_INDEX];
	}

	ship_intelligence (ShipPtr, ObjectsOfConcern, ConcernCounter);

	if (lpEvalDesc->ObjectPtr)
	{
		HELEMENT h, hNext;
		ELEMENT *CrystalPtr;

		h = (StarShipPtr->old_status_flags & WEAPON) ?
				GetTailElement () : (HELEMENT)0;
		for (; h; h = hNext)
		{
			LockElement (h, &CrystalPtr);
			hNext = GetPredElement (CrystalPtr);
			if (!(CrystalPtr->state_flags & NONSOLID)
					&& CrystalPtr->next.image.farray == StarShipPtr->RaceDescPtr->ship_data.weapon
					&& CrystalPtr->preprocess_func
                    && dynamic_cast<ChenjesuCrystal *>(uqm::detail::borrowEntity(disp_q, CrystalPtr))
					&& CrystalPtr->life_span > 0
					&& elementsOfSamePlayer (CrystalPtr, ShipPtr))
			{
				if (ObjectsOfConcern[ENEMY_SHIP_INDEX].ObjectPtr)
				{
					COUNT which_turn;

					if ((which_turn = PlotIntercept (CrystalPtr,
							ObjectsOfConcern[ENEMY_SHIP_INDEX].ObjectPtr,
							CrystalPtr->life_span,
							fragmentRange() / 2)) == 0
							|| (which_turn == 1
							&& PlotIntercept (CrystalPtr,
							ObjectsOfConcern[ENEMY_SHIP_INDEX].ObjectPtr,
							CrystalPtr->life_span, 0) == 0))
						StarShipPtr->ship_input_state &= ~WEAPON;
					else if (StarShipPtr->weapon_counter == 0)
					{
						StarShipPtr->ship_input_state |= WEAPON;
						lpEvalDesc = &ObjectsOfConcern[ENEMY_SHIP_INDEX];
					}

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
			{
				StarShipPtr->ship_input_state &= ~WEAPON;
				if (lpEvalDesc == &ObjectsOfConcern[ENEMY_WEAPON_INDEX])
					StarShipPtr->weapon_counter = 3;
			}
			else if (StarShipPtr->weapon_counter == 0
					&& ship_weapons (ShipPtr, lpEvalDesc->ObjectPtr, fragmentRange() / 2))
				StarShipPtr->ship_input_state |= WEAPON;
		}
	}

	if (StarShipPtr->special_counter < MaxDoggies)
	{
		if (lpEvalDesc->ObjectPtr
				&& StarShipPtr->RaceDescPtr->ship_info.energy_level <= tuning().ship.specialEnergyCost
				&& !(StarShipPtr->ship_input_state & WEAPON))
			StarShipPtr->ship_input_state |= SPECIAL;
	}
}

static COUNT
initialize_crystal (ELEMENT *ShipPtr, HELEMENT CrystalArray[])
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
	MissileBlock.pixoffs = ChenjesuOffset;
	MissileBlock.speed = missileStats().speed;
	MissileBlock.hit_points = missileStats().hitPoints;
	MissileBlock.damage = missileStats().damage;
	MissileBlock.life = missileStats().lifetime;
	MissileBlock.preprocess_func = nullptr;
	MissileBlock.blast_offs = MissileOffset;
	CrystalArray[0] = uqm::combat::publishMissile<ChenjesuCrystal>(MissileBlock);


	return (1);
}

static void
chenjesu_postprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if ((StarShipPtr->cur_status_flags & SPECIAL)
			&& StarShipPtr->special_counter < MaxDoggies
			&& DeltaEnergy (ElementPtr, -tuning().ship.specialEnergyCost))
	{
		spawn_doggy (ElementPtr);
	}

	StarShipPtr->special_counter = 1;
			/* say there is one doggy, because ship_postprocess will
			 * decrement special_counter */
}

static void
chenjesu_preprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if (StarShipPtr->special_counter > 1) /* only when STANDARD
											 * computer opponent
											 */
		StarShipPtr->special_counter += MaxDoggies;
	if (StarShipPtr->cur_status_flags
			& StarShipPtr->old_status_flags
			& WEAPON)
		++StarShipPtr->weapon_counter;
}

namespace {
class ChenjesuShip final : public uqm::combat::Ship {
public:
    explicit ChenjesuShip(const RACE_DESC &definition) noexcept
        : Ship(definition) {
        legacyData().cyborg_control.intelligence_func = chenjesu_intelligence;
    }
private:
    void beforeMotion(ELEMENT &element) noexcept override {
        chenjesu_preprocess(&element);
    }
    void afterMotion(ELEMENT &element) noexcept override {
        chenjesu_postprocess(&element);
    }
    COUNT createWeapons(ELEMENT &element, uqm::combat::WeaponSlots &weapons) noexcept override {
        return initialize_crystal(&element, weapons.data());
    }
};
} // namespace

std::unique_ptr<uqm::combat::Ship>
uqm::combat::makeChenjesuShip()
{

	return std::make_unique<ChenjesuShip>(chenjesuDefinition());
}

