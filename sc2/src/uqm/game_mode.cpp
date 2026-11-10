#include "game_mode.hpp"
#include <stdexcept>
namespace uqm {
GameMode::GameMode(ModeDispatcher &host, std::uint32_t seed)
    : engine_(MAX_DISPLAY_ELEMENTS, seed), host_(&host) {}
void SessionExit::validate() const {
    priority();
    if ((kind == SessionExitKind::LoadGame) != loadSlot.has_value())
        throw std::invalid_argument("only load requests must carry a save slot");
}
int SessionExit::priority() const {
    switch (kind) {
        case SessionExitKind::Shutdown: return 3;
        case SessionExitKind::Failed: return 2;
        case SessionExitKind::LoadGame: return 1;
        case SessionExitKind::ReturnToTitle: return 0;
    }
    throw std::invalid_argument("unknown session exit kind");
}
}
