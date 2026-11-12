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

#include "ipdisp.h"
#include "game_collections.hpp"
#include "system_runtime.hpp"
#include "combat_entity_legacy.hpp"

#include "collide.h"
#include "globdata.h"
#include "init.h"
#include "races.h"
#include "process.h"
#include "grpinfo.h"
#include "encount.h"
		// for EncounterGroup, EncounterRace
#include "libs/mathlib.h"

namespace {
// Captured by each native flight entity. All references outlive its flight
// scene; destruction performs no callbacks or gameplay state publication.
struct FlightScene {
    uqm::SystemGeometry geometry;
    const std::unique_ptr<uqm::SolarSystem> &bodies;
    uqm::SolarSystemLocation &location;
    uqm::StoryState &story;
    QUEUE &groups;
    ACTIVITY &activity;
    VELOCITY_DESC &velocity;
    STAMP &shipStamp;
    BYTE &encounterGroup;
    SIZE &encounterRace;
};
}


void
uqm::notifyGroups (QUEUE &groups, COUNT which_race, BYTE target_loc)
{
	// NOTE: "Others" includes the group causing the notification too.

    for (auto *GroupPtr : detail::systemGroups(groups)) {
		if (GroupPtr->race_id != which_race)
		{
			continue;
		}

		if (target_loc == IPNL_INTERCEPT_PLAYER)
		{
			GroupPtr->task &= ~IGNORE_FLAGSHIP;
			// XXX: orbit_pos is abused here to store the previous
			//   group destination, before the intercept task.
			//   Returned to dest_loc below.
			GroupPtr->orbit_pos = GroupPtr->dest_loc;
			GroupPtr->dest_loc = IPNL_INTERCEPT_PLAYER;
		}
		else if (target_loc == IPNL_ALL_CLEAR)
		{
			GroupPtr->task |= IGNORE_FLAGSHIP;
			
			if (GroupPtr->dest_loc == IPNL_INTERCEPT_PLAYER)
			{	// The group was intercepting, so send it back where it came
				// XXX: orbit_pos was abused to store the previous
				//   group destination, before the intercept task.
				GroupPtr->dest_loc = GroupPtr->orbit_pos;
				GroupPtr->orbit_pos = NORMALIZE_FACING (TFB_Random ());

			}
			// If the group wasn't intercepting, it will just continue
			// going about its business.

			if (!(GroupPtr->task & REFORM_GROUP))
			{
				if ((GroupPtr->task & ~IGNORE_FLAGSHIP) != EXPLORE)
					GroupPtr->group_counter = 0;
				else
					GroupPtr->group_counter = ((COUNT) TFB_Random ()
							% MAX_REVOLUTIONS) << FACING_SHIFT;
			}
		}
		else
		{	// Send the group to the location.
			// XXX: There is currently no use of such notify that I know of.
			GroupPtr->task |= IGNORE_FLAGSHIP;
			GroupPtr->dest_loc = target_loc;
		}

	}
}

namespace {
static SIZE
zoomRadiusForLocation (const FlightScene &scene, BYTE location)
{
	if (location == 0)
	{	// In outer system view; use current zoom radius
		return scene.geometry.sun.radius;
	}
	else
	{	// In inner system view; always max zoom
		return MAX_ZOOM_RADIUS;
	}
}

static inline void
adjustDeltaVforZoom (SIZE zoom, SIZE *dx, SIZE *dy)
{
	if (zoom == MIN_ZOOM_RADIUS)
	{
		*dx >>= 2;
		*dy >>= 2;
	}
	else if (zoom < MAX_ZOOM_RADIUS)
	{
		*dx >>= 1;
		*dy >>= 1;
	}
}

static BYTE getFlagshipLocation(const FlightScene &scene) {
    return scene.bodies->inInnerSystem() ? 1 + scene.bodies->innerPlanet().id().planet : 0;
}

static void
ip_group_preprocess (const FlightScene &scene, IP_GROUP &group, ELEMENT *ElementPtr)
{
#define TRACK_WAIT 5
	BYTE task;
	BYTE target_loc, group_loc, flagship_loc;
	SIZE radius;
	POINT dest_pt;
	SIZE vdx, vdy;
	ELEMENT *EPtr;
	IP_GROUP *GroupPtr;

	EPtr = ElementPtr;
	EPtr->state_flags &= ~(DISAPPEARING | NONSOLID); // "I'm not quite dead"
	++EPtr->life_span; // so that it will 'die' again next time

    GroupPtr = &group;
	group_loc = GroupPtr->sys_loc; // save old location
	DisplayArray[EPtr->PrimIndex].Object.Point = GroupPtr->loc;

	radius = zoomRadiusForLocation (scene, group_loc);
	dest_pt = locationToDisplay (GroupPtr->loc, radius);
	EPtr->current.location.x = DISPLAY_TO_WORLD (dest_pt.x)
			+ (COORD)(LOG_SPACE_WIDTH >> 1)
			- (LOG_SPACE_WIDTH >> (MAX_REDUCTION + 1));
	EPtr->current.location.y = DISPLAY_TO_WORLD (dest_pt.y)
			+ (COORD)(LOG_SPACE_HEIGHT >> 1)
			- (LOG_SPACE_HEIGHT >> (MAX_REDUCTION + 1));

	InitIntersectStartPoint (EPtr);

	flagship_loc = getFlagshipLocation (scene);

	task = GroupPtr->task;

	if ((task & REFORM_GROUP) && --GroupPtr->group_counter == 0)
	{	// Finished reforming the group
		task &= ~REFORM_GROUP;
		GroupPtr->task = task;
		if ((task & ~IGNORE_FLAGSHIP) != EXPLORE)
			GroupPtr->group_counter = 0;
		else
			GroupPtr->group_counter = ((COUNT)TFB_Random ()
					% MAX_REVOLUTIONS) << FACING_SHIFT;
	}

	// If fleeing *and* ignoring flagship
	if ((task & ~(IGNORE_FLAGSHIP | REFORM_GROUP)) == FLEE
			&& (task & IGNORE_FLAGSHIP))
	{	// Make fleeing groups non-collidable
		EPtr->state_flags |= NONSOLID;
	}

	target_loc = GroupPtr->dest_loc;
	if (!(task & (IGNORE_FLAGSHIP | REFORM_GROUP)))
	{
		if (target_loc == IPNL_INTERCEPT_PLAYER && task != FLEE)
		{
			/* if intercepting flagship */
			target_loc = flagship_loc;
			if (EPtr->thrust_wait > TRACK_WAIT)
			{
				EPtr->thrust_wait = 0;
				ZeroVelocityComponents (&EPtr->velocity);
			}
		}
		else if (group_loc == flagship_loc)
		{
			long detect_dist;

			detect_dist = 1200;
			if (group_loc != 0) /* if in planetary views */
			{
				detect_dist *= (MAX_ZOOM_RADIUS / MIN_ZOOM_RADIUS);
				if (GroupPtr->race_id == URQUAN_DRONE_SHIP)
					detect_dist <<= 1;
			}
			vdx = scene.location.flight.position.x - GroupPtr->loc.x;
			vdy = scene.location.flight.position.y - GroupPtr->loc.y;
			if ((long)vdx * vdx
					+ (long)vdy * vdy < (long)detect_dist * detect_dist)
			{
				EPtr->thrust_wait = 0;
				ZeroVelocityComponents (&EPtr->velocity);

				uqm::notifyGroups (scene.groups, GroupPtr->race_id, IPNL_INTERCEPT_PLAYER);
				task = GroupPtr->task;
				target_loc = GroupPtr->dest_loc;
				if (target_loc == IPNL_INTERCEPT_PLAYER)
					target_loc = flagship_loc;
			}
		}
	}

	GetCurrentVelocityComponents (&EPtr->velocity, &vdx, &vdy);

	task &= ~IGNORE_FLAGSHIP;
#ifdef NEVER
	if (task <= FLEE || (task == ON_STATION	&& GroupPtr->dest_loc == 0))
#else
	if (task <= ON_STATION)
#endif /* NEVER */
	{
		BOOLEAN Transition;
		SIZE dx, dy;
		SIZE delta_x, delta_y;
		COUNT angle;

		Transition = FALSE;
		if (task == FLEE)
		{
			dest_pt.x = GroupPtr->loc.x << 1;
			dest_pt.y = GroupPtr->loc.y << 1;
		}
		else if (((task != ON_STATION ||
				GroupPtr->dest_loc == IPNL_INTERCEPT_PLAYER)
				&& group_loc == target_loc)
				|| (task == ON_STATION &&
				GroupPtr->dest_loc != IPNL_INTERCEPT_PLAYER
				&& group_loc == 0))
		{
			if (GroupPtr->dest_loc == IPNL_INTERCEPT_PLAYER)
				dest_pt = scene.location.flight.position;
			else
			{
				COUNT orbit_dist;
				POINT org;

				if (task != ON_STATION)
				{
					orbit_dist = ORBIT_RADIUS;
					org.x = org.y = 0;
				}
				else
				{
					orbit_dist = STATION_RADIUS;
					org = scene.geometry.planetLocation (target_loc - 1);
				}

				angle = FACING_TO_ANGLE (GroupPtr->orbit_pos + 1);
				dest_pt.x = org.x + COSINE (angle, orbit_dist);
				dest_pt.y = org.y + SINE (angle, orbit_dist);
				if (GroupPtr->loc.x == dest_pt.x
						&& GroupPtr->loc.y == dest_pt.y)
				{
					BYTE next_loc;

					GroupPtr->orbit_pos = NORMALIZE_FACING (
							ANGLE_TO_FACING (angle));
					angle += FACING_TO_ANGLE (1);
					dest_pt.x = org.x + COSINE (angle, orbit_dist);
					dest_pt.y = org.y + SINE (angle, orbit_dist);

					EPtr->thrust_wait = (BYTE)~0;
					if (GroupPtr->group_counter)
						--GroupPtr->group_counter;
					else if (task == EXPLORE
							&& (next_loc = (BYTE)(((COUNT)TFB_Random ()
							% scene.geometry.sun.NumPlanets)
							+ 1)) != target_loc)
					{
						EPtr->thrust_wait = 0;
						target_loc = next_loc;
						GroupPtr->dest_loc = next_loc;
					}
				}
			}
		}
		else if (group_loc == 0)
		{
			if (GroupPtr->dest_loc == IPNL_INTERCEPT_PLAYER)
				dest_pt = scene.geometry.sun.location;
			else
				dest_pt = scene.geometry.planetLocation (target_loc - 1);
		}
		else
		{
			if (task == ON_STATION)
				target_loc = 0;

			dest_pt.x = GroupPtr->loc.x << 1;
			dest_pt.y = GroupPtr->loc.y << 1;
		}

		delta_x = dest_pt.x - GroupPtr->loc.x;
		delta_y = dest_pt.y - GroupPtr->loc.y;
		angle = ARCTAN (delta_x, delta_y);

		if (EPtr->thrust_wait && EPtr->thrust_wait != (BYTE)~0)
			--EPtr->thrust_wait;
		else if ((vdx == 0 && vdy == 0)
				|| angle != GetVelocityTravelAngle (&EPtr->velocity))
		{
			SIZE speed;

			if (EPtr->thrust_wait &&
					GroupPtr->dest_loc != IPNL_INTERCEPT_PLAYER)
			{
#define ORBIT_SPEED 60
				speed = ORBIT_SPEED;
				if (task == ON_STATION)
					speed >>= 1;
			}
			else
			{
				SIZE RaceIPSpeed[] =
				{
					RACE_IP_SPEED
				};

				speed = RaceIPSpeed[GroupPtr->race_id];
				EPtr->thrust_wait = TRACK_WAIT;
			}

			vdx = COSINE (angle, speed);
			vdy = SINE (angle, speed);
			SetVelocityComponents (&EPtr->velocity, vdx, vdy);
		}

		dx = vdx;
		dy = vdy;
		if (group_loc == target_loc)
		{
			if (target_loc == 0)
			{
				if (task == FLEE)
					goto CheckGetAway;
			}
			else if (target_loc == GroupPtr->dest_loc)
			{
PartialRevolution:
				if ((long)((COUNT)(dx * dx) + (COUNT)(dy * dy))
						>= (long)delta_x * delta_x + (long)delta_y * delta_y)
				{
					GroupPtr->loc = dest_pt;
					vdx = 0;
					vdy = 0;
					ZeroVelocityComponents (&EPtr->velocity);
				}
			}
		}
		else
		{
			if (group_loc == 0)
			{	// In outer system
				adjustDeltaVforZoom (radius, &dx, &dy);

				if (task == ON_STATION && GroupPtr->dest_loc)
					goto PartialRevolution;
				else if ((long)((COUNT)(dx * dx) + (COUNT)(dy * dy))
						>= (long)delta_x * delta_x + (long)delta_y * delta_y)
					Transition = TRUE;
			}
			else
			{	// In inner system; also leaving outer CheckGetAway hack
CheckGetAway:
				dest_pt = locationToDisplay (GroupPtr->loc, radius);
				if (dest_pt.x < 0
						|| dest_pt.x >= SIS_SCREEN_WIDTH
						|| dest_pt.y < 0
						|| dest_pt.y >= SIS_SCREEN_HEIGHT)
					Transition = TRUE;
			}

			if (Transition)
			{
						/* no collisions during transition */
				EPtr->state_flags |= NONSOLID;

				vdx = 0;
				vdy = 0;
				ZeroVelocityComponents (&EPtr->velocity);
				if (group_loc != 0)
				{
					GroupPtr->loc = scene.geometry.planetLocation (group_loc - 1);
					group_loc = 0;
					GroupPtr->sys_loc = 0;
				}
				else if (target_loc == 0)
				{
					/* Group completely left the star system */
					EPtr->life_span = 0;
					EPtr->state_flags |= DISAPPEARING | NONSOLID;
					GroupPtr->in_system = 0;
					return;
				}
				else
				{
					POINT entryPt;

					if (target_loc == GroupPtr->dest_loc)
					{
						GroupPtr->orbit_pos = NORMALIZE_FACING (
								ANGLE_TO_FACING (angle + HALF_CIRCLE));
						GroupPtr->group_counter =
								((COUNT)TFB_Random () % MAX_REVOLUTIONS)
								<< FACING_SHIFT;
					}
					// The group enters inner system exactly on the edge of a
					// circle with radius = 9/16 * window-dim, which is
					// different from how the flagship enters, but similar
					// in the way that the group will never show up in any
					// of the corners.
					entryPt.x = (SIS_SCREEN_WIDTH >> 1) - COSINE (angle,
							SIS_SCREEN_WIDTH * 9 / 16);
					entryPt.y = (SIS_SCREEN_HEIGHT >> 1) - SINE (angle,
							SIS_SCREEN_HEIGHT * 9 / 16);
					GroupPtr->loc = displayToLocation (entryPt,
							MAX_ZOOM_RADIUS);
					group_loc = target_loc;
					GroupPtr->sys_loc = target_loc;
				}
			}
		}
	}

	radius = zoomRadiusForLocation (scene, group_loc);
	adjustDeltaVforZoom (radius, &vdx, &vdy);
	GroupPtr->loc.x += vdx;
	GroupPtr->loc.y += vdy;

	dest_pt = locationToDisplay (GroupPtr->loc, radius);
	EPtr->next.location.x = DISPLAY_TO_WORLD (dest_pt.x)
			+ (COORD)(LOG_SPACE_WIDTH >> 1)
			- (LOG_SPACE_WIDTH >> (MAX_REDUCTION + 1));
	EPtr->next.location.y = DISPLAY_TO_WORLD (dest_pt.y)
			+ (COORD)(LOG_SPACE_HEIGHT >> 1)
			- (LOG_SPACE_HEIGHT >> (MAX_REDUCTION + 1));

	// Don't draw the group if it's not at flagship location,
	// or flash the group while it's reforming
	if (group_loc != flagship_loc
			|| ((task & REFORM_GROUP)
			&& (GroupPtr->group_counter & 1)))
	{
		SetPrimType (&DisplayArray[EPtr->PrimIndex], NO_PRIM);
		EPtr->state_flags |= NONSOLID;
	}
	else
	{
		SetPrimType (&DisplayArray[EPtr->PrimIndex], STAMP_PRIM);
		if (task & REFORM_GROUP)
			 EPtr->state_flags |= NONSOLID;
	}

	EPtr->state_flags |= CHANGING;
}

static void
flag_ship_collision (const FlightScene &scene, ELEMENT *ElementPtr0, POINT *pPt0,
		ELEMENT *ElementPtr1, POINT *pPt1)
{
	if (scene.activity & START_ENCOUNTER)
		return; // ignore the rest of the collisions

	if (!(ElementPtr1->state_flags & COLLISION))
	{	// The other element's collision has not been processed yet
		// Defer starting the encounter until it is.
		ElementPtr0->state_flags |= COLLISION | NONSOLID;
	}
	else
	{	// Both element's collisions have now been processed
		ElementPtr1->state_flags &= ~COLLISION;
		scene.activity |= START_ENCOUNTER;
	}
	(void) pPt0;  /* Satisfying compiler (unused parameter) */
	(void) pPt1;  /* Satisfying compiler (unused parameter) */
}

static void
ip_group_collision (const FlightScene &scene, IP_GROUP &group, ELEMENT *ElementPtr0, POINT *pPt0,
		ELEMENT *ElementPtr1, POINT *pPt1)
{
	IP_GROUP *GroupPtr;
	void *OtherPtr;

	if (scene.activity & START_ENCOUNTER)
		return; // ignore the rest of the collisions

    GroupPtr = &group;
    OtherPtr = ElementPtr1->pParent; // C projection marks NPC groups, never dereferenced as a ship.
	if (OtherPtr)
	{	// Collision with another group
		// Prevent the groups from coalescing into a single ship icon
		if ((ElementPtr0->state_flags & COLLISION)
				|| (ElementPtr1->current.location.x == ElementPtr1->next.location.x
				&& ElementPtr1->current.location.y == ElementPtr1->next.location.y))
		{
			ElementPtr0->state_flags &= ~COLLISION;
		}
		else
		{
			ElementPtr1->state_flags |= COLLISION;

			GroupPtr->loc = DisplayArray[ElementPtr0->PrimIndex].Object.Point;
			ElementPtr0->next.location = ElementPtr0->current.location;
			InitIntersectEndPoint (ElementPtr0);
		}
	}
	else // if (!OtherPtr)
	{	// Collision with a flagship
		scene.encounterGroup = GroupPtr->group_id;

		GroupPtr->task |= REFORM_GROUP;
		GroupPtr->group_counter = 100;
		// Send "all clear" for the time being. After the encounter, if
		// the player battles the group, the "intercept" notify will be
		// resent.
		uqm::notifyGroups (scene.groups, GroupPtr->race_id, IPNL_ALL_CLEAR);

		if (!(ElementPtr1->state_flags & COLLISION))
		{	// The other element's collision has not been processed yet
			// Defer starting the encounter until it is.
			ElementPtr0->state_flags |= COLLISION | NONSOLID;
		}
		else
		{	// Both element's collisions have now been processed
			ElementPtr1->state_flags &= ~COLLISION;
			scene.activity |= START_ENCOUNTER;
		}
	}
	(void) pPt0;  /* Satisfying compiler (unused parameter) */
	(void) pPt1;  /* Satisfying compiler (unused parameter) */
}

class GroupEntity final : public uqm::combat::CombatEntity {
public:
    GroupEntity(const FlightScene &scene, IP_GROUP &group) : scene_(scene), group_(group) {}
    void update(ELEMENT &) override {}
    void expire(ELEMENT &self) override { ip_group_preprocess(scene_, group_, &self); }
    void collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) override {
        ip_group_collision(scene_, group_, &self, &point, &other, &otherPoint);
    }
private:
    FlightScene scene_;
    IP_GROUP &group_;
};
static void
spawn_ip_group (const FlightScene &scene, IP_GROUP *GroupPtr)
{
	HELEMENT hIPSHIPElement;

	hIPSHIPElement = AllocElement ();
	if (hIPSHIPElement)
	{
		ELEMENT *IPSHIPElementPtr;

		LockElement (hIPSHIPElement, &IPSHIPElementPtr);
		// Must have mass_points for collisions to work
		IPSHIPElementPtr->mass_points = 1;
		IPSHIPElementPtr->hit_points = 1;
		IPSHIPElementPtr->state_flags =
				CHANGING | FINITE_LIFE | IGNORE_VELOCITY;

		SetPrimType (&DisplayArray[IPSHIPElementPtr->PrimIndex], STAMP_PRIM);
		// XXX: Hack: farray points to FRAME[3] and given FRAME
		IPSHIPElementPtr->current.image.farray = &GroupPtr->melee_icon;
		IPSHIPElementPtr->current.image.frame = SetAbsFrameIndex (
					GroupPtr->melee_icon, 1);
			/* preprocessing has a side effect
			 * we wish to avoid.  So death_func
			 * is used instead, but will achieve
			 * same result without the side
			 * effect (InitIntersectFrame)
			 */
        uqm::detail::attachEntity(*IPSHIPElementPtr, std::make_unique<GroupEntity>(scene, *GroupPtr),
            uqm::detail::EntityHooks::Expiration | uqm::detail::EntityHooks::Collision);

		{
			SIZE radius;
			POINT pt;

			radius = zoomRadiusForLocation (scene, GroupPtr->sys_loc);
			pt = locationToDisplay (GroupPtr->loc, radius);

			IPSHIPElementPtr->current.location.x =
					DISPLAY_TO_WORLD (pt.x)
					+ (COORD)(LOG_SPACE_WIDTH >> 1)
					- (LOG_SPACE_WIDTH >> (MAX_REDUCTION + 1));
			IPSHIPElementPtr->current.location.y =
					DISPLAY_TO_WORLD (pt.y)
					+ (COORD)(LOG_SPACE_HEIGHT >> 1)
					- (LOG_SPACE_HEIGHT >> (MAX_REDUCTION + 1));
		}

		IPSHIPElementPtr->pParent = GroupPtr; // Borrowed C collision marker; the native entity holds the typed reference.

		SetUpElement (IPSHIPElementPtr);
		IPSHIPElementPtr->IntersectControl.IntersectStamp.frame =
				DecFrameIndex (stars_in_space);

		UnlockElement (hIPSHIPElement);

		PutElement (hIPSHIPElement);
	}
}

#define FLIP_WAIT 42

static void
flag_ship_preprocess (const FlightScene &scene, ELEMENT *ElementPtr)
{
	if (--ElementPtr->thrust_wait == 0)
		/* juggle list after flagship */
	{
		HELEMENT hSuccElement;

		if ((hSuccElement = GetSuccElement (ElementPtr))
				&& hSuccElement != GetTailElement ())
		{
			HELEMENT hPredElement;
			ELEMENT *TailPtr;

			LockElement (GetTailElement (), &TailPtr);
			hPredElement = _GetPredLink (TailPtr);
			UnlockElement (GetTailElement ());

			RemoveElement (hSuccElement);
			PutElement (hSuccElement);
		}

		ElementPtr->thrust_wait = FLIP_WAIT;
	}

	{
		BYTE flagship_loc, ec;
		SIZE vdx, vdy, radius;
		POINT pt;

		GetCurrentVelocityComponents (&scene.velocity, &vdx, &vdy);

		flagship_loc = getFlagshipLocation (scene);
		radius = zoomRadiusForLocation (scene, flagship_loc);
		adjustDeltaVforZoom (radius, &vdx, &vdy);

		pt = locationToDisplay (scene.location.flight.position, radius);
		ElementPtr->current.location.x = DISPLAY_TO_WORLD (pt.x)
				+ (COORD)(LOG_SPACE_WIDTH >> 1)
				- (LOG_SPACE_WIDTH >> (MAX_REDUCTION + 1));
		ElementPtr->current.location.y = DISPLAY_TO_WORLD (pt.y)
				+ (COORD)(LOG_SPACE_HEIGHT >> 1)
				- (LOG_SPACE_HEIGHT >> (MAX_REDUCTION + 1));
		InitIntersectStartPoint (ElementPtr);

		scene.location.flight.position.x += vdx;
		scene.location.flight.position.y += vdy;

		pt = locationToDisplay (scene.location.flight.position, radius);
		ElementPtr->next.location.x = DISPLAY_TO_WORLD (pt.x)
				+ (COORD)(LOG_SPACE_WIDTH >> 1)
				- (LOG_SPACE_WIDTH >> (MAX_REDUCTION + 1));
		ElementPtr->next.location.y = DISPLAY_TO_WORLD (pt.y)
				+ (COORD)(LOG_SPACE_HEIGHT >> 1)
				- (LOG_SPACE_HEIGHT >> (MAX_REDUCTION + 1));

		scene.shipStamp.origin = pt;
		ElementPtr->next.image.frame = scene.shipStamp.frame;

		if (ElementPtr->sys_loc == flagship_loc)
		{
			if (ElementPtr->state_flags & NONSOLID)
				ElementPtr->state_flags &= ~NONSOLID;
		}
		else /* no collisions during transition */
		{
			ElementPtr->state_flags |= NONSOLID;
			ElementPtr->sys_loc = flagship_loc;
		}

		if ((ec = scene.story.read(uqm::StoryCounter::EscapeCounter))
				&& !(scene.activity & START_ENCOUNTER))
		{
			ElementPtr->state_flags |= NONSOLID;

			--ec;
			scene.story.set(uqm::StoryCounter::EscapeCounter, ec);
		}

		ElementPtr->state_flags |= CHANGING;
	}
}

class FlagshipEntity final : public uqm::combat::CombatEntity {
public:
    explicit FlagshipEntity(const FlightScene &scene) : scene_(scene) {}
    void update(ELEMENT &self) override { flag_ship_preprocess(scene_, &self); }
    void collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) override {
        flag_ship_collision(scene_, &self, &point, &other, &otherPoint);
    }
private:
    FlightScene scene_;
};
static void
spawn_flag_ship (const FlightScene &scene)
{
	HELEMENT hFlagShipElement;

	hFlagShipElement = AllocElement ();
	if (hFlagShipElement)
	{
		ELEMENT *FlagShipElementPtr;

		LockElement (hFlagShipElement, &FlagShipElementPtr);
		FlagShipElementPtr->hit_points = 1;
		// Must have mass_points for collisions to work
		FlagShipElementPtr->mass_points = 1;
		FlagShipElementPtr->sys_loc = getFlagshipLocation (scene);
		FlagShipElementPtr->state_flags = APPEARING | IGNORE_VELOCITY;
		if (scene.story.read(uqm::StoryCounter::EscapeCounter))
			FlagShipElementPtr->state_flags |= NONSOLID;
		FlagShipElementPtr->life_span = NORMAL_LIFE;
		FlagShipElementPtr->thrust_wait = FLIP_WAIT;
		SetPrimType (&DisplayArray[FlagShipElementPtr->PrimIndex], STAMP_PRIM);
		FlagShipElementPtr->current.image.farray =
				&scene.shipStamp.frame;
		FlagShipElementPtr->current.image.frame =
				scene.shipStamp.frame;
        uqm::detail::attachEntity(*FlagShipElementPtr, std::make_unique<FlagshipEntity>(scene),
            uqm::detail::EntityHooks::Update | uqm::detail::EntityHooks::Collision);

		FlagShipElementPtr->current.location.x =
				DISPLAY_TO_WORLD (GLOBAL (ShipStamp.origin.x))
				+ (COORD)(LOG_SPACE_WIDTH >> 1)
				- (LOG_SPACE_WIDTH >> (MAX_REDUCTION + 1));
		FlagShipElementPtr->current.location.y =
				DISPLAY_TO_WORLD (GLOBAL (ShipStamp.origin.y))
				+ (COORD)(LOG_SPACE_HEIGHT >> 1)
				- (LOG_SPACE_HEIGHT >> (MAX_REDUCTION + 1));

		UnlockElement (hFlagShipElement);

		PutElement (hFlagShipElement);
	}
}

} // namespace
void
uqm::runSystemMissions (SOLARSYS_STATE &system)
{
    const FlightScene scene{{system.SunDesc[0], system.PlanetDesc}, system.worlds->system,
        systemLocation(system), systemStory(system), GLOBAL(ip_group_q), GLOBAL(CurrentActivity),
        GLOBAL(velocity), GLOBAL(ShipStamp), EncounterGroup, EncounterRace};
	HSHIPFRAG hGroup, hNextGroup;

	spawn_flag_ship (scene);

	if (scene.encounterRace >= 0)
	{	// There was a battle. Call in reinforcements.
		uqm::notifyGroups (scene.groups, scene.encounterRace, IPNL_INTERCEPT_PLAYER);
		scene.encounterRace = -1;
	}

	for (hGroup = GetHeadLink (&scene.groups);
			hGroup; hGroup = hNextGroup)
	{
		IP_GROUP *GroupPtr;

		GroupPtr = LockIpGroup (&scene.groups, hGroup);
		hNextGroup = _GetSuccLink (GroupPtr);

		if (GroupPtr->in_system)
			spawn_ip_group (scene, GroupPtr);

		UnlockIpGroup (&scene.groups, hGroup);
	}
}


extern "C" void NotifyOthers(COUNT race, BYTE target) {
    uqm::notifyGroups(GLOBAL(ip_group_q), race, target);
}
