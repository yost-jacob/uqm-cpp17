#include "system_behavior.hpp"
#include "game_state.hpp"
#include "globdata.h"
#include <stdexcept>
namespace uqm {
namespace {
// The legacy dialogue engine remains synchronous inside this native child mode.
// result borrows the suspended caller's stack and is consumed before it resumes.
class SystemEncounterMode final : public GameMode {
public:
    SystemEncounterMode(GameSession &session, CONVERSATION conversation, COUNT &result)
        : GameMode(session.stateOwner(), session.playerId()), session_(session),
          conversation_(conversation), result_(result) {}
    ModeResult run() override {
        result_ = InitCommunication(conversation_);
        if (auto exit = session_.menuExit()) return *exit;
        return uqm::ModeCompleted{};
    }
private:
    GameSession &session_;
    CONVERSATION conversation_;
    COUNT &result_;
};
}
void SystemBehavior::attachSession(GameSession &session) {
    if (&session.player().campaign() != &campaign_)
        throw std::invalid_argument("system and session have different campaigns");
    if (session_ && session_ != &session)
        throw std::logic_error("system is already attached to another session");
    session_ = &session;
}
COUNT SystemBehavior::communicate(CONVERSATION conversation) {
    if (!session_) return InitCommunication(conversation); // Legacy debug/test entry.
    COUNT result = 0;
    if (auto exit = session_->swapGameMode(std::make_unique<SystemEncounterMode>(*session_, conversation, result))) {
        // Let the existing C exploration stack unwind using its established flags.
        GLOBAL(CurrentActivity) |= exit->kind == SessionExitKind::LoadGame ? CHECK_LOAD : CHECK_ABORT;
    }
    return result;
}
}
