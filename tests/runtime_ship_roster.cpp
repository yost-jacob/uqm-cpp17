#include "uqm/runtime_ship_roster_legacy.hpp"
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
