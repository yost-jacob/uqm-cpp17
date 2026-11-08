#include "game_mode.hpp"
#include "game_state.hpp"
#include <stdexcept>
#include <utility>

namespace uqm {
GameMode::GameMode(std::shared_ptr<GameState> state, PlayerId player)
    : state_(std::move(state)), player_(player) {
    if (!state_) throw std::invalid_argument("mode requires game state");
    state_->player(player);
}
const std::shared_ptr<GameState> &GameMode::stateOwner() const noexcept { return state_; }
PlayerId GameMode::playerId() const noexcept { return player_; }
namespace {
int exitPriority(SessionExitKind kind) {
    switch (kind) {
        case SessionExitKind::Shutdown: return 3;
        case SessionExitKind::Failed: return 2;
        case SessionExitKind::LoadGame: return 1;
        case SessionExitKind::ReturnToTitle: return 0;
    }
    throw std::invalid_argument("unknown session exit kind");
}
}
void GameSession::requestMenuExit(SessionExit exit) {
    const auto priority = exitPriority(exit.kind);
    if ((exit.kind == SessionExitKind::LoadGame) != exit.loadSlot.has_value())
        throw std::invalid_argument("only load requests must carry a save slot");
    if (!pendingExit_ || priority > exitPriority(pendingExit_->kind))
        pendingExit_ = std::move(exit);
}
void GameSession::requestLoad(unsigned slot) { requestMenuExit(SessionExit::load(slot)); }
MenuResult GameSession::menuExit() const { return pendingExit_; }
SessionExit GameSession::run(std::unique_ptr<GameMode> initialMode) {
    if (running_) {
        try { throw std::logic_error("session dispatcher is already running"); }
        catch (...) { return SessionExit::failed(std::current_exception()); }
    }
    running_ = true;
    pendingExit_.reset();
    currentMode_ = std::move(initialMode);
    SessionExit exit = SessionExit::title();
    try {
        if (!currentMode_) throw std::logic_error("session has no initial mode");
        for (;;) {
            auto result = currentMode_->run();
            if (auto *direct = std::get_if<SessionExit>(&result))
                requestMenuExit(*direct);
            if (pendingExit_) {
                exit = *pendingExit_; // Requests propagate only after the mode unwinds.
                break;
            }
            auto &next = std::get<std::unique_ptr<GameMode>>(result);
            if (!next) throw std::logic_error("mode returned a null successor");
            currentMode_ = std::move(next);
        }
    } catch (...) {
        exit = SessionExit::failed(std::current_exception());
    }
    currentMode_.reset(); // No old mode survives into load or application teardown.
    pendingExit_.reset();
    running_ = false;
    return exit;
}
} // namespace uqm
