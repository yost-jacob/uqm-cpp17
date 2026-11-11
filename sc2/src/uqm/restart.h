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

#ifndef UQM_RESTART_H_
#define UQM_RESTART_H_

#include "libs/compiler.h"

#if defined(__cplusplus)
extern "C" {
#endif


typedef enum UQM_TITLE_CHOICE {
    UQM_TITLE_NEW_GAME, UQM_TITLE_LOAD_GAME, UQM_TITLE_ARCADE,
    UQM_TITLE_QUIT, UQM_TITLE_ATTRACT
} UQM_TITLE_CHOICE;
/* One menu visit only. Session selection and ownership belong to C++. */
extern UQM_TITLE_CHOICE RunTitleMenu (void);
/* Called before retiring the campaign whose ending is being displayed. */
extern void ShowCampaignEnding (void);

#if defined(__cplusplus)
}
#endif

#endif  /* UQM_RESTART_H_ */
