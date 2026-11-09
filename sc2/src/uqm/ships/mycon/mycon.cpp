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
#include <algorithm>
#include "resinst.h"

// Balance values are immutable, loaded from base/ships.json at startup.
static const uqm::combat::ShipTuning &tuning() {
    static const auto &value = uqm::combat::shipTuning("mycon");
    return value;
}

static const uqm::combat::ProjectileStats &plasmaStats() {
    return tuning().projectile("plasma");
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto MyconOffset = 24;
constexpr auto MissileOffset = 0;
constexpr auto NumPlasmas = 11;
constexpr auto NumGloballs = 8;

constexpr auto RegenerationAmount = 4;

static const RACE_DESC &myconDefinition() {
    static const RACE_DESC definition = {
        { /* SHIP_INFO */
            FIRES_FORE | SEEKING_WEAPON,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, tuning().ship.maxCrew,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            MYCON_RACE_STRINGS,
            MYCON_ICON_MASK_PMAP_ANIM,
            MYCON_MICON_MASK_PMAP_ANIM,
            NULL, NULL, NULL
        },
        { /* FLEET_STUFF */
            1070 / SPHERE_RADIUS_INCREMENT * 2, /* Initial SoI radius */
            { /* Known location (center of SoI) */
                6392, 2200,
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
                MYCON_BIG_MASK_PMAP_ANIM,
                MYCON_MED_MASK_PMAP_ANIM,
                MYCON_SML_MASK_PMAP_ANIM,
            },
            {
                PLASMA_BIG_MASK_PMAP_ANIM,
                PLASMA_MED_MASK_PMAP_ANIM,
                PLASMA_SML_MASK_PMAP_ANIM,
            },
            {
                NULL_RESOURCE,
                NULL_RESOURCE,
                NULL_RESOURCE,
            },
            {
                MYCON_CAPTAIN_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
            MYCON_VICTORY_SONG,
            MYCON_SHIP_SOUNDS,
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            NULL, NULL
        },
        {
            0,
            uqm::combat::weaponRangeHint(DISPLAY_TO_WORLD (800)),
            NULL,
        },

        0,
        0, /* ship_owner */
    };
    return definition;
}

class PlasmaProjectile final : public uqm::combat::ConfiguredProjectile {
public:
    using ConfiguredProjectile::ConfiguredProjectile;
    void collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) override;
    void update(ELEMENT &element) override {
        const auto &stats = definition().stats;
        const unsigned duration = std::max<unsigned>(1, (stats.lifetime + NumPlasmas - 1) / NumPlasmas);
        if (element.mass_points > element.hit_points)
            element.life_span = static_cast<COUNT>(std::min<unsigned>(stats.lifetime, element.hit_points * duration));
        else element.hit_points = static_cast<BYTE>((unsigned(element.life_span) * stats.damage + stats.lifetime - 1) / stats.lifetime);
        element.mass_points = element.hit_points;
        const auto frame = static_cast<COUNT>(std::clamp<int>(NumPlasmas - (element.life_span + duration - 1) / duration, 0, NumPlasmas - 1));
        if (frame != GetFrameIndex(element.next.image.frame)) {
            element.next.image.frame = SetAbsFrameIndex(element.next.image.frame, frame);
            element.state_flags |= CHANGING;
        }
        ConfiguredProjectile::update(element);
    }
};

static void
plasma_blast_preprocess (ELEMENT *ElementPtr)
{
	if (ElementPtr->life_span >= ElementPtr->thrust_wait)
		ElementPtr->next.image.frame =
				IncFrameIndex (ElementPtr->next.image.frame);
	else
		ElementPtr->next.image.frame =
				DecFrameIndex (ElementPtr->next.image.frame);
	if (ElementPtr->hTarget)
	{
		ELEMENT *ShipPtr;

		LockElement (ElementPtr->hTarget, &ShipPtr);
		ElementPtr->next.location = ShipPtr->next.location;
		UnlockElement (ElementPtr->hTarget);
	}

	ElementPtr->state_flags |= CHANGING;
}

void PlasmaProjectile::collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) {
    auto *ElementPtr0 = &self;
    auto *pPt0 = &point;
    auto *ElementPtr1 = &other;
    auto *pPt1 = &otherPoint;

	SIZE old_mass;
	HELEMENT hBlastElement;

	old_mass = (SIZE)ElementPtr0->mass_points;
	if ((ElementPtr0->pParent != ElementPtr1->pParent
			|| (ElementPtr1->state_flags & PLAYER_SHIP))
			&& (hBlastElement =
			weapon_collision (ElementPtr0, pPt0, ElementPtr1, pPt1)))
	{
		SIZE num_animations;
		ELEMENT *BlastElementPtr;

		LockElement (hBlastElement, &BlastElementPtr);
		BlastElementPtr->pParent = ElementPtr0->pParent;
		if (!(ElementPtr1->state_flags & PLAYER_SHIP))
			BlastElementPtr->hTarget = 0;
		else
		{
			STARSHIP *StarShipPtr;

			GetElementStarShip (ElementPtr1, &StarShipPtr);
			BlastElementPtr->hTarget = StarShipPtr->hShip;
		}

		BlastElementPtr->current.location = ElementPtr1->current.location;

		if ((num_animations =
				(old_mass * NumGloballs +
				(plasmaStats().damage - 1)) / plasmaStats().damage) == 0)
			num_animations = 1;

		BlastElementPtr->thrust_wait = (BYTE)num_animations;
		BlastElementPtr->life_span = (num_animations << 1) - 1;
		{
			BlastElementPtr->preprocess_func = plasma_blast_preprocess;
		}
		BlastElementPtr->current.image.farray = ElementPtr0->next.image.farray;
		BlastElementPtr->current.image.frame =
				SetAbsFrameIndex (BlastElementPtr->current.image.farray[0],
				NumPlasmas);

		UnlockElement (hBlastElement);
	}
}


static void
mycon_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
		COUNT ConcernCounter)
{
	STARSHIP *StarShipPtr;
	EVALUATE_DESC *lpEvalDesc;

	lpEvalDesc = &ObjectsOfConcern[ENEMY_WEAPON_INDEX];
	if (lpEvalDesc->ObjectPtr && lpEvalDesc->MoveState == ENTICE)
	{
		if ((lpEvalDesc->ObjectPtr->state_flags & FINITE_LIFE)
				&& !(lpEvalDesc->ObjectPtr->state_flags & CREW_OBJECT))
			lpEvalDesc->MoveState = AVOID;
		else
			lpEvalDesc->MoveState = PURSUE;
	}

	ship_intelligence (ShipPtr, ObjectsOfConcern, ConcernCounter);

	GetElementStarShip (ShipPtr, &StarShipPtr);
	if (ObjectsOfConcern[ENEMY_WEAPON_INDEX].MoveState == PURSUE)
		StarShipPtr->ship_input_state &= ~THRUST; /* don't pursue seekers */

	lpEvalDesc = &ObjectsOfConcern[ENEMY_SHIP_INDEX];
	if (StarShipPtr->weapon_counter == 0
			&& lpEvalDesc->ObjectPtr
			&& (lpEvalDesc->which_turn <= 16
			|| ShipPtr->crew_level == StarShipPtr->RaceDescPtr->ship_info.max_crew))
	{
		COUNT travel_facing, direction_facing;
		SIZE delta_x, delta_y;

		travel_facing = NORMALIZE_FACING (
				ANGLE_TO_FACING (GetVelocityTravelAngle (&ShipPtr->velocity)
				+ HALF_CIRCLE)
				);
		delta_x = lpEvalDesc->ObjectPtr->current.location.x
				- ShipPtr->current.location.x;
		delta_y = lpEvalDesc->ObjectPtr->current.location.y
				- ShipPtr->current.location.y;
		direction_facing = NORMALIZE_FACING (
				ANGLE_TO_FACING (ARCTAN (delta_x, delta_y))
				);

		if (NORMALIZE_FACING (direction_facing
				- StarShipPtr->ShipFacing
				+ ANGLE_TO_FACING (QUADRANT))
				<= ANGLE_TO_FACING (HALF_CIRCLE)
				&& (!(StarShipPtr->cur_status_flags &
				(SHIP_BEYOND_MAX_SPEED | SHIP_IN_GRAVITY_WELL))
				|| NORMALIZE_FACING (direction_facing
				- travel_facing + ANGLE_TO_FACING (OCTANT))
				<= ANGLE_TO_FACING (QUADRANT)))
			StarShipPtr->ship_input_state |= WEAPON;
	}

	if (StarShipPtr->special_counter == 0)
	{
		StarShipPtr->ship_input_state &= ~SPECIAL;
		StarShipPtr->RaceDescPtr->cyborg_control.WeaponRange = DISPLAY_TO_WORLD (800);
		if (ShipPtr->crew_level < StarShipPtr->RaceDescPtr->ship_info.max_crew)
		{
			StarShipPtr->RaceDescPtr->cyborg_control.WeaponRange = plasmaStats().speed * plasmaStats().lifetime;
			if (StarShipPtr->RaceDescPtr->ship_info.energy_level >= tuning().ship.specialEnergyCost
					&& !(StarShipPtr->ship_input_state & WEAPON))
				StarShipPtr->ship_input_state |= SPECIAL;
		}
	}
}

static COUNT
initialize_plasma (ELEMENT *ShipPtr, HELEMENT PlasmaArray[])
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
	MissileBlock.flags = 0;
	MissileBlock.pixoffs = MyconOffset;
	MissileBlock.speed = plasmaStats().speed;
	MissileBlock.hit_points = plasmaStats().damage;
	MissileBlock.damage = plasmaStats().damage;
	MissileBlock.life = plasmaStats().lifetime;
	MissileBlock.preprocess_func = nullptr;
	MissileBlock.blast_offs = MissileOffset;
	PlasmaArray[0] = uqm::combat::publishMissile<PlasmaProjectile>(MissileBlock, plasmaStats().flight);


	return (1);
}

static void
mycon_postprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if ((StarShipPtr->cur_status_flags & SPECIAL)
			&& StarShipPtr->special_counter == 0
			&& ElementPtr->crew_level != StarShipPtr->RaceDescPtr->ship_info.max_crew
			&& DeltaEnergy (ElementPtr, -tuning().ship.specialEnergyCost))
	{
		SIZE add_crew;

		ProcessSound (SetAbsSoundIndex (
						/* GROW_NEW_CREW */
				StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 1), ElementPtr);
		if ((add_crew = RegenerationAmount) >
				StarShipPtr->RaceDescPtr->ship_info.max_crew - ElementPtr->crew_level)
			add_crew = StarShipPtr->RaceDescPtr->ship_info.max_crew - ElementPtr->crew_level;
		DeltaCrew (ElementPtr, add_crew);
		
		StarShipPtr->special_counter =
				StarShipPtr->RaceDescPtr->characteristics.special_wait;
	}
}

namespace {
class MyconShip final : public uqm::combat::Ship {
public:
    explicit MyconShip(const RACE_DESC &definition) noexcept
        : Ship(definition) {
        legacyData().cyborg_control.intelligence_func = mycon_intelligence;
    }
private:
    void afterMotion(ELEMENT &element) noexcept override {
        mycon_postprocess(&element);
    }
    COUNT createWeapons(ELEMENT &element, uqm::combat::WeaponSlots &weapons) noexcept override {
        return initialize_plasma(&element, weapons.data());
    }
};
} // namespace

std::unique_ptr<uqm::combat::Ship>
uqm::combat::makeMyconShip()
{

	return std::make_unique<MyconShip>(myconDefinition());
}

