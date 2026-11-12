#include "uqm/hyper_navigation.hpp"
#include "uqm/game_state.hpp"
#include "uqm/navigation_entities.hpp"
#include <boost/test/unit_test.hpp>
#include <stdexcept>
#include <algorithm>
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

namespace {
uqm::SpaceNavigation *calledNavigation = nullptr;
unsigned hookCalls[10]{};
}
// The real engine-owned entity dispatch is under test. Gameplay kernels and
// shared ship updates are replaced at this boundary, without a session lookup.
BOOLEAN uqm::SpaceNavigation::hyper_transition(ELEMENT *) { calledNavigation = this; ++hookCalls[0]; return TRUE; }
void uqm::SpaceNavigation::unhyper_transition(ELEMENT *) { calledNavigation = this; ++hookCalls[1]; }
void uqm::SpaceNavigation::hyper_death(ELEMENT *) { calledNavigation = this; ++hookCalls[2]; }
void uqm::SpaceNavigation::arilou_space_death(ELEMENT *) { calledNavigation = this; ++hookCalls[3]; }
void uqm::SpaceNavigation::encounter_transition(ELEMENT *) { calledNavigation = this; ++hookCalls[4]; }
void uqm::SpaceNavigation::hyper_collision(ELEMENT *, POINT *, ELEMENT *, POINT *) { calledNavigation = this; ++hookCalls[5]; }
void uqm::SpaceNavigation::arilou_space_collision(ELEMENT *, POINT *, ELEMENT *, POINT *) { calledNavigation = this; ++hookCalls[6]; }
void uqm::SpaceNavigation::encounter_collision(ELEMENT *, POINT *, ELEMENT *, POINT *) { calledNavigation = this; ++hookCalls[7]; }
extern "C" void ship_preprocess(ELEMENT *) { ++hookCalls[8]; }
extern "C" void ship_postprocess(ELEMENT *) { ++hookCalls[9]; }

BOOST_AUTO_TEST_CASE(navigation_entity_hooks_keep_their_owner_and_transition_phase)
{
    using namespace uqm::detail;
    uqm::GameState state{{uqm::SinglePlayerId, "Player"}};
    uqm::CampaignClock clock;
    uqm::SimulationEngine engine{4};
    uqm::SimulationBinding selected{engine};
    uqm::SpaceNavigation hyper{state.player(uqm::SinglePlayerId), engine, clock, uqm::NavigationRealm::HyperSpace};
    uqm::SpaceNavigation quasi{state.player(uqm::SinglePlayerId), engine, clock, uqm::NavigationRealm::QuasiSpace};
    std::fill(std::begin(hookCalls), std::end(hookCalls), 0u);
    auto &star = engine.allocateElement()->get();
    auto &portal = engine.allocateElement()->get();
    auto &encounter = engine.allocateElement()->get();
    auto &ship = engine.allocateElement()->get();
    const auto hooks = EntityHooks::Collision | EntityHooks::Expiration;
    attachEntity(star, std::make_unique<NavigationStar>(hyper), hooks);
    attachEntity(portal, std::make_unique<NavigationPortal>(quasi), hooks);
    attachEntity(encounter, std::make_unique<NavigationEncounter>(hyper), hooks);
    POINT point{};
    for (auto *self : {&star, &portal, &encounter}) {
        self->death_func(self);
        self->collision_func(self, &point, &ship, &point);
        BOOST_TEST(calledNavigation == (self == &portal ? &quasi : &hyper));
    }
    auto behavior = std::make_unique<NavigationFlagship>(hyper);
    auto &flagship = *behavior;
    attachEntity(ship, std::move(behavior), EntityHooks::Update | EntityHooks::Postprocess);
    ship.preprocess_func(&ship); ship.postprocess_func(&ship);
    flagship.phase(ship, NavigationFlagship::Phase::Arriving);
    BOOST_TEST(ship.postprocess_func == nullptr);
    ship.preprocess_func(&ship);
    flagship.phase(ship, NavigationFlagship::Phase::Departing);
    BOOST_TEST(ship.postprocess_func == nullptr);
    ship.preprocess_func(&ship);
    BOOST_TEST(calledNavigation == &hyper);
    flagship.phase(ship, NavigationFlagship::Phase::Flying);
    BOOST_TEST(ship.postprocess_func != nullptr);
    for (const auto count : hookCalls) BOOST_TEST(count == 1u);
    engine.reset(); // Cancels owners without calling gameplay expiration hooks.
    for (const auto count : hookCalls) BOOST_TEST(count == 1u);
}

BOOST_AUTO_TEST_CASE(encounter_views_do_not_own_campaign_records_or_recycled_engine_slots)
{
    uqm::SimulationEngine engine{1};
    uqm::SimulationBinding selected{engine};
    ENCOUNTER record{};
    record.race_id = 3;
    record.num_ships = 2;
    record.ShipList[0].crew_level = 7;
    uqm::NavigationEncounterViews views;
    auto &first = engine.allocateElement()->get();
    views.insert(record, uqm::combat::EntityHandle::capture(disp_q, first));
    BOOST_REQUIRE(views.find(record, engine));
    BOOST_TEST(&views.find(record, engine)->get() == &first);
    engine.releaseElement(first);
    BOOST_TEST(!views.find(record, engine));
    auto &replacement = engine.allocateElement()->get();
    BOOST_TEST(!views.find(record, engine)); // Same pool slot is a different identity.
    views.insert(record, uqm::combat::EntityHandle::capture(disp_q, replacement));
    BOOST_REQUIRE(views.find(record, engine));
    views.erase(record);
    BOOST_TEST(!views.find(record, engine));
    views.insert(record, uqm::combat::EntityHandle::capture(disp_q, replacement));
    views.clear();
    BOOST_TEST(!views.find(record, engine));
    BOOST_TEST(record.race_id == 3u);
    BOOST_TEST(record.num_ships == 2u);
    BOOST_TEST(record.ShipList[0].crew_level == 7u);
    // Dropping the index does not destroy the live projection either.
    BOOST_TEST(uqm::combat::EntityHandle::capture(disp_q, replacement).resolve(disp_q).has_value());
    views.insert(record, uqm::combat::EntityHandle::capture(disp_q, replacement));
    engine.reset();
    BOOST_TEST(!views.find(record, engine));
}
