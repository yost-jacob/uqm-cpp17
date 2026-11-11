#include "uqm/conversation_story.hpp"
#include "uqm/game_state.hpp"
#include <boost/test/unit_test.hpp>
BOOST_AUTO_TEST_CASE(mycon_ramble_couples_cyclic_memory_discovery_and_visit_choice_without_rng) {
    uqm::GameState game{uqm::UserProfile{{999}, "mycon"}};
    auto &story = game.player({999}).story();
    uqm::MyconRamblePolicy policy;
    for (unsigned i = 1; i <= 32; ++i)
        policy.phrases.push_back("RAMBLE_" + std::to_string(i));
    policy.revealAt = 24;
    policy.revealMinimum = 2;
    bool madeChoice = false;
    story.setNamed("MYCON_RAMBLE", 24);
    auto result = uqm::prepareMyconRamble(story, false, "come_in_peace", policy, madeChoice);
    BOOST_TEST(result.values.at("speech")[0].get<std::string>() == "RAMBLE_25");
    BOOST_TEST(story.readNamed("MYCON_RAMBLE") == 25u);
    BOOST_TEST(story.readNamed("KNOW_ABOUT_SHATTERED") == 2u);
    BOOST_TEST(!madeChoice);
    story.setNamed("MYCON_RAMBLE", 31);
    story.setNamed("MYCON_INSULTS", 7);
    result = uqm::prepareMyconRamble(story, false, "insult_8", policy, madeChoice);
    BOOST_TEST(story.readNamed("MYCON_RAMBLE") == 0u);
    BOOST_TEST(story.readNamed("MYCON_INSULTS") == 0u);
    BOOST_TEST(madeChoice);
    BOOST_TEST(result.values.at("made_choice").get<bool>());
    uqm::prepareMyconRamble(story, true, "lets_be_friends", policy, madeChoice);
    BOOST_TEST(madeChoice);
    BOOST_TEST(story.readNamed("MYCON_INFO") == 0u);
    const auto previous = story.readNamed("MYCON_RAMBLE");
    policy.revealMinimum = 100;
    BOOST_CHECK_THROW(uqm::prepareMyconRamble(story, false, "insult_1", policy, madeChoice),
                      std::invalid_argument);
    BOOST_TEST(story.readNamed("MYCON_RAMBLE") == previous);
}
