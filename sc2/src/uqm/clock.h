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

#ifndef UQM_CLOCK_H_
#define UQM_CLOCK_H_

#include "libs/compiler.h"
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* Save projections only; the live clock and scheduler belong to GameSession. */
enum { START_YEAR = 2155, HYPERSPACE_CLOCK_RATE = 5, INTERPLANETARY_CLOCK_RATE = 30 };
typedef struct {
    BYTE day_index, month_index;
    COUNT year_index;
    SIZE tick_count, day_in_ticks;
} CLOCK_STATE;
typedef struct event {
    BYTE day_index, month_index;
    COUNT year_index;
    BYTE func_index;
} EVENT;
typedef enum { ABSOLUTE_EVENT = 0, RELATIVE_EVENT } EVENT_TYPE;

/* C adapters select the active campaign. Snapshots own their scalar values.
 * Scheduler iteration is for the gameplay thread, without intervening dispatch. */
CLOCK_STATE GetCampaignClockState(void);
size_t CampaignEventCount(void);
BOOLEAN ReadCampaignEvent(size_t index, EVENT *event);
void ClearCampaignEvents(void);
BOOLEAN RestoreCampaignEvent(const EVENT *event);
void ExportCampaignClock(void);
BOOLEAN ImportCampaignClock(void);
BOOLEAN ValidateEvent(EVENT_TYPE type, COUNT *month, COUNT *day, COUNT *year);
BOOLEAN AddEvent(EVENT_TYPE type, COUNT month, COUNT day, COUNT year, BYTE selector);
void MoveGameClockDays(COUNT days);

#if defined(__cplusplus)
}
#endif

#endif /* UQM_CLOCK_H_ */
