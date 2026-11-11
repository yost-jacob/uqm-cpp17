#include "uqm/fleet.hpp"
#include "uqm/fleet_legacy.hpp"
#include "uqm/game_state.hpp"
#include "uqm/globdata.h"
#include "uqm/encount.h"
#include "uqm/build.h"
#include <boost/test/unit_test.hpp>
#include <stdexcept>
#include <type_traits>
using uqm::Fleet;
using uqm::ShipId;
namespace {
uqm::ShipSpec spathi(BYTE slot, COUNT crew = 1) { return {SPATHI_ID, SPATHI_SHIP, 21, crew, 30, slot}; }
}
static_assert(!std::is_copy_constructible_v<uqm::Ship>);
static_assert(!std::is_copy_constructible_v<Fleet>);
static_assert(!std::is_move_constructible_v<Fleet>);
static_assert(std::is_same_v<decltype(std::declval<const Fleet &>().find(ShipId{})),
    std::optional<std::reference_wrapper<const uqm::Ship>>>);

BOOST_AUTO_TEST_CASE(fleet_creation_validates_overrides_and_returns_stable_distinct_identity)
{
    Fleet fleet{2};
    const auto first = fleet.create(spathi(0));
    BOOST_REQUIRE(first);
    auto &fwiffo = fleet.find(*first)->get();
    BOOST_TEST(fwiffo.species() == SPATHI_ID);
    BOOST_TEST(fwiffo.captain() == 21u);
    BOOST_TEST(fwiffo.crew() == 1u);
    BOOST_TEST(fwiffo.maxCrew() == 30u);
    const auto second = fleet.create(spathi(1, 17));
    BOOST_REQUIRE(second);
    BOOST_TEST(first->value != second->value);
    BOOST_TEST(&fleet.find(*first)->get() == &fwiffo);
    BOOST_TEST(!fleet.create(spathi(1)));
    BOOST_CHECK_THROW(fleet.create(spathi(2)), std::invalid_argument);
    BOOST_CHECK_THROW(fleet.create(spathi(0, 31)), std::invalid_argument);
    BOOST_TEST(fleet.size() == 2u);
    BOOST_CHECK_THROW(fwiffo.updateCrew(31), std::out_of_range);
    BOOST_TEST(fwiffo.crew() == 1u);
}
BOOST_AUTO_TEST_CASE(fleet_dead_roster_slots_remain_until_pruning_and_withdrawn_survivors_remain_alive)
{
    Fleet fleet{3};
    const auto dead = *fleet.create(spathi(0));
    const auto withdrawn = *fleet.create(spathi(1, 17));
    const auto survivor = *fleet.create(spathi(2, 30));
    fleet.find(dead)->get().destroy();
    fleet.find(withdrawn)->get().updateCrew(4);
    BOOST_TEST(fleet.size() == 3u);
    BOOST_TEST(!fleet.find(dead)->get().alive());
    BOOST_CHECK_THROW(fleet.find(dead)->get().updateCrew(1), std::logic_error);
    fleet.pruneRetired();
    BOOST_TEST(!fleet.find(dead));
    BOOST_TEST(fleet.size() == 2u);
    BOOST_TEST(fleet.find(withdrawn)->get().crew() == 4u);
    BOOST_TEST(fleet.find(survivor)->get().alive());
    const auto replacement = *fleet.create(spathi(0));
    BOOST_TEST(replacement.value != dead.value);
    fleet.clear();
    BOOST_TEST(!fleet.find(replacement));
    BOOST_TEST(fleet.size() == 0u);
    BOOST_TEST(fleet.create(spathi(0))->value != replacement.value);
}
BOOST_AUTO_TEST_CASE(player_fleets_are_independent_and_c_buffer_is_the_same_owned_storage)
{
    uqm::GameState other{{uqm::PlayerId{2}, "Other"}};
    auto &active = uqm::gameSession().player().fleet();
    active.clear();
    const auto id = *active.create(spathi(0));
    auto *queue = GetPlayerFleetQueue();
    auto *record = LockShipFrag(queue, GetHeadLink(queue));
    BOOST_TEST(record->ship_id == id.value);
    record->crew_level = 5; // Legacy UI/load writes remain immediately visible.
    BOOST_TEST(active.find(id)->get().crew() == 5u);
    active.find(id)->get().updateCrew(7);
    BOOST_TEST(record->crew_level == 7u);
    BOOST_TEST(other.player(uqm::PlayerId{2}).fleet().size() == 0u);
    active.clear();
}
BOOST_AUTO_TEST_CASE(combat_results_follow_vessel_identity_after_reordering_and_ignore_stale_or_unbound_ids)
{
    auto &fleet = uqm::gameSession().player().fleet();
    fleet.clear();
    const auto first = *fleet.create(spathi(0, 30));
    const auto second = *fleet.create(spathi(1, 17));
    STARSHIP participant{};
    participant.playerNr = RPG_PLAYER_NUM;
    participant.fleet_owner = GetPlayerFleetQueue();
    participant.fleet_ship_id = first.value;
    participant.crew_level = 9;
    auto *queue = GetPlayerFleetQueue();
    const auto head = GetHeadLink(queue);
    RemoveQueue(queue, head);
    PutQueue(queue, head); // Opposite order from the combat roster.
    UpdateShipFragCrew(&participant);
    BOOST_TEST(fleet.find(first)->get().crew() == 9u);
    BOOST_TEST(fleet.find(second)->get().crew() == 17u);
    fleet.erase(first);
    const auto replacement = *fleet.create(spathi(0, 30));
    participant.crew_level = 0;
    UpdateShipFragCrew(&participant); // Reused storage cannot receive old results.
    BOOST_TEST(fleet.find(replacement)->get().crew() == 30u);
    participant.fleet_ship_id = 0; // Flagship/Super Melee.
    UpdateShipFragCrew(&participant);
    BOOST_TEST(fleet.find(replacement)->get().crew() == 30u);
    fleet.clear();
}
BOOST_AUTO_TEST_CASE(c_build_preserves_native_identity_and_pool_reuse_never_reuses_ids)
{
    Fleet fleet{1};
    auto &queue = uqm::detail::FleetAccess::queue(fleet);
    const auto first = CreateFleetFragment(&queue, SPATHI_ID);
    BOOST_REQUIRE(first);
    const auto id = LockShipFrag(&queue, first)->ship_id;
    BOOST_TEST(id != 0u);
    BOOST_TEST(CreateFleetFragment(&queue, SPATHI_ID) == nullptr);
    ReinitQueue(&queue); // Save/load starts another roster in the same pool.
    const auto second = CreateFleetFragment(&queue, EARTHLING_ID);
    BOOST_TEST(second == first);
    BOOST_TEST(LockShipFrag(&queue, second)->ship_id != id);
    BOOST_TEST(!fleet.find({id}));
}

BOOST_AUTO_TEST_CASE(npc_results_use_identity_and_preserve_infinite_fleet_sentinel)
{
    QUEUE saved{};
    SwapQueues(&saved, &GLOBAL(npc_built_ship_q));
    struct Restore {
        QUEUE &saved;
        ~Restore() { UninitQueue(&GLOBAL(npc_built_ship_q)); SwapQueues(&saved, &GLOBAL(npc_built_ship_q)); }
    } restore{saved};
    InitQueue(&GLOBAL(npc_built_ship_q), 2, QUEUE_SHIP_FRAGMENT);
    const auto link = CreateFleetFragment(&GLOBAL(npc_built_ship_q), SPATHI_ID);
    auto *record = LockShipFrag(&GLOBAL(npc_built_ship_q), link);
    record->crew_level = INFINITE_FLEET;
    record->max_crew = 30;
    STARSHIP participant{};
    participant.playerNr = NPC_PLAYER_NUM;
    participant.fleet_owner = &GLOBAL(npc_built_ship_q);
    participant.fleet_ship_id = record->ship_id;
    participant.crew_level = 0;
    UpdateShipFragCrew(&participant);
    BOOST_TEST(record->crew_level == INFINITE_FLEET);
    record->crew_level = 30;
    participant.crew_level = 13;
    UpdateShipFragCrew(&participant);
    BOOST_TEST(record->crew_level == 13u);
    participant.crew_level = 0;
    UpdateShipFragCrew(&participant);
    BOOST_TEST(!uqm::detail::FleetAccess::ship(*record).alive());
}

BOOST_AUTO_TEST_CASE(combat_result_targets_its_explicit_fleet_even_outside_the_process_player)
{
    Fleet first{1}, second{1};
    const auto a = *first.create(spathi(0, 30));
    const auto b = *second.create(spathi(0, 30));
    STARSHIP participant{};
    participant.playerNr = RPG_PLAYER_NUM;
    participant.fleet_owner = &uqm::detail::FleetAccess::queue(second);
    participant.fleet_ship_id = b.value;
    participant.crew_level = 8;
    UpdateShipFragCrew(&participant);
    BOOST_TEST(second.find(b)->get().crew() == 8u);
    BOOST_TEST(first.find(a)->get().crew() == 30u);
    participant.fleet_ship_id = a.value; // Wrong owning fleet cannot receive it.
    UpdateShipFragCrew(&participant);
    BOOST_TEST(first.find(a)->get().crew() == 30u);
    BOOST_TEST(second.find(b)->get().crew() == 8u);
    participant.fleet_ship_id = b.value;
    participant.crew_level = 100; // Edited combat tuning vs saved capacity.
    UpdateShipFragCrew(&participant);
    BOOST_TEST(second.find(b)->get().crew() == 30u);
    participant.crew_level = 0;
    UpdateShipFragCrew(&participant);
    participant.crew_level = 7; // Final retirement cannot be undone by a late result.
    UpdateShipFragCrew(&participant);
    BOOST_TEST(!second.find(b)->get().alive());
}
