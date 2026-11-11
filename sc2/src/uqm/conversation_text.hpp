#ifndef UQM_CONVERSATION_TEXT_HPP
#define UQM_CONVERSATION_TEXT_HPP
#include "dialogue/legacy_turn.hpp"
#include "libs/strlib.h"
namespace uqm {
// Validate the complete mapping before importing edits. Table stays owned by the
// conversation resource owner; clip/timestamp/name metadata is never replaced.
// Call before publishing text pointers to playback or response rendering.
std::size_t applyConversationText(const dialogue::Json &definition, const std::string &resource,
                                  STRING phrases);
} // namespace uqm
#endif
