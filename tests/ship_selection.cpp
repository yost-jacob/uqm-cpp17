#include "uqm/ship_selection.hpp"
#include "uqm/runtime_ship_roster_legacy.hpp"
#include "uqm/game_state.hpp"
#include "uqm/sis_state.hpp"
#include "uqm/pickship.h"
#include "uqm/supermelee/pickmele.h"
#include "uqm/build.h"
#include <boost/test/unit_test.hpp>
#include <stdexcept>
namespace {
HSTARSHIP selected = nullptr;
unsigned pickerCalls = 0;
STARSHIP &add(uqm::RuntimeShipRoster &roster, uqm::RosterSide side) {
    auto &queue = uqm::detail::RuntimeRosterAccess::queues(roster)[static_cast<unsigned>(side)];
    auto handle = AllocLink(&queue);
    auto &ship = *static_cast<STARSHIP *>(handle); ship = {};
    PutQueue(&queue, handle); return ship;
}
}
extern "C" HSTARSHIP PickCampaignStarShip() { ++pickerCalls; return selected; }
extern "C" BOOLEAN GetNextMeleeStarShip(COUNT, HSTARSHIP *result) { ++pickerCalls; *result = selected; return TRUE; }
BOOST_AUTO_TEST_CASE(player_selection_uses_the_owned_flagship_and_validates_dialog_handles)
{
    using namespace uqm;
    GameState state{{SinglePlayerId, "Selection"}};
    auto &player = state.player(SinglePlayerId);
    RuntimeShipRoster roster, foreign;
    auto &flagship = add(roster, RosterSide::First);
    add(roster, RosterSide::Second); roster.beginBattle();
    pickerCalls = 0; selected = nullptr;
    auto solo = selectCampaignPlayerShip(roster, player, std::nullopt);
    BOOST_REQUIRE(solo); BOOST_TEST(&solo->get() == &flagship); BOOST_TEST(pickerCalls == 0u);
    // The flagship is the last queue entry in campaign construction. Reuse a
    // fresh roster here so the real selection rule sees that same ordering.
    roster.clear();
    auto &escort2 = add(roster, RosterSide::First);
    auto &sis = add(roster, RosterSide::First);
    add(roster, RosterSide::Second); roster.beginBattle();
    selected = &escort2;
    auto choice = selectCampaignPlayerShip(roster, player, std::nullopt);
    BOOST_REQUIRE(choice); BOOST_TEST(&choice->get() == &escort2);
    BOOST_TEST(roster.remaining(RosterSide::First) == 2u);
    selected = &add(foreign, RosterSide::First);
    BOOST_TEST(!selectCampaignPlayerShip(roster, player, std::nullopt));
    BOOST_TEST(roster.remaining(RosterSide::First) == 2u);
    selected = &sis;
    BOOST_REQUIRE(selectCampaignPlayerShip(roster, player, std::nullopt));
    BOOST_TEST(roster.remaining(RosterSide::First) == 1u);
}
BOOST_AUTO_TEST_CASE(player_retirement_records_defeat_or_caps_withdrawal_fuel_without_a_destructor_commit)
{
    using namespace uqm;
    GameState state{{SinglePlayerId, "Selection"}};
    auto &player = state.player(SinglePlayerId);
    RuntimeShipRoster roster;
    auto &sis = add(roster, RosterSide::First);
    sis.crew_level = 4;
    for (const DWORD fuel : {DWORD(0), DWORD(5 * FUEL_TANK_SCALE), DWORD(8 * FUEL_TANK_SCALE)}) {
        player.setFuel(fuel);
        BOOST_TEST(!selectCampaignPlayerShip(roster, player, std::ref(sis)));
        BOOST_TEST(player.fuel() == (fuel > 5 * FUEL_TANK_SCALE ? fuel - 5 * FUEL_TANK_SCALE : 0));
    }
    player.setCrew(23); sis.crew_level = 0;
    BOOST_TEST(!selectCampaignPlayerShip(roster, player, std::ref(sis)));
    BOOST_TEST(player.crew() == static_cast<COUNT>(~0u));
}
BOOST_AUTO_TEST_CASE(opponent_selection_retains_order_and_recycles_infinite_fleet_identity_with_one_random_draw)
{
    using namespace uqm;
    RuntimeShipRoster roster, other;
    auto &first = add(roster, RosterSide::Second);
    auto &second = add(roster, RosterSide::Second);
    roster.beginBattle(); RandomStream random(123), expected(123);
    BOOST_TEST(&selectCampaignOpponentShip(roster, std::nullopt, false, random)->get() == &first);
    BOOST_TEST(&selectCampaignOpponentShip(roster, std::ref(first), false, random)->get() == &second);
    BOOST_TEST(!selectCampaignOpponentShip(roster, std::ref(second), false, random));
    BOOST_TEST(random.state() == 123u);
    first.crew_level = 0; first.max_crew = 17; roster.setRemaining(RosterSide::Second, 1);
    auto recycled = selectCampaignOpponentShip(roster, std::ref(first), true, random);
    BOOST_REQUIRE(recycled); BOOST_TEST(&recycled->get() == &first);
    BOOST_TEST(first.crew_level == 17u); BOOST_TEST(first.playerNr == 1);
    BOOST_TEST(first.captains_name_index == ((expected() & (NUM_CAPTAINS_NAMES - 1)) + NAME_OFFSET));
    BOOST_TEST(random.state() == expected.state());
    BOOST_TEST(roster.remaining(RosterSide::Second) == 2u);
    auto &outsider = add(other, RosterSide::Second);
    const auto before = random.state();
    BOOST_CHECK_THROW(selectCampaignOpponentShip(roster, std::ref(outsider), true, random), std::invalid_argument);
    BOOST_TEST(random.state() == before);
}
BOOST_AUTO_TEST_CASE(arcade_selection_requires_both_sides_and_borrows_only_its_own_side)
{
    using namespace uqm;
    RuntimeShipRoster roster;
    auto &first = add(roster, RosterSide::First);
    auto &second = add(roster, RosterSide::Second);
    roster.beginBattle(); pickerCalls = 0;
    selected = &first;
    BOOST_TEST(!selectArcadeShip(roster, RosterSide::Second));
    selected = &second;
    BOOST_TEST(&selectArcadeShip(roster, RosterSide::Second)->get() == &second);
    roster.setRemaining(RosterSide::First, 0); pickerCalls = 0;
    BOOST_TEST(!selectArcadeShip(roster, RosterSide::Second)); BOOST_TEST(pickerCalls == 0u);
    BOOST_TEST(&selectNavigationShip(roster)->get() == &first);
}
