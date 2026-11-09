#include "conversation.hpp"
#include <stdexcept>
namespace {
uqm::ConversationScriptState *active = nullptr;
uqm::ConversationIntro pendingIntro{}; // Legacy presentation request before mode construction.
uqm::ConversationScriptState &script() {
    if (!active) throw std::logic_error("conversation script outside its owning mode");
    return *active;
}
}
namespace uqm {
ConversationBinding::ConversationBinding(ConversationScriptState &state) noexcept : previous_(active) { active = &state; }
ConversationBinding::~ConversationBinding() { active = previous_; }
ConversationRuntime &activeConversation() {
    auto *owner = script().owner;
    if (!owner) throw std::logic_error("conversation callback has no runtime owner");
    return *owner;
}
ConversationIntro takeConversationIntro() noexcept {
    const auto result = pendingIntro;
    pendingIntro = {};
    return result;
}
}
extern "C" LOCDATA *GetConversationData(void) { return &script().data; }
extern "C" UNICODE *GetConversationPhraseBuffer(void) { return script().phrases.data(); }
extern "C" void SetCommIntroMode(CommIntroMode mode, TimeCount duration) {
    if (active && active->owner) active->owner->setIntro(mode, duration);
    else pendingIntro = {mode, duration};
}
