#include "game_menu.hpp"
#include "game_state.hpp"
#include "mode_rendering.hpp"
#include "game_session_c.h"
#include "gameopt.h"
#include "globdata.h"
#include "setup.h"
#include "menustat.h"
#include "controls.h"
#include "sounds.h"
#include "libs/inplib.h"

namespace uqm {
MenuResult runGameOptionsMenu(GameSession &session) {
    auto &random = simulationEngine().random(); // Borrow the calling mode's stream.
    SessionActivation legacy{session};
    RandomBinding randomBinding{random};
    auto drawing = suspendDrawingBindings({ScreenContext, StatusContext});
    struct FlashCleanup { ~FlashCleanup() { SetFlashRect(nullptr); } } flash;
    MENU_STATE menu{};
    if (session.activities().last == CHECK_LOAD) {
        DrawMenuStateStrings(PM_SAVE_GAME, UQM_MENU_LOAD);
        if (!LegacyPickGame(FALSE, TRUE))
            session.requestMenuExit(SessionExit::title());
    } else {
        menu.CurState = UQM_MENU_SAVE;
        DrawMenuStateStrings(PM_SAVE_GAME, menu.CurState);
        SetFlashRect(SFR_MENU_3DO);
        SetMenuSounds(MENU_SOUND_ARROWS, MENU_SOUND_SELECT);
        menu.InputFunc = LegacyDoGameOptions;
        DoInput(&menu, TRUE);
    }
    if (QuitPosted) return SessionExit::shutdown();
    if (auto exit = session.menuExit()) return exit;
    if (session.activity() & CHECK_ABORT) return SessionExit::title();
    return std::nullopt;
}
}
extern "C" BOOLEAN GameOptions(void) {
    // Boolean/flag translation exists only while enclosing menus remain C.
    try {
        auto &session = uqm::gameSession();
        if (auto exit = uqm::runGameOptionsMenu(session)) {
            session.requestMenuExit(*exit);
            GLOBAL(CurrentActivity) |= exit->kind == uqm::SessionExitKind::LoadGame ? CHECK_LOAD : CHECK_ABORT;
            return FALSE;
        }
        return TRUE;
    } catch (...) {
        uqm::gameSession().requestMenuExit(uqm::SessionExit::failed(std::current_exception()));
        GLOBAL(CurrentActivity) |= CHECK_ABORT;
        return FALSE;
    }
}
