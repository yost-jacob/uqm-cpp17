#include "encounter_runtime.hpp"
#include "game_state.hpp"
namespace uqm {
EncounterRuntime::EncounterRuntime(GameSession &session, const STAR_DESC *star, SimulationEngine &engine) noexcept
    : story_(session.player().story()), star_(star), engine_(engine), session_(session),
      campaign_(detail::encounterLegacy(session.player())) {}
CONVERSATION conversationForRestoredEncounter(const StoryState &story) {
    if (story.read(StoryCounter::FoundPlutoSpathi) == 1)
        return SPATHI_CONVERSATION;
    const auto context = story.read(StoryCounter::EncounterContext);
    if (context == 0) return TALKING_PET_CONVERSATION;
    constexpr unsigned broadcasterLocations = (1u << 4) | (1u << 5);
    if (context & broadcasterLocations) return ILWRATH_CONVERSATION;
    return CHMMR_CONVERSATION;
}
} // namespace uqm
