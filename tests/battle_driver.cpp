#include "uqm/battle_driver.hpp"
#include "uqm/battle_scene.hpp"
#include "uqm/hyper_navigation.hpp"
#include "uqm/game_state.hpp"
#include "uqm/simulation_engine.hpp"
#include "uqm/globdata.h"
#include "uqm/battle.h"
#include "uqm/battlecontrols.h"
#include "uqm/pickship.h"
#include "uqm/ship.h"
#include <algorithm>
#include "uqm/tactrans.h"
#include "uqm/supermelee/netplay/netmelee.h"
#include "uqm/settings.h"
#include "libs/graphics/gfx_common.h"
#include <boost/test/unit_test.hpp>
#include <vector>
#include <stdexcept>
namespace {
std::vector<int> events;
bool selectOK = true, readyOK = true, failScene = false;
}
// Device/asset boundary: the real C++ orchestration owns these scopes while the
// existing UI/input/timing backend is substituted. Completion is still explicit.
void uqm::detail::BattleScene::initialize() {
    events.push_back(1); bound_ = true;
    if (failScene) throw std::runtime_error("scene assets failed");
}
void uqm::detail::BattleScene::complete() {
    events.push_back(8); completed_ = true; GLOBAL(CurrentActivity) &= ~IN_BATTLE;
}
uqm::detail::BattleScene::~BattleScene() { if (bound_) events.push_back(completed_ ? 9 : 10); }
void uqm::detail::BattleScene::playMusic() { events.push_back(4); }
extern "C" {
NetConnection *netConnections[NUM_PLAYERS]{};

void initChecksumBuffers() { events.push_back(12); }
void uninitChecksumBuffers() { events.push_back(13); }
bool NetConnection_getDiscriminant(const NetConnection *) { return false; }
void initBattleInputBuffers() { events.push_back(2); }
void uninitBattleInputBuffers() { events.push_back(6); }
void initBattleStateDataConnections() {}
void setBattleActiveConnections(bool active) { events.push_back(active ? 3 : 7); }
bool negotiateReadyConnections(bool, NetState) { return readyOK; }
bool waitResetConnections(NetState) { events.push_back(11); return true; }
void ResetWinnerStarShip() {}
void StopDitty() {}
BOOLEAN GetInitialStarShips() { return selectOK ? TRUE : FALSE; }
BOOLEAN GetNextStarShip(STARSHIP *, COUNT) { return selectOK ? TRUE : FALSE; }
void MeleeGameOver() { events.push_back(5); }
}
BOOST_AUTO_TEST_CASE(combat_driver_keeps_completion_explicit_and_releases_network_presence_on_selection_failure)
{
    const auto activity = GLOBAL(CurrentActivity);
    struct Restore { ACTIVITY value; ~Restore() { GLOBAL(CurrentActivity) = value; selectOK = readyOK = true; failScene = false; } } restore{activity};
    for (const bool selected : {true, false}) for (const bool ready : {true, false}) {
        GLOBAL(CurrentActivity) = SUPER_MELEE;
        selectOK = selected; readyOK = ready; events.clear();
        uqm::SimulationEngine engine(2); uqm::SimulationBinding binding(engine);
        uqm::CombatDriver driver(engine); driver.run();
        BOOST_TEST((GLOBAL(CurrentActivity) & CHECK_ABORT) == 0u);
        BOOST_TEST(events.front() == 1);
        BOOST_TEST(events[events.size() - 2] == 8);
        BOOST_TEST(events.back() == 9);
        auto position = [](int event) { return std::find(events.begin(), events.end(), event) - events.begin(); };
        BOOST_TEST(position(6) < position(7));
        BOOST_TEST(position(7) < position(8));
        BOOST_TEST(position(selected && ready ? 5 : 11) < position(6));
    }
}
BOOST_AUTO_TEST_CASE(combat_driver_asset_failure_cancels_without_publishing_results)
{
    events.clear(); failScene = true;
    uqm::SimulationEngine engine(2); uqm::SimulationBinding binding(engine);
    uqm::CombatDriver driver(engine);
    BOOST_CHECK_THROW(driver.run(), std::runtime_error);
    failScene = false;
    const std::vector<int> expected{1, 10};
    BOOST_TEST(events == expected, boost::test_tools::per_element());
}

BOOST_AUTO_TEST_CASE(scripted_instant_victory_skips_selection_and_frames_but_completes_the_scene)
{
    const auto activity = GLOBAL(CurrentActivity);
    const auto instant = instantVictory;
    GLOBAL(CurrentActivity) = IN_ENCOUNTER;
    events.clear(); instantVictory = TRUE;
    uqm::SimulationEngine engine(2); uqm::SimulationBinding binding(engine);
    uqm::CombatDriver driver(engine); driver.run();
    const std::vector<int> expected{1, 8, 9};
    BOOST_TEST(events == expected, boost::test_tools::per_element());
    BOOST_TEST(instantVictory == FALSE);
    BOOST_TEST(GetBattleCounters()[0] == 1u); BOOST_TEST(GetBattleCounters()[1] == 0u);
    GLOBAL(CurrentActivity) = activity; instantVictory = instant;
}
BOOST_AUTO_TEST_CASE(navigation_runs_its_typed_hook_without_combat_result_ui)
{
    const auto activity = GLOBAL(CurrentActivity);
    GLOBAL(CurrentActivity) = IN_HYPERSPACE;
    events.clear();
    uqm::SimulationEngine engine(2); uqm::SimulationBinding binding(engine);
    unsigned frames = 0;
    auto state = std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Player"});
    uqm::GameSession session{state, uqm::SinglePlayerId};
    uqm::HyperSpaceNavigation navigation{session.player(), engine, session.clock()};
    // Native scene orchestration does not need the legacy callback binding.
    BOOST_CHECK_THROW(session.navigation(), std::logic_error);
    uqm::runSpaceNavigation(navigation, [&] { ++frames; });
    BOOST_CHECK_THROW(session.navigation(), std::logic_error);
    BOOST_TEST(frames == 1u);
    BOOST_TEST(std::count(events.begin(), events.end(), 5) == 0);
    BOOST_TEST(events[events.size() - 2] == 8); BOOST_TEST(events.back() == 9);
    GLOBAL(CurrentActivity) = activity;
}
