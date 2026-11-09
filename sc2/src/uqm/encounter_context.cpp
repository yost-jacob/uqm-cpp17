#include "encounter_runtime.hpp"
#include "game_state.hpp"
namespace uqm {
EncounterRuntime::EncounterRuntime(GameSession &session, const STAR_DESC *star) noexcept
    : campaign_(session.player().campaign()), star_(star), session_(&session) {}
CONVERSATION conversationForRestoredEncounter(const CampaignState &campaign) {
    if (campaign.read(StoryCounter::FoundPlutoSpathi) == 1)
        return SPATHI_CONVERSATION;
    const auto context = campaign.read(StoryCounter::EncounterContext);
    if (context == 0) return TALKING_PET_CONVERSATION;
    constexpr unsigned broadcasterLocations = (1u << 4) | (1u << 5);
    if (context & broadcasterLocations) return ILWRATH_CONVERSATION;
    return CHMMR_CONVERSATION;
}
} // namespace uqm
