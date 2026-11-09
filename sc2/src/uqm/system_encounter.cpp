#include "system_behavior.hpp"
#include "encounter_runtime.hpp"
#include "game_state.hpp"
#include "globdata.h"
#include <stdexcept>
namespace uqm {
namespace {
// The legacy dialogue engine remains synchronous inside this native child mode.
// result borrows the suspended caller's stack and is consumed before it resumes.
class SystemEncounterMode final : public GameMode {
public:
    SystemEncounterMode(GameSession &session, const STAR_DESC *star, CONVERSATION conversation, COUNT &result)
        : GameMode(session.stateOwner(), session.playerId()), session_(session),
          conversation_(conversation), result_(result), encounter_(session, star) {}
    ModeResult run() override {
        result_ = encounter_.communicate(conversation_);
        if (auto exit = session_.menuExit()) return *exit;
        return uqm::ModeCompleted{};
    }
private:
    GameSession &session_;
    CONVERSATION conversation_;
    COUNT &result_;
    EncounterRuntime encounter_;
};
}
void SystemBehavior::attachSession(GameSession &session) {
    if (&session.player().story() != &story_)
        throw std::invalid_argument("system and session have different story states");
    if (session_ && session_ != &session)
        throw std::logic_error("system is already attached to another session");
    session_ = &session;
}
COUNT SystemBehavior::communicate(CONVERSATION conversation) {
    if (!session_) { // Native debug/test entry still uses the supplied story state.
        EncounterRuntime encounter{story_, star_};
        return encounter.communicate(conversation);
    }
    COUNT result = 0;
    if (auto exit = session_->swapGameMode(std::make_unique<SystemEncounterMode>(*session_, star_, conversation, result))) {
        // Let the existing C exploration stack unwind using its established flags.
        GLOBAL(CurrentActivity) |= exit->kind == SessionExitKind::LoadGame ? CHECK_LOAD : CHECK_ABORT;
    }
    return result;
}
}
