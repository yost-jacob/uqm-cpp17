#ifndef UQM_CONVERSATION_CONTEXT_HPP_
#define UQM_CONVERSATION_CONTEXT_HPP_
#include "comm.h"
#include <array>
namespace uqm {
class ConversationRuntime;
class GameSession;
struct ConversationScriptState {
    LOCDATA data{};
    std::array<UNICODE, CONVERSATION_PHRASE_CAPACITY> phrases{};
    ConversationRuntime *owner = nullptr; // Borrowed only while bound.
};
class ConversationBinding {
public:
    explicit ConversationBinding(ConversationScriptState &state) noexcept;
    ~ConversationBinding();
    ConversationBinding(const ConversationBinding &) = delete;
    ConversationBinding &operator=(const ConversationBinding &) = delete;
private:
    ConversationScriptState *previous_;
};
ConversationRuntime &activeConversation(); // Throws outside a running script owner.
struct ConversationIntro { CommIntroMode mode = CIM_DEFAULT; TimeCount duration = 0; };
// Session-owned pre-entry requests and the credits compositor's C window view.
struct ConversationPresentation {
    ConversationIntro intro{};
    RECT window{{static_cast<COORD>(SIS_ORG_X), static_cast<COORD>(SIS_ORG_Y)}, {0, 0}};
};
ConversationIntro takeConversationIntro(GameSession &session) noexcept;
} // namespace uqm
#endif
