#ifndef UQM_CONVERSATION_CUES_HPP
#define UQM_CONVERSATION_CUES_HPP
#include "dialogue/state.hpp"
#include "libs/callback.h"
#include <string>
#include <vector>
namespace uqm {
// Bind only media operations registered for this resource. Binding validates but
// does not invoke callbacks. Unknown names throw before graph story actions run.
CallbackFunction conversationCue(const std::string &resource, const std::string &name);
std::vector<CallbackFunction> conversationCues(const std::string &resource,
                                               const dialogue::Json &names);
} // namespace uqm
#endif
