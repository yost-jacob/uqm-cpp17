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

#include "uqm/globdata.h"
#include "libs/mathlib.h"

// Balance values are immutable, loaded from base/ships.json for the active session.
static const uqm::combat::ShipTuning &tuning() {
    return uqm::combat::shipTuning("vux");
}
static const uqm::combat::ProjectileStats &limpetStats() {
    return tuning().projectile("limpet");
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto VuxOffset = 12;
constexpr auto LaserBase = 150;
constexpr auto LaserRange = DISPLAY_TO_WORLD (LaserBase + VuxOffset);
constexpr auto LimpetOffset = 8;
constexpr auto MinThrustIncrement = DISPLAY_TO_WORLD (1);
constexpr auto WarpOffset = 46;
constexpr auto MaxxEntryDist = DISPLAY_TO_WORLD ((LaserBase + VuxOffset + WarpOffset) << 1);
constexpr auto MaxyEntryDist = DISPLAY_TO_WORLD ((LaserBase + VuxOffset + WarpOffset) << 1);

// WarpOffset extends the entry distance beyond the laser range. Entry distances remain
// relative to the laser geometry, not the display resolution.

static RACE_DESC vuxDefinition() {
    const RACE_DESC definition = {
        { /* SHIP_INFO */
            FIRES_FORE | SEEKING_SPECIAL | IMMEDIATE_WEAPON,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, tuning().ship.maxCrew,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            VUX_RACE_STRINGS,
            VUX_ICON_MASK_PMAP_ANIM,
            VUX_MICON_MASK_PMAP_ANIM,
            NULL, NULL, NULL
        },
        { /* FLEET_STUFF */
            900 / SPHERE_RADIUS_INCREMENT * 2, /* Initial SoI radius */
            { /* Known location (center of SoI) */
                4412, 1558,
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
                VUX_BIG_MASK_PMAP_ANIM,
                VUX_MED_MASK_PMAP_ANIM,
                VUX_SML_MASK_PMAP_ANIM,
            },
            {
                SLIME_MASK_PMAP_ANIM,
                NULL_RESOURCE,
                NULL_RESOURCE,
            },
            {
                LIMPETS_BIG_MASK_PMAP_ANIM,
                LIMPETS_MED_MASK_PMAP_ANIM,
                LIMPETS_SML_MASK_PMAP_ANIM,
            },
            {
                VUX_CAPTAIN_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
            VUX_VICTORY_SONG,
            VUX_SHIP_SOUNDS,
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

namespace {
class VuxLimpet final : public uqm::combat::ConfiguredProjectile {
public:
    using ConfiguredProjectile::ConfiguredProjectile;
    void collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) override;
    void update(ELEMENT &element) override {
        ConfiguredProjectile::update(element);
        element.next.image.frame = IncFrameIndex(element.next.image.frame);
        element.state_flags |= CHANGING;
    }
};
}

void VuxLimpet::collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) {
    auto *ElementPtr0 = &self;
    auto *pPt0 = &point;
    auto *ElementPtr1 = &other;
    auto *pPt1 = &otherPoint;

	if (ElementPtr1->state_flags & PLAYER_SHIP)
	{
		STAMP s;
		STARSHIP *StarShipPtr;
		RACE_DESC *RDPtr;

		GetElementStarShip (ElementPtr1, &StarShipPtr);
		RDPtr = StarShipPtr->RaceDescPtr;

		if (++RDPtr->characteristics.turn_wait == 0)
			--RDPtr->characteristics.turn_wait;
		if (++RDPtr->characteristics.thrust_wait == 0)
			--RDPtr->characteristics.thrust_wait;
		if (RDPtr->characteristics.thrust_increment <= MinThrustIncrement)
		{
			RDPtr->characteristics.max_thrust =
					RDPtr->characteristics.thrust_increment << 1;
		}
		else
		{
			COUNT num_thrusts;

			num_thrusts = RDPtr->characteristics.max_thrust /
					RDPtr->characteristics.thrust_increment;
			--RDPtr->characteristics.thrust_increment;
			RDPtr->characteristics.max_thrust =
					RDPtr->characteristics.thrust_increment * num_thrusts;
		}
		RDPtr->cyborg_control.ManeuverabilityIndex = 0;

		GetElementStarShip (ElementPtr0, &StarShipPtr);
		ProcessSound (SetAbsSoundIndex (
						/* LIMPET_AFFIXES */
				StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 2), ElementPtr1);
		s.frame = SetAbsFrameIndex (
				StarShipPtr->RaceDescPtr->ship_data.weapon[0], (COUNT)TFB_Random ()
				);
		ModifySilhouette (ElementPtr1, &s, MODIFY_IMAGE);
	}

	ElementPtr0->hit_points = 0;
	ElementPtr0->life_span = 0;
	ElementPtr0->state_flags |= COLLISION | DISAPPEARING;

	(void) pPt0;  /* Satisfying compiler (unused parameter) */
	(void) pPt1;  /* Satisfying compiler (unused parameter) */
}


static void
spawn_limpets (ELEMENT *ElementPtr)
{
	HELEMENT Limpet;
	STARSHIP *StarShipPtr;
	MISSILE_BLOCK MissileBlock;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	MissileBlock.farray = StarShipPtr->RaceDescPtr->ship_data.special;
	MissileBlock.face = StarShipPtr->ShipFacing + HALF_CIRCLE;
	MissileBlock.index = 0;
	MissileBlock.sender = ElementPtr->playerNr;
	MissileBlock.flags = IGNORE_SIMILAR;
	MissileBlock.pixoffs = LimpetOffset;
	MissileBlock.speed = limpetStats().speed;
	MissileBlock.hit_points = limpetStats().hitPoints;
	MissileBlock.damage = limpetStats().damage;
	MissileBlock.life = limpetStats().lifetime;
	MissileBlock.preprocess_func = nullptr;
	MissileBlock.blast_offs = 0;

	MissileBlock.cx = ElementPtr->next.location.x;
	MissileBlock.cy = ElementPtr->next.location.y;
	Limpet = uqm::combat::publishMissile<VuxLimpet>(MissileBlock, limpetStats().flight);
	if (Limpet)
	{
		ELEMENT *LimpetPtr;

		LockElement (Limpet, &LimpetPtr);

		SetElementStarShip (LimpetPtr, StarShipPtr);
		UnlockElement (Limpet);

		PutElement (Limpet);
	}
}

static COUNT
initialize_horrific_laser (ELEMENT *ShipPtr, HELEMENT LaserArray[])
{
	STARSHIP *StarShipPtr;
	uqm::combat::BeamLaunch beam{};

	GetElementStarShip (ShipPtr, &StarShipPtr);
	beam.facing = StarShipPtr->ShipFacing;
	beam.origin.x = ShipPtr->next.location.x;
	beam.origin.y = ShipPtr->next.location.y;
	beam.displacement.x = COSINE (FACING_TO_ANGLE (beam.facing), LaserRange);
	beam.displacement.y = SINE (FACING_TO_ANGLE (beam.facing), LaserRange);
	beam.player = ShipPtr->playerNr;
	beam.flags = IGNORE_SIMILAR;
	beam.muzzleOffset = VuxOffset;
	beam.color = BUILD_COLOR (MAKE_RGB15 (0x0A, 0x1F, 0x0A), 0x0A);
	LaserArray[0] = uqm::combat::publishBeam(beam);

	return (1);
}

static void
vux_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
		COUNT ConcernCounter)
{
	EVALUATE_DESC *lpEvalDesc;
	STARSHIP *StarShipPtr;

	lpEvalDesc = &ObjectsOfConcern[ENEMY_SHIP_INDEX];
	 lpEvalDesc->MoveState = PURSUE;
	if (ObjectsOfConcern[ENEMY_WEAPON_INDEX].ObjectPtr != 0
			&& ObjectsOfConcern[ENEMY_WEAPON_INDEX].MoveState == ENTICE)
	{
		if ((ObjectsOfConcern[ENEMY_WEAPON_INDEX].ObjectPtr->state_flags
				& FINITE_LIFE)
				&& !(ObjectsOfConcern[ENEMY_WEAPON_INDEX].ObjectPtr->state_flags
				& CREW_OBJECT))
			ObjectsOfConcern[ENEMY_WEAPON_INDEX].MoveState = AVOID;
		else
			ObjectsOfConcern[ENEMY_WEAPON_INDEX].MoveState = PURSUE;
	}

	ship_intelligence (ShipPtr,
			ObjectsOfConcern, ConcernCounter);

	GetElementStarShip (ShipPtr, &StarShipPtr);
	if (StarShipPtr->special_counter == 0
			&& lpEvalDesc->ObjectPtr != 0
			&& lpEvalDesc->which_turn <= 12
			&& (StarShipPtr->ship_input_state & (LEFT | RIGHT))
			&& StarShipPtr->RaceDescPtr->ship_info.energy_level >=
			(BYTE)(StarShipPtr->RaceDescPtr->ship_info.max_energy >> 1))
		StarShipPtr->ship_input_state |= SPECIAL;
	else
		StarShipPtr->ship_input_state &= ~SPECIAL;
}

static void
vux_postprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if ((StarShipPtr->cur_status_flags & SPECIAL)
			&& StarShipPtr->special_counter == 0
			&& DeltaEnergy (ElementPtr, -tuning().ship.specialEnergyCost))
	{
		ProcessSound (SetAbsSoundIndex (
						/* LAUNCH_LIMPET */
				StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 1), ElementPtr);
		spawn_limpets (ElementPtr);

		StarShipPtr->special_counter =
				StarShipPtr->RaceDescPtr->characteristics.special_wait;
	}
}

static void
vux_preprocess (ELEMENT *ElementPtr)
{
	if (ElementPtr->state_flags & APPEARING)
	{
		COUNT facing;
		STARSHIP *StarShipPtr;

		GetElementStarShip (ElementPtr, &StarShipPtr);
		facing = StarShipPtr->ShipFacing;
		if (LOBYTE (GLOBAL (CurrentActivity)) != IN_ENCOUNTER
				&& TrackShip (ElementPtr, &facing) >= 0)
		{
			ELEMENT *OtherShipPtr;

			LockElement (ElementPtr->hTarget, &OtherShipPtr);

			do
			{
				SIZE dx, dy;

				ElementPtr->current.location.x =
						(OtherShipPtr->current.location.x -
						(MaxxEntryDist >> 1)) +
						((COUNT)TFB_Random () % MaxxEntryDist);
				ElementPtr->current.location.y =
						(OtherShipPtr->current.location.y -
						(MaxyEntryDist >> 1)) +
						((COUNT)TFB_Random () % MaxyEntryDist);
				dx = OtherShipPtr->current.location.x -
						ElementPtr->current.location.x;
				dy = OtherShipPtr->current.location.y -
						ElementPtr->current.location.y;
				facing = NORMALIZE_FACING (
						ANGLE_TO_FACING (ARCTAN (dx, dy))
						);
				ElementPtr->current.image.frame =
						SetAbsFrameIndex (ElementPtr->current.image.frame,
						facing);

				ElementPtr->current.location.x =
						WRAP_X (DISPLAY_ALIGN (ElementPtr->current.location.x));
				ElementPtr->current.location.y =
						WRAP_Y (DISPLAY_ALIGN (ElementPtr->current.location.y));
			} while (CalculateGravity (ElementPtr)
					|| TimeSpaceMatterConflict (ElementPtr));

			UnlockElement (ElementPtr->hTarget);
			ElementPtr->hTarget = 0;

			ElementPtr->next = ElementPtr->current;
			InitIntersectStartPoint (ElementPtr);
			InitIntersectEndPoint (ElementPtr);
			InitIntersectFrame (ElementPtr);

			StarShipPtr->ShipFacing = facing;
		}

		uqm::combat::shipFor(*StarShipPtr->RaceDescPtr).enablePreprocessing(false);
	}
}

namespace {
class VuxShip final : public uqm::combat::Ship {
public:
    explicit VuxShip(const RACE_DESC &definition) noexcept
        : Ship(definition) {
        legacyData().cyborg_control.intelligence_func = vux_intelligence;
    }
private:
    void beforeMotion(ELEMENT &element) noexcept override {
        vux_preprocess(&element);
    }
    void afterMotion(ELEMENT &element) noexcept override {
        vux_postprocess(&element);
    }
    COUNT createWeapons(ELEMENT &element, uqm::combat::WeaponSlots &weapons) noexcept override {
        return initialize_horrific_laser(&element, weapons.data());
    }
};
} // namespace

std::unique_ptr<uqm::combat::Ship>
uqm::combat::makeVuxShip()
{

	return std::make_unique<VuxShip>(vuxDefinition());
}

