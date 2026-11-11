#ifndef UQM_DESIGN_CONVERSATION_PRESENTATION_HPP
#define UQM_DESIGN_CONVERSATION_PRESENTATION_HPP
#include "engine.hpp"
namespace uqm::design::conversation {
// Device boundary only. ConversationMode owns an implementation and its asset
// handles for the complete visit; this interface is not an SDL implementation.
class Presentation {
  public:
    virtual ~Presentation() = default;
    // Format resolved typed insertions and resolve phrase IDs, retain composite and optional voice
    // assets through the session cache, and apply cues. Same entry is idempotent:
    // redrawing/resuming must not restart music, animation or voice playback.
    virtual void show(const DialogueView &, const Json &phraseBank) = 0;
    virtual void showResponses(std::size_t entry, const std::vector<ResponseView> &) = 0;
    virtual void suspend() = 0;
    virtual void resume() = 0;
    // Stop/cancel playback and callbacks before asset owners/session are released.
    virtual void stop() noexcept = 0;
};
} // namespace uqm::design::conversation
#endif
