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

#include "uqm/globdata.h"
#include "libs/mathlib.h"

// Balance values are immutable, loaded from base/ships.json at startup.
static const uqm::combat::ShipTuning &tuning() {
    static const auto &value = uqm::combat::shipTuning("melnorme");
    return value;
}
static const uqm::combat::ProjectileStats &cmissileStats() {
    static const auto &value = tuning().projectile("cmissile");
    return value;
}

static const uqm::combat::ProjectileStats &chargeStats() {
    static const auto &value = tuning().projectile("charge");
    return value;
}

// Fixed geometry, frame layout, capacities, and derived calculations.
constexpr auto MelnormeOffset = 24;
constexpr auto LevelCounter = 72;
constexpr auto MaxPump = 4;
constexpr auto MinPumpitudeAnims = 3;
constexpr auto NumPumpAnims = 5;
constexpr auto ReverseDir = (BYTE)(1 << 7);
constexpr auto CmissileOffset = 4;

namespace {
enum class MelnormeWeaponMode { Primary, TestPrimary, Confusion };
void selectMelnormeWeapon(RACE_DESC &, MelnormeWeaponMode) noexcept;
}

static const RACE_DESC &melnormeDefinition() {
    static const RACE_DESC definition = {
        { /* SHIP_INFO */
            FIRES_FORE,
            tuning().ship.meleeCost, /* Super Melee cost */
            tuning().ship.maxCrew, tuning().ship.maxCrew,
            tuning().ship.maxEnergy, tuning().ship.maxEnergy,
            MELNORME_RACE_STRINGS,
            MELNORME_ICON_MASK_PMAP_ANIM,
            MELNORME_MICON_MASK_PMAP_ANIM,
            NULL, NULL, NULL
        },
        { /* FLEET_STUFF */
            INFINITE_RADIUS, /* Initial sphere of influence radius */
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
                MELNORME_BIG_MASK_PMAP_ANIM,
                MELNORME_MED_MASK_PMAP_ANIM,
                MELNORME_SML_MASK_PMAP_ANIM,
            },
            {
                PUMPUP_BIG_MASK_PMAP_ANIM,
                PUMPUP_MED_MASK_PMAP_ANIM,
                PUMPUP_SML_MASK_PMAP_ANIM,
            },
            {
                CONFUSE_BIG_MASK_PMAP_ANIM,
                CONFUSE_MED_MASK_PMAP_ANIM,
                CONFUSE_SML_MASK_PMAP_ANIM,
            },
            {
                MELNORME_CAPTAIN_MASK_PMAP_ANIM,
                NULL, NULL, NULL, NULL, NULL
            },
            MELNORME_VICTORY_SONG,
            MELNORME_SHIP_SOUNDS,
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            { NULL, NULL, NULL },
            NULL, NULL
        },
        {
            0,
            uqm::combat::weaponRangeHint(chargeStats().speed * chargeStats().lifetime),
            NULL,
        },

        0,
        0, /* ship_owner */
    };
    return definition;
}

namespace {
class MelnormeCharge final : public uqm::combat::Projectile {
public:
    using Projectile::Projectile;
    bool hasPostprocess() const noexcept override { return true; }
    void postprocess(ELEMENT &element) override;
    void collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) override;
    void release(ELEMENT &element) noexcept { phase_ = Phase::Flying; element.postprocess_func = nullptr; }
    void update(ELEMENT &element) override {
        if (phase_ != Phase::Flying) return;
        auto *ElementPtr = &element;

	if (--ElementPtr->thrust_wait & 1)
	{
		COUNT frame_index;

		frame_index = GetFrameIndex (ElementPtr->current.image.frame);
		if (((ElementPtr->turn_wait & ReverseDir)
				&& (frame_index % NumPumpAnims) != 0)
				|| (!(ElementPtr->turn_wait & ReverseDir)
				&& ((frame_index + 1) % NumPumpAnims) == 0))
		{
			--frame_index;
			ElementPtr->turn_wait |= ReverseDir;
		}
		else
		{
			++frame_index;
			ElementPtr->turn_wait &= ~ReverseDir;
		}

		ElementPtr->next.image.frame = SetAbsFrameIndex (
				ElementPtr->current.image.frame, frame_index);

		ElementPtr->state_flags |= CHANGING;
	}
    }
private:
    enum class Phase { Charging, Flying };
    Phase phase_ = Phase::Charging;
};
}


static COUNT initialize_pump_up (ELEMENT *ShipPtr, HELEMENT PumpUpArray[]);

void MelnormeCharge::postprocess(ELEMENT &element) {
    auto *ElementPtr = &element;

	if (ElementPtr->state_flags & APPEARING)
	{
		ZeroVelocityComponents (&ElementPtr->velocity);
	}
	else
	{
		HELEMENT hPumpUp;
		ELEMENT *EPtr;
		ELEMENT *ShipPtr;
		STARSHIP *StarShipPtr;

		GetElementStarShip (ElementPtr, &StarShipPtr);
		LockElement (StarShipPtr->hShip, &ShipPtr);
		initialize_pump_up (ShipPtr, &hPumpUp);
		DeltaEnergy (ShipPtr, 0);
		UnlockElement (StarShipPtr->hShip);
        if (!hPumpUp) return; // Pool exhaustion expires the old charge normally.

		LockElement (hPumpUp, &EPtr);

		EPtr->current.image.frame = ElementPtr->current.image.frame;
		EPtr->turn_wait = ElementPtr->turn_wait;
		EPtr->thrust_wait = ElementPtr->thrust_wait;
		if (--EPtr->thrust_wait == 0)
		{
			if ((EPtr->turn_wait & ~ReverseDir) < MaxPump - 1)
			{
				++EPtr->turn_wait;
				EPtr->current.image.frame = SetRelFrameIndex (
						EPtr->current.image.frame, NumPumpAnims);
				ProcessSound (SetAbsSoundIndex (
						StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 2),
						EPtr);
			}
			EPtr->thrust_wait = LevelCounter;
		}

		EPtr->mass_points = EPtr->hit_points =
				(chargeStats().damage << (ElementPtr->turn_wait & ~ReverseDir));
		SetElementStarShip (EPtr, StarShipPtr);

		if (EPtr->thrust_wait & 1)
		{
			COUNT frame_index;

			frame_index = GetFrameIndex (EPtr->current.image.frame);
			if (((EPtr->turn_wait & ReverseDir)
					&& (frame_index % NumPumpAnims) != 0)
					|| (!(EPtr->turn_wait & ReverseDir)
					&& ((frame_index + 1) % NumPumpAnims) == 0))
			{
				--frame_index;
				EPtr->turn_wait |= ReverseDir;
			}
			else
			{
				++frame_index;
				EPtr->turn_wait &= ~ReverseDir;
			}

			EPtr->current.image.frame = SetAbsFrameIndex (
					EPtr->current.image.frame, frame_index);
		}

		if (StarShipPtr->cur_status_flags & StarShipPtr->old_status_flags
				& WEAPON)
		{
			StarShipPtr->weapon_counter = tuning().ship.weaponWait;
		}
		else
		{
			COUNT angle;

			EPtr->life_span = chargeStats().lifetime;
			auto *next = dynamic_cast<MelnormeCharge *>(uqm::detail::borrowProjectile(disp_q, EPtr));
            assert(next);
            next->release(*EPtr);
			EPtr->postprocess_func = 0;

			angle = FACING_TO_ANGLE (StarShipPtr->ShipFacing);
			SetVelocityComponents (&EPtr->velocity,
					COSINE (angle, WORLD_TO_VELOCITY (chargeStats().speed)),
					SINE (angle, WORLD_TO_VELOCITY (chargeStats().speed)));

			ProcessSound (SetAbsSoundIndex (
					StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 3), EPtr);
		}

		UnlockElement (hPumpUp);
		PutElement (hPumpUp);

		SetPrimType (&(GLOBAL (DisplayArray))[ElementPtr->PrimIndex],
				NO_PRIM);
		ElementPtr->state_flags |= NONSOLID;
	}
}


static void
animate (ELEMENT *ElementPtr)
{
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

void MelnormeCharge::collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) {
    auto *ElementPtr0 = &self;
    auto *pPt0 = &point;
    auto *ElementPtr1 = &other;
    auto *pPt1 = &otherPoint;

	RECT r;
	BYTE old_thrust_wait;
	HELEMENT hBlastElement;

	GetFrameRect (ElementPtr0->next.image.frame, &r);

	old_thrust_wait = ElementPtr0->thrust_wait;
	ElementPtr0->blast_offset = r.extent.width >> 1;
	hBlastElement = weapon_collision (ElementPtr0, pPt0, ElementPtr1, pPt1);
	ElementPtr0->thrust_wait = old_thrust_wait;

	if (hBlastElement)
	{
		ELEMENT *BlastElementPtr;

		LockElement (hBlastElement, &BlastElementPtr);

		BlastElementPtr->life_span =
				MinPumpitudeAnims
				+ (ElementPtr0->turn_wait & ~ReverseDir);
		BlastElementPtr->turn_wait = BlastElementPtr->next_turn = 0;
		{
			BlastElementPtr->preprocess_func = animate;
		}

		BlastElementPtr->current.image.farray = ElementPtr0->next.image.farray;
		BlastElementPtr->current.image.frame =
				SetAbsFrameIndex (BlastElementPtr->current.image.farray[0],
				MaxPump * NumPumpAnims);

		UnlockElement (hBlastElement);
	}
}


static COUNT
initialize_pump_up (ELEMENT *ShipPtr, HELEMENT PumpUpArray[])
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
	MissileBlock.pixoffs = MelnormeOffset;
	MissileBlock.speed = DISPLAY_TO_WORLD (MelnormeOffset);
	MissileBlock.hit_points = chargeStats().damage;
	MissileBlock.damage = chargeStats().damage;
	MissileBlock.life = 2;
	MissileBlock.preprocess_func = 0;
	MissileBlock.blast_offs = 0;
	PumpUpArray[0] = uqm::combat::publishMissile<MelnormeCharge>(MissileBlock);

	if (PumpUpArray[0])
	{
		ELEMENT *PumpUpPtr;

		LockElement (PumpUpArray[0], &PumpUpPtr);

		PumpUpPtr->thrust_wait = LevelCounter;
		UnlockElement (PumpUpArray[0]);
	}

	return (1);
}

namespace {
class MelnormeConfusion final : public uqm::combat::Projectile {
public:
    using Projectile::Projectile;
    void collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) override;
    void update(ELEMENT &element) override {
        element.next.image.frame = SetAbsFrameIndex(element.current.image.frame,
            (GetFrameIndex(element.current.image.frame) + 1) & 7);
        element.state_flags |= CHANGING;
    }
};
}
static void
confuse_preprocess (ELEMENT *ElementPtr)
{
	if (!(ElementPtr->state_flags & NONSOLID))
	{
		ElementPtr->next.image.frame = SetAbsFrameIndex (
				ElementPtr->current.image.frame,
				(GetFrameIndex (ElementPtr->current.image.frame) + 1) & 7);
		ElementPtr->state_flags |= CHANGING;
	}
	else if (ElementPtr->hTarget == 0)
	{
		ElementPtr->life_span = 0;
		ElementPtr->state_flags |= DISAPPEARING;
	}
	else
	{
		ELEMENT *eptr;

		LockElement (ElementPtr->hTarget, &eptr);

		ElementPtr->next.location = eptr->next.location;

		if (ElementPtr->turn_wait)
		{
			HELEMENT hEffect;
			STARSHIP *StarShipPtr;

			if (GetFrameIndex (ElementPtr->next.image.frame =
					IncFrameIndex (ElementPtr->current.image.frame)) == 0)
				ElementPtr->next.image.frame =
						SetRelFrameIndex (ElementPtr->next.image.frame, -8);

			GetElementStarShip (eptr, &StarShipPtr);
			StarShipPtr->ship_input_state =
					(StarShipPtr->ship_input_state
					& ~(LEFT | RIGHT | SPECIAL))
					| ElementPtr->turn_wait;

			hEffect = AllocElement ();
			if (hEffect)
			{
				LockElement (hEffect, &eptr);
				eptr->playerNr = ElementPtr->playerNr;
				eptr->state_flags = FINITE_LIFE | NONSOLID | CHANGING;
				eptr->life_span = 1;
				eptr->current = eptr->next = ElementPtr->next;
				eptr->preprocess_func = confuse_preprocess;
				SetPrimType (&(GLOBAL (DisplayArray))[eptr->PrimIndex],
						STAMP_PRIM);

				GetElementStarShip (ElementPtr, &StarShipPtr);
				SetElementStarShip (eptr, StarShipPtr);
				eptr->hTarget = ElementPtr->hTarget;

				UnlockElement (hEffect);
				PutElement (hEffect);
			}
		}

		UnlockElement (ElementPtr->hTarget);
	}
}

void MelnormeConfusion::collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) {
    auto *ElementPtr0 = &self;
    auto *pPt0 = &point;
    auto *ElementPtr1 = &other;
    auto *pPt1 = &otherPoint;

	if (ElementPtr1->state_flags & PLAYER_SHIP)
	{
		HELEMENT hConfusionElement, hNextElement;
		ELEMENT *ConfusionPtr;
		STARSHIP *StarShipPtr;

		GetElementStarShip (ElementPtr0, &StarShipPtr);
		for (hConfusionElement = GetHeadElement ();
				hConfusionElement; hConfusionElement = hNextElement)
		{
			LockElement (hConfusionElement, &ConfusionPtr);
			if (elementsOfSamePlayer (ConfusionPtr, ElementPtr0)
					&& ConfusionPtr->current.image.farray ==
					StarShipPtr->RaceDescPtr->ship_data.special
					&& (ConfusionPtr->state_flags & NONSOLID))
			{
				UnlockElement (hConfusionElement);
				break;
			}
			hNextElement = GetSuccElement (ConfusionPtr);
			UnlockElement (hConfusionElement);
		}

		if (hConfusionElement || (hConfusionElement = AllocElement ()))
		{
			LockElement (hConfusionElement, &ConfusionPtr);

			if (ConfusionPtr->state_flags == 0) /* not allocated before */
			{
				InsertElement (hConfusionElement, GetHeadElement ());

				ConfusionPtr->current = ElementPtr0->next;
				ConfusionPtr->current.image.frame = SetAbsFrameIndex (
						ConfusionPtr->current.image.frame, 8
						);
				ConfusionPtr->next = ConfusionPtr->current;
				ConfusionPtr->playerNr = ElementPtr0->playerNr;
				ConfusionPtr->state_flags = FINITE_LIFE | NONSOLID | CHANGING;
				ConfusionPtr->preprocess_func = confuse_preprocess;
				SetPrimType (
						&(GLOBAL (DisplayArray))[ConfusionPtr->PrimIndex],
						NO_PRIM
						);

				SetElementStarShip (ConfusionPtr, StarShipPtr);
				GetElementStarShip (ElementPtr1, &StarShipPtr);
				ConfusionPtr->hTarget = StarShipPtr->hShip;
			}

			ConfusionPtr->life_span = 400;
			ConfusionPtr->turn_wait =
					(BYTE)(1 << ((BYTE)TFB_Random () & 1)); /* LEFT or RIGHT */

			UnlockElement (hConfusionElement);
		}

		ElementPtr0->hit_points = 0;
		ElementPtr0->life_span = 0;
		ElementPtr0->state_flags |= DISAPPEARING | COLLISION | NONSOLID;
	}
	(void) pPt0;  /* Satisfying compiler (unused parameter) */
	(void) pPt1;  /* Satisfying compiler (unused parameter) */
}


static COUNT
initialize_confusion (ELEMENT *ShipPtr, HELEMENT ConfusionArray[])
{
	STARSHIP *StarShipPtr;
	MISSILE_BLOCK ConfusionBlock;

	GetElementStarShip (ShipPtr, &StarShipPtr);
	ConfusionBlock.cx = ShipPtr->next.location.x;
	ConfusionBlock.cy = ShipPtr->next.location.y;
	ConfusionBlock.farray = StarShipPtr->RaceDescPtr->ship_data.special;
	ConfusionBlock.index = 0;
	ConfusionBlock.face = StarShipPtr->ShipFacing;
	ConfusionBlock.sender = ShipPtr->playerNr;
	ConfusionBlock.flags = IGNORE_SIMILAR;
	ConfusionBlock.pixoffs = MelnormeOffset;
	ConfusionBlock.speed = cmissileStats().speed;
	ConfusionBlock.hit_points = cmissileStats().hitPoints;
	ConfusionBlock.damage = cmissileStats().damage;
	ConfusionBlock.life = cmissileStats().lifetime;
	ConfusionBlock.preprocess_func = nullptr;
	ConfusionBlock.blast_offs = CmissileOffset;
	ConfusionArray[0] = uqm::combat::publishMissile<MelnormeConfusion>(ConfusionBlock);

	if (ConfusionArray[0])
	{
		ELEMENT *CMissilePtr;

		LockElement (ConfusionArray[0], &CMissilePtr);

		SetElementStarShip (CMissilePtr, StarShipPtr);
		UnlockElement (ConfusionArray[0]);
	}
	return (1);
}

static COUNT
initialize_test_pump_up (ELEMENT *ShipPtr, HELEMENT PumpUpArray[])
{
	STARSHIP *StarShipPtr;
	MISSILE_BLOCK MissileBlock;
	//ELEMENT *PumpUpPtr;

	GetElementStarShip (ShipPtr, &StarShipPtr);
	MissileBlock.cx = ShipPtr->next.location.x;
	MissileBlock.cy = ShipPtr->next.location.y;
	MissileBlock.farray = StarShipPtr->RaceDescPtr->ship_data.weapon;
	MissileBlock.face = StarShipPtr->ShipFacing;
	MissileBlock.index = 0;
	MissileBlock.sender = ShipPtr->playerNr;
	MissileBlock.flags = IGNORE_SIMILAR;
	MissileBlock.pixoffs = MelnormeOffset;
	MissileBlock.speed = chargeStats().speed;
	MissileBlock.hit_points = chargeStats().damage;
	MissileBlock.damage = chargeStats().damage;
	MissileBlock.life = chargeStats().lifetime;
	MissileBlock.preprocess_func = 0;
	MissileBlock.blast_offs = 0;
	PumpUpArray[0] = uqm::combat::publishMissile(MissileBlock, chargeStats().flight);

	return (1);
}

static void
melnorme_intelligence (ELEMENT *ShipPtr, EVALUATE_DESC *ObjectsOfConcern,
		COUNT ConcernCounter)
{
	BYTE old_count;
	STARSHIP *StarShipPtr;
	EVALUATE_DESC *lpEvalDesc;

	GetElementStarShip (ShipPtr, &StarShipPtr);

	selectMelnormeWeapon(*StarShipPtr->RaceDescPtr, MelnormeWeaponMode::TestPrimary);
	old_count = StarShipPtr->weapon_counter;

	if (StarShipPtr->weapon_counter == tuning().ship.weaponWait)
		StarShipPtr->weapon_counter = 0;

	lpEvalDesc = &ObjectsOfConcern[ENEMY_SHIP_INDEX];
	if (lpEvalDesc->ObjectPtr)
	{
		if (StarShipPtr->RaceDescPtr->ship_info.energy_level < tuning().ship.specialEnergyCost
				+ tuning().ship.weaponEnergyCost
				&& !(StarShipPtr->old_status_flags & WEAPON))
			lpEvalDesc->MoveState = ENTICE;
		else
		{
			STARSHIP *EnemyStarShipPtr;

			GetElementStarShip (lpEvalDesc->ObjectPtr, &EnemyStarShipPtr);
			if (!(EnemyStarShipPtr->RaceDescPtr->ship_info.ship_flags
					& IMMEDIATE_WEAPON))
				lpEvalDesc->MoveState = PURSUE;
		}
	}
	ship_intelligence (ShipPtr, ObjectsOfConcern, ConcernCounter);

	if (StarShipPtr->weapon_counter == 0
			&& (old_count != 0
			|| ((StarShipPtr->special_counter
			|| StarShipPtr->RaceDescPtr->ship_info.energy_level >= tuning().ship.specialEnergyCost
			+ tuning().ship.weaponEnergyCost)
			&& !(StarShipPtr->ship_input_state & WEAPON))))
		StarShipPtr->ship_input_state ^= WEAPON;

	StarShipPtr->ship_input_state &= ~SPECIAL;
	if (StarShipPtr->special_counter == 0
			&& StarShipPtr->RaceDescPtr->ship_info.energy_level >= tuning().ship.specialEnergyCost)
	{
		BYTE old_input_state;

		old_input_state = StarShipPtr->ship_input_state;

		selectMelnormeWeapon(*StarShipPtr->RaceDescPtr, MelnormeWeaponMode::Confusion);

		++ShipPtr->turn_wait;
		++ShipPtr->thrust_wait;
		ship_intelligence (ShipPtr, ObjectsOfConcern, ENEMY_SHIP_INDEX + 1);
		--ShipPtr->thrust_wait;
		--ShipPtr->turn_wait;

		if (StarShipPtr->ship_input_state & WEAPON)
		{
			StarShipPtr->ship_input_state &= ~WEAPON;
			StarShipPtr->ship_input_state |= SPECIAL;
		}

		StarShipPtr->ship_input_state = (unsigned char)(old_input_state
				| (StarShipPtr->ship_input_state & SPECIAL));
	}

	StarShipPtr->weapon_counter = old_count;

	selectMelnormeWeapon(*StarShipPtr->RaceDescPtr, MelnormeWeaponMode::Primary);
}

static void
melnorme_postprocess (ELEMENT *ElementPtr)
{
	STARSHIP *StarShipPtr;

	GetElementStarShip (ElementPtr, &StarShipPtr);
	if ((StarShipPtr->cur_status_flags & SPECIAL)
			&& StarShipPtr->special_counter == 0
			&& DeltaEnergy (ElementPtr, -tuning().ship.specialEnergyCost))
	{
		HELEMENT Confusion;

		initialize_confusion (ElementPtr, &Confusion);
		if (Confusion)
		{
			ELEMENT *CMissilePtr;
			LockElement (Confusion, &CMissilePtr);
			
			ProcessSound (SetAbsSoundIndex (
					StarShipPtr->RaceDescPtr->ship_data.ship_sounds, 1), CMissilePtr);
			
			UnlockElement (Confusion);
			PutElement (Confusion);
			StarShipPtr->special_counter =
					StarShipPtr->RaceDescPtr->characteristics.special_wait;
		}
	}
}

namespace {
class MelnormeShip final : public uqm::combat::Ship {
public:
    explicit MelnormeShip(const RACE_DESC &definition) noexcept
        : Ship(definition) {
        legacyData().cyborg_control.intelligence_func = melnorme_intelligence;
    }
    void setWeaponMode(MelnormeWeaponMode mode) noexcept { mode_ = mode; }
private:
    void afterMotion(ELEMENT &element) noexcept override {
        melnorme_postprocess(&element);
    }
    COUNT createWeapons(ELEMENT &element, uqm::combat::WeaponSlots &weapons) noexcept override {
        switch (mode_) {
            case MelnormeWeaponMode::TestPrimary: return initialize_test_pump_up(&element, weapons.data());
            case MelnormeWeaponMode::Confusion: return initialize_confusion(&element, weapons.data());
            case MelnormeWeaponMode::Primary: return initialize_pump_up(&element, weapons.data());
        }
        return 0;
    }
    MelnormeWeaponMode mode_ = MelnormeWeaponMode::Primary;
};
void selectMelnormeWeapon(RACE_DESC &descriptor, MelnormeWeaponMode mode) noexcept {
    auto &ship = static_cast<MelnormeShip &>(uqm::combat::shipFor(descriptor));
    ship.setWeaponMode(mode);
}
} // namespace

std::unique_ptr<uqm::combat::Ship>
uqm::combat::makeMelnormeShip()
{

	return std::make_unique<MelnormeShip>(melnormeDefinition());
}

