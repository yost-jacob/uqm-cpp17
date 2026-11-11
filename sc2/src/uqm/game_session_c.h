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
// Implemented in sc2/src/uqm/gameplay_modes.cpp. This C-linkage adapter delegates
// to GameSession::runCampaign and contains exceptions at the C boundary.
// Any negative slot starts without a requested save; nonnegative slots request
// a load, validated by runCampaign. Decoding waits until old modes are destroyed.
// The caller repeats on LOAD, exits on SHUTDOWN, and returns to title otherwise.
// Remove this adapter when its C callers can use the typed C++ interface.
UQM_SESSION_EXIT RunGameSession(int loadSlot);
// Standalone arcade visit; only call after campaign teardown at the title menu.
UQM_SESSION_EXIT RunArcadeSession(void);
BOOLEAN RequestGameSessionLoad(COUNT slot); // Read-only summary check, then request only.
// Existing C menu drawing/input callbacks, called only by the C++ menu adapter.
BOOLEAN LegacyPickGame(BOOLEAN saving, BOOLEAN fromMainMenu);
BOOLEAN LegacyDoGameOptions(struct menu_state *menu);
#ifdef __cplusplus
}
#endif
#endif
