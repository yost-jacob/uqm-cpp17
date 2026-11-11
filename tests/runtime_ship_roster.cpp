#include "uqm/runtime_ship_roster_legacy.hpp"
#include "uqm/battle.h"
#include "uqm/simulation_engine.hpp"
#include "uqm/ship_behavior.hpp"
#include <boost/test/unit_test.hpp>
using namespace uqm;
namespace {
STARSHIP &add(RuntimeShipRoster &roster, RosterSide side) {
    auto &queue = detail::RuntimeRosterAccess::queues(roster)[static_cast<unsigned>(side)];
    auto handle = AllocLink(&queue);
    auto &ship = *static_cast<STARSHIP *>(handle); ship = {};
    PutQueue(&queue, handle); return ship;
}
}
BOOST_AUTO_TEST_CASE(runtime_rosters_are_mode_local_and_keep_fixed_participant_capacity)
{
    SimulationEngine first(2), second(2);
    auto &roster = first.shipRoster();
    auto &queue = detail::RuntimeRosterAccess::queues(roster)[0];
    for (unsigned slot = 0; slot < MAX_SHIPS_PER_SIDE; ++slot) add(roster, RosterSide::First);
    BOOST_TEST(roster.size(RosterSide::First) == MAX_SHIPS_PER_SIDE);
    BOOST_TEST(!AllocLink(&queue));
    BOOST_TEST(second.shipRoster().size(RosterSide::First) == 0u);
    BOOST_TEST(roster.size(RosterSide::Second) == 0u);
    first.reset(); // Scene reset must retain the already prepared participants.
    BOOST_TEST(roster.size(RosterSide::First) == MAX_SHIPS_PER_SIDE);
    first.shutdown(); BOOST_TEST(roster.size(RosterSide::First) == 0u);
}
BOOST_AUTO_TEST_CASE(combat_roster_handoff_preserves_identity_and_returns_results)
{
    RuntimeShipRoster encounter, combat;
    auto &ship = add(encounter, RosterSide::Second); ship.crew_level = 17;
    auto *identity = &ship;
    {
        RuntimeRosterTransfer handoff(encounter, combat);
        BOOST_TEST(encounter.size(RosterSide::Second) == 0u);
        BOOST_TEST(combat.size(RosterSide::Second) == 1u);
        auto &queue = detail::RuntimeRosterAccess::queues(combat)[1];
        auto *participant = LockStarShip(&queue, GetHeadLink(&queue));
        BOOST_TEST(participant == identity); participant->crew_level = 6;
    }
    BOOST_TEST(combat.size(RosterSide::Second) == 0u);
    BOOST_TEST(encounter.size(RosterSide::Second) == 1u);
    BOOST_TEST(identity->crew_level == 6u);
}

BOOST_AUTO_TEST_CASE(roster_teardown_releases_behavior_without_committing_completion)
{
    class Behavior final : public uqm::combat::Ship {
    public:
        Behavior(unsigned &destroyed, unsigned &completed)
            : Ship(RACE_DESC{}), destroyed_(destroyed), completed_(completed) {}
        ~Behavior() override { ++destroyed_; }
    protected:
        COUNT createWeapons(ELEMENT &, uqm::combat::WeaponSlots &) noexcept override { return 0; }
        void onFinish() noexcept override { ++completed_; }
    private:
        unsigned &destroyed_, &completed_;
    };
    unsigned destroyed = 0, completed = 0;
    {
        RuntimeShipRoster roster;
        auto behavior = std::make_unique<Behavior>(destroyed, completed);
        add(roster, RosterSide::First).RaceDescPtr = &behavior.release()->legacyData();
        roster.clear(RosterSide::First);
        BOOST_TEST(destroyed == 1u); BOOST_TEST(completed == 0u);
        roster.clear(); BOOST_TEST(destroyed == 1u);
        behavior = std::make_unique<Behavior>(destroyed, completed);
        add(roster, RosterSide::Second).RaceDescPtr = &behavior.release()->legacyData();
    }
    BOOST_TEST(destroyed == 2u); BOOST_TEST(completed == 0u);
}

BOOST_AUTO_TEST_CASE(roster_availability_is_mode_local_and_returns_with_battle_outcomes)
{
    SimulationEngine encounter(2), combat(2), unrelated(2);
    auto &roster = encounter.shipRoster();
    add(roster, RosterSide::First); add(roster, RosterSide::First);
    add(roster, RosterSide::Second); roster.beginBattle();
    {
        SimulationBinding active(encounter);
        BOOST_TEST(GetBattleCounters()[0] == 2u);
        {
            SimulationBinding other(unrelated);
            BOOST_TEST(GetBattleCounters()[0] == 0u);
            GetBattleCounters()[0] = 4; // Exercise the real C buffer projection.
        }
        BOOST_TEST(GetBattleCounters()[0] == 2u);
    }
    {
        RuntimeRosterTransfer transfer(roster, combat.shipRoster());
        BOOST_TEST(roster.remaining(RosterSide::First) == 0u);
        combat.shipRoster().retire(RosterSide::Second);
        combat.reset();
        BOOST_TEST(combat.shipRoster().remaining(RosterSide::First) == 2u);
        combat.shipRoster().clear(); // Last-battle outcome survives resource cleanup.
    }
    BOOST_TEST(roster.size(RosterSide::First) == 0u);
    BOOST_TEST(roster.remaining(RosterSide::First) == 2u);
    BOOST_TEST(roster.remaining(RosterSide::Second) == 0u);
    BOOST_TEST(unrelated.shipRoster().remaining(RosterSide::First) == 4u);
    roster.beginBattle(); BOOST_TEST(roster.remaining(RosterSide::First) == 0u);
}

BOOST_AUTO_TEST_CASE(native_participant_creation_initializes_recycled_slots_and_preserves_outcome)
{
    RuntimeShipRoster roster;
    roster.setRemaining(RosterSide::First, 3);
    auto first = roster.append(RosterSide::First, EARTHLING_ID);
    BOOST_REQUIRE(first);
    BOOST_TEST(first->get().playerNr == 0u);
    BOOST_TEST(first->get().SpeciesID == EARTHLING_ID);
    BOOST_TEST(first->get().crew_level == 0u);
    BOOST_TEST(first->get().fleet_ship_id == 0u);
    BOOST_TEST(first->get().RaceDescPtr == nullptr);
    first->get().crew_level = 9;
    first->get().captains_name_index = 12;
    roster.clear(RosterSide::First);
    auto recycled = roster.append(RosterSide::First, SPATHI_ID);
    BOOST_REQUIRE(recycled);
    BOOST_TEST(recycled->get().SpeciesID == SPATHI_ID);
    BOOST_TEST(recycled->get().crew_level == 0u);
    BOOST_TEST(recycled->get().captains_name_index == 0u);
    BOOST_TEST(roster.remaining(RosterSide::First) == 3u);
    for (unsigned i = 1; i < MAX_SHIPS_PER_SIDE; ++i)
        BOOST_REQUIRE(roster.append(RosterSide::First, EARTHLING_ID));
    BOOST_TEST(!roster.append(RosterSide::First, EARTHLING_ID));
    BOOST_TEST(roster.contains(RosterSide::First, recycled->get()));
    BOOST_TEST(roster.size(RosterSide::First) == MAX_SHIPS_PER_SIDE);
    auto opponent = roster.append(RosterSide::Second, SPATHI_ID);
    BOOST_REQUIRE(opponent);
    BOOST_TEST(opponent->get().playerNr == 1u);
}

BOOST_AUTO_TEST_CASE(native_captain_selection_uses_explicit_stream_and_side_species_scope)
{
    RuntimeShipRoster roster;
    uqm::RandomStream random(4321), expected(4321), unrelated(99);
    uqm::RandomBinding parentStream{unrelated};
    const auto first = roster.chooseCaptain(RosterSide::First, EARTHLING_ID, random);
    BOOST_TEST(first == (expected() & 15u) + 5u);
    BOOST_TEST(random.state() == expected.state());
    // Opposite side and other species do not reserve names for this species.
    roster.append(RosterSide::Second, EARTHLING_ID)->get().captains_name_index = first;
    roster.append(RosterSide::First, SPATHI_ID)->get().captains_name_index = first;
    random.reseed(4321);
    BOOST_TEST(roster.chooseCaptain(RosterSide::First, EARTHLING_ID, random) == first);
    roster.clear();
    std::array<bool, 16> used{};
    random.reseed(4321);
    for (unsigned i = 0; i < MAX_SHIPS_PER_SIDE; ++i) {
        const auto name = roster.chooseCaptain(RosterSide::First, EARTHLING_ID, random);
        BOOST_REQUIRE(name >= 5u);
        BOOST_REQUIRE(name < 21u);
        BOOST_TEST(!used[name - 5]);
        used[name - 5] = true;
        roster.append(RosterSide::First, EARTHLING_ID)->get().captains_name_index = name;
    }
    BOOST_TEST(unrelated.state() == 99u);
}
