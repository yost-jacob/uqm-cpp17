#include "uqm/encounter_runtime.hpp"
#include "uqm/game_state.hpp"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(restored_encounters_use_the_supplied_campaign_and_preserve_story_precedence)
{
    uqm::GameState first{{uqm::SinglePlayerId, "First"}}, second{{uqm::SinglePlayerId, "Second"}};
    auto &campaign = first.player(uqm::SinglePlayerId).campaign();
    for (const unsigned context : {0u, 1u, 16u, 32u, 48u, 255u}) {
        campaign.set(uqm::StoryCounter::EncounterContext, context);
        for (const unsigned fwiffo : {0u, 1u, 2u, 3u}) {
            campaign.set(uqm::StoryCounter::FoundPlutoSpathi, fwiffo);
            const auto expected = fwiffo == 1 ? SPATHI_CONVERSATION
                : context == 0 ? TALKING_PET_CONVERSATION
                : (context & 48u) ? ILWRATH_CONVERSATION : CHMMR_CONVERSATION;
            BOOST_TEST(static_cast<int>(uqm::conversationForRestoredEncounter(campaign)) == static_cast<int>(expected));
            BOOST_TEST(campaign.read(uqm::StoryCounter::EncounterContext) == context);
            BOOST_TEST(campaign.read(uqm::StoryCounter::FoundPlutoSpathi) == fwiffo);
        }
    }
    BOOST_TEST(static_cast<int>(uqm::conversationForRestoredEncounter(second.player(uqm::SinglePlayerId).campaign()))
        == static_cast<int>(TALKING_PET_CONVERSATION));
}
