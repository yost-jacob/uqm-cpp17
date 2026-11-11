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

#include "conversation.hpp"
#include "game_state.hpp"
#include "mode_rendering.hpp"
#include <stdexcept>
#include "comm.h"

#include "build.h"
#include "commanim.h"
#include "commglue.h"
#include "controls.h"
#include "colors.h"
#include "sis.h"
#include "units.h"
#include "encount.h"
#include "starmap.h"
#include "endian_uqm.h"
#include "gamestr.h"
#include "options.h"
#include "oscill.h"
#include "save.h"
#include "settings.h"
#include "setup.h"
#include "sounds.h"
#include "nameref.h"
#include "uqmdebug.h"
#include "libs/graphics/gfx_common.h"
#include "libs/inplib.h"
#include "libs/sound/sound.h"
#include "libs/sound/trackplayer.h"
#include "libs/log.h"

#include <ctype.h>

using namespace uqm;
constexpr auto OSCILLOSCOPE_RATE = ONE_SECOND / 32;
constexpr auto COMM_ANIM_RATE = ONE_SECOND / 40;
constexpr auto FOREGROUND_VOL = NORMAL_VOLUME;
static int backgroundVolume() { return usingSpeech ? NORMAL_VOLUME / 2 : NORMAL_VOLUME; }
RECT CommWndRect = {{SIS_ORG_X, SIS_ORG_Y}, {0, 0}}; // Legacy credits layout boundary.
/* _count_lines - sees how many lines a given input string would take to
 * display given the line wrapping information
 */
int
uqm::ConversationRuntime::_count_lines(TEXT *pText)
{
	SIZE text_width;
	const char *pStr;
	int numLines = 0;
	bool eol;

	text_width = data_.AlienTextWidth;
	SetContextFont (data_.AlienFont);

	pStr = pText->pStr;
	do
	{
		++numLines;
		pText->pStr = pStr;
		eol = getLineWithinWidth (pText, &pStr, text_width, (COUNT)~0);
	} while (!eol);
	pText->pStr = pStr;

	return numLines;
}

// status == -1: draw highlighted player dialog option
// status == -2: draw non-highlighted player dialog option
// status == -4: use current context, and baseline from pTextIn
// status ==  1:  draw alien speech; subtitle cache is used
COORD
uqm::ConversationRuntime::add_text(int status, TEXT *pTextIn)
{
	COUNT maxchars, numchars;
	TEXT locText;
	TEXT *pText;
	SIZE leading;
	const char *pStr;
	SIZE text_width;
	int num_lines = 0;

	bool eol;
	CONTEXT OldContext = NULL;

	BatchGraphics ();

	maxchars = (COUNT)~0;
	if (status == 1)
	{
		if (last_subtitle == pTextIn->pStr)
		{
			// draws cached subtitle
			STAMP s;

			s.origin.x = 0;
			s.origin.y = 0;
			s.frame = resources_.textCacheFrame;
			DrawStamp (&s);
			UnbatchGraphics ();
			return last_baseline;
		}
		else
		{
			// draw to subtitle cache; prepare first
			OldContext = SetContext (resources_.textCache);
			ClearDrawable ();

			last_subtitle = pTextIn->pStr;
		}

		text_width = data_.AlienTextWidth;
		SetContextFont (data_.AlienFont);
		GetContextFontLeading (&leading);

		pText = pTextIn;
	}
	else if (GetContextFontLeading (&leading), status <= -4)
	{
		text_width = (SIZE) (SIS_SCREEN_WIDTH - 8 - (TEXT_X_OFFS << 2));

		pText = pTextIn;
	}
	else
	{
		text_width = (SIZE) (SIS_SCREEN_WIDTH - 8 - (TEXT_X_OFFS << 2));

		switch (status)
		{
			case -3:
				// Unknown. Never reached; color matches the background color.
				SetContextForeGroundColor (
						BUILD_COLOR (MAKE_RGB15 (0x00, 0x00, 0x14), 0x01));
				break;
			case -2:
				// Not highlighted dialog options.
				SetContextForeGroundColor (COMM_PLAYER_TEXT_NORMAL_COLOR);
				break;
			case -1:
				// Currently highlighted dialog option.
				SetContextForeGroundColor (COMM_PLAYER_TEXT_HIGHLIGHT_COLOR);
				break;
		}

		maxchars = pTextIn->CharCount;
		locText = *pTextIn;
		locText.baseline.x -= 8;
		locText.CharCount = (COUNT)~0;
		locText.pStr = STR_BULLET;
		font_DrawText (&locText);

		locText = *pTextIn;
		pText = &locText;
		pText->baseline.y -= leading;
	}

	numchars = 0;
	pStr = pText->pStr;

	if (status > 0 && (data_.AlienTextValign &
			(VALIGN_MIDDLE | VALIGN_BOTTOM)))
	{
		num_lines = _count_lines(pText);
		if (data_.AlienTextValign == VALIGN_BOTTOM)
			pText->baseline.y -= (leading * num_lines);
		else if (data_.AlienTextValign == VALIGN_MIDDLE)
			pText->baseline.y -= ((leading * num_lines) / 2);
		if (pText->baseline.y < 0)
			pText->baseline.y = 0;
	}

	do
	{
		pText->pStr = pStr;
		pText->baseline.y += leading;

		eol = getLineWithinWidth (pText, &pStr, text_width, maxchars);

		maxchars -= pText->CharCount;
		if (maxchars != 0)
			--maxchars;
		numchars += pText->CharCount;

		if (status <= 0)
		{
			// Player dialog option or (status == -4) other non-alien
			// text.
			if (pText->baseline.y < SIS_SCREEN_HEIGHT)
				font_DrawText (pText);

			if (status < -4 && pText->baseline.y >= -status - 10)
			{
				// Never actually reached. Status is never <-4.
				++pStr;
				break;
			}
		}
		else
		{
			// Alien speech
			font_DrawTracedText (pText,
					data_.AlienTextFColor, data_.AlienTextBColor);
		}
	} while (!eol && maxchars);
	pText->pStr = pStr;

	if (status == 1)
	{
		STAMP s;

		// We were drawing to cache -- flush to screen
		SetContext (OldContext);
		s.origin.x = s.origin.y = 0;
		s.frame = resources_.textCacheFrame;
		DrawStamp (&s);

		last_baseline = pText->baseline.y;
	}

	UnbatchGraphics ();
	return (pText->baseline.y);
}

// This function calculates how much of a string can be fitted within
// a specific width, up to a newline or terminating \0.
// pText is the text to be fitted. pText->CharCount will be set to the
// number of characters that fitted.
// startNext will be filled with the start of the first word that
// doesn't fit in one line, or if an entire line fits, to the character
// past the newline, or if the entire string fits, to the end of the
// string.
// maxWidth is the maximum number of pixels that a line may be wide
//   ASSUMPTION: there are no words in the text wider than maxWidth
// maxChars is the maximum number of characters (not bytes) that are to
// be fitted.
// TRUE is returned if a complete line fitted
// FALSE otherwise
BOOLEAN
getLineWithinWidth(TEXT *pText, const char **startNext,
		SIZE maxWidth, COUNT maxChars)
{
	bool eol;
			// The end of the line of text has been reached.
	bool done;
			// We cannot add any more words.
	RECT rect;
	COUNT oldCount;
	const char *ptr;
	const char *wordStart;
	UniChar ch;
	COUNT charCount;

	//GetContextClipRect (&rect);

	eol = FALSE;
	done = FALSE;
	oldCount = 1;
	charCount = 0;
	ch = '\0';
	ptr = pText->pStr;
	for (;;)
	{
		wordStart = ptr;

		// Scan one word.
		for (;;)
		{
			if (*ptr == '\0')
			{
				eol = TRUE;
				done = TRUE;
				break;
			}
			ch = getCharFromString (&ptr);
			eol = ch == '\0' || ch == '\n' || ch == '\r';
			done = eol || charCount >= maxChars;
			if (done || ch == ' ')
				break;
			charCount++;
		}

		oldCount = pText->CharCount;
		pText->CharCount = charCount;
		TextRect (pText, &rect, NULL);

		if (rect.extent.width >= maxWidth)
		{
			pText->CharCount = oldCount;
			*startNext = wordStart;
			return FALSE;
		}

		if (done)
		{
			*startNext = ptr;
			return eol ? TRUE : FALSE;
		}
		charCount++;
				// For the space in between words.
	}
}

void
uqm::ConversationRuntime::DrawSISComWindow()
{
	CONTEXT OldContext;

	if (LOBYTE (GLOBAL (CurrentActivity)) != WON_LAST_BATTLE)
	{
		RECT r;

		OldContext = SetContext (SpaceContext);

		r.corner.x = 0;
		r.corner.y = SLIDER_Y + SLIDER_HEIGHT;
		r.extent.width = SIS_SCREEN_WIDTH;
		r.extent.height = SIS_SCREEN_HEIGHT - r.corner.y;
		SetContextForeGroundColor (COMM_PLAYER_BACKGROUND_COLOR);
		DrawFilledRectangle (&r);

		SetContext (OldContext);
	}
}





void
uqm::ConversationRuntime::RefreshResponses(ENCOUNTER_STATE *pES)
{
	COORD y;
	BYTE response;
	SIZE leading;
	STAMP s;

	SetContext (SpaceContext);
	GetContextFontLeading (&leading);
	BatchGraphics ();

	DrawSISComWindow ();
	y = SLIDER_Y + SLIDER_HEIGHT + 1;
	for (response = pES->top_response; response < pES->num_responses;
			++response)
	{
		pES->response_list[response].response_text.baseline.x = TEXT_X_OFFS + 8;
		pES->response_list[response].response_text.baseline.y = y + leading;
		pES->response_list[response].response_text.align = ALIGN_LEFT;
		if (response == pES->cur_response)
			y = add_text (-1, &pES->response_list[response].response_text);
		else
			y = add_text (-2, &pES->response_list[response].response_text);
	}

	if (pES->top_response)
	{
		s.origin.y = SLIDER_Y + SLIDER_HEIGHT + 1;
		s.frame = SetAbsFrameIndex (ActivityFrame, 6);
	}
	else if (y > SIS_SCREEN_HEIGHT)
	{
		s.origin.y = SIS_SCREEN_HEIGHT - 2;
		s.frame = SetAbsFrameIndex (ActivityFrame, 7);
	}
	else
		s.frame = 0;
	if (s.frame)
	{
		RECT r;

		GetFrameRect (s.frame, &r);
		s.origin.x = SIS_SCREEN_WIDTH - r.extent.width - 1;
		DrawStamp (&s);
	}

	UnbatchGraphics ();
}

void
uqm::ConversationRuntime::FeedbackPlayerPhrase(UNICODE *pStr)
{
	SetContext (SpaceContext);

	BatchGraphics ();
	DrawSISComWindow ();
	if (pStr[0])
	{
		TEXT ct;

		ct.baseline.x = SIS_SCREEN_WIDTH >> 1;
		ct.baseline.y = SLIDER_Y + SLIDER_HEIGHT + 13;
		ct.align = ALIGN_CENTER;
		ct.CharCount = (COUNT)~0;

		ct.pStr = GAME_STRING (FEEDBACK_STRING_BASE);
				// "(In response to your statement)"
		SetContextForeGroundColor (COMM_RESPONSE_INTRO_TEXT_COLOR);
		font_DrawText (&ct);

		ct.baseline.y += 16;
		SetContextForeGroundColor (COMM_FEEDBACK_TEXT_COLOR);
		ct.pStr = pStr;
		add_text (-4, &ct);
	}
	UnbatchGraphics ();
}

void
uqm::ConversationRuntime::InitSpeechGraphics()
{
	InitOscilloscope (SetAbsFrameIndex (ActivityFrame, 9));

	InitSlider (0, SLIDER_Y, SIS_SCREEN_WIDTH,
			SetAbsFrameIndex (ActivityFrame, 5),
			SetAbsFrameIndex (ActivityFrame, 2));
}

void
uqm::ConversationRuntime::UpdateSpeechGraphics()
{
	auto &NextTime = nextSpeechFrame_;
	CONTEXT OldContext;

	if (GetTimeCounter () < NextTime)
		return; // too early

	NextTime = GetTimeCounter () + OSCILLOSCOPE_RATE;

	OldContext = SetContext (RadarContext);
	DrawOscilloscope ();
	SetContext (SpaceContext);
	DrawSlider ();
	SetContext (OldContext);
}

void
uqm::ConversationRuntime::UpdateAnimations(bool paused)
{
	auto &NextTime = nextAnimationFrame_;
	CONTEXT OldContext;
	BOOLEAN change;

	if (GetTimeCounter () < NextTime)
		return; // too early

	NextTime = GetTimeCounter () + COMM_ANIM_RATE;

	OldContext = SetContext (resources_.animation);
	BatchGraphics ();
	// Advance and draw ambient, transit and talk animations
	change = animations_.ProcessCommAnimations(clear_subtitles, paused ? TRUE : FALSE);
	if (change || clear_subtitles)
		RedrawSubtitles ();
	UnbatchGraphics ();
	clear_subtitles = FALSE;
	SetContext (OldContext);
}

void
uqm::ConversationRuntime::UpdateCommGraphics()
{
	UpdateAnimations (false);
	UpdateSpeechGraphics ();
}

// Derived from INPUT_STATE_DESC
BOOLEAN
uqm::ConversationRuntime::DoTalkSegue(TALKING_STATE *pTS)
{
	bool left = false;
	bool right = false;
	COUNT curTrack;

	if (exiting())
	{
		pTS->ended = true;
		return FALSE;
	}

	if (PulsedInputState.menu[KEY_MENU_CANCEL])
	{
		JumpTrack ();
		pTS->ended = true;
		return FALSE;
	}

	if (optSmoothScroll == OPT_PC)
	{
		left = PulsedInputState.menu[KEY_MENU_LEFT] != 0;
		right = PulsedInputState.menu[KEY_MENU_RIGHT] != 0;
	}
	else if (optSmoothScroll == OPT_3DO)
	{
		left = CurrentInputState.menu[KEY_MENU_LEFT] != 0;
		right = CurrentInputState.menu[KEY_MENU_RIGHT] != 0;
	}

#if DEMO_MODE || CREATE_JOURNAL
	left = false;
	right = false;
#endif

	if (right)
	{
		SetSliderImage (SetAbsFrameIndex (ActivityFrame, 3));
		if (optSmoothScroll == OPT_PC)
			FastForward_Page ();
		else if (optSmoothScroll == OPT_3DO)
			FastForward_Smooth ();
		pTS->seeking = true;
	}
	else if (left || pTS->rewind)
	{
		pTS->rewind = false;
		SetSliderImage (SetAbsFrameIndex (ActivityFrame, 4));
		if (optSmoothScroll == OPT_PC)
			FastReverse_Page ();
		else if (optSmoothScroll == OPT_3DO)
			FastReverse_Smooth ();
		pTS->seeking = true;
	}
	else if (pTS->seeking)
	{
		// This is only done once the seeking is over (in the smooth
		// scroll case, once the user releases the seek button)
		pTS->seeking = false;
		SetSliderImage (SetAbsFrameIndex (ActivityFrame, 2));
	}
	else
	{
		// This used to have a buggy guard condition, which
		// would cause the animations to remain paused in a couple cases
		// after seeking back to the beginning.
		// Broken cases were: Syreen "several hours later" and Starbase
		// VUX Beast analysis by the scientist.
		CheckSubtitles ();
	}

	// XXX: When seeking, all animations (talking and ambient) stop
	// progressing. This is an original 3DO behavior, and I see no
	// reason why the animations cannot continue while seeking.
	UpdateAnimations (pTS->seeking);
	UpdateSpeechGraphics ();

	curTrack = PlayingTrack ();
	pTS->ended = !pTS->seeking && !curTrack;

	SleepThreadUntil (pTS->NextTime);
	// Need a high enough framerate for 3DO smooth seeking
	pTS->NextTime = GetTimeCounter () + ONE_SECOND / 60;

	return pTS->seeking || (curTrack && curTrack <= pTS->waitTrack) ? TRUE : FALSE;
}

void
uqm::ConversationRuntime::runCommAnimFrame()
{
	UpdateCommGraphics ();
	SleepThread (COMM_ANIM_RATE);
}

BOOLEAN
uqm::ConversationRuntime::TalkSegue(COUNT wait_track)
{
	TALKING_STATE talkingState{};
    talkingState.owner = this;

	// Transition animation to talking state, if necessary
	if (wantTalkingAnim(data_) && haveTalkingAnim(data_))
	{
		if (haveTransitionAnim(data_))
			setRunIntroAnim(data_);

		setRunTalkingAnim(data_);

		// wait until the transition finishes
		while (runningIntroAnim(data_) && !exiting())
			runCommAnimFrame ();
	}

	if (wait_track == 0)
	{	// Restarting with a rewind
		wait_track = WAIT_TRACK_ALL;
		talkingState.rewind = true;
	}
	else if (!PlayingTrack ())
	{	// initial start of player
		PlayTrack ();
		assert (PlayingTrack ());
	}

	// Run the talking controls
	SetMenuSounds (MENU_SOUND_NONE, MENU_SOUND_NONE);
	talkingState.InputFunc = [](TALKING_STATE *state) { return state->owner->DoTalkSegue(state); };
	talkingState.waitTrack = wait_track;
	DoInput (&talkingState, FALSE);

	ClearSubtitles ();

	if (talkingState.ended)
	{	// reached the end; set STOP icon
		SetSliderImage (SetAbsFrameIndex (ActivityFrame, 8));
	}

	// transition back to silent, if necessary
	if (runningTalkingAnim(data_))
		setStopTalkingAnim(data_);

	// Wait until the animation task stops "talking"
	while (runningTalkingAnim(data_) && !exiting())
		runCommAnimFrame ();

	return talkingState.ended ? TRUE : FALSE;
}

void
uqm::ConversationRuntime::CommIntroTransition()
{
	if (curIntroMode == CIM_CROSSFADE_SCREEN)
	{
		ScreenTransition (3, NULL);
		UnbatchGraphics ();
	}
	else if (curIntroMode == CIM_CROSSFADE_SPACE)
	{
		RECT r;
		r.corner.x = SIS_ORG_X;
		r.corner.y = SIS_ORG_Y;
		r.extent.width = SIS_SCREEN_WIDTH;
		r.extent.height = SIS_SCREEN_HEIGHT;
		ScreenTransition (3, &r);
		UnbatchGraphics ();
	}
	else if (curIntroMode == CIM_CROSSFADE_WINDOW)
	{
		ScreenTransition (3, &window_);
		UnbatchGraphics ();
	}
	else if (curIntroMode == CIM_FADE_IN_SCREEN)
	{
		UnbatchGraphics ();
		FadeScreen (FadeAllToColor, fadeTime);
	}
	else
	{	// Uknown transition
		// Have to unbatch anyway or no more graphics, ever
		UnbatchGraphics ();
		assert (0 && "Unknown comm intro transition");
	}

	// Reset the mode for next time. Everything that needs a
	// different one will let us know.
	curIntroMode = CIM_DEFAULT;
}

void
uqm::ConversationRuntime::AlienTalkSegue(COUNT wait_track)
{
	// this skips any talk segues that follow an aborted one
	if (exiting() || TalkingFinished)
		return;

	if (!(&input_)->Initialized)
	{
		InitSpeechGraphics ();
		SetColorMap (GetColorMapAddress (data_.AlienColorMap));
		SetContext (resources_.animation);
		animations_.DrawAlienFrame(NULL, 0, TRUE);
		UpdateSpeechGraphics ();
		CommIntroTransition ();

		(&input_)->Initialized = TRUE;

		PlayMusic (data_.AlienSong, TRUE, 1);
		SetMusicVolume (backgroundVolume());

		animations_.InitCommAnimations();

		LastActivity &= ~CHECK_LOAD;
	}

	TalkingFinished = TalkSegue (wait_track);
	if (TalkingFinished)
		FadeMusic (FOREGROUND_VOL, ONE_SECOND);
}


BOOLEAN
uqm::ConversationRuntime::DoConvSummary(SUMMARY_STATE *pSS)
{
#define DELTA_Y_SUMMARY 8
#define MAX_SUMM_ROWS ((SIS_SCREEN_HEIGHT - SLIDER_Y - SLIDER_HEIGHT) \
			/ DELTA_Y_SUMMARY) - 1

	if (!pSS->Initialized)
	{
		pSS->PrintNext = TRUE;
		pSS->NextSub = GetFirstTrackSubtitle ();
		pSS->LeftOver = NULL;
		pSS->InputFunc = [](SUMMARY_STATE *state) { return state->owner->DoConvSummary(state); };
		pSS->Initialized = TRUE;
		DoInput (pSS, FALSE);
	}
	else if (exiting())
	{
		return FALSE; // bail out
	}
	else if (PulsedInputState.menu[KEY_MENU_SELECT]
			|| PulsedInputState.menu[KEY_MENU_CANCEL]
			|| PulsedInputState.menu[KEY_MENU_RIGHT])
	{
		if (pSS->NextSub)
		{	// we want the next page
			pSS->PrintNext = TRUE;
		}
		else
		{	// no more, we are done
			return FALSE;
		}
	}
	else if (pSS->PrintNext)
	{	// print the next page
		RECT r;
		TEXT t;
		int row;

		r.corner.x = 0;
		r.corner.y = 0;
		r.extent.width = SIS_SCREEN_WIDTH;
		r.extent.height = SIS_SCREEN_HEIGHT - SLIDER_Y - SLIDER_HEIGHT + 2;

		SetContext (resources_.animation);
		SetContextForeGroundColor (COMM_HISTORY_BACKGROUND_COLOR);
		DrawFilledRectangle (&r);

		SetContextForeGroundColor (COMM_HISTORY_TEXT_COLOR);

		r.extent.width -= 2 + 2;
		t.baseline.x = 2;
		t.align = ALIGN_LEFT;
		t.baseline.y = DELTA_Y_SUMMARY;
		SetContextFont (TinyFont);

		for (row = 0; row < MAX_SUMM_ROWS && pSS->NextSub;
				++row, pSS->NextSub = GetNextTrackSubtitle (pSS->NextSub))
		{
			const char *next = NULL;

			if (pSS->LeftOver)
			{	// some text left from last subtitle
				t.pStr = pSS->LeftOver;
				pSS->LeftOver = NULL;
			}
			else
			{
				t.pStr = GetTrackSubtitleText (pSS->NextSub);
				if (!t.pStr)
					continue;
			}

			t.CharCount = (COUNT)~0;
			for ( ; row < MAX_SUMM_ROWS &&
					!getLineWithinWidth (&t, &next, r.extent.width, (COUNT)~0);
					++row)
			{
				font_DrawText (&t);
				t.baseline.y += DELTA_Y_SUMMARY;
				t.pStr = next;
				t.CharCount = (COUNT)~0;
			}

			if (row >= MAX_SUMM_ROWS)
			{	// no more space on screen, but some text left over
				// from the current subtitle
				pSS->LeftOver = next;
				break;
			}

			// this subtitle fit completely
			font_DrawText (&t);
			t.baseline.y += DELTA_Y_SUMMARY;
		}

		if (row >= MAX_SUMM_ROWS && (pSS->NextSub || pSS->LeftOver))
		{	// draw *MORE*
			TEXT mt;
			UNICODE buffer[80];

			mt.baseline.x = SIS_SCREEN_WIDTH >> 1;
			mt.baseline.y = t.baseline.y;
			mt.align = ALIGN_CENTER;
			snprintf (buffer, sizeof (buffer), "%s%s%s", // "MORE"
					STR_MIDDLE_DOT, GAME_STRING (FEEDBACK_STRING_BASE + 1),
					STR_MIDDLE_DOT);
			mt.pStr = buffer;
			SetContextForeGroundColor (COMM_MORE_TEXT_COLOR);
			font_DrawText (&mt);
		}


		pSS->PrintNext = FALSE;
	}
	else
	{
		SleepThread (ONE_SECOND / 20);
	}

	return TRUE; // keep going
}

// Called when the player presses the select button on a response.
void
uqm::ConversationRuntime::SelectResponse(ENCOUNTER_STATE *pES)
{
	TEXT *response_text =
			&pES->response_list[pES->cur_response].response_text;
	utf8StringCopy (pES->phrase_buf.data(), pES->phrase_buf.size(),
			response_text->pStr);
	FeedbackPlayerPhrase (pES->phrase_buf.data());
	StopTrack ();
	ClearSubtitles ();
	SetSliderImage (SetAbsFrameIndex (ActivityFrame, 2));

	FadeMusic (backgroundVolume(), ONE_SECOND);

	TalkingFinished = FALSE;
	pES->num_responses = 0;
	(*pES->response_list[pES->cur_response].response_func)
			(pES->response_list[pES->cur_response].response_ref);
}

// Called when the player presses the cancel button in comm.
void
uqm::ConversationRuntime::SelectConversationSummary(ENCOUNTER_STATE *pES)
{
	SUMMARY_STATE SummaryState{};
    SummaryState.owner = this;

	if (pES)
		FeedbackPlayerPhrase (pES->phrase_buf.data());

	SummaryState.Initialized = FALSE;
	DoConvSummary (&SummaryState);

	if (pES)
		RefreshResponses (pES);
	clear_subtitles = TRUE;
}

void
uqm::ConversationRuntime::SelectReplay(ENCOUNTER_STATE *pES)
{
	FadeMusic (backgroundVolume(), ONE_SECOND);
	if (pES)
		FeedbackPlayerPhrase (pES->phrase_buf.data());

	TalkSegue (0);
}

void
uqm::ConversationRuntime::PlayerResponseInput(ENCOUNTER_STATE *pES)
{
	BYTE response;

	if (pES->top_response == (BYTE)~0)
	{
		pES->top_response = 0;
		RefreshResponses (pES);
	}

	if (PulsedInputState.menu[KEY_MENU_SELECT])
	{
		SelectResponse (pES);
	}
	else if (PulsedInputState.menu[KEY_MENU_CANCEL] &&
			LOBYTE (GLOBAL (CurrentActivity)) != WON_LAST_BATTLE)
	{
		SelectConversationSummary (pES);
	}
	else
	{
		response = pES->cur_response;
		if (PulsedInputState.menu[KEY_MENU_LEFT])
		{
			SelectReplay (pES);

			if (!exiting())
			{
				RefreshResponses (pES);
				FadeMusic (FOREGROUND_VOL, ONE_SECOND);
			}
		}
		else if (PulsedInputState.menu[KEY_MENU_UP])
			response = (BYTE)((response + (BYTE)(pES->num_responses - 1))
					% pES->num_responses);
		else if (PulsedInputState.menu[KEY_MENU_DOWN])
			response = (BYTE)((BYTE)(response + 1) % pES->num_responses);

		if (response != pES->cur_response)
		{
			COORD y;

			BatchGraphics ();
			add_text (-2,
					&pES->response_list[pES->cur_response].response_text);

			pES->cur_response = response;

			y = add_text (-1,
					&pES->response_list[pES->cur_response].response_text);
			if (response < pES->top_response)
			{
				pES->top_response = 0;
				RefreshResponses (pES);
			}
			else if (y > SIS_SCREEN_HEIGHT)
			{
				pES->top_response = response;
				RefreshResponses (pES);
			}
			UnbatchGraphics ();
		}

		UpdateCommGraphics ();

		SleepThreadUntil (pES->NextTime);
		pES->NextTime = GetTimeCounter () + COMM_ANIM_RATE;
	}
}

// Derived from INPUT_STATE_DESC
BOOLEAN
uqm::ConversationRuntime::DoLastReplay(LAST_REPLAY_STATE *pLRS)
{
	if (exiting())
		return FALSE;

	if (GetTimeCounter () > pLRS->TimeOut)
		return FALSE; // timed out and done

	if (PulsedInputState.menu[KEY_MENU_CANCEL] &&
			LOBYTE (GLOBAL (CurrentActivity)) != WON_LAST_BATTLE)
	{
		FadeMusic (backgroundVolume(), ONE_SECOND);
		SelectConversationSummary (NULL);
		pLRS->TimeOut = FadeMusic (0, ONE_SECOND * 2) + ONE_SECOND / 60;
	}
	else if (PulsedInputState.menu[KEY_MENU_LEFT])
	{
		SelectReplay (NULL);
		pLRS->TimeOut = FadeMusic (0, ONE_SECOND * 2) + ONE_SECOND / 60;
	}

	UpdateCommGraphics ();

	SleepThreadUntil (pLRS->NextTime);
	pLRS->NextTime = GetTimeCounter () + COMM_ANIM_RATE;

	return TRUE;
}

BOOLEAN
uqm::ConversationRuntime::DoCommunication(ENCOUNTER_STATE *pES)
{
    if (exiting()) { stopPlayback(); return FALSE; }
	SetMenuSounds (MENU_SOUND_UP | MENU_SOUND_DOWN, MENU_SOUND_SELECT);

	// First, finish playing all queued tracks if not done yet
	if (!TalkingFinished)
	{
		AlienTalkSegue (WAIT_TRACK_ALL);
		return TRUE;
	}

	if (exiting())
		;
	else if (pES->num_responses == 0)
	{
		// The player doesn't get a chance to say anything,
		// but can still review alien's last phrases.
		LAST_REPLAY_STATE replayState{};
    replayState.owner = this;
		replayState.TimeOut = FadeMusic (0, ONE_SECOND * 3) + ONE_SECOND / 60;
		replayState.InputFunc = [](LAST_REPLAY_STATE *state) { return state->owner->DoLastReplay(state); };
		DoInput (&replayState, FALSE);
	}
	else
	{
		PlayerResponseInput (pES);
		return TRUE;
	}

    stopPlayback();

	return FALSE;
}

void
uqm::ConversationRuntime::DoResponsePhrase(RESPONSE_REF R, RESPONSE_FUNC response_func,
		UNICODE *ConstructStr)
{
	ENCOUNTER_STATE *pES = (&input_);
	RESPONSE_ENTRY *pEntry;

	if (exiting())
		return;

	if (pES->num_responses == 0)
	{
		pES->cur_response = 0;
		pES->top_response = (BYTE)~0;
	}

    if (pES->num_responses >= MaxResponses) {
        throw std::out_of_range("conversation response capacity exceeded");
    }
	pEntry = &pES->response_list[pES->num_responses];
	pEntry->response_ref = R;
	pEntry->response_text.pStr = ConstructStr;
	if (pEntry->response_text.pStr)
		pEntry->response_text.CharCount = (COUNT)~0;
	else
	{
		STRING locString;

		locString = SetAbsStringTableIndex (data_.ConversationPhrases,
				(COUNT) (R - 1));
		pEntry->response_text.pStr =
				(UNICODE *) GetStringAddress (locString);
		pEntry->response_text.CharCount = GetStringLength (locString);
//#define BVT_PROBLEM
#ifdef BVT_PROBLEM
		if (pEntry->response_text.pStr[pEntry->response_text.CharCount - 1]
				== '\0')
			--pEntry->response_text.CharCount;
#endif /* BVT_PROBLEM */
	}
	pEntry->response_func = response_func;
	++pES->num_responses;
}

void
uqm::ConversationRuntime::run()
{
    if (started_) throw std::logic_error("conversation cannot be run twice");
    started_ = true;
    oldContext_ = GetContext();
    GetMenuSounds(&oldArrows_, &oldSelect_);
    binding_.emplace(resources_.script);
    const auto intro = takeConversationIntro();
    setIntro(intro.mode, intro.duration);
    auto &ES = input_;
    ES.owner = this;
    MUSIC_REF SongRef = nullptr;
    Color TextBack = {0, 0, 0x84, 0};
    window_ = CommWndRect; // Legacy credits supply the outer presentation rect.
	TalkingFinished = FALSE;

	ES.InputFunc = [](ENCOUNTER_STATE *state) { return state->owner->DoCommunication(state); };
	resources_.playerFont = LoadFont (PLAYER_FONT);

	data_.AlienFrame = CaptureDrawable (
			LoadGraphic (data_.AlienFrameRes));
	data_.AlienFont = LoadFont (data_.AlienFontRes);
	data_.AlienColorMap = CaptureColorMap (
			LoadColorMap (data_.AlienColorMapRes));
	if ((data_.AlienSongFlags & LDASF_USE_ALTERNATE)
			&& data_.AlienAltSongRes)
		SongRef = LoadMusic (data_.AlienAltSongRes);
	if (SongRef)
		data_.AlienSong = SongRef;
	else
		data_.AlienSong = LoadMusic (data_.AlienSongRes);

	data_.ConversationPhrases = CaptureStringTable (
			LoadStringTable (data_.ConversationPhrasesRes));

    if (!resources_.playerFont || !data_.AlienFrame || !data_.AlienFont || !data_.ConversationPhrases) {
        throw std::runtime_error("could not load required conversation resources");
    }
	SubtitleText.baseline = data_.AlienTextBaseline;
	SubtitleText.align = data_.AlienTextAlign;


	// init subtitle cache context
	resources_.textCache = CreateContext ("Conversation.TextCache");
	resources_.textCacheFrame = CaptureDrawable (
			CreateDrawable (WANT_PIXMAP | WANT_ALPHA, SIS_SCREEN_WIDTH,
			SIS_SCREEN_HEIGHT - SLIDER_Y - SLIDER_HEIGHT + 2, 1));
    if (!resources_.textCache || !resources_.textCacheFrame) {
        throw std::runtime_error("could not create conversation subtitle cache");
    }
	SetContext (resources_.textCache);
	SetContextFGFrame (resources_.textCacheFrame);
	SetContextBackGroundColor (TextBack);
	ClearDrawable ();

	ES.phrase_buf[0] = '\0';

	SetContext (SpaceContext);
	oldSpaceFont_ = SetContextFont (resources_.playerFont);
    fontBound_ = true;

	{
		RECT r;

		resources_.animation = CreateContext ("Conversation.Animation");
        if (!resources_.animation) throw std::runtime_error("could not create conversation animation context");
		SetContext (resources_.animation);
		SetContextFGFrame (Screen);
		GetFrameRect (data_.AlienFrame, &r);
		r.extent.width = SIS_SCREEN_WIDTH;
		window_.extent = r.extent;

		SetTransitionSource (NULL);
		BatchGraphics ();
		if (LOBYTE (GLOBAL (CurrentActivity)) == WON_LAST_BATTLE)
		{
			r.corner = window_.corner;
			SetContextClipRect (&r);
		}
		else
		{
			r.corner.x = SIS_ORG_X;
			r.corner.y = SIS_ORG_Y;
			SetContextClipRect (&r);
			window_.corner = r.corner;

			DrawSISFrame ();
			// TODO: find a better way to do this, perhaps set the titles
			// forward from callers.
			if (story_.read(StoryCounter::EncounterContext) == (BYTE)~0
					&& story_.read(StoryFlag::StarbaseAccessGranted))
			{	// Talking to allied Starbase
				DrawSISMessage (GAME_STRING (STARBASE_STRING_BASE + 1));
						// "Starbase Commander"
				DrawSISTitle (GAME_STRING (STARBASE_STRING_BASE + 0));
						// "Starbase"
			}
			else
			{	// Default titles: star name + planet name
				DrawSISMessage (NULL);
				DrawSISTitle (GLOBAL_SIS (PlanetName));
			}
		}

		CommWndRect = window_; // Compatibility copy for the credits compositor.
		DrawSISComWindow ();
	}


	LastActivity |= CHECK_LOAD; /* prevent spurious input */
    playback_ = true;
	(*data_.init_encounter_func) ();
	DoInput (&ES, FALSE);
    finishScript();
}



void
uqm::ConversationRuntime::RedrawSubtitles()
{
	TEXT t;

	if (!optSubtitles)
		return;

	if (SubtitleText.pStr)
	{
		t = SubtitleText;
		add_text (1, &t);
	}
}

void
uqm::ConversationRuntime::ClearSubtitles()
{
	clear_subtitles = TRUE;
	last_subtitle = NULL;
	SubtitleText.pStr = NULL;
	SubtitleText.CharCount = 0;
}

void
uqm::ConversationRuntime::CheckSubtitles()
{
	const UNICODE *pStr;
	POINT baseline;
	TEXT_ALIGN align;

	pStr = GetTrackSubtitle ();
	baseline = data_.AlienTextBaseline;
	align = data_.AlienTextAlign;

	if (pStr != SubtitleText.pStr ||
			SubtitleText.baseline.x != baseline.x ||
			SubtitleText.baseline.y != baseline.y ||
			SubtitleText.align != align)
	{	// Subtitles changed
		clear_subtitles = TRUE;
		// Baseline may be updated by the ZFP
		SubtitleText.baseline = baseline;
		SubtitleText.align = align;
		// Make a note in the logs if the update was multiframe
		if (SubtitleText.pStr == pStr)
		{
			log_add (log_Warning, "Dialog text and location changed out of sync");
		}

		SubtitleText.pStr = pStr;
		// may have been cleared too
		if (pStr)
			SubtitleText.CharCount = (COUNT)~0;
		else
			SubtitleText.CharCount = 0;
	}
}



extern "C" void AlienTalkSegue(COUNT track) { uqm::activeConversation().AlienTalkSegue(track); }
extern "C" void DoResponsePhrase(RESPONSE_REF response, RESPONSE_FUNC handler, UNICODE *text) {
    uqm::activeConversation().DoResponsePhrase(response, handler, text);
}
extern "C" void EnableTalkingAnim(BOOLEAN enable) { uqm::activeConversation().enableTalking(enable != FALSE); }

std::unique_ptr<uqm::ModeSuspension> uqm::ConversationRuntime::suspend() {
    return suspendDrawingBindings({SpaceContext, RadarContext, resources_.animation, resources_.textCache});
}
void uqm::ConversationRuntime::resume() {
    if (!started_ || session_.menuExit() || (GLOBAL(CurrentActivity) & (CHECK_ABORT | CHECK_LOAD))) return;
    const auto oldContext = SetContext(resources_.animation);
    animations_.redraw();
    clear_subtitles = TRUE;
    RedrawSubtitles();
    SetContext(SpaceContext);
    DrawSISComWindow();
    if (input_.num_responses) RefreshResponses(&input_);
    SetContext(oldContext);
}
