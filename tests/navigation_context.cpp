#include "uqm/hyper_navigation.hpp"
#include "uqm/game_state.hpp"
#include <boost/test/unit_test.hpp>
#include <stdexcept>
BOOST_AUTO_TEST_CASE(navigation_callbacks_borrow_only_within_the_owning_session_scope)
{
    auto state = std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Player"});
    uqm::GameSession session{state, uqm::SinglePlayerId};
    uqm::SimulationEngine firstEngine, secondEngine;
    uqm::SpaceNavigation first{session.player(), firstEngine, session.clock(), uqm::NavigationRealm::HyperSpace}, second{session.player(), secondEngine, session.clock(), uqm::NavigationRealm::HyperSpace};
    BOOST_CHECK_THROW(session.navigation(), std::logic_error);
    {
        uqm::NavigationBinding outer{session, first};
        BOOST_TEST(&session.navigation() == &first);
        try {
            uqm::NavigationBinding inner{session, second};
            BOOST_TEST(&session.navigation() == &second);
            throw std::runtime_error("unwind");
        } catch (const std::runtime_error &) {}
        BOOST_TEST(&session.navigation() == &first);
    }
    BOOST_CHECK_THROW(session.navigation(), std::logic_error);
}
BOOST_AUTO_TEST_CASE(navigation_rejects_borrowing_another_players_state)
{
    auto state = std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Player"});
    uqm::GameSession session{state, uqm::SinglePlayerId};
    uqm::GameState other{{uqm::SinglePlayerId, "Other"}};
    uqm::SimulationEngine engine;
    uqm::SpaceNavigation navigation{other.player(uqm::SinglePlayerId), engine, session.clock(), uqm::NavigationRealm::HyperSpace};
    BOOST_CHECK_THROW((uqm::NavigationBinding{session, navigation}), std::invalid_argument);
    BOOST_CHECK_THROW(session.navigation(), std::logic_error);
}
BOOST_AUTO_TEST_CASE(portal_position_belongs_to_the_player_and_survives_navigation_mode_recreation)
{
    uqm::GameState first{{uqm::SinglePlayerId, "First"}}, second{{uqm::SinglePlayerId, "Second"}};
    auto &player = first.player(uqm::SinglePlayerId);
    player.navigationState().portalX = 1234;
    player.navigationState().portalY = 5678;
    uqm::SimulationEngine firstEngine, secondEngine;
    uqm::CampaignClock clock;
    { uqm::SpaceNavigation visit{player, firstEngine, clock, uqm::NavigationRealm::HyperSpace}; }
    { uqm::SpaceNavigation nextVisit{player, secondEngine, clock, uqm::NavigationRealm::HyperSpace}; }
    BOOST_TEST(player.navigationState().portalX == 1234);
    BOOST_TEST(player.navigationState().portalY == 5678);
    BOOST_TEST(second.player(uqm::SinglePlayerId).navigationState().portalX == 0);
}
BOOST_AUTO_TEST_CASE(navigation_rejects_another_sessions_clock_even_for_the_same_player)
{
    auto state = std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Player"});
    uqm::GameSession first{state, uqm::SinglePlayerId}, second{state, uqm::SinglePlayerId};
    uqm::SimulationEngine engine;
    uqm::SpaceNavigation navigation{first.player(), engine, second.clock(), uqm::NavigationRealm::HyperSpace};
    BOOST_CHECK_THROW((uqm::NavigationBinding{first, navigation}), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(portal_handoff_preserves_outgoing_location_until_successor_entry)
{
    uqm::GameState state{{uqm::SinglePlayerId, "Player"}};
    auto &player = state.player(uqm::SinglePlayerId);
    uqm::CampaignClock clock;
    uqm::SimulationEngine engine;
    for (const auto source : {uqm::NavigationRealm::HyperSpace, uqm::NavigationRealm::QuasiSpace}) {
        const bool fromQuasi = source == uqm::NavigationRealm::QuasiSpace;
        player.enterNavigation(fromQuasi);
        player.story().set(uqm::StoryCounter::ArilouSpaceSide, fromQuasi ? 3 : 0);
        player.travel().flight.facing = 7;
        uqm::SpaceNavigation outgoing{player, engine, clock, source};
        const auto destination = fromQuasi ? uqm::NavigationRealm::HyperSpace : uqm::NavigationRealm::QuasiSpace;
        outgoing.prepareRealmTransition(destination, {123, 456});
        BOOST_TEST(outgoing.isQuasiSpace() == fromQuasi);
        BOOST_TEST(std::holds_alternative<uqm::QuasiSpaceLocation>(player.location()) == fromQuasi);
        BOOST_TEST(player.story().read(uqm::StoryCounter::ArilouSpaceSide) == (fromQuasi ? 0u : 3u));
        // Callbacks still have the correct outgoing variant after the story marker changes.
        outgoing.prepareRealmTransition(destination, {1234567, 2345678});
        player.enterNavigation(!fromQuasi);
        BOOST_TEST(std::holds_alternative<uqm::QuasiSpaceLocation>(player.location()) == !fromQuasi);
        BOOST_TEST(player.travel().position.x == 1234567);
        BOOST_TEST(player.travel().position.y == 2345678);
        BOOST_TEST(player.travel().flight.facing == 7u);
    }
}
