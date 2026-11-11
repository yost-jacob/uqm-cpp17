#ifndef UQM_DIALOGUE_LIVE_DEFINITION_HPP
#define UQM_DIALOGUE_LIVE_DEFINITION_HPP
#include "engine.hpp"
#include <string_view>
namespace uqm::dialogue {
// Validate metadata/text completely before a live table is changed. Native IDs
// and clip/timestamp resources remain attached to their original indexed entries.
std::uint64_t textFingerprint(std::string_view text) noexcept;
void validateLiveDefinition(const Json &definition, const std::string &resource,
                            std::size_t phraseCount);
} // namespace uqm::dialogue
#endif
