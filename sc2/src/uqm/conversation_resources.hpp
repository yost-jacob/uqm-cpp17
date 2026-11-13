#ifndef UQM_CONVERSATION_RESOURCES_HPP_
#define UQM_CONVERSATION_RESOURCES_HPP_
#include "conversation_context.hpp"
namespace uqm {
// Owns loaded handles, including partial initialization. Script callbacks may
// change frame/colormap indices; release functions accept those captured views.
// configuration supplies borrowed resource IDs (normally static native names);
// those names must outlive this owner. Loaded handles never transfer from it.
class ConversationResources {
public:
    explicit ConversationResources(const LOCDATA &configuration) noexcept;
    ~ConversationResources();
    ConversationResources(const ConversationResources &) = delete;
    ConversationResources &operator=(const ConversationResources &) = delete;
    ConversationScriptState script;
    FONT playerFont = nullptr;
    CONTEXT animation = nullptr, textCache = nullptr;
    FRAME textCacheFrame = nullptr;
};
}
#endif
