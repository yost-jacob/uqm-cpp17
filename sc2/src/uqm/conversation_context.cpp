#include "conversation.hpp"
#include "game_state.hpp"
#include "gameev.h"
#include <stdexcept>
namespace {
uqm::ConversationScriptState *active = nullptr;
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
ConversationIntro takeConversationIntro(GameSession &session) noexcept {
    auto &pending = session.conversationPresentation().intro;
    const auto result = pending;
    pending = {};
    return result;
}
}
extern "C" LOCDATA *GetConversationData(void) { return &script().data; }
extern "C" UNICODE *GetConversationPhraseBuffer(void) { return script().phrases.data(); }
extern "C" void SetCommIntroMode(CommIntroMode mode, TimeCount duration) {
    if (active && active->owner) active->owner->setIntro(mode, duration);
    else uqm::gameSession().conversationPresentation().intro = {mode, duration};
}

extern "C" RECT *GetConversationWindow(void) { return &uqm::gameSession().conversationPresentation().window; }

uqm::dialogue::Json uqm::ConversationRuntime::encounterContext(bool speech) const {
    const auto date = session_.clock().date();
    const auto atGroup = [&](StoryCounter group) {
        return LOBYTE(campaign_.activity) == IN_INTERPLANETARY && campaign_.battleGroupRef
            && campaign_.battleGroupRef == story_.read(group);
    };
    return {{"won_last_battle", LOBYTE(campaign_.activity) == WON_LAST_BATTLE},
        {"in_last_battle", LOBYTE(campaign_.activity) == IN_LAST_BATTLE},
        {"cyborg_enabled", (campaign_.flags & CYBORG_ENABLED) != 0},
        {"using_speech", speech}, {"has_npc_ships", GetHeadLink(&campaign_.ships) != nullptr},
        {"calendar_campaign_year", static_cast<int>(date.year) - START_YEAR},
        {"years_to_kohrah_victory", YEARS_TO_KOHRAH_VICTORY},
        {"thraddash_body_threshold", THRADDASH_BODY_THRESHOLD},
        {"calendar_month", date.month}, {"calendar_day", date.day},
        {"at_samatra", atGroup(StoryCounter::SamatraGrpoffs0)},
        {"at_probe_message", atGroup(StoryCounter::UrquanProbeGrpoffs0)},
        {"at_colony", atGroup(StoryCounter::ColonyGrpoffs0)}};
}
