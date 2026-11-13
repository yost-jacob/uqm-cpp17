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
        : GameMode(session.dispatcher(), session.nextModeSeed()), session_(session), conversation_(std::move(conversation)) {
        if (!conversation_ || !conversation_->belongsTo(session))
            throw std::invalid_argument("conversation requires its owning session");
    }
    ModeResult run() override {
        RandomBinding random{conversation_->random()};
        conversation_->run(); // Dispatcher destroys the child before restoring its parent.
        if (auto exit = session_.menuExit()) return *exit;
        if (QuitPosted) return SessionExit::shutdown();
        if (detail::encounterLegacy(session_.player()).activity & CHECK_ABORT) return SessionExit::title();
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
