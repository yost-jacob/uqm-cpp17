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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef UQM_BATTLE_H_
#define UQM_BATTLE_H_

#include "options.h"
#include "libs/compiler.h"
#include "libs/timelib.h"

#if defined (NETPLAY)
typedef DWORD BattleFrameCounter;
#endif

#include "init.h"
		// For NUM_SIDES

#if defined(__cplusplus)
extern "C" {
#endif

// Compatibility borrow of the selected roster's availability/result counters.
BYTE *GetBattleCounters(void);
// C adapters for the selected session's one-shot scripted battle request.
BOOLEAN GetInstantVictory(void);
void SetInstantVictory(BOOLEAN value);
#if defined (NETPLAY)
BattleFrameCounter GetBattleFrameCount(void);
#endif
#ifdef NETPLAY
COUNT GetPlayerOrder (COUNT i);
#else
#	define GetPlayerOrder(i) (i)
#endif

#ifdef __cplusplus
inline constexpr auto BATTLE_FRAME_RATE = ONE_SECOND / 24;
#else
enum { BATTLE_FRAME_RATE = ONE_SECOND / 24 };
#endif

// Borrow the selected native scene's soundtrack. Call only during its visit;
// ownership and release belong to BattleScene, not this C request.
extern void BattleSong (BOOLEAN DoPlay);

#if defined(__cplusplus)
}
#endif

#endif  /* UQM_BATTLE_H_ */
