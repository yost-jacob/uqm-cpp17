#include "conversation.hpp"
#include "game_state.hpp"
#include "globdata.h"
#include "libs/inplib.h"
#include <stdexcept>
namespace uqm {
namespace {
class ConversationMode final : public GameMode {
public:
    ConversationMode(GameSession &session, std::unique_ptr<ConversationRuntime> conversation)
        : GameMode(session.stateOwner(), session.playerId()), session_(session), conversation_(std::move(conversation)) {
        if (!conversation_ || !conversation_->belongsTo(session))
            throw std::invalid_argument("conversation requires its owning session");
    }
    ModeResult run() override {
        try {
            conversation_->run();
        } catch (...) {
            // The nested dispatcher currently aborts on failure. Destroy this
            // runtime first so partial assets and script borrows still unwind.
            conversation_.reset();
            throw;
        }
        if (auto exit = session_.menuExit()) return *exit;
        if (QuitPosted) return SessionExit::shutdown();
        if (GLOBAL(CurrentActivity) & CHECK_ABORT) return SessionExit::title();
        return ModeCompleted{};
    }
protected:
    std::unique_ptr<ModeSuspension> suspend() override { return conversation_->suspend(); }
    void resume() override { conversation_->resume(); }
private:
    GameSession &session_;
    std::unique_ptr<ConversationRuntime> conversation_;
};
}
std::unique_ptr<GameMode> createConversationMode(GameSession &session, std::unique_ptr<ConversationRuntime> conversation) {
    return std::make_unique<ConversationMode>(session, std::move(conversation));
}
}
