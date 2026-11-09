#include "uqm/encounter_runtime.hpp"
#include "uqm/game_state.hpp"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(restored_encounters_use_the_supplied_campaign_and_preserve_story_precedence)
{
    uqm::GameState first{{uqm::SinglePlayerId, "First"}}, second{{uqm::SinglePlayerId, "Second"}};
    auto &story = first.player(uqm::SinglePlayerId).story();
    for (const unsigned context : {0u, 1u, 16u, 32u, 48u, 255u}) {
        story.set(uqm::StoryCounter::EncounterContext, context);
        for (const unsigned fwiffo : {0u, 1u, 2u, 3u}) {
            story.set(uqm::StoryCounter::FoundPlutoSpathi, fwiffo);
            const auto expected = fwiffo == 1 ? SPATHI_CONVERSATION
                : context == 0 ? TALKING_PET_CONVERSATION
                : (context & 48u) ? ILWRATH_CONVERSATION : CHMMR_CONVERSATION;
            BOOST_TEST(static_cast<int>(uqm::conversationForRestoredEncounter(story)) == static_cast<int>(expected));
            BOOST_TEST(story.read(uqm::StoryCounter::EncounterContext) == context);
            BOOST_TEST(story.read(uqm::StoryCounter::FoundPlutoSpathi) == fwiffo);
        }
    }
    BOOST_TEST(static_cast<int>(uqm::conversationForRestoredEncounter(second.player(uqm::SinglePlayerId).story()))
        == static_cast<int>(TALKING_PET_CONVERSATION));
}
