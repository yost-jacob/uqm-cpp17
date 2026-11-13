//Copyright Paul Reiche, Fred Ford. 1992-2002
// SPDX-License-Identifier: GPL-2.0-or-later
#include "group_archive.hpp"
#include "game_state.hpp"
#include "encounter_legacy.hpp"
#include "world_archive.hpp"
#include "system_flight.hpp"
#include "build.h"
#include "libs/mathlib.h"
#include "libs/log.h"
#include <stdexcept>
#include <memory>

namespace {
HIPGROUP buildGroup(uqm::GameSession &session, QUEUE &groups, BYTE race) {
    const auto &definition = session.raceFleets().at(race);
    const auto handle = AllocLink(&groups);
    if (!handle) throw std::length_error("system group capacity exceeded");
    auto &group = *LockIpGroup(&groups, handle);
    group = {};
    group.race_id = race;
    group.melee_icon = definition.melee_icon;
    UnlockIpGroup(&groups, handle);
    PutQueue(&groups, handle);
    return handle;
}
}
extern "C" void
ReadGroupHeader (GAME_STATE_FILE *fp, GROUP_HEADER *pGH)
{
	sread_8   (fp, &pGH->NumGroups);
	sread_8   (fp, &pGH->day_index);
	sread_8   (fp, &pGH->month_index);
	sread_8   (fp, NULL); /* padding */
	sread_16  (fp, &pGH->star_index);
	sread_16  (fp, &pGH->year_index);
	sread_a32 (fp, pGH->GroupOffset, NUM_SAVED_BATTLE_GROUPS + 1);
}

extern "C" void
WriteGroupHeader (GAME_STATE_FILE *fp, const GROUP_HEADER *pGH)
{
	swrite_8   (fp, pGH->NumGroups);
	swrite_8   (fp, pGH->day_index);
	swrite_8   (fp, pGH->month_index);
	swrite_8   (fp, 0); /* padding */
	swrite_16  (fp, pGH->star_index);
	swrite_16  (fp, pGH->year_index);
	swrite_a32 (fp, pGH->GroupOffset, NUM_SAVED_BATTLE_GROUPS + 1);
}

extern "C" void
ReadShipFragment (GAME_STATE_FILE *fp, SHIP_FRAGMENT *FragPtr)
{
	BYTE tmpb;

	sread_16 (fp, NULL); /* unused: was which_side */
	sread_8  (fp, &FragPtr->captains_name_index);
	sread_8  (fp, NULL); /* padding; for savegame compat */
	sread_16 (fp, NULL); /* unused: was ship_flags */
	sread_8  (fp, &FragPtr->race_id);
	sread_8  (fp, &FragPtr->index);
	// XXX: reading crew as BYTE to maintain savegame compatibility
	sread_8  (fp, &tmpb);
	FragPtr->crew_level = tmpb;
	sread_8  (fp, &tmpb);
	FragPtr->max_crew = tmpb;
	sread_8  (fp, &FragPtr->energy_level);
	sread_8  (fp, &FragPtr->max_energy);
	sread_16 (fp, NULL); /* unused; was loc.x */
	sread_16 (fp, NULL); /* unused; was loc.y */
}

extern "C" void
WriteShipFragment (GAME_STATE_FILE *fp, const SHIP_FRAGMENT *FragPtr)
{
	swrite_16 (fp, 0); /* unused: was which_side */
	swrite_8  (fp, FragPtr->captains_name_index);
	swrite_8  (fp, 0); /* padding; for savegame compat */
	swrite_16 (fp, 0); /* unused: was ship_flags */
	swrite_8  (fp, FragPtr->race_id);
	swrite_8  (fp, FragPtr->index);
	// XXX: writing crew as BYTE to maintain savegame compatibility
	swrite_8  (fp, FragPtr->crew_level);
	swrite_8  (fp, FragPtr->max_crew);
	swrite_8  (fp, FragPtr->energy_level);
	swrite_8  (fp, FragPtr->max_energy);
	swrite_16 (fp, 0); /* unused; was loc.x */
	swrite_16 (fp, 0); /* unused; was loc.y */
}

extern "C" void
ReadIpGroup (GAME_STATE_FILE *fp, IP_GROUP *GroupPtr)
{
	BYTE tmpb;

	sread_16 (fp, NULL); /* unused; was which_side */
	sread_8  (fp, NULL); /* unused; was captains_name_index */
	sread_8  (fp, NULL); /* padding; for savegame compat */
	sread_16 (fp, &GroupPtr->group_counter);
	sread_8  (fp, &GroupPtr->race_id);
	sread_8  (fp, &tmpb); /* was var2 */
	GroupPtr->sys_loc = LONIBBLE (tmpb);
	GroupPtr->task = HINIBBLE (tmpb);
	sread_8  (fp, &GroupPtr->in_system); /* was crew_level */
	sread_8  (fp, NULL); /* unused; was max_crew */
	sread_8  (fp, &tmpb); /* was energy_level */
	GroupPtr->dest_loc = LONIBBLE (tmpb);
	GroupPtr->orbit_pos = HINIBBLE (tmpb);
	sread_8  (fp, &GroupPtr->group_id); /* was max_energy */
	sread_16s(fp, &GroupPtr->loc.x);
	sread_16s(fp, &GroupPtr->loc.y);
}

extern "C" void
WriteIpGroup (GAME_STATE_FILE *fp, const IP_GROUP *GroupPtr)
{
	swrite_16 (fp, 0); /* unused; was which_side */
	swrite_8  (fp, 0); /* unused; was captains_name_index */
	swrite_8  (fp, 0); /* padding; for savegame compat */
	swrite_16 (fp, GroupPtr->group_counter);
	swrite_8  (fp, GroupPtr->race_id);
	assert (GroupPtr->sys_loc < 0x10 && GroupPtr->task < 0x10);
	swrite_8  (fp, MAKE_BYTE (GroupPtr->sys_loc, GroupPtr->task));
			/* was var2 */
	swrite_8  (fp, GroupPtr->in_system); /* was crew_level */
	swrite_8  (fp, 0); /* unused; was max_crew */
	assert (GroupPtr->dest_loc < 0x10 && GroupPtr->orbit_pos < 0x10);
	swrite_8  (fp, MAKE_BYTE (GroupPtr->dest_loc, GroupPtr->orbit_pos));
			/* was energy_level */
	swrite_8  (fp, GroupPtr->group_id); /* was max_energy */
	swrite_16 (fp, GroupPtr->loc.x);
	swrite_16 (fp, GroupPtr->loc.y);
}

void uqm::GroupArchive::reset(bool firstTime)
{
	GAME_STATE_FILE *fp;

	assert (NUM_SAVED_BATTLE_GROUPS >= MAX_BATTLE_GROUPS);

	fp = detail::openStateFile(session_.world(), RANDGRPINFO_FILE, "wb");
	if (fp)
	{
		GROUP_HEADER GH;

		memset (&GH, 0, sizeof (GH));
		GH.star_index = (COUNT)~0;
		WriteGroupHeader (fp, &GH);
		CloseStateFile (fp);
	}

	if (firstTime && (fp = detail::openStateFile(session_.world(), DEFGRPINFO_FILE, "wb")))
	{
		// Group headers cannot start with offset 0 in 'defined' group
		// info file, so bump it (because offset 0 is reserved to
		// indicate the 'random' group info file).
		swrite_8 (fp, 0);
		CloseStateFile (fp);
	}
}

void uqm::GroupArchive::flush(GROUP_HEADER &header, DWORD offset, BYTE which_group, GAME_STATE_FILE *fp)
{
    auto *pGH = &header;
    auto campaign = detail::encounterLegacy(session_.player());
	if (which_group == GROUP_LIST)
	{
		HIPGROUP hGroup, hNextGroup;

		/* If the group list was never written before, add it */
		if (pGH->GroupOffset[0] == 0)
			pGH->GroupOffset[0] = LengthStateFile (fp);

		// XXX: npc_built_ship_q must be empty because the wipe-out
		//   procedure is actually the writing of the npc_built_ship_q
		//   out as the group in question
		assert (!GetHeadLink (&campaign.ships));

		/* Weed out the groups that left the system first */
		for (hGroup = GetHeadLink (&campaign.groups);
				hGroup; hGroup = hNextGroup)
		{
			BYTE in_system;
			BYTE group_id;
			IP_GROUP *GroupPtr;

			GroupPtr = LockIpGroup (&campaign.groups, hGroup);
			hNextGroup = _GetSuccLink (GroupPtr);
			in_system = GroupPtr->in_system;
			group_id = GroupPtr->group_id;
			UnlockIpGroup (&campaign.groups, hGroup);

			if (!in_system)
			{
				// The following 'if' is needed because GROUP_LIST is only
				// ever flushed to RANDGRPINFO_FILE, but the current group
				// may need to be updated in the DEFGRPINFO_FILE as well.
				// In that case, PutGroupInfo() will update the correct file.
				if (campaign.battleGroupRef)
					save(campaign.battleGroupRef, group_id);
				else
					flush(*pGH, GROUPS_RANDOM, group_id, fp);
				// This will also wipe the group out in the RANDGRPINFO_FILE
				pGH->GroupOffset[group_id] = 0;
				RemoveQueue (&campaign.groups, hGroup);
				FreeIpGroup (&campaign.groups, hGroup);
			}
		}
	}
	else if (which_group > pGH->NumGroups)
	{	/* Group not present yet -- add it */
		pGH->NumGroups = which_group;
		pGH->GroupOffset[which_group] = LengthStateFile (fp);
	}
	
	SeekStateFile (fp, offset, SEEK_SET);
	WriteGroupHeader (fp, pGH);

#ifdef DEBUG_GROUPS
	log_add (log_Debug, "1)FlushGroupInfo(%lu): WG = %u(%lu), NG = %u, "
			"SI = %u", offset, which_group, pGH->GroupOffset[which_group],
			pGH->NumGroups, pGH->star_index);
#endif /* DEBUG_GROUPS */

	if (which_group == GROUP_LIST)
	{
		/* Write out ip_group_q as group 0 */
		HIPGROUP hGroup, hNextGroup;
		BYTE NumGroups = CountLinks (&campaign.groups);

		SeekStateFile (fp, pGH->GroupOffset[0], SEEK_SET);
		swrite_8 (fp, lastEncountered_);
		swrite_8 (fp, NumGroups);

		hGroup = GetHeadLink (&campaign.groups);
		for ( ; NumGroups; --NumGroups, hGroup = hNextGroup)
		{
			IP_GROUP *GroupPtr;
			
			GroupPtr = LockIpGroup (&campaign.groups, hGroup);
			hNextGroup = _GetSuccLink (GroupPtr);

			swrite_8 (fp, GroupPtr->race_id);

#ifdef DEBUG_GROUPS
			log_add (log_Debug, "F) type %u, loc %u<%d, %d>, task 0x%02x:%u",
					GroupPtr->race_id,
					GET_GROUP_LOC (GroupPtr),
					GroupPtr->loc.x,
					GroupPtr->loc.y,
					GET_GROUP_MISSION (GroupPtr),
					GET_GROUP_DEST (GroupPtr));
#endif /* DEBUG_GROUPS */

			WriteIpGroup (fp, GroupPtr);
			
			UnlockIpGroup (&campaign.groups, hGroup);
		}
	}
	else
	{
		/* Write out npc_built_ship_q as 'which_group' group */
		HSHIPFRAG hStarShip, hNextShip;
		BYTE NumShips = CountLinks (&campaign.ships);
		BYTE RaceType = 0;

		hStarShip = GetHeadLink (&campaign.ships);
		if (NumShips > 0)
		{
			SHIP_FRAGMENT *FragPtr;

			/* The first ship in a group defines the alien race */
			FragPtr = LockShipFrag (&campaign.ships, hStarShip);
			RaceType = FragPtr->race_id;
			UnlockShipFrag (&campaign.ships, hStarShip);
		}

		SeekStateFile (fp, pGH->GroupOffset[which_group], SEEK_SET);
		swrite_8 (fp, RaceType);
		swrite_8 (fp, NumShips);

		for ( ; NumShips; --NumShips, hStarShip = hNextShip)
		{
			SHIP_FRAGMENT *FragPtr;

			FragPtr = LockShipFrag (&campaign.ships, hStarShip);
			hNextShip = _GetSuccLink (FragPtr);

			swrite_8 (fp, FragPtr->race_id);
			WriteShipFragment (fp, FragPtr);

			UnlockShipFrag (&campaign.ships, hStarShip);
		}
	}
}

bool uqm::GroupArchive::read(DWORD offset, BYTE which_group, const SystemGeometry *geometry, QUEUE &groups)
{
    if (which_group > NUM_SAVED_BATTLE_GROUPS && which_group != GROUP_INIT_IP)
        throw std::out_of_range("invalid archive group ID");
    auto campaign = detail::encounterLegacy(session_.player());
	GAME_STATE_FILE *fp;
	GROUP_HEADER GH;

	if (offset != GROUPS_RANDOM && which_group != GROUP_LIST)
		fp = detail::openStateFile(session_.world(), DEFGRPINFO_FILE, "r+b");
	else
		fp = detail::openStateFile(session_.world(), RANDGRPINFO_FILE, "r+b");

	if (!fp)
		return FALSE;

    std::unique_ptr<GAME_STATE_FILE, decltype(&CloseStateFile)> file{fp, CloseStateFile};

	SeekStateFile (fp, offset, SEEK_SET);
	ReadGroupHeader (fp, &GH);
    if (GH.NumGroups > NUM_SAVED_BATTLE_GROUPS)
        throw std::out_of_range("invalid archive group count");
#ifdef DEBUG_GROUPS
	log_add (log_Debug, "GetGroupInfo(%lu): %u(%lu) out of %u", offset,
			which_group, which_group == GROUP_INIT_IP ? 0 : GH.GroupOffset[which_group], GH.NumGroups);
#endif /* DEBUG_GROUPS */

	if (which_group == GROUP_INIT_IP)
	{
		COUNT month_index, day_index, year_index;

		ReinitQueue (&groups);
#ifdef DEBUG_GROUPS
		log_add (log_Debug, "%u == %u", GH.star_index,
				(COUNT)(session_.player().navigationState().currentStar.value().value));
#endif /* DEBUG_GROUPS */

		/* Check if the requested groups are valid for this star system
		 * and if they are still current (not expired) */
		day_index = GH.day_index;
		month_index = GH.month_index;
		year_index = GH.year_index;
		if (offset == GROUPS_RANDOM
				&& (GH.star_index != (COUNT)(session_.player().navigationState().currentStar.value().value)
				|| !session_.clock().accepts({year_index, month_index, day_index})))
		{
#ifdef DEBUG_GROUPS
			if (GH.star_index == session_.player().navigationState().currentStar.value().value)
				log_add (log_Debug, "GetGroupInfo: battle groups out of "
						"date %u/%u/%u!", month_index, day_index,
						year_index);
#endif /* DEBUG_GROUPS */

			file.reset();
			/* Erase random groups (out of date) */
			fp = detail::openStateFile(session_.world(), RANDGRPINFO_FILE, "wb");
            file.reset(fp);
			memset (&GH, 0, sizeof (GH));
			GH.star_index = (COUNT)~0;
			WriteGroupHeader (fp, &GH);
			file.reset();
			
			return FALSE;
		}

		/* Read IP groups into ip_group_q and send them on their missions */
		for (which_group = 1; which_group <= GH.NumGroups; ++which_group)
		{
			BYTE task, group_loc;
			DWORD rand_val;
			BYTE RaceType;
			BYTE NumShips;
			HIPGROUP hGroup;
			IP_GROUP *GroupPtr;

			if (GH.GroupOffset[which_group] == 0)
				continue;

			SeekStateFile (fp, GH.GroupOffset[which_group], SEEK_SET);
			sread_8 (fp, &RaceType);
			sread_8 (fp, &NumShips);
			if (!NumShips)
				continue; /* group is dead */

			hGroup = buildGroup(session_, groups, RaceType);
			GroupPtr = LockIpGroup (&groups, hGroup);
			GroupPtr->group_id = which_group;
			GroupPtr->in_system = 1;

			rand_val = TFB_Random ();
			task = (BYTE)(LOBYTE (LOWORD (rand_val)) % ON_STATION);
			if (task == FLEE)
				task = ON_STATION;
			GroupPtr->orbit_pos = NORMALIZE_FACING (
					LOBYTE (HIWORD (rand_val)));

			group_loc = geometry->sun.NumPlanets;
			if (group_loc == 1 && task == EXPLORE)
				task = IN_ORBIT;
			else
				group_loc = (BYTE)((HIBYTE (LOWORD (rand_val)) % group_loc) + 1);
			GroupPtr->dest_loc = group_loc;
			rand_val = TFB_Random ();
			GroupPtr->loc.x = (LOWORD (rand_val) % 10000) - 5000;
			GroupPtr->loc.y = (HIWORD (rand_val) % 10000) - 5000;
			GroupPtr->group_counter = 0;
			if (task == EXPLORE)
			{
				GroupPtr->group_counter = ((COUNT)TFB_Random () %
						MAX_REVOLUTIONS) << FACING_SHIFT;
			}
			else if (task == ON_STATION)
			{
				COUNT angle;
				POINT org;

				org = geometry->planetLocation (group_loc - 1);
				angle = FACING_TO_ANGLE (GroupPtr->orbit_pos + 1);
				GroupPtr->loc.x = org.x + COSINE (angle, STATION_RADIUS);
				GroupPtr->loc.y = org.y + SINE (angle, STATION_RADIUS);
				group_loc = 0;
			}

			GroupPtr->task = task;
			GroupPtr->sys_loc = group_loc;

#ifdef DEBUG_GROUPS
			log_add (log_Debug, "battle group %u(0x%04x) strength "
					"%u, type %u, loc %u<%d, %d>, task %u",
					which_group,
					hGroup,
					NumShips,
					RaceType,
					group_loc,
					GroupPtr->loc.x,
					GroupPtr->loc.y,
					task);
#endif /* DEBUG_GROUPS */

			UnlockIpGroup (&groups, hGroup);
		}

		if (offset != GROUPS_RANDOM)
			reset(false); 	/* Wipe out random battle groups */
		else if (session_.clock().accepts({year_index, month_index, day_index}))
		{
			file.reset();
			return TRUE;
		}

		file.reset();
		return GetHeadLink (&groups) ? TRUE : FALSE;
	}
	
	if (!GH.GroupOffset[which_group])
	{
		/* Group not present */
		file.reset();
		return FALSE;
	}


	if (which_group == GROUP_LIST)
	{
		BYTE NumGroups;
		COUNT ShipsLeftInLEG;

		// XXX: Hack: First, save the state of last encountered group, if any.
		//   The assumption here is that we read the group list immediately
		//   after an IP encounter, and npc_built_ship_q contains whatever
		//   ships are left in the encountered group (can be none).
		ShipsLeftInLEG = CountLinks (&campaign.ships);
		
		SeekStateFile (fp, GH.GroupOffset[0], SEEK_SET);
		sread_8 (fp, &lastEncountered_);

		if (lastEncountered_)
		{
			// The following 'if' is needed because GROUP_LIST is only
			// ever read from RANDGRPINFO_FILE, but the lastEncountered_
			// may need to be updated in the DEFGRPINFO_FILE as well.
			// In that case, PutGroupInfo() will update the correct file.
			if (campaign.battleGroupRef)
				save(campaign.battleGroupRef, lastEncountered_);
			else
				flush(GH, offset, lastEncountered_, fp);
		}
		ReinitQueue (&campaign.ships);

		/* Read group 0 into ip_group_q */
		ReinitQueue (&groups);
		/* Need a seek because Put/Flush has moved the file ptr */
		SeekStateFile (fp, GH.GroupOffset[0] + 1, SEEK_SET);
		sread_8 (fp, &NumGroups);

		while (NumGroups--)
		{
			BYTE group_id;
			BYTE RaceType;
			HSHIPFRAG hGroup;
			IP_GROUP *GroupPtr;

			sread_8 (fp, &RaceType);

			hGroup = buildGroup(session_, groups, RaceType);
			GroupPtr = LockIpGroup (&groups, hGroup);
			ReadIpGroup (fp, GroupPtr);
			group_id = GroupPtr->group_id;

#ifdef DEBUG_GROUPS
			log_add (log_Debug, "G) type %u, loc %u<%d, %d>, task 0x%02x:%u",
					RaceType,
					GroupPtr->sys_loc,
					GroupPtr->loc.x,
					GroupPtr->loc.y,
					GroupPtr->task,
					GroupPtr->dest_loc);
#endif /* DEBUG_GROUPS */

			UnlockIpGroup (&groups, hGroup);

			if (group_id == lastEncountered_ && !ShipsLeftInLEG)
			{
				/* No ships left in the last encountered group, remove it */
#ifdef DEBUG_GROUPS
				log_add (log_Debug, " -- REMOVING");
#endif /* DEBUG_GROUPS */
				RemoveQueue (&groups, hGroup);
				FreeIpGroup (&groups, hGroup);
			}
		}

		file.reset();
		return GetHeadLink (&groups) ? TRUE : FALSE;
	}
	else
	{
		/* Read 'which_group' group into npc_built_ship_q */
		BYTE NumShips;

		// XXX: Hack: The assumption here is that we only read the makeup
		//   of a particular group when initializing an encounter, which
		//   makes this group 'last encountered'. Also the state of all
		//   groups is saved here. This may make working with savegames
		//   harder in the future, as special care will have to be taken
		//   when loading a game into an encounter.
		lastEncountered_ = which_group;
		// The following 'if' is needed because GROUP_LIST is only
		// ever written to RANDGRPINFO_FILE, but the group we are reading
		// may be in the DEFGRPINFO_FILE as well.
		// In that case, PutGroupInfo() will update the correct file.
		// Always calling PutGroupInfo() here would also be acceptable now.
		if (offset != GROUPS_RANDOM)
			save(GROUPS_RANDOM, GROUP_LIST);
		else
			flush(GH, GROUPS_RANDOM, GROUP_LIST, fp);
		ReinitQueue (&groups);

		ReinitQueue (&campaign.ships);
		// skip RaceType
		SeekStateFile (fp, GH.GroupOffset[which_group] + 1, SEEK_SET);
		sread_8 (fp, &NumShips);

		while (NumShips--)
		{
			BYTE RaceType;
			HSHIPFRAG hStarShip;
			SHIP_FRAGMENT *FragPtr;

			sread_8 (fp, &RaceType);

			hStarShip = CloneShipFragmentFromTemplate(RaceType, &session_.raceFleets().at(RaceType), &campaign.ships, 0);

			FragPtr = LockShipFrag (&campaign.ships, hStarShip);
			ReadShipFragment (fp, FragPtr);
			UnlockShipFrag (&campaign.ships, hStarShip);
		}

		file.reset();
		return GetHeadLink (&campaign.ships) ? TRUE : FALSE;
	}
}

DWORD uqm::GroupArchive::save(DWORD offset, BYTE which_group)
{
    if (which_group > NUM_SAVED_BATTLE_GROUPS && which_group != GROUP_INIT_IP)
        throw std::out_of_range("invalid archive group ID");
    auto campaign = detail::encounterLegacy(session_.player());
	GAME_STATE_FILE *fp;
	GROUP_HEADER GH;

	if (offset != GROUPS_RANDOM && which_group != GROUP_LIST)
		fp = detail::openStateFile(session_.world(), DEFGRPINFO_FILE, "r+b");
	else
		fp = detail::openStateFile(session_.world(), RANDGRPINFO_FILE, "r+b");

	if (!fp)
		return offset;

    const std::unique_ptr<GAME_STATE_FILE, decltype(&CloseStateFile)> file{fp, CloseStateFile};

	if (offset == GROUPS_ADD_NEW)
	{
		offset = LengthStateFile (fp);
		SeekStateFile (fp, offset, SEEK_SET);
		memset (&GH, 0, sizeof (GH));
		GH.star_index = (COUNT)~0;
		WriteGroupHeader (fp, &GH);
	}

	// XXX: This is a bit dangerous. The assumption here is that we are
	//   only called to write GROUP_LIST in the GROUPS_RANDOM context,
	//   which is true right now and in which case we would seek to 0 anyway.
	//   The latter also makes guarding the seek with
	//   'if (which_group != GROUP_LIST)' moot.
	if (which_group != GROUP_LIST)
	{
		SeekStateFile (fp, offset, SEEK_SET);
		if (which_group == GROUP_SAVE_IP)
		{
			lastEncountered_ = 0;
			which_group = GROUP_LIST;
		}
	}
	ReadGroupHeader (fp, &GH);
    if (GH.NumGroups > NUM_SAVED_BATTLE_GROUPS)
        throw std::out_of_range("invalid archive group count");


    {
        const auto expiry = session_.clock().resolve({0, 0, 7});
        GH.day_index = static_cast<BYTE>(expiry.day);
        GH.month_index = static_cast<BYTE>(expiry.month);
        GH.year_index = static_cast<COUNT>(expiry.year);
    }
	GH.star_index = session_.player().navigationState().currentStar.value().value;

#ifdef DEBUG_GROUPS
	log_add (log_Debug, "PutGroupInfo(%lu): %u out of %u -- %u/%u/%u",
			offset, which_group, GH.NumGroups,
			GH.month_index, GH.day_index, GH.year_index);
#endif /* DEBUG_GROUPS */

	flush(GH, offset, which_group, fp);


	return (offset);
}



bool uqm::GroupArchive::load(DWORD offset, BYTE group) {
    if (group == GROUP_INIT_IP)
        throw std::invalid_argument("group initialization requires system geometry");
    return read(offset, group, nullptr, detail::encounterLegacy(session_.player()).groups);
}
bool uqm::GroupArchive::initialize(DWORD offset, const SystemGeometry &geometry, QUEUE &groups) {
    return read(offset, GROUP_INIT_IP, &geometry, groups);
}
