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

namespace {
unsigned arenaLoads = 0, arenaReleases = 0;
bool failArenaLoad = false;
BYTE arenaSelector = 0;
}
// The real backend publishes partially loaded frames as it goes. Substitute it
// here to verify ownership on cancellation and failure without graphics setup.
extern "C" void load_gravity_well(BYTE selector) {
    ++arenaLoads;
    arenaSelector = selector;
    if (failArenaLoad) throw std::runtime_error("arena load failed");
}
extern "C" void free_gravity_well() { ++arenaReleases; }

BOOST_AUTO_TEST_CASE(encounter_arena_releases_on_completion_replacement_and_unwind)
{
    arenaLoads = arenaReleases = 0;
    {
        uqm::EncounterArena arena;
        arena.reset();
        BOOST_TEST(arenaReleases == 0u);
        arena.load(3);
        BOOST_TEST(arenaSelector == 3);
        arena.load(7);
        BOOST_TEST(arenaReleases == 1u);
        arena.reset();
        arena.reset();
        BOOST_TEST(arenaReleases == 2u);
    }
    BOOST_TEST(arenaReleases == 2u);
    try {
        uqm::EncounterArena arena;
        arena.load(9);
        throw std::runtime_error("child mode failed");
    } catch (const std::runtime_error &) {}
    BOOST_TEST(arenaReleases == 3u);
    failArenaLoad = true;
    try {
        uqm::EncounterArena arena;
        arena.load(11);
    } catch (const std::runtime_error &) {}
    failArenaLoad = false;
    BOOST_TEST(arenaLoads == 4u);
    BOOST_TEST(arenaReleases == 4u);
}
