#include "uqm/hyper_navigation.hpp"
#include "uqm/game_state.hpp"
#include <boost/test/unit_test.hpp>
#include <stdexcept>
BOOST_AUTO_TEST_CASE(navigation_callbacks_borrow_only_within_the_owning_session_scope)
{
    auto state = std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Player"});
    uqm::GameSession session{state, uqm::SinglePlayerId};
    uqm::SimulationEngine firstEngine, secondEngine;
    uqm::HyperSpaceNavigation first{session.player(), firstEngine}, second{session.player(), secondEngine};
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
    uqm::HyperSpaceNavigation navigation{other.player(uqm::SinglePlayerId), engine};
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
    { uqm::HyperSpaceNavigation visit{player, firstEngine}; }
    { uqm::HyperSpaceNavigation nextVisit{player, secondEngine}; }
    BOOST_TEST(player.navigationState().portalX == 1234);
    BOOST_TEST(player.navigationState().portalY == 5678);
    BOOST_TEST(second.player(uqm::SinglePlayerId).navigationState().portalX == 0);
}
