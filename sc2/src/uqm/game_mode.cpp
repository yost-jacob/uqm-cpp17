#include "game_mode.hpp"
#include "game_state.hpp"
#include <stdexcept>
#include <cstdlib>
#include <cstdio>
#include <utility>

namespace uqm {
GameMode::GameMode(std::shared_ptr<GameState> state, PlayerId player)
    : state_(std::move(state)), player_(player) {
    if (!state_) throw std::invalid_argument("mode requires game state");
    state_->player(player);
}
const std::shared_ptr<GameState> &GameMode::stateOwner() const noexcept { return state_; }
PlayerId GameMode::playerId() const noexcept { return player_; }
MenuResult GameSession::swapGameMode(std::unique_ptr<GameMode> child) {
    if (!running_ || !currentMode_ || !child || child->parentMode_
            || child->state_.get() != state_.get() || child->player_ != player_)
        throw std::invalid_argument("nested mode requires the active session and a fresh matching child");
    try {
        auto suspension = currentMode_->suspend();
        child->parentMode_.swap(currentMode_);
        currentMode_.swap(child);
        auto result = currentMode_->run();
        if (auto *exit = std::get_if<SessionExit>(&result)) requestMenuExit(*exit);
        else if (!std::holds_alternative<ModeCompleted>(result))
            throw std::logic_error("nested mode must complete or return a session exit");
        auto completed = std::move(currentMode_);
        currentMode_ = std::move(completed->parentMode_);
        completed.reset(); // RAII cleanup finishes before restoring bindings/redrawing.
        suspension.reset();
        if (!pendingExit_) currentMode_->resume();
        return pendingExit_;
    } catch (...) {
        // TODO: IMPLEMENT GRACEFUL SHUTDOWN: restore suspended ownership, unwind
        // every parent, report the failure, and tear down the session safely.
        // Never let a generic handler destroy a parent still running on this stack.
        std::fputs("Fatal error in nested game mode; graceful shutdown is not implemented.\n", stderr);
        std::abort();
    }
}
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
            if (std::holds_alternative<ModeCompleted>(result)) break;
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
