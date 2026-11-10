#ifndef UQM_GAME_SESSION_C_H_
#define UQM_GAME_SESSION_C_H_
#include "libs/compiler.h"
struct menu_state;
#ifdef __cplusplus
extern "C" {
#endif
enum { UQM_MENU_SAVE = 0, UQM_MENU_LOAD = 1 };
BOOLEAN RequestGameSessionLoad(COUNT slot); // Read-only summary check, then request only.
// Existing C menu drawing/input callbacks, called only by the C++ menu adapter.
BOOLEAN LegacyPickGame(BOOLEAN saving, BOOLEAN fromMainMenu);
BOOLEAN LegacyDoGameOptions(struct menu_state *menu);
#ifdef __cplusplus
}
#endif
#endif
