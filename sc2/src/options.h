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

/*
 * Eventually this should include all configuration stuff, 
 * for now there's few options which indicate 3do/pc flavors.
 */

#ifndef OPTIONS_H
#define OPTIONS_H

#include "port.h"
#include "game_configuration.h"
#include "libs/compiler.h"
#include "libs/uio.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define OPT_3DO 0x01
#define OPT_PC  0x02
#define OPT_ALL 0xFF

/* Transitional C lvalues; native application code uses context.configuration(). */
#define optWhichCoarseScan (GetGameConfiguration()->coarseScan)
#define optWhichMenu (GetGameConfiguration()->menuStyle)
#define optWhichFonts (GetGameConfiguration()->fonts)
#define optWhichIntro (GetGameConfiguration()->intro)
#define optWhichShield (GetGameConfiguration()->shield)
#define optSmoothScroll (GetGameConfiguration()->smoothScroll)
#define optMeleeScale (GetGameConfiguration()->meleeScale)
#define opt3doMusic (GetGameConfiguration()->music3do)
#define optRemixMusic (GetGameConfiguration()->remixMusic)
#define optSpeech (GetGameConfiguration()->speech)
#define optSubtitles (GetGameConfiguration()->subtitles)
#define optStereoSFX (GetGameConfiguration()->stereoSFX)
#define optKeepAspectRatio (GetGameConfiguration()->keepAspectRatio)
#define optGamma (GetGameConfiguration()->gamma)

#define GAMMA_SCALE 1000

extern uio_DirHandle *contentDir;
extern uio_DirHandle *configDir;
extern uio_DirHandle *saveDir;
extern uio_DirHandle *meleeDir;
extern char baseContentPath[PATH_MAX];

extern const char **optAddons;

#define input_templates (GetGameConfiguration()->inputTemplates)

void prepareContentDir (const char *contentDirName, const char *addonDirName, const char *execFile);
void prepareConfigDir (const char *configDirName);
void prepareMeleeDir (void);
void prepareSaveDir (void);
void prepareAddons (const char **addons);
void prepareShadowAddons (const char **addons);
void unprepareAllDirs (void);

BOOLEAN loadAddon (const char *addon);
int loadIndices (uio_DirHandle *baseDir);

bool setGammaCorrection (float gamma);

#if defined(__cplusplus)
}
#endif

#endif

