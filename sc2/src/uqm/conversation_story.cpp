#include "conversation_story.hpp"
#include "game_state.hpp"
#include "globdata.h"
#include <algorithm>
namespace uqm {
void completeConversationPolicy(StoryState &story, std::string_view resource, bool hostile, bool salvage) {
    if (resource == "comm.druuge.dialogue" && hostile && !salvage && !story.readNamed("DRUUGE_MANNER") && !story.readNamed("ATTACKED_DRUUGE")) {
        story.setNamed("ATTACKED_DRUUGE", 1);
        story.setNamed("DRUUGE_VISITS", 0);
    }
    if (resource == "comm.starbase.dialogue") {
        story.setNamed("MOONBASE_ON_SHIP", 0);
        if (story.readNamed("CHMMR_BOMB_STATE") == 2) story.setNamed("CHMMR_BOMB_STATE", 3);
    }
}
void validateMyconRamblePolicy(const MyconRamblePolicy &policy) {
    constexpr unsigned capacity = 1u << (END_MYCON_RAMBLE - MYCON_RAMBLE + 1);
    constexpr unsigned knowledgeMaximum =
        (1u << (END_KNOW_ABOUT_SHATTERED - KNOW_ABOUT_SHATTERED + 1)) - 1;
    if (policy.phrases.empty() || policy.phrases.size() > capacity ||
        policy.revealAt >= policy.phrases.size() || policy.revealMinimum > knowledgeMaximum)
        throw std::invalid_argument("invalid Mycon ramble policy");
}
dialogue::Result prepareMyconRamble(StoryState &story, bool home, std::string_view response,
                                    const MyconRamblePolicy &policy, bool &madeChoice) {
    validateMyconRamblePolicy(policy);
    const auto counter = story.readNamed("MYCON_RAMBLE");
    const auto &phrase = policy.phrases.at(counter); // Check saved index before any mutation.
    const bool choice = home ? response != "lets_be_friends" && response != "came_to_homeworld" &&
                                   response != "submit_to_us"
                             : response != "come_in_peace" && response != "gonna_die";
    story.setNamed("MYCON_RAMBLE", (counter + 1) % policy.phrases.size());
    if (counter == policy.revealAt)
        story.setNamed("KNOW_ABOUT_SHATTERED",
                       std::max(story.readNamed("KNOW_ABOUT_SHATTERED"), policy.revealMinimum));
    if (choice) {
        const auto field = home ? "MYCON_INFO" : "MYCON_INSULTS";
        const unsigned period = home ? 1u << (END_MYCON_INFO - MYCON_INFO + 1)
                                     : 1u << (END_MYCON_INSULTS - MYCON_INSULTS + 1);
        story.setNamed(field, (story.readNamed(field) + 1) % period);
        madeChoice = true;
    }
    return {"ok", {{"speech", dialogue::Json::array({phrase})}, {"made_choice", madeChoice}}};
}
} // namespace uqm
