#ifndef UQM_DIALOGUE_LEGACY_TURN_HPP
#define UQM_DIALOGUE_LEGACY_TURN_HPP
#include "engine.hpp"
#include <functional>
#include <string_view>
namespace uqm::dialogue {
// Transitional, explicit handler boundary. Callback borrows its conversation
// owner and must not outlive it. No native callback executes while graphs load.
class LegacyTurnState : public State {
  public:
    using Callback = std::function<Result(std::optional<std::size_t>)>;
    LegacyTurnState(std::string script, Callback callback);
    Result legacyTurn(const std::string &script, const Json &selection) override;

  private:
    std::string script_;
    Callback callback_;
    bool started_ = false;
};
// Validate metadata/text completely before a live table is changed. Native IDs
// and clip/timestamp resources remain attached to their original indexed entries.
std::uint64_t textFingerprint(std::string_view text) noexcept;
void validateLiveDefinition(const Json &definition, const std::string &resource,
                            std::size_t phraseCount);
} // namespace uqm::dialogue
#endif
