#ifndef UQM_GAME_SESSION_C_H_
#define UQM_GAME_SESSION_C_H_
#include "libs/compiler.h"
struct menu_state;
#ifdef __cplusplus
extern "C" {
#endif
enum { UQM_MENU_SAVE = 0, UQM_MENU_LOAD = 1 };
// Stable C buffer for the native SessionExit; slot is meaningful only for LOAD.
enum { UQM_SESSION_TITLE, UQM_SESSION_LOAD, UQM_SESSION_SHUTDOWN, UQM_SESSION_FAILED };
typedef struct UQM_SESSION_EXIT { int kind; unsigned slot; } UQM_SESSION_EXIT;
// -1 means a new/title-selected session. Nonnegative slots decode only after
// previous menus/modes and session runtime have completed their cleanup.
UQM_SESSION_EXIT RunGameSession(int loadSlot);
BOOLEAN RequestGameSessionLoad(COUNT slot); // Read-only summary check, then request only.
// Existing C menu drawing/input callbacks, called only by the C++ menu adapter.
BOOLEAN LegacyPickGame(BOOLEAN saving, BOOLEAN fromMainMenu);
BOOLEAN LegacyDoGameOptions(struct menu_state *menu);
#ifdef __cplusplus
}
#endif
#endif
