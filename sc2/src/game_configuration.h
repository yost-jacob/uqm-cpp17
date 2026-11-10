#ifndef UQM_GAME_CONFIGURATION_H_
#define UQM_GAME_CONFIGURATION_H_
#include "libs/compiler.h"
/* Plain C projection of application settings, never part of a campaign save. */
typedef enum {
    CONTROL_TEMPLATE_KB_1, CONTROL_TEMPLATE_KB_2, CONTROL_TEMPLATE_KB_3,
    CONTROL_TEMPLATE_JOY_1, CONTROL_TEMPLATE_JOY_2, CONTROL_TEMPLATE_JOY_3,
    NUM_TEMPLATES
} CONTROL_TEMPLATE;
typedef struct _input_template { char name[30]; } INPUT_TEMPLATE;
typedef struct UQM_GAME_CONFIGURATION {
    int coarseScan, menuStyle, fonts, intro, shield, smoothScroll, meleeScale;
    BOOLEAN music3do, remixMusic, speech, subtitles, stereoSFX, keepAspectRatio;
    float gamma;
    int graphicsDriver, graphicsFlags, width, height; // Effective video settings.
    int audioDriver, audioFlags;
    float musicScale, sfxScale, speechScale;
    BOOLEAN musicDisabled, soundDisabled;
    CONTROL_TEMPLATE playerControls[2];
    INPUT_TEMPLATE inputTemplates[NUM_TEMPLATES];
} UQM_GAME_CONFIGURATION;
#ifdef __cplusplus
extern "C" {
#endif
/* Stable address through startup, the context lifetime and backend shutdown.
 * Access does not synchronize settings changes; backend rules still apply. */
UQM_GAME_CONFIGURATION *GetGameConfiguration(void);
#ifdef __cplusplus
}
#endif
#endif
