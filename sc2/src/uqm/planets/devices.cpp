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

#include "libs/threadlib.h"
#include "../device_menu.hpp"
#include "../system_runtime.hpp"
#include "../world_legacy.hpp"
#include "../game_state.hpp"
#include <algorithm>
#include <stdexcept>
#include "../build.h"
#include "../colors.h"
#include "../gendef.h"
#include "../starmap.h"
#include "../encount.h"
#include "../gamestr.h"
#include "../controls.h"
#include "../save.h"
#include "../settings.h"
#include "../shipcont.h"
#include "../setup.h"
#include "../state.h"
#include "../sis.h"
		// for ClearSISRect()
#include "../grpinfo.h"
#include "../sounds.h"
#include "../util.h"
#include "../hyper.h"
		// for SaveSisHyperState()
#include "planets.h"
		// for SaveSolarSysLocation() and tests
#include "libs/strlib.h"


constexpr int DEVICE_ICON_WIDTH = 16;
constexpr int DEVICE_ICON_HEIGHT = 16;

constexpr int DEVICE_ORG_Y = 33;
constexpr int DEVICE_SPACING_Y = (DEVICE_ICON_HEIGHT + 2);

constexpr int DEVICE_COL_0 = 4;
constexpr int DEVICE_COL_1 = 40;

constexpr int DEVICE_SEL_ORG_X = (DEVICE_COL_0 + DEVICE_ICON_WIDTH);
constexpr int DEVICE_SEL_WIDTH = (FIELD_WIDTH + 1 - DEVICE_SEL_ORG_X + 1);

constexpr int ICON_OFS_Y = 1;
constexpr int NAME_OFS_Y = 2;
constexpr int TEXT_BASELINE = 6;
constexpr int TEXT_SPACING_Y = 7;

constexpr int MAX_VIS_DEVICES = ((129 - DEVICE_ORG_Y) / DEVICE_SPACING_Y);


constexpr int PORTAL_FUEL_COST = 10 * FUEL_TANK_SCALE;

struct DeviceMenuState
{
    uqm::DeviceMenu &owner;
	std::vector<BYTE> list;
			// List of all devices player has
	COUNT topIndex = 0;
			// Index of the top device displayed
};




static void
DrawDevice (COUNT device, COUNT pos, bool selected)
{
	RECT r;
	TEXT t;

	t.align = ALIGN_CENTER;
	t.baseline.x = DEVICE_COL_1;

	r.extent.width = DEVICE_SEL_WIDTH;
	r.extent.height = TEXT_SPACING_Y * 2;
	r.corner.x = DEVICE_SEL_ORG_X;

	// draw line background
	r.corner.y = DEVICE_ORG_Y + pos * DEVICE_SPACING_Y + NAME_OFS_Y;
	SetContextForeGroundColor (selected ?
			DEVICES_SELECTED_BACK_COLOR : DEVICES_BACK_COLOR);
	DrawFilledRectangle (&r);

	SetContextFont (TinyFont);

	// print device name
	SetContextForeGroundColor (selected ?
			DEVICES_SELECTED_NAME_COLOR : DEVICES_NAME_COLOR);
	t.baseline.y = r.corner.y + TEXT_BASELINE;
	t.pStr = GAME_STRING (device + DEVICE_STRING_BASE + 1);
	t.CharCount = utf8StringPos (t.pStr, ' ');
	font_DrawText (&t);
	t.baseline.y += TEXT_SPACING_Y;
	t.pStr = skipUTF8Chars (t.pStr, t.CharCount + 1);
	t.CharCount = (COUNT)~0;
	font_DrawText (&t);
}

static void
DrawDevicesDisplay (DeviceMenuState *devState)
{
	TEXT t;
	RECT r;
	STAMP s;
	COORD cy;
	COUNT i;

	r.corner.x = 2;
	r.corner.y = 20;
	r.extent.width = FIELD_WIDTH + 1;
	// XXX: Shouldn't the height be 1 less? This draws the bottom border
	//   1 pixel too low. Or if not, why do we need another box anyway?
	r.extent.height = 129 - r.corner.y;
	DrawStarConBox (&r, 1,
			SHADOWBOX_MEDIUM_COLOR, SHADOWBOX_DARK_COLOR,
			TRUE, DEVICES_BACK_COLOR);

	// print the "DEVICES" title
	SetContextFont (StarConFont);
	t.baseline.x = (STATUS_WIDTH >> 1) - 1;
	t.baseline.y = r.corner.y + 7;
	t.align = ALIGN_CENTER;
	t.pStr = GAME_STRING (DEVICE_STRING_BASE);
	t.CharCount = (COUNT)~0;
	SetContextForeGroundColor (DEVICES_SELECTED_NAME_COLOR);
	font_DrawText (&t);

	s.origin.x = DEVICE_COL_0;
	cy = DEVICE_ORG_Y;

	// draw device icons and print names
	for (i = 0; i < MAX_VIS_DEVICES; ++i, cy += DEVICE_SPACING_Y)
	{
		COUNT devIndex = devState->topIndex + i;

		if (devIndex >= static_cast<SIZE>(devState->list.size()))
			break;

		// draw device icon
		s.origin.y = cy + ICON_OFS_Y;
		s.frame = SetAbsFrameIndex (MiscDataFrame,
				77 + devState->list[devIndex]);
		DrawStamp (&s);

		DrawDevice (devState->list[devIndex], i, false);
	}
}

static void
DrawDevices (DeviceMenuState *devState, COUNT OldDevice, COUNT NewDevice)
{
	BatchGraphics ();

	SetContext (StatusContext);

	if (OldDevice > NUM_DEVICES)
	{	// Asked for the initial display or refresh
		DrawDevicesDisplay (devState);

		// do not draw unselected again this time
		OldDevice = NewDevice;
	}

	if (OldDevice != NewDevice)
	{	// unselect the previous element
		DrawDevice (devState->list[OldDevice], OldDevice - devState->topIndex,
				false);
	}

	if (NewDevice < NUM_DEVICES)
	{	// select the new element
		DrawDevice (devState->list[NewDevice], NewDevice - devState->topIndex,
				true);
	}

	UnbatchGraphics ();
}

// Returns TRUE if the broadcaster has been successfully activated,
// and FALSE otherwise.
bool
uqm::DeviceMenu::useCaster()
{
	if (inHQSpace ())
	{
		if (story_.read(uqm::StoryCounter::ArilouSpaceSide) <= 1)
		{
			story_.set(uqm::StoryFlag::UsedBroadcaster, 1);
			return TRUE;
		}
		return FALSE;
	}

	if (LOBYTE (GLOBAL (CurrentActivity)) != IN_INTERPLANETARY
			|| !system_)
		return FALSE;

	if (inOrbit()
			&& matchWorld (system_, &uqm::detail::selectedDescriptor(*system_),
				1, MATCH_PLANET)
			&& uqm::systemStar(*system_).Index == CHMMR_DEFINED
			&& !story_.read(uqm::StoryFlag::ChmmrFreed))
	{
		// In orbit around the Chenjesu/Mmrnmhrm home planet.
		NextActivity |= CHECK_LOAD;  /* fake a load game */
		GLOBAL (CurrentActivity) |= START_ENCOUNTER;

		EncounterGroup = 0;
		PutGroupInfo (GROUPS_RANDOM, GROUP_SAVE_IP);
		ReinitQueue (&GLOBAL (ip_group_q));
		assert (CountLinks (&GLOBAL (npc_built_ship_q)) == 0);

		story_.set(uqm::StoryCounter::EncounterContext, 1 << 7);
		uqm::saveSolarSystemLocation(*system_);
		return TRUE;
	}

	{
		bool FoundIlwrath;
		HIPGROUP hGroup;

		FoundIlwrath = (uqm::systemStar(*system_).Index == ILWRATH_DEFINED)
				&& StartSphereTracking (ILWRATH_SHIP);
				// In the Ilwrath home system and they are alive?

		if (!FoundIlwrath &&
				(hGroup = GetHeadLink (&GLOBAL (ip_group_q))))
		{
			// Is an Ilwrath ship in the system?
			IP_GROUP *GroupPtr;

			GroupPtr = LockIpGroup (&GLOBAL (ip_group_q), hGroup);
			FoundIlwrath = (GroupPtr->race_id == ILWRATH_SHIP);
			UnlockIpGroup (&GLOBAL (ip_group_q), hGroup);
		}

		if (FoundIlwrath)
		{
			NextActivity |= CHECK_LOAD; /* fake a load game */
			GLOBAL (CurrentActivity) |= START_ENCOUNTER;

			EncounterGroup = 0;
			PutGroupInfo (GROUPS_RANDOM, GROUP_SAVE_IP);
			ReinitQueue (&GLOBAL (ip_group_q));
			assert (CountLinks (&GLOBAL (npc_built_ship_q)) == 0);

			if (uqm::systemStar(*system_).Index == ILWRATH_DEFINED)
			{
				// Ilwrath home system.
				story_.set(uqm::StoryCounter::EncounterContext, 1 << 4);
			}
			else
			{
				// Ilwrath ship.
				story_.set(uqm::StoryCounter::EncounterContext, 1 << 5);
			}
			
			if (inOrbit())
				uqm::saveSolarSystemLocation(*system_);
			return TRUE;
		}
	}

	return FALSE;
}

uqm::DeviceStatus
uqm::DeviceMenu::invoke(BYTE which_device)
{
	BYTE val;

	switch (which_device)
	{
		case ROSY_SPHERE_DEVICE:
			val = story_.read(uqm::StoryCounter::UltronCondition);
			if (val)
			{
				story_.set(uqm::StoryCounter::UltronCondition, val + 1);
				story_.set(uqm::StoryFlag::RosySphereOnShip, 0);
				story_.set(uqm::StoryFlag::DiscussedUltron, 0);
				story_.set(uqm::StoryFlag::SupoxUltronHelp, 0);
				return DeviceStatus::Success;
			}
			break;
		case ARTIFACT_2_DEVICE:
			break;
		case ARTIFACT_3_DEVICE:
			break;
		case SUN_EFFICIENCY_DEVICE:
			if (LOBYTE (GLOBAL (CurrentActivity)) == IN_INTERPLANETARY
					&& inOrbit())
			{
				PlayMenuSound (MENU_SOUND_INVOKED);
				SleepThreadUntil (FadeScreen (FadeAllToWhite, ONE_SECOND * 1)
						+ (ONE_SECOND * 2));
				if (uqm::systemStar(*system_).Index != CHMMR_DEFINED
						|| !matchWorld (system_,
								&uqm::detail::selectedDescriptor(*system_),
								1, MATCH_PLANET))
				{
					FadeScreen (FadeAllToColor, ONE_SECOND * 2);
				}
				else
				{
					story_.set(uqm::StoryFlag::ChmmrEmerging, 1);

					EncounterGroup = 0;
					GLOBAL (CurrentActivity) |= START_ENCOUNTER;

					PutGroupInfo (GROUPS_RANDOM, GROUP_SAVE_IP);
					ReinitQueue (&GLOBAL (ip_group_q));
					assert (CountLinks (&GLOBAL (npc_built_ship_q)) == 0);

					CloneShipFragment (CHMMR_SHIP,
							&GLOBAL (npc_built_ship_q), 0);
				}
				return DeviceStatus::SuccessNoSound;
			}
			break;
		case UTWIG_BOMB_DEVICE:
			story_.set(uqm::StoryFlag::UtwigBomb, 0);
			GLOBAL (CurrentActivity) &= ~IN_BATTLE;
			GLOBAL_SIS (CrewEnlisted) = (COUNT)~0;
			return DeviceStatus::Success;
		case ULTRON_0_DEVICE:
			break;
		case ULTRON_1_DEVICE:
			break;
		case ULTRON_2_DEVICE:
			break;
		case ULTRON_3_DEVICE:
			break;
		case MAIDENS_DEVICE:
			break;
		case TALKING_PET_DEVICE:
			NextActivity |= CHECK_LOAD; /* fake a load game */
			GLOBAL (CurrentActivity) |= START_ENCOUNTER;
			story_.set(uqm::StoryCounter::EncounterContext, 0);
			if (inHQSpace ())
			{
				if (!encounters_->empty())
				{
					story_.set(uqm::StoryFlag::ShipToCompel, 1);
                    if (encounters_->current() == encounters_->end())
                        encounters_->select(encounters_->begin());
				}
				GLOBAL (CurrentActivity) &= ~IN_BATTLE;

				SaveSisHyperState ();
			}
			else
			{
				EncounterGroup = 0;
				if (GetHeadLink (&GLOBAL (ip_group_q)))
				{
					story_.set(uqm::StoryFlag::ShipToCompel, 1);

					PutGroupInfo (GROUPS_RANDOM, GROUP_SAVE_IP);
					ReinitQueue (&GLOBAL (ip_group_q));
					assert (CountLinks (&GLOBAL (npc_built_ship_q)) == 0);
				}

				if (uqm::systemStar(*system_).Index == SAMATRA_DEFINED)
				{
					story_.set(uqm::StoryFlag::ReadyToConfuseUrquan, 1);
				}
				if (inOrbit())
					uqm::saveSolarSystemLocation(*system_);
			}
			return DeviceStatus::Success;
		case AQUA_HELIX_DEVICE:
			val = story_.read(uqm::StoryCounter::UltronCondition);
			if (val)
			{
				story_.set(uqm::StoryCounter::UltronCondition, val + 1);
				story_.set(uqm::StoryFlag::AquaHelixOnShip, 0);
				story_.set(uqm::StoryFlag::DiscussedUltron, 0);
				story_.set(uqm::StoryFlag::SupoxUltronHelp, 0);
				return DeviceStatus::Success;
			}
			break;
		case CLEAR_SPINDLE_DEVICE:
			val = story_.read(uqm::StoryCounter::UltronCondition);
			if (val)
			{
				story_.set(uqm::StoryCounter::UltronCondition, val + 1);
				story_.set(uqm::StoryFlag::ClearSpindleOnShip, 0);
				story_.set(uqm::StoryFlag::DiscussedUltron, 0);
				story_.set(uqm::StoryFlag::SupoxUltronHelp, 0);
				return DeviceStatus::Success;
			}
			break;
		case UMGAH_HYPERWAVE_DEVICE:
		case BURVIX_HYPERWAVE_DEVICE:
			if (useCaster())
				return DeviceStatus::Success;
			break;
		case TAALO_PROTECTOR_DEVICE:
			break;
		case EGG_CASING0_DEVICE:
		case EGG_CASING1_DEVICE:
		case EGG_CASING2_DEVICE:
			break;
		case SYREEN_SHUTTLE_DEVICE:
			break;
		case VUX_BEAST_DEVICE:
			break;
		case DESTRUCT_CODE_DEVICE:
			break;
		case PORTAL_SPAWNER_DEVICE:
			if (inHyperSpace ()
					&& GLOBAL_SIS (FuelOnBoard) >= PORTAL_FUEL_COST)
			{
				/* No DeltaSISGauges because the flagship picture
				 * is currently obscured.
				 */
				GLOBAL_SIS (FuelOnBoard) -= PORTAL_FUEL_COST;
				story_.set(uqm::StoryCounter::PortalCounter, 1);
				return DeviceStatus::Success;
			}
			break;
		case URQUAN_WARP_DEVICE:
			break;
		case LUNAR_BASE_DEVICE:
			break;
	}

	return DeviceStatus::Failure;
}

BOOLEAN
uqm::DeviceMenu::manipulate(MENU_STATE *pMS)
{
	auto *devState = static_cast<DeviceMenuState *>(pMS->privData);
	bool select, cancel, back, forward;
	bool pagefwd, pageback;
	
	select = PulsedInputState.menu[KEY_MENU_SELECT];
	cancel = PulsedInputState.menu[KEY_MENU_CANCEL];
	back = PulsedInputState.menu[KEY_MENU_UP] ||
			PulsedInputState.menu[KEY_MENU_LEFT];
	forward = PulsedInputState.menu[KEY_MENU_DOWN]
			|| PulsedInputState.menu[KEY_MENU_RIGHT];
	pagefwd = PulsedInputState.menu[KEY_MENU_PAGE_DOWN];
	pageback = PulsedInputState.menu[KEY_MENU_PAGE_UP];

	if (GLOBAL (CurrentActivity) & CHECK_ABORT)
		return FALSE;

	if (cancel)
	{
		return FALSE;
	}
	else if (select)
	{
		DeviceStatus status;

		status = devState->owner.invoke(devState->list[pMS->CurState]);
		if (status == DeviceStatus::Failure)
			PlayMenuSound (MENU_SOUND_FAILURE);
		else if (status == DeviceStatus::Success)
			PlayMenuSound (MENU_SOUND_INVOKED);

		return status == DeviceStatus::Failure ? TRUE : FALSE;
	}
	else
	{
		SIZE NewTop;
		SIZE NewState;

		NewTop = devState->topIndex;
		NewState = pMS->CurState;
		
		if (back)
			--NewState;
		else if (forward)
			++NewState;
		else if (pagefwd)
			NewState += MAX_VIS_DEVICES;
		else if (pageback)
			NewState -= MAX_VIS_DEVICES;

		if (NewState < 0)
			NewState = 0;
		else if (NewState >= static_cast<SIZE>(devState->list.size()))
			NewState = static_cast<SIZE>(devState->list.size()) - 1;

		if (NewState < NewTop || NewState >= NewTop + MAX_VIS_DEVICES)
			NewTop = NewState - NewState % MAX_VIS_DEVICES;

		if (NewState != pMS->CurState)
		{
			if (NewTop != devState->topIndex)
			{	// redraw the display
				devState->topIndex = NewTop;
				DrawDevices (devState, (COUNT)~0, NewState);
			}
			else
			{	// move selection to new device
				DrawDevices (devState, pMS->CurState, NewState);
			}
			pMS->CurState = NewState;
		}

		SleepThread (ONE_SECOND / 30);
	}

	return TRUE;
}

BOOLEAN
uqm::DeviceMenu::run()
{
	MENU_STATE MenuState{};
	DeviceMenuState DevicesState{*this, inventoryDevices(story_)};

	MenuState.privData = &DevicesState;


	if (DevicesState.list.empty())
		return FALSE;

	DrawDevices (&DevicesState, (COUNT)~0, MenuState.CurState);

    struct SoundBindings {
        MENU_SOUND_FLAGS arrows{}, select{};
        SoundBindings() { GetMenuSounds(&arrows, &select); }
        ~SoundBindings() { SetMenuSounds(arrows, select); }
    } sounds;
	SetMenuSounds (MENU_SOUND_ARROWS | MENU_SOUND_PAGEUP | MENU_SOUND_PAGEDOWN,
			MENU_SOUND_SELECT);

	MenuState.InputFunc = manipulate;
	DoInput (&MenuState, TRUE);


	if (GLOBAL_SIS (CrewEnlisted) != (COUNT)~0
			&& !(GLOBAL (CurrentActivity) & CHECK_ABORT))
	{
		ClearSISRect (DRAW_SIS_DISPLAY);

		if (!story_.read(uqm::StoryCounter::PortalCounter)
				&& !(GLOBAL (CurrentActivity) & START_ENCOUNTER)
				&& GLOBAL_SIS (CrewEnlisted) != (COUNT)~0)
			return TRUE;
	}
	
	return FALSE;
}


bool uqm::DeviceMenu::inOrbit() const {
    return system_ && uqm::systemLocation(*system_).orbit.has_value();
}

uqm::DeviceMenu::DeviceMenu(StoryState &story, SOLARSYS_STATE &system)
    : story_(story), system_(&system) {
    if (&systemStory(system) != &story)
        throw std::invalid_argument("device menu and exploration have different campaigns");
}
