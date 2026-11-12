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

#include "hyper_navigation.hpp"
#include "device_menu.hpp"
#include "game_state.hpp"
#include "game_assets.hpp"
#include "game_collections.hpp"
#include "navigation_entities.hpp"
#include <utility>
#include "battle.h"
#include "battle_driver.hpp"
#include "clock.h"
#include "gameev.h"
#include <stdexcept>
#include "hyper.h"

#include "build.h"
#include "collide.h"
#include "colors.h"
#include "controls.h"
#include "gameopt.h"
#include "menustat.h"
		// for DrawMenuStateStrings()
#include "encount.h"
#include "starmap.h"
#include "ship.h"
#include "shipcont.h"
#include "process.h"
#include "globdata.h"
#include "sis.h"
#include "units.h"
#include "init.h"
#include "nameref.h"
#include "resinst.h"
#include "setup.h"
#include "sounds.h"
#include "options.h"
#include "libs/graphics/gfx_common.h"
#include "libs/mathlib.h"


namespace {
using uqm::detail::EntityHooks;
using uqm::detail::NavigationFlagship;
using uqm::detail::NavigationStar;
using uqm::detail::NavigationPortal;
using uqm::detail::NavigationEncounter;
NavigationFlagship &flagship(ELEMENT &element) {
    auto *owner = dynamic_cast<NavigationFlagship *>(uqm::detail::borrowEntity(disp_q, &element));
    if (!owner) throw std::logic_error("navigation operation requires its live flagship entity");
    return *owner;
}
}
uqm::SpaceNavigation &uqm::navigationFor(ELEMENT &element) { return flagship(element).navigation; }

#define XOFFS ((RADAR_SCAN_WIDTH + (UNIT_SCREEN_WIDTH << 2)) >> 1)
#define YOFFS ((RADAR_SCAN_HEIGHT + (UNIT_SCREEN_HEIGHT << 2)) >> 1)

static HELEMENT getSisElement(void);
namespace uqm {
// One run's asset lease. Elements borrow the stable address of stars until the
// scene has reset its engine. The cache outlives this scope; playback state does
// not live in the shared graphics. No other realm's artwork is loaded here.
class NavigationPresentation {
public:
    explicit NavigationPresentation(AssetCache &cache) noexcept : cache_(cache) {}
    ~NavigationPresentation() {
        unbind();
        if (colors) DestroyColorMap(ReleaseColorMap(colors));
    }
    NavigationPresentation(const NavigationPresentation &) = delete;
    NavigationPresentation &operator=(const NavigationPresentation &) = delete;
    void load(bool quasiSpace) {
        if (ambient_) return;
        ambient_ = cache_.find({AssetKind::Graphic, AMBIENT_MASK_PMAP_ANIM});
        realm_ = cache_.find({AssetKind::Graphic,
                quasiSpace ? ARISPACE_MASK_PMAP_ANIM : HYPERSTARS_MASK_PMAP_ANIM});
        stars = dynamic_cast<const GraphicAsset &>(*realm_).frame();
        colors = CaptureColorMap(LoadColorMap(quasiSpace ? ARISPACE_COLOR_TAB : HYPER_COLOR_TAB));
        if (!colors) throw std::runtime_error("could not load navigation color map");
    }
    void bind() {
        if (bound_) return;
        previousStars_ = stars_in_space;
        stars_in_space = dynamic_cast<const GraphicAsset &>(*ambient_).frame();
        bound_ = true;
    }
    void unbind() noexcept {
        if (!bound_) return;
        stars_in_space = previousStars_;
        bound_ = false;
    }
    FRAME stars = nullptr; // Borrowed by ELEMENT::image.farray; stable until teardown.
    COLORMAP colors = nullptr;
private:
    AssetCache &cache_;
    AssetCache::Pointer ambient_, realm_;
    FRAME previousStars_ = nullptr;
    bool bound_ = false;
};
} // namespace uqm

// HyperspaceMenu() items
enum HyperMenuItems
{
	// XXX: Must match the enum in menustat.h
	STARMAP = 1,
	EQUIP_DEVICE,
	CARGO,
	ROSTER,
	GAME_MENU,
	NAVIGATION,
};


void
uqm::SpaceNavigation::MoveSIS(SIZE *pdx, SIZE *pdy)
{
	SIZE new_dx, new_dy;
    auto &position = travel().position;

	new_dx = *pdx;
	position.x -= new_dx;
	if (position.x < 0)
	{
		new_dx += (SIZE)position.x;
		position.x = 0;
	}
	else if (position.x > MAX_X_LOGICAL)
	{
		new_dx += (SIZE)(position.x - MAX_X_LOGICAL);
		position.x = MAX_X_LOGICAL;
	}

	new_dy = *pdy;
	position.y -= new_dy;
	if (position.y < 0)
	{
		new_dy += (SIZE)position.y;
		position.y = 0;
	}
	else if (position.y > MAX_Y_LOGICAL)
	{
		new_dy += (SIZE)(position.y - MAX_Y_LOGICAL);
		position.y = MAX_Y_LOGICAL;
	}

	if (new_dx != *pdx || new_dy != *pdy)
	{
		HELEMENT hElement, hNextElement;

		*pdx = new_dx;
		*pdy = new_dy;

		for (hElement = GetTailElement ();
				hElement != 0; hElement = hNextElement)
		{
			ELEMENT *ElementPtr;

			LockElement (hElement, &ElementPtr);

			if (!(ElementPtr->state_flags & PLAYER_SHIP))
				hNextElement = GetPredElement (ElementPtr);
			else
			{
				ElementPtr->next.location.x = (LOG_SPACE_WIDTH >> 1) - new_dx;
				ElementPtr->next.location.y = (LOG_SPACE_HEIGHT >> 1) - new_dy;
				hNextElement = 0;
			}

			UnlockElement (hElement);
		}
	}

	if (player_.fuel() && !isQuasiSpace())
	{
		COUNT cur_fuel_ticks;
		COUNT hyper_dist;
		DWORD adj_dx, adj_dy;

		if (new_dx < 0)
			new_dx = -new_dx;
		hyper_dx += new_dx;
		if (new_dy < 0)
			new_dy = -new_dy;
		hyper_dy += new_dy;

		/* These macros are also used in the fuel estimate on the starmap. */
		adj_dx = LOGX_TO_UNIVERSE(16 * hyper_dx);
		adj_dy = MAX_Y_UNIVERSE - LOGY_TO_UNIVERSE(16 * hyper_dy);

		hyper_dist = square_root (adj_dx * adj_dx + adj_dy * adj_dy) 
					+ hyper_extra;
		cur_fuel_ticks = hyper_dist >> 4;

		if (cur_fuel_ticks > (COUNT)fuel_ticks)
		{
#ifndef TESTING
			DeltaSISGauges (0, fuel_ticks - cur_fuel_ticks, 0);
#endif /* TESTING */
			if (cur_fuel_ticks > 0x00FF)
			{
				hyper_dx = 0;
				hyper_extra = hyper_dist & ((1 << 4) - 1);
				hyper_dy = 0;
				cur_fuel_ticks = 0;
			}

			fuel_ticks = (BYTE)cur_fuel_ticks;
		}
	}
}

void
uqm::SpaceNavigation::check_hyperspace_encounter(void)
{
	BYTE nextType = 0;
	POINT universe;
	
	universe.x = LOGX_TO_UNIVERSE (travel().position.x);
	universe.y = LOGY_TO_UNIVERSE (travel().position.y);
    for (const auto *FleetPtr : session_.raceFleets()) {
        if (!(legacy_.activity & IN_BATTLE)) break;
        const BYTE Type = nextType++;
        COUNT encounter_radius;
		encounter_radius = FleetPtr->actual_strength;
		if (encounter_radius)
		{
			BYTE encounter_flags;
			SIZE dx, dy;
			COUNT percent;

			encounter_flags = 0;
			percent = session_.navigationRules().spawnPercent.at(Type);
			
			if (encounter_radius != INFINITE_RADIUS)
			{
				encounter_radius =
						(encounter_radius * SPHERE_RADIUS_INCREMENT) >> 1;
			}
			else /* encounter_radius == infinity */
			{

				encounter_radius = (MAX_X_UNIVERSE + 1) << 1;
				if (Type == SLYLANDRO_SHIP)
				{
					encounter_flags = ONE_SHOT_ENCOUNTER;
					if (!story_.read(uqm::StoryFlag::StarbaseAccessGranted))
						percent = 100;
					else
						percent *= story_.read(uqm::StoryCounter::SlylandroMultiplier);
				}
				else if (Type == MELNORME_SHIP
						&& (player_.fuel() == 0
						|| story_.read(uqm::StoryFlag::UsedBroadcaster))
						&& story_.read(uqm::StoryCounter::MelnormeAnger) < 3)
				{
					if (!story_.read(uqm::StoryFlag::UsedBroadcaster))
						percent = 30;
					else
						percent = 100;
					encounter_flags = ONE_SHOT_ENCOUNTER;
				}

				// There can be only one! (of either Slylandro or Melnorme)
                for (const auto &record : records_) {
                    if (record.race_id == Type) { percent = 0; break; }
                }


				if (percent == 100 && Type == MELNORME_SHIP)
				{
					story_.set(uqm::StoryFlag::BroadcasterResponse, 1);
				}
			}

			dx = universe.x - FleetPtr->loc.x;
			if (dx < 0)
				dx = -dx;
			dy = universe.y - FleetPtr->loc.y;
			if (dy < 0)
				dy = -dy;
			if ((COUNT)dx < encounter_radius
					&& (COUNT)dy < encounter_radius
					&& (DWORD)dx * dx + (DWORD)dy * dy <
					(DWORD)encounter_radius * encounter_radius
					&& ((COUNT)engine_.random()() % 100) < percent)
			{
                // Capacity is a gameplay limit, not an allocation request.
                if (auto entry = records_.insert(); entry != records_.end()) {
                    entry->origin = FleetPtr->loc;
                    entry->radius = encounter_radius;
                    entry->flags = encounter_flags;
                    entry->race_id = Type;
                }
			}
		}

	}

	story_.set(uqm::StoryFlag::UsedBroadcaster, 0);
}

BOOLEAN
uqm::SpaceNavigation::LoadHyperspace(void)
{
	hyper_dx = 0;
	hyper_dy = 0;
	hyper_extra = 0;
	fuel_ticks = 1;

	legacy_.shipStamp.origin.x = -MAX_X_UNIVERSE;
	legacy_.shipStamp.origin.y = -MAX_Y_UNIVERSE;

    presentation_->load(isQuasiSpace());
    presentation_->bind();

	if (!(LastActivity & CHECK_LOAD))
		RepairSISBorder ();
	else
	{
		if (LOBYTE (LastActivity) == 0)
		{
			DrawSISFrame ();
		}
		else
		{
			ClearSISRect (DRAW_SIS_DISPLAY);
			RepairSISBorder ();
		}
	}
	if (!(legacy_.autopilot.x != ~0 && legacy_.autopilot.y != ~0))
	{
		DrawSISMessage (NULL);
	}

	SetContext (RadarContext);
	SetContextBackGroundColor (
			BUILD_COLOR (MAKE_RGB15 (0x00, 0x0E, 0x00), 0x6C));

	SetContext (SpaceContext);
	if (!isQuasiSpace())
	{
		SetContextBackGroundColor (
				BUILD_COLOR (MAKE_RGB15 (0x07, 0x00, 0x00), 0x2F));
		SetColorMap (GetColorMapAddress (presentation_->colors));
	}
	else
	{
		SetContextBackGroundColor (
				BUILD_COLOR (MAKE_RGB15 (0x00, 0x1A, 0x00), 0x2F));
		SetColorMap (GetColorMapAddress (presentation_->colors));
		story_.set(uqm::StoryFlag::UsedBroadcaster, 0);
		story_.set(uqm::StoryFlag::BroadcasterResponse, 0);
	}
//    ClearDrawable ();

	ClearSISRect (CLEAR_SIS_RADAR);

	return TRUE;
}

BOOLEAN
uqm::SpaceNavigation::FreeHyperspace(void)
{
    presentation_->unbind();

	return TRUE;
}

void
uqm::SpaceNavigation::ElementToUniverse(ELEMENT *ElementPtr, POINT *pPt)
{
	SDWORD log_x, log_y;

	log_x = travel().position.x
			+ (ElementPtr->next.location.x - (LOG_SPACE_WIDTH >> 1));
	log_y = travel().position.y
			+ (ElementPtr->next.location.y - (LOG_SPACE_HEIGHT >> 1));
	pPt->x = LOGX_TO_UNIVERSE (log_x);
	pPt->y = LOGY_TO_UNIVERSE (log_y);
}

void
uqm::SpaceNavigation::cleanup_hyperspace(void)
{
    for (auto entry = records_.begin(); entry != records_.end(); ++entry) {
        if (auto element = encounters_.find(*entry, engine_)) {
            if (element->get().hTarget) records_.select(entry);
        }
    }
    encounters_.clear();
}



void
uqm::SpaceNavigation::InterplanetaryTransition(ELEMENT *ElementPtr)
{
	travel().flight.planet = 0;
	travel().flight.loadOrbit = 0;
	travel().flight.facing = 0; /* Not reentering the system */
	story_.set(uqm::StoryFlag::UsedBroadcaster, 0);
	if (!isQuasiSpace())
	{
		// Enter a solar system from HyperSpace.
		transition_ = NavigationTransition{NavigationDestination::SolarSystem, selectedStar().value()};
		story_.set(uqm::StoryCounter::EscapeCounter, 0);
	}
	else
	{
		POINT pt;

		legacy_.autopilot.x = ~0;
		legacy_.autopilot.y = ~0;

		ElementToUniverse (ElementPtr, &pt);
        const auto id = starsNear(pt, 5, 5).at(0);
        selectedStar() = id;
        const auto &position = catalogue().at(id).position();
		if (position.x == ARILOU_HOME_X && position.y == ARILOU_HOME_Y)
		{
			// Meet the Arilou.
			transition_ = NavigationTransition{NavigationDestination::Encounter, selectedStar()};
		}
		else
		{
			// Transition from QuasiSpace to HyperSpace through
			// one of the permanent portals.
			COUNT index;
			const POINT portal_pt[] = QUASISPACE_PORTALS_HYPERSPACE_ENDPOINTS;

			index = id.value - QUASISPACE_FIRST;
			prepareRealmTransition(NavigationRealm::HyperSpace,
                    {UNIVERSE_TO_LOGX (portal_pt[index].x), UNIVERSE_TO_LOGY (portal_pt[index].y)});
		}
	}
    // The successor mode converts the location variant after this scene unwinds.
}

/* Enter QuasiSpace from HyperSpace by any portal, or HyperSpace from
 * QuasiSpace through the periodically opening portal.
 */
void
uqm::SpaceNavigation::ArilouSpaceTransition(void)
{
	travel().flight.facing = 0; /* Not reentering the system */
	story_.set(uqm::StoryFlag::UsedBroadcaster, 0);
	legacy_.autopilot.x = ~0;
	legacy_.autopilot.y = ~0;
	if (!isQuasiSpace())
	{
		// From HyperSpace to QuasiSpace.
        prepareRealmTransition(NavigationRealm::QuasiSpace,
                {UNIVERSE_TO_LOGX (QUASI_SPACE_X), UNIVERSE_TO_LOGY (QUASI_SPACE_Y)});
        // A player-induced portal is consumed on entry; the periodic one is not.
        if (story_.read(uqm::StoryCounter::PortalCounter) != 0)
            story_.set(uqm::StoryCounter::PortalCounter, 0);
	}
	else
	{
		// From QuasiSpace to HyperSpace through the periodically appearing
		// portal.
		prepareRealmTransition(NavigationRealm::HyperSpace,
                {UNIVERSE_TO_LOGX (ARILOU_SPACE_X), UNIVERSE_TO_LOGY (ARILOU_SPACE_Y)});
	}
    // Keep the outgoing location valid until the dispatcher runs the successor.
}

void
uqm::SpaceNavigation::unhyper_transition(ELEMENT *ElementPtr)
{
	COUNT frame_index;

	ElementPtr->state_flags |= CHANGING;

	frame_index = GetFrameIndex (ElementPtr->current.image.frame);
	if (frame_index == 0)
		frame_index += ANGLE_TO_FACING (FULL_CIRCLE);
	else if (frame_index < ANGLE_TO_FACING (FULL_CIRCLE))
		frame_index = NORMALIZE_FACING (frame_index + 1);
	else if (++frame_index == GetFrameCount (ElementPtr->current.image.frame))
	{
		cleanup_hyperspace ();

		// C frame-loop stop signal only; destination authority is transition_.
		legacy_.activity &= ~IN_BATTLE;
		switch ((TransitionType) ElementPtr->turn_wait)
		{
			case RANDOM_ENCOUNTER_TRANSITION: {
                STARSHIP *ship;
                GetElementStarShip(ElementPtr, &ship);
                saveFacing(ship->ShipFacing + 1); // Existing saved-facing encoding.
                transition_ = NavigationTransition{NavigationDestination::Encounter, std::nullopt};
                break;
            }
			case INTERPLANETARY_TRANSITION:
				InterplanetaryTransition (ElementPtr);
				break;
			case ARILOU_SPACE_TRANSITION:
				ArilouSpaceTransition ();
				break;
		}

		ZeroVelocityComponents (&ElementPtr->velocity);
		SetPrimType (&DisplayArray[ElementPtr->PrimIndex], NO_PRIM);
		return;
	}
	ElementPtr->next.image.frame =
			SetAbsFrameIndex (ElementPtr->current.image.frame, frame_index);
}

void
uqm::SpaceNavigation::init_transition(ELEMENT *ElementPtr0, ELEMENT *ElementPtr1,
		TransitionType which_transition)
{
    if (transition_) return;
	SIZE dx, dy;
	SIZE num_turns;
	STARSHIP *StarShipPtr;

	dx = WORLD_TO_VELOCITY (ElementPtr0->next.location.x
			- ElementPtr1->next.location.x);
	dy = WORLD_TO_VELOCITY (ElementPtr0->next.location.y
			- ElementPtr1->next.location.y);

	ElementPtr1->state_flags |= NONSOLID;
	flagship(*ElementPtr1).phase(*ElementPtr1, NavigationFlagship::Phase::Departing);
	ElementPtr1->turn_wait = (BYTE) which_transition;

	GetElementStarShip (ElementPtr1, &StarShipPtr);
	num_turns = GetFrameCount (ElementPtr1->next.image.frame)
			- ANGLE_TO_FACING (FULL_CIRCLE)
			+ NORMALIZE_FACING (ANGLE_TO_FACING (FULL_CIRCLE)
			- StarShipPtr->ShipFacing);
	if (num_turns == 0)
		num_turns = 1;

	SetVelocityComponents (&ElementPtr1->velocity,
			dx / num_turns, dy / num_turns);
}

BOOLEAN
uqm::SpaceNavigation::hyper_transition(ELEMENT *ElementPtr)
{
	if (ElementPtr->state_flags & APPEARING)
	{
		if (LastActivity & CHECK_LOAD)
		{
			LastActivity &= ~CHECK_LOAD;

			ElementPtr->current = ElementPtr->next;
			SetUpElement (ElementPtr);

			ElementPtr->state_flags |= DEFY_PHYSICS;

			return FALSE;
		}
		else
		{
			flagship(*ElementPtr).phase(*ElementPtr, NavigationFlagship::Phase::Arriving);
			ElementPtr->state_flags |= NONSOLID;
			ElementPtr->next.image.frame =
					SetAbsFrameIndex (ElementPtr->current.image.frame,
					GetFrameCount (ElementPtr->current.image.frame) - 1);
		}
	}
	else
	{
		COUNT frame_index;

		frame_index = GetFrameIndex (ElementPtr->current.image.frame);
		if (frame_index-- <= ANGLE_TO_FACING (FULL_CIRCLE))
		{
			STARSHIP *StarShipPtr;

			if (frame_index == ANGLE_TO_FACING (FULL_CIRCLE) - 1)
				frame_index = 0;
			else
				frame_index = NORMALIZE_FACING (frame_index);

			GetElementStarShip (ElementPtr, &StarShipPtr);
			if (frame_index == StarShipPtr->ShipFacing)
			{
				flagship(*ElementPtr).phase(*ElementPtr, NavigationFlagship::Phase::Flying);
				ElementPtr->state_flags &= ~NONSOLID;
			}
		}

		ElementPtr->state_flags |= CHANGING;
		ElementPtr->next.image.frame =
				SetAbsFrameIndex (ElementPtr->current.image.frame,
				frame_index);

		if (!(ElementPtr->state_flags & NONSOLID))
		{
			ElementPtr->current = ElementPtr->next;
			SetUpElement (ElementPtr);

			ElementPtr->state_flags |= DEFY_PHYSICS;
		}
	}

	return TRUE;
}

void
uqm::SpaceNavigation::hyper_collision(ELEMENT *ElementPtr0, POINT *pPt0,
		ELEMENT *ElementPtr1, POINT *pPt1)
{
    if (transition_) return;
	if ((ElementPtr1->state_flags & PLAYER_SHIP)
			&& story_.read(uqm::StoryCounter::PortalCounter) == 0)
	{
		SIZE dx, dy;
		POINT pt;
		STARSHIP *StarShipPtr;

		ElementToUniverse (ElementPtr0, &pt);

		const auto id = starsNear(pt, 5, 5).at(0);
        const auto &position = catalogue().at(id).position();

		GetElementStarShip (ElementPtr1, &StarShipPtr);
		GetCurrentVelocityComponents (&ElementPtr1->velocity, &dx, &dy);
		if (selectedStar() == id
				|| (ElementPtr1->state_flags & APPEARING)
				|| !(dx || dy || (StarShipPtr->cur_status_flags
				& (LEFT | RIGHT | THRUST | WEAPON | SPECIAL))))
		{
			selectedStar() = id;
			ElementPtr0->state_flags |= DEFY_PHYSICS | COLLISION;
		}
		else if ((legacy_.activity & IN_BATTLE)
				&& (legacy_.autopilot.x == ~0
				|| legacy_.autopilot.y == ~0
				|| (legacy_.autopilot.x == position.x
				&& legacy_.autopilot.y == position.y)))
		{
			selectedStar() = id;
			ElementPtr0->state_flags |= COLLISION;

			init_transition (ElementPtr0, ElementPtr1,
					INTERPLANETARY_TRANSITION);
		}
	}
	(void) pPt0;  /* Satisfying compiler (unused parameter) */
	(void) pPt1;  /* Satisfying compiler (unused parameter) */
}

void
uqm::SpaceNavigation::hyper_death(ELEMENT *ElementPtr)
{
	if (!(ElementPtr->state_flags & DEFY_PHYSICS)
			&& (legacy_.activity & IN_BATTLE))
		selectedStar().reset();
}

void
uqm::SpaceNavigation::arilou_space_death(ELEMENT *ElementPtr)
{
    settlePortalSide((ElementPtr->state_flags & DEFY_PHYSICS) != 0);
}

void
uqm::SpaceNavigation::arilou_space_collision(ELEMENT *ElementPtr0,
		POINT *pPt0, ELEMENT *ElementPtr1, POINT *pPt1)
{
    if (transition_) return;
	COUNT which_side;

	if (!(ElementPtr1->state_flags & PLAYER_SHIP))
		return;

	which_side = story_.read(uqm::StoryCounter::ArilouSpaceSide);
	if (which_side == 0 || which_side == 3)
	{
		if (ElementPtr1->state_flags & DEFY_PHYSICS)
		{
			story_.set(uqm::StoryCounter::ArilouSpaceSide, which_side ^ 1);
		}
		else
		{
			init_transition (ElementPtr0, ElementPtr1,
					ARILOU_SPACE_TRANSITION);
		}
	}

	ElementPtr0->state_flags |= DEFY_PHYSICS | COLLISION;
	(void) pPt0;  /* Satisfying compiler (unused parameter) */
	(void) pPt1;  /* Satisfying compiler (unused parameter) */
}

HELEMENT
uqm::SpaceNavigation::AllocHyperElement(const POINT *elem_pt)
{
	HELEMENT hHyperSpaceElement;

	hHyperSpaceElement = AllocElement ();
	if (hHyperSpaceElement)
	{
		ELEMENT *HyperSpaceElementPtr;

		LockElement (hHyperSpaceElement, &HyperSpaceElementPtr);
		HyperSpaceElementPtr->playerNr = NEUTRAL_PLAYER_NUM;
		HyperSpaceElementPtr->state_flags = CHANGING | FINITE_LIFE;
		HyperSpaceElementPtr->life_span = 1;
		HyperSpaceElementPtr->mass_points = 1;

		{
			long lx, ly;

			lx = UNIVERSE_TO_LOGX (elem_pt->x)
					+ (LOG_SPACE_WIDTH >> 1) - travel().position.x;
			HyperSpaceElementPtr->current.location.x = WRAP_X (lx);

			ly = UNIVERSE_TO_LOGY (elem_pt->y)
					+ (LOG_SPACE_HEIGHT >> 1) - travel().position.y;
			HyperSpaceElementPtr->current.location.y = WRAP_Y (ly);
		}

		SetPrimType (&DisplayArray[HyperSpaceElementPtr->PrimIndex],
				STAMP_PRIM);
		HyperSpaceElementPtr->current.image.farray =
				&presentation_->stars;

		UnlockElement (hHyperSpaceElement);
	}

	return hHyperSpaceElement;
}

void
uqm::SpaceNavigation::AddAmbientElement(void)
{
	HELEMENT hHyperSpaceElement;

	hHyperSpaceElement = AllocElement ();
	if (hHyperSpaceElement)
	{
		SIZE dx, dy;
		DWORD rand_val;
		ELEMENT *HyperSpaceElementPtr;

		LockElement (hHyperSpaceElement, &HyperSpaceElementPtr);
		HyperSpaceElementPtr->playerNr = NEUTRAL_PLAYER_NUM;
		HyperSpaceElementPtr->state_flags =
				APPEARING | FINITE_LIFE | NONSOLID;
		SetPrimType (&DisplayArray[HyperSpaceElementPtr->PrimIndex],
				STAMP_PRIM);
		HyperSpaceElementPtr->preprocess_func = animation_preprocess;

		rand_val = engine_.random()();
		dy = LOWORD (rand_val);
		dx = (SIZE)(LOBYTE (dy) % SPACE_WIDTH) - (SPACE_WIDTH >> 1);
		dy = (SIZE)(HIBYTE (dy) % SPACE_HEIGHT) - (SPACE_HEIGHT >> 1);
		HyperSpaceElementPtr->current.location.x = (LOG_SPACE_WIDTH >> 1)
				+ DISPLAY_TO_WORLD (dx);
		HyperSpaceElementPtr->current.location.y = (LOG_SPACE_HEIGHT >> 1)
				+ DISPLAY_TO_WORLD (dy);
		HyperSpaceElementPtr->current.image.farray = &stars_in_space;

		if (HIWORD (rand_val) & 7)
		{
			HyperSpaceElementPtr->life_span = 14;
			HyperSpaceElementPtr->current.image.frame = stars_in_space;
		}
		else
		{
			HyperSpaceElementPtr->life_span = 12;
			HyperSpaceElementPtr->current.image.frame =
					SetAbsFrameIndex (stars_in_space, 14);
		}

		UnlockElement (hHyperSpaceElement);

		InsertElement (hHyperSpaceElement, GetHeadElement ());
	}
}

#define NUM_VORTEX_TRANSITIONS 9
#define VORTEX_WAIT 1

void
uqm::SpaceNavigation::encounter_transition(ELEMENT *ElementPtr)
{
	ElementPtr->state_flags &= ~DISAPPEARING;
	ElementPtr->life_span = 1;
	if (ElementPtr->turn_wait)
	{
		--ElementPtr->turn_wait;
	}
	else
	{
		FRAME f;

		if (ElementPtr->hit_points)
		{
			f = DecFrameIndex (ElementPtr->current.image.frame);
			ElementPtr->next.image.frame = f;
		}
		else
		{
			f = IncFrameIndex (ElementPtr->current.image.frame);
			if (f != ElementPtr->current.image.farray[0])
				ElementPtr->next.image.frame = f;
			else
				ElementPtr->death_func = NULL;
		}

		ElementPtr->turn_wait = VORTEX_WAIT;
	}
}

static HELEMENT
getSisElement(void)
{
	HSTARSHIP hSis;
	HELEMENT hShip;
	STARSHIP *StarShipPtr;

	hSis = GetHeadLink (&race_q[RPG_PLAYER_NUM]);
	if (!hSis)
		return NULL;

	StarShipPtr = LockStarShip (&race_q[RPG_PLAYER_NUM], hSis);
	hShip = StarShipPtr->hShip;
	UnlockStarShip (&race_q[RPG_PLAYER_NUM], hSis);

#ifdef DEBUG
	{
		ELEMENT *ElementPtr;
		LockElement (hShip, &ElementPtr);
		assert (ElementPtr->state_flags & PLAYER_SHIP);
		UnlockElement (hShip);
	}
#endif

	return hShip;
}

void
uqm::SpaceNavigation::encounter_collision(ELEMENT *ElementPtr0, POINT *pPt0,
		ELEMENT *ElementPtr1, POINT *pPt1)
{
    if (transition_) return;
    if (!(ElementPtr1->state_flags & PLAYER_SHIP)
            || !(legacy_.activity & IN_BATTLE)) return;
    init_transition(ElementPtr0, ElementPtr1, RANDOM_ENCOUNTER_TRANSITION);
    for (auto &record : records_) {
        if (auto element = encounters_.find(record, engine_))
            element->get().state_flags |= NONSOLID | IGNORE_SIMILAR;
    }

	// Mark this element as collided with flagship
	// XXX: We could simply set hTarget to 1 or to ElementPtr1,
	//   but that would be too hacky ;)
	ElementPtr0->hTarget = getSisElement ();
	ZeroVelocityComponents (&ElementPtr0->velocity);

	(void) pPt0;  /* Satisfying compiler (unused parameter) */
	(void) pPt1;  /* Satisfying compiler (unused parameter) */
}

HELEMENT
uqm::SpaceNavigation::AddEncounterElement(ENCOUNTER *EncounterPtr, POINT *puniverse)
{
	BOOLEAN NewEncounter;
	HELEMENT hElement;
	POINT enc_pt;
	
	if (isQuasiSpace())
		return 0;

	if (EncounterPtr->flags & ENCOUNTER_REFORMING)
	{
		EncounterPtr->flags &= ~ENCOUNTER_REFORMING;

		EncounterPtr->transition_state = 100;
		if ((EncounterPtr->flags & ONE_SHOT_ENCOUNTER)
				|| EncounterPtr->num_ships == 0)
			return 0;
	}

	if (EncounterPtr->num_ships)
	{
		NewEncounter = FALSE;
		enc_pt = EncounterPtr->loc_pt;
	}
	else
	{
		BYTE Type;
		SIZE dx, dy;
		COUNT i;
		COUNT NumShips;
		DWORD radius_squared;

		NewEncounter = TRUE;

		radius_squared = (DWORD)EncounterPtr->radius * EncounterPtr->radius;

		Type = EncounterPtr->race_id;
		const auto composition = session_.navigationRules().encounterSize.at(Type);
		NumShips = composition.minimum;
		for (i = composition.maximum - NumShips; i; --i)
		{
			if ((COUNT)engine_.random()() % 100 < 50)
				++NumShips;
		}

		if (NumShips > MAX_HYPER_SHIPS)
			NumShips = MAX_HYPER_SHIPS;

		EncounterPtr->num_ships = NumShips;
		const auto &definition = session_.raceFleets().at(Type);
		for (i = 0; i < NumShips; ++i)
		{
			BRIEF_SHIP_INFO *BSIPtr = &EncounterPtr->ShipList[i];
			BSIPtr->race_id = Type;
			BSIPtr->crew_level = definition.crew_level;
			BSIPtr->max_crew = definition.max_crew;
			BSIPtr->max_energy = definition.max_energy;
		}

		do
		{
			DWORD rand_val;

			rand_val = engine_.random()();

			enc_pt.x = puniverse->x
					+ (LOWORD (rand_val) % (XOFFS << 1)) - XOFFS;
			if (enc_pt.x < 0)
				enc_pt.x = 0;
			else if (enc_pt.x > MAX_X_UNIVERSE)
				enc_pt.x = MAX_X_UNIVERSE;
			enc_pt.y = puniverse->y
					+ (HIWORD (rand_val) % (YOFFS << 1)) - YOFFS;
			if (enc_pt.y < 0)
				enc_pt.y = 0;
			else if (enc_pt.y > MAX_Y_UNIVERSE)
				enc_pt.y = MAX_Y_UNIVERSE;

			dx = enc_pt.x - EncounterPtr->origin.x;
			dy = enc_pt.y - EncounterPtr->origin.y;
		} while ((DWORD)((long)dx * dx + (long)dy * dy) > radius_squared);

		EncounterPtr->loc_pt = enc_pt;
		EncounterPtr->log_x = UNIVERSE_TO_LOGX (enc_pt.x);
		EncounterPtr->log_y = UNIVERSE_TO_LOGY (enc_pt.y);
	}

	hElement = AllocHyperElement (&enc_pt);
	if (hElement)
	{
		SIZE i;
		ELEMENT *ElementPtr;

		LockElement (hElement, &ElementPtr);

        uqm::detail::attachEntity(*ElementPtr, std::make_unique<NavigationEncounter>(*this), EntityHooks::Collision);
		i = EncounterPtr->transition_state;
		if (i || NewEncounter)
		{
			if (i < 0)
			{
				i = -i;
				ElementPtr->hit_points = 1;
			}
			if (i == 0 || i > NUM_VORTEX_TRANSITIONS)
				i = NUM_VORTEX_TRANSITIONS;

			ElementPtr->current.image.frame = SetRelFrameIndex (
					ElementPtr->current.image.farray[0], -i);
			uqm::detail::enableEntityHooks(*ElementPtr, EntityHooks::Expiration);
		}
		else
		{
			ElementPtr->current.image.frame =
					DecFrameIndex (ElementPtr->current.image.farray[0]);
		}

		ElementPtr->turn_wait = VORTEX_WAIT;
		ElementPtr->preprocess_func = NULL;
		ElementPtr->postprocess_func = NULL;

		SetUpElement (ElementPtr);

		ElementPtr->IntersectControl.IntersectStamp.frame =
				DecFrameIndex (stars_in_space);
		SetPrimType (&DisplayArray[ElementPtr->PrimIndex], NO_PRIM);
		ElementPtr->state_flags |= NONSOLID | IGNORE_VELOCITY;

		UnlockElement (hElement);

		InsertElement (hElement, GetTailElement ());
	}
	
	if (hElement) encounters_.insert(*EncounterPtr,
            uqm::combat::EntityHandle::capture(disp_q, *static_cast<ELEMENT *>(hElement)));
	return hElement;
}

#define GRID_OFFSET 200

void
uqm::SpaceNavigation::DrawHyperGrid(COORD ux, COORD uy, COORD ox, COORD oy)
{
	COORD sx, sy, ex, ey;
	RECT r;

	ClearDrawable ();
	SetContextForeGroundColor (
			BUILD_COLOR (MAKE_RGB15 (0x00, 0x10, 0x00), 0x6B));

	sx = ux - (RADAR_SCAN_WIDTH >> 1);
	if (sx  < 0)
		sx = 0;
	else
		sx -= sx % GRID_OFFSET;
	ex = ux + (RADAR_SCAN_WIDTH >> 1);
	if (ex > MAX_X_UNIVERSE + 1)
		ex = MAX_X_UNIVERSE + 1;

	sy = uy - (RADAR_SCAN_HEIGHT >> 1);
	if (sy < 0)
		sy = 0;
	else
		sy -= sy % GRID_OFFSET;
	ey = uy + (RADAR_SCAN_HEIGHT >> 1);
	if (ey > MAX_Y_UNIVERSE + 1)
		ey = MAX_Y_UNIVERSE + 1;

	r.corner.y = (COORD) ((long)(MAX_Y_UNIVERSE - ey)
			* RADAR_HEIGHT / RADAR_SCAN_HEIGHT) - oy;
	r.extent.width = 1;
	r.extent.height = ((COORD) ((long)(MAX_Y_UNIVERSE - sy)
			* RADAR_HEIGHT / RADAR_SCAN_HEIGHT) - oy) - r.corner.y + 1;
	for (ux = sx; ux <= ex; ux += GRID_OFFSET)
	{
		r.corner.x = (COORD) ((long)ux * RADAR_WIDTH / RADAR_SCAN_WIDTH) - ox;
		DrawFilledRectangle (&r);
	}

	r.corner.x = (COORD) ((long)sx * RADAR_WIDTH / RADAR_SCAN_WIDTH) - ox;
	r.extent.width = ((COORD) ((long)ex * RADAR_WIDTH / RADAR_SCAN_WIDTH)
			- ox) - r.corner.x + 1;
	r.extent.height = 1;
	for (uy = sy; uy <= ey; uy += GRID_OFFSET)
	{
		r.corner.y = (COORD)((long)(MAX_Y_UNIVERSE - uy)
				* RADAR_HEIGHT / RADAR_SCAN_HEIGHT) - oy;
		DrawFilledRectangle (&r);
	}
}

// Returns false iff the encounter is to be removed.
bool
uqm::SpaceNavigation::ProcessEncounter(ENCOUNTER *EncounterPtr, POINT *puniverse,
		COORD ox, COORD oy, STAMP *stamp)
{
	ELEMENT *ElementPtr;
	COORD ex, ey;

	auto element = encounters_.find(*EncounterPtr, engine_);
    if (!element) {
        if (!AddEncounterElement(EncounterPtr, puniverse)) return false;
        element = encounters_.find(*EncounterPtr, engine_);
    }
    ElementPtr = &element->get();

	if (ElementPtr->death_func)
	{
		if (EncounterPtr->transition_state && ElementPtr->turn_wait == 0)
		{
			--EncounterPtr->transition_state;
			if (EncounterPtr->transition_state >= NUM_VORTEX_TRANSITIONS)
				++ElementPtr->turn_wait;
			else if (EncounterPtr->transition_state ==
					-NUM_VORTEX_TRANSITIONS)
			{
				ElementPtr->death_func = NULL;
				return false;
			}
			else
				SetPrimType (&DisplayArray[ElementPtr->PrimIndex],
						STAMP_PRIM);
		}
	}
	else
	{
		SIZE delta_x, delta_y;
		COUNT encounter_radius;

		ElementPtr->life_span = 1;
		GetNextVelocityComponents (&ElementPtr->velocity,
				&delta_x, &delta_y, 1);
		if (ElementPtr->thrust_wait)
			--ElementPtr->thrust_wait;
		else if (!ElementPtr->hTarget)
		{	// This is an encounter that did not collide with flagship
			// The colliding encounter does not move
			COUNT cur_facing, delta_facing;

			cur_facing = ANGLE_TO_FACING (
					GetVelocityTravelAngle (&ElementPtr->velocity));
			delta_facing = NORMALIZE_FACING (cur_facing - ANGLE_TO_FACING (
					ARCTAN (puniverse->x - EncounterPtr->loc_pt.x,
					puniverse->y - EncounterPtr->loc_pt.y)));
			if (delta_facing || (delta_x == 0 && delta_y == 0))
			{
				SIZE speed;

                constexpr auto EncounterTrackWait = 3;
				speed = session_.navigationRules().pursuitSpeed.at(EncounterPtr->race_id);
				if (delta_facing < ANGLE_TO_FACING (HALF_CIRCLE))
					--cur_facing;
				else
					++cur_facing;
				if (NORMALIZE_FACING (delta_facing + ANGLE_TO_FACING (OCTANT))
						> ANGLE_TO_FACING (QUADRANT))
				{
					if (delta_facing < ANGLE_TO_FACING (HALF_CIRCLE))
						--cur_facing;
					else
						++cur_facing;
					speed >>= 1;
				}
				cur_facing = FACING_TO_ANGLE (cur_facing);
				SetVelocityComponents (&ElementPtr->velocity,
						COSINE (cur_facing, speed), SINE (cur_facing, speed));
				GetNextVelocityComponents (&ElementPtr->velocity,
						&delta_x, &delta_y, 1);

				ElementPtr->thrust_wait = EncounterTrackWait;
			}
		}
		EncounterPtr->log_x += delta_x;
		EncounterPtr->log_y -= delta_y;
		EncounterPtr->loc_pt.x = LOGX_TO_UNIVERSE (EncounterPtr->log_x);
		EncounterPtr->loc_pt.y = LOGY_TO_UNIVERSE (EncounterPtr->log_y);

		encounter_radius = EncounterPtr->radius + (GRID_OFFSET >> 1);
		delta_x = EncounterPtr->loc_pt.x - EncounterPtr->origin.x;
		if (delta_x < 0)
			delta_x = -delta_x;
		delta_y = EncounterPtr->loc_pt.y - EncounterPtr->origin.y;
		if (delta_y < 0)
			delta_y = -delta_y;
		if ((COUNT)delta_x >= encounter_radius
				|| (COUNT)delta_y >= encounter_radius
				|| (DWORD)delta_x * delta_x + (DWORD)delta_y * delta_y >=
				(DWORD)encounter_radius * encounter_radius)
		{
			// Encounter globe traveled outside the SoI and now disappears
			ElementPtr->state_flags |= NONSOLID;
			ElementPtr->life_span = 0;

			if (EncounterPtr->transition_state == 0)
			{
				uqm::detail::enableEntityHooks(*ElementPtr, EntityHooks::Expiration);
				EncounterPtr->transition_state = -1;
				ElementPtr->hit_points = 1;
			}
			else
			{
				ElementPtr->death_func = NULL;
				return false;
			}
		}
	}

	ex = EncounterPtr->loc_pt.x;
	ey = EncounterPtr->loc_pt.y;
	if (ex - puniverse->x >= -UNIT_SCREEN_WIDTH
			&& ex - puniverse->x <= UNIT_SCREEN_WIDTH
			&& ey - puniverse->y >= -UNIT_SCREEN_HEIGHT
			&& ey - puniverse->y <= UNIT_SCREEN_HEIGHT)
	{
		ElementPtr->next.location.x =
				(SIZE)(EncounterPtr->log_x - travel().position.x)
				+ (LOG_SPACE_WIDTH >> 1);
		ElementPtr->next.location.y =
				(SIZE)(EncounterPtr->log_y - travel().position.y)
				+ (LOG_SPACE_HEIGHT >> 1);
		if ((ElementPtr->state_flags & NONSOLID)
				&& EncounterPtr->transition_state == 0)
		{
			ElementPtr->current.location = ElementPtr->next.location;
			SetPrimType (&DisplayArray[ElementPtr->PrimIndex],
					STAMP_PRIM);
			if (ElementPtr->death_func == 0)
			{
				InitIntersectStartPoint (ElementPtr);
				ElementPtr->state_flags &= ~NONSOLID;
			}
		}
	}
	else
	{
		ElementPtr->state_flags |= NONSOLID;
		if (ex - puniverse->x < -XOFFS || ex - puniverse->x > XOFFS
				|| ey - puniverse->y < -YOFFS || ey - puniverse->y > YOFFS)
		{
			ElementPtr->life_span = 0;
			ElementPtr->death_func = NULL;
			return false;
		}

		SetPrimType (&DisplayArray[ElementPtr->PrimIndex], NO_PRIM);
	}

		
	stamp->origin.x = (COORD)((long)ex * RADAR_WIDTH / RADAR_SCAN_WIDTH) - ox;
	stamp->origin.y = (COORD)((long)(MAX_Y_UNIVERSE - ey) * RADAR_HEIGHT
			/ RADAR_SCAN_HEIGHT) - oy;
	DrawStamp (stamp);

	return true;
}

void
uqm::SpaceNavigation::ProcessEncounters(POINT *puniverse, COORD ox, COORD oy)
{
    STAMP stamp{};
    stamp.frame = SetAbsFrameIndex(stars_in_space, 91);
    for (auto entry = records_.begin(); entry != records_.end();) {
        if (!ProcessEncounter(&*entry, puniverse, ox, oy, &stamp)) {
            encounters_.erase(*entry);
            entry = records_.erase(entry);
        } else ++entry;
    }
}

void
uqm::SpaceNavigation::SeedUniverse(void)
{
	COORD ox, oy;
	COORD sx, sy, ex, ey;
	SWORD portalCounter, arilouSpaceCounter, arilouSpaceSide;
	POINT universe;
	FRAME blip_frame;
	STAMP s;
	HELEMENT hHyperSpaceElement;
	ELEMENT *HyperSpaceElementPtr;

	universe.x = LOGX_TO_UNIVERSE (travel().position.x);
	universe.y = LOGY_TO_UNIVERSE (travel().position.y);

	blip_frame = SetAbsFrameIndex (stars_in_space, 90);

	SetContext (RadarContext);
	BatchGraphics ();
	
	ox = (COORD)((long)universe.x * RADAR_WIDTH / RADAR_SCAN_WIDTH)
			- (RADAR_WIDTH >> 1);
	oy = (COORD)((long)(MAX_Y_UNIVERSE - universe.y)
			* RADAR_HEIGHT / RADAR_SCAN_HEIGHT) - (RADAR_HEIGHT >> 1);

	ex = (COORD)((long)legacy_.shipStamp.origin.x
			* RADAR_WIDTH / RADAR_SCAN_WIDTH) - (RADAR_WIDTH >> 1);
	ey = (COORD)((long)(MAX_Y_UNIVERSE - legacy_.shipStamp.origin.y)
			* RADAR_HEIGHT / RADAR_SCAN_HEIGHT) - (RADAR_HEIGHT >> 1);

	arilouSpaceCounter = story_.read(uqm::StoryCounter::ArilouSpaceCounter);
	arilouSpaceSide = story_.read(uqm::StoryCounter::ArilouSpaceSide);

//    if (ox != ex || oy != ey)
	{
		DrawHyperGrid (universe.x, universe.y, ox, oy);

		{
			for (const auto id : starsNear(universe, XOFFS, YOFFS))
			{
				const auto &star = catalogue().at(id);
				BYTE star_type;

				ex = star.position().x;
				ey = star.position().y;
				star_type = static_cast<BYTE>(star.properties().type);
				if (arilouSpaceSide >= 2 &&
						ex == ARILOU_HOME_X && ey == ARILOU_HOME_Y)
					star_type = SUPER_GIANT_STAR;

				s.origin.x = (COORD)((long)ex * RADAR_WIDTH
						/ RADAR_SCAN_WIDTH) - ox;
				s.origin.y = (COORD)((long)(MAX_Y_UNIVERSE - ey)
						* RADAR_HEIGHT / RADAR_SCAN_HEIGHT) - oy;
				s.frame = SetRelFrameIndex (blip_frame,
						star_type + 2);
				DrawStamp (&s);
			}
		}
	}

	portalCounter = story_.read(uqm::StoryCounter::PortalCounter);
	if (portalCounter || arilouSpaceCounter)
	{
		COUNT i;
		struct PortalImage { POINT position; COUNT frame; };
        PortalImage portals[2]{};
				// Temporary presentation values for
				// QuasiSpace portals that need to be taken into account.
				// i is set to the number of active portals (max 2).

		i = 0;
		if (portalCounter)
		{
			// A player-created QuasiSpace portal is opening.
			auto &portal = player_.navigationState();

			portals[i].frame = ((portalCounter - 1) >> 1) + 18;
			if (portalCounter == 1) {
                portal.portalX = universe.x;
                portal.portalY = universe.y;
            }
			portals[i].position = {portal.portalX, portal.portalY};
			++i;

			if (++portalCounter == (10 + 1))
				portalCounter = (9 + 1);

			story_.set(uqm::StoryCounter::PortalCounter, portalCounter);
		}

		if (arilouSpaceCounter)
		{
			// The periodically appearing QuasiSpace portal is open.
			portals[i].frame = arilouSpaceCounter >> 1;
			if (arilouSpaceSide <= 1)
			{
				// The player is in HyperSpace
				portals[i].frame += 18;
				portals[i].position.x = ARILOU_SPACE_X;
				portals[i].position.y = ARILOU_SPACE_Y;
			}
			else
			{
				// The player is in QuasiSpace
				portals[i].position.x = QUASI_SPACE_X;
				portals[i].position.y = QUASI_SPACE_Y;
			}
			++i;
		}

		// Process the i portals from SD.
		do
		{
			--i;
			sx = portals[i].position.x - universe.x + XOFFS;
			sy = portals[i].position.y - universe.y + YOFFS;
			if (sx < 0 || sy < 0 || sx >= (XOFFS << 1) || sy >= (YOFFS << 1))
				continue;

			ex = portals[i].position.x;
			ey = portals[i].position.y;
			s.origin.x = (COORD)((long)ex * RADAR_WIDTH / RADAR_SCAN_WIDTH)
					- ox;
			s.origin.y = (COORD)((long)(MAX_Y_UNIVERSE - ey)
					* RADAR_HEIGHT / RADAR_SCAN_HEIGHT) - oy;
			s.frame = SetAbsFrameIndex (stars_in_space, 95);
			DrawStamp (&s);

			ex -= universe.x;
			if (ex < 0)
				ex = -ex;
			ey -= universe.y;
			if (ey < 0)
				ey = -ey;

			if (ex > (XOFFS / NUM_RADAR_SCREENS)
					|| ey > (YOFFS / NUM_RADAR_SCREENS))
				continue;

			hHyperSpaceElement = AllocHyperElement (&portals[i].position);
			if (hHyperSpaceElement == 0)
				continue;

			LockElement (hHyperSpaceElement, &HyperSpaceElementPtr);
			HyperSpaceElementPtr->current.image.frame = SetAbsFrameIndex (
					presentation_->stars,
					portals[i].frame);
			HyperSpaceElementPtr->preprocess_func = NULL;
			HyperSpaceElementPtr->postprocess_func = NULL;
			uqm::detail::attachEntity(*HyperSpaceElementPtr, std::make_unique<NavigationPortal>(*this), EntityHooks::Collision);

			SetUpElement (HyperSpaceElementPtr);

			if (arilouSpaceSide == 1 || arilouSpaceSide == 2)
				uqm::detail::enableEntityHooks(*HyperSpaceElementPtr, EntityHooks::Expiration);
			else
			{
				HyperSpaceElementPtr->death_func = NULL;
				HyperSpaceElementPtr->IntersectControl.IntersectStamp.frame =
						DecFrameIndex (stars_in_space);
			}

			UnlockElement (hHyperSpaceElement);

			InsertElement (hHyperSpaceElement, GetHeadElement ());
		} while (i);
	}

	{
		for (const auto id : starsNear(universe, XOFFS, YOFFS))
		{
			const auto &star = catalogue().at(id);
            const POINT position{static_cast<COORD>(star.position().x), static_cast<COORD>(star.position().y)};
			ex = position.x - universe.x;
			if (ex < 0)
				ex = -ex;
			ey = position.y - universe.y;
			if (ey < 0)
				ey = -ey;
			if (ex > (XOFFS / NUM_RADAR_SCREENS)
					|| ey > (YOFFS / NUM_RADAR_SCREENS))
				continue;

			hHyperSpaceElement = AllocHyperElement (&position);
			if (hHyperSpaceElement == 0)
				continue;


			LockElement (hHyperSpaceElement, &HyperSpaceElementPtr);
			HyperSpaceElementPtr->current.image.frame = SetAbsFrameIndex (
					presentation_->stars,
					static_cast<COUNT>(star.properties().type) * NUM_STAR_COLORS
                    + static_cast<COUNT>(star.properties().color));
			HyperSpaceElementPtr->preprocess_func = NULL;
			HyperSpaceElementPtr->postprocess_func = NULL;
			uqm::detail::attachEntity(*HyperSpaceElementPtr, std::make_unique<NavigationStar>(*this), EntityHooks::Collision);

			SetUpElement (HyperSpaceElementPtr);

			if (selectedStar() == id
					&& story_.read(uqm::StoryCounter::PortalCounter) == 0)
				uqm::detail::enableEntityHooks(*HyperSpaceElementPtr, EntityHooks::Expiration);
			else
			{
				HyperSpaceElementPtr->death_func = NULL;
				HyperSpaceElementPtr->IntersectControl.IntersectStamp.frame =
						DecFrameIndex (stars_in_space);
			}
			UnlockElement (hHyperSpaceElement);

			InsertElement (hHyperSpaceElement, GetHeadElement ());
		}
		ProcessEncounters (&universe, ox, oy);
	}

	s.origin.x = RADAR_WIDTH >> 1;
	s.origin.y = RADAR_HEIGHT >> 1;
	s.frame = blip_frame;
	DrawStamp (&s);

	{
		// draws borders to mini-map
		
		RECT r;
		SetContextForeGroundColor (
				BUILD_COLOR (MAKE_RGB15 (0x0E, 0x0E, 0x0E), 0x00));
		r.corner.x = 0;
		r.corner.y = 0;
		r.extent.width = RADAR_WIDTH - 1;
		r.extent.height = 1;
		DrawFilledRectangle (&r);
		r.extent.width = 1;
		r.extent.height = RADAR_HEIGHT - 1;
		DrawFilledRectangle (&r);

		SetContextForeGroundColor (
				BUILD_COLOR (MAKE_RGB15 (0x06, 0x06, 0x06), 0x00));
		r.corner.x = RADAR_WIDTH - 1;
		r.corner.y = 1;
		r.extent.height = RADAR_HEIGHT - 1;
		DrawFilledRectangle (&r);
		r.corner.x = 1;
		r.corner.y = RADAR_HEIGHT - 1;
		r.extent.width = RADAR_WIDTH - 2;
		r.extent.height = 1;
		DrawFilledRectangle (&r);

		SetContextForeGroundColor (
				BUILD_COLOR (MAKE_RGB15 (0x08, 0x08, 0x08), 0x00));
		r.corner.x = 0;
		DrawPoint (&r.corner);
		r.corner.x = RADAR_WIDTH - 1;
		r.corner.y = 0;
		DrawPoint (&r.corner);
	}

	UnbatchGraphics ();

	SetContext (StatusContext);

	if (!(LOWORD (engine_.random()()) & 7))
		AddAmbientElement ();

	if (universe.x != legacy_.shipStamp.origin.x
			|| universe.y != legacy_.shipStamp.origin.y)
	{
		legacy_.shipStamp.origin = universe;
		DrawHyperCoords (universe);
	}
}

BOOLEAN
uqm::SpaceNavigation::DoHyperspaceMenu(MENU_STATE *pMS)
{
	bool select = PulsedInputState.menu[KEY_MENU_SELECT];
	BOOLEAN handled;

	if ((legacy_.activity & (CHECK_ABORT | CHECK_LOAD))
			|| player_.crew() == (COUNT)~0)
		return FALSE;

	handled = DoMenuChooser (pMS, PM_STARMAP);
	if (handled)
		return TRUE;

	if (!select)
		return TRUE;

	SetFlashRect (NULL);

	switch (pMS->CurState)
	{
		case EQUIP_DEVICE:
			select = uqm::DeviceMenu{story_, records_}.run();
			if (story_.read(uqm::StoryCounter::PortalCounter))
			{	// A player-induced portal to QuasiSpace is opening
				return FALSE;
			}
			if (legacy_.activity & START_ENCOUNTER)
			{	// Adapt the legacy device result at the menu boundary.
                transition_ = NavigationTransition{NavigationDestination::Encounter, std::nullopt};
				return FALSE;
			}
			break;
		case CARGO:
			CargoMenu ();
			break;
		case ROSTER:
			select = RosterMenu ();
			break;
		case GAME_MENU:
			if (!GameOptions ())
				return FALSE; // abort or load
			break;
		case STARMAP:
			StarMap ();
			return FALSE;
		case NAVIGATION:
			return FALSE;
	}

	if (!(legacy_.activity & CHECK_ABORT))
	{
		if (select)
		{	// 3DO menu jumps to NAVIGATE after a successful submenu run
			if (optWhichMenu != OPT_PC)
				pMS->CurState = NAVIGATION;
			DrawMenuStateStrings (PM_STARMAP, pMS->CurState);
		}
		SetFlashRect (SFR_MENU_3DO);
	}

	return TRUE;
}

void
uqm::SpaceNavigation::HyperspaceMenu(void)
{
    // This menu borrows device bindings only. Restore them before navigation's
    // scene can unwind, including exceptions from nested legacy menus.
    struct MenuBatch {
        MenuBatch() { UnbatchGraphics(); }
        ~MenuBatch() { BatchGraphics(); }
    } batch;
    {
        struct MenuContext {
            CONTEXT previous = SetContext(SpaceContext);
            Color background = SetContextBackGroundColor(BLACK_COLOR);
            ~MenuContext() {
                SetContext(SpaceContext);
                SetContextBackGroundColor(background);
                SetContext(previous);
            }
        } context;
        MENU_STATE menu{};
        menu.privData = this;
        menu.InputFunc = [](MENU_STATE *state) {
            return static_cast<SpaceNavigation *>(state->privData)->DoHyperspaceMenu(state);
        };
        menu.Initialized = TRUE;
        menu.CurState = STARMAP;
        {
            struct MenuFlash { ~MenuFlash() { SetFlashRect(nullptr); } } flash;
            DrawMenuStateStrings(PM_STARMAP, STARMAP);
            SetFlashRect(SFR_MENU_3DO);
            SetMenuSounds(MENU_SOUND_ARROWS, MENU_SOUND_SELECT);
            DoInput(&menu, TRUE);
        }
        SetContext(SpaceContext);
        if (!(legacy_.activity & (CHECK_ABORT | CHECK_LOAD))) {
            ClearSISRect(CLEAR_SIS_RADAR);
            WaitForNoInput(ONE_SECOND / 2, FALSE);
        }
    }
    if (!(legacy_.activity & IN_BATTLE)) cleanup_hyperspace();
}

void
SaveSisHyperState(void)
{
	HELEMENT hSisElement;
	ELEMENT *ElementPtr;
	STARSHIP *StarShipPtr;

	// Update 'travel().flight.facing' to the direction the flagship is facing
	hSisElement = getSisElement ();
	if (!hSisElement)
	{	// Happens when saving a game from Hyperspace encounter screen
		return;
	}
	//if (ElementPtr->state_flags & PLAYER_SHIP)
	LockElement (hSisElement, &ElementPtr);
	GetElementStarShip (ElementPtr, &StarShipPtr);
	// XXX: Solar system reentry test depends on ShipFacing != 0
	uqm::navigationFor(*ElementPtr).saveFacing(StarShipPtr->ShipFacing + 1);
	UnlockElement (hSisElement);
}


namespace {
uqm::SpaceNavigation &flagshipNavigation() {
    const auto handle = getSisElement();
    if (!handle) throw std::logic_error("navigation callback without a live flagship");
    return uqm::navigationFor(*static_cast<ELEMENT *>(handle));
}
}
// C clock/camera boundaries have no entity argument. Resolve only the selected
// engine's flagship owner, never the active campaign/session.
void MoveSIS(SIZE *pdx, SIZE *pdy) { flagshipNavigation().MoveSIS(pdx, pdy); }
void check_hyperspace_encounter(void) { flagshipNavigation().check_hyperspace_encounter(); }

namespace uqm {
std::optional<NavigationTransition> SpaceNavigation::run() {
    transition_.reset();
    SimulationBinding simulation{engine_};
    NavigationPresentation presentation{session_.assets()};
    struct PresentationBorrow {
        NavigationPresentation *&slot;
        NavigationPresentation *previous;
        ~PresentationBorrow() { slot = previous; }
    } borrow{presentation_, std::exchange(presentation_, &presentation)};
    // The scene destroys its entities before this asset lease ends, even on failure.
    runSpaceNavigation(*this, [this, attached = false]() mutable {
        if (!attached) {
            const auto handle = getSisElement();
            if (!handle) throw std::logic_error("navigation scene has no flagship");
            auto &element = *static_cast<ELEMENT *>(handle);
            uqm::detail::attachEntity(element, std::make_unique<NavigationFlagship>(*this),
                    EntityHooks::Update | EntityHooks::Postprocess);
            attached = true;
        }
        frame();
    });
    // Session exits win over any final-frame contact. The dispatcher must not
    // launch a successor against state about to be replaced by load/shutdown.
    if (session_.menuExit() || (legacy_.activity & (CHECK_ABORT | CHECK_LOAD)))
        return std::nullopt;
    return transition_;
}
void SpaceNavigation::frame() {
    clock_.tick();
    auto counter = story_.read(uqm::StoryCounter::ArilouSpaceCounter);
    if (story_.read(uqm::StoryFlag::ArilouSpace) == OPENING) {
        if (counter < 9) ++counter;
    } else if (counter > 0) --counter;
    story_.set(uqm::StoryCounter::ArilouSpaceCounter, counter);
    if (!(legacy_.activity & (CHECK_ABORT | CHECK_LOAD))) SeedUniverse();
    DrawAutoPilotMessage(FALSE);
}
}
