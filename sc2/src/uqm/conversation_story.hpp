#ifndef UQM_CONVERSATION_STORY_HPP
#define UQM_CONVERSATION_STORY_HPP
#include "dialogue/state.hpp"
#include <string_view>
#include <vector>
namespace uqm {
class StoryState;
struct MyconRamblePolicy {
    std::vector<std::string> phrases;
    std::size_t revealAt = 0;
    unsigned revealMinimum = 0;
};
void validateMyconRamblePolicy(const MyconRamblePolicy &policy);
// Bounded native operation for Mycon's coupled cyclic memory/choice bookkeeping.
// Policy and current index are validated before writes. No RNG or playback here.
// madeChoice is visit-local and never cleared by a later ordinary greeting.
dialogue::Result prepareMyconRamble(StoryState &story, bool home, std::string_view response,
                                    const MyconRamblePolicy &policy, bool &madeChoice);
} // namespace uqm
#endif
