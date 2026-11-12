#include "uqm/hyper_navigation.hpp"
#include "uqm/game_state.hpp"
#include "uqm/navigation_entities.hpp"
#include <boost/test/unit_test.hpp>
#include <stdexcept>
#include <algorithm>
#include <fstream>
#include "uqm/game_definitions_legacy.hpp"
BOOST_AUTO_TEST_CASE(portal_position_belongs_to_the_player_and_survives_navigation_mode_recreation)
{
    auto first = std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "First"});
    uqm::GameState second{{uqm::SinglePlayerId, "Second"}};
    uqm::GameSession session{first, uqm::SinglePlayerId};
    auto &player = session.player();
    player.navigationState().portalX = 1234;
    player.navigationState().portalY = 5678;
    uqm::SimulationEngine firstEngine, secondEngine;
    { uqm::SpaceNavigation visit{session, firstEngine, uqm::NavigationRealm::HyperSpace}; }
    { uqm::SpaceNavigation nextVisit{session, secondEngine, uqm::NavigationRealm::HyperSpace}; }
    BOOST_TEST(player.navigationState().portalX == 1234);
    BOOST_TEST(player.navigationState().portalY == 5678);
    BOOST_TEST(second.player(uqm::SinglePlayerId).navigationState().portalX == 0);
}
BOOST_AUTO_TEST_CASE(portal_handoff_preserves_outgoing_location_until_successor_entry)
{
    auto state = std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Player"});
    uqm::GameSession session{state, uqm::SinglePlayerId};
    auto &player = session.player();
    uqm::SimulationEngine engine;
    for (const auto source : {uqm::NavigationRealm::HyperSpace, uqm::NavigationRealm::QuasiSpace}) {
        const bool fromQuasi = source == uqm::NavigationRealm::QuasiSpace;
        player.enterNavigation(fromQuasi);
        player.story().set(uqm::StoryCounter::ArilouSpaceSide, fromQuasi ? 3 : 0);
        player.travel().flight.facing = 7;
        uqm::SpaceNavigation outgoing{session, engine, source};
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
    auto state = std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Player"});
    uqm::GameSession session{state, uqm::SinglePlayerId};
    uqm::SimulationEngine engine{4};
    uqm::SimulationBinding selected{engine};
    uqm::SpaceNavigation hyper{session, engine, uqm::NavigationRealm::HyperSpace};
    uqm::SpaceNavigation quasi{session, engine, uqm::NavigationRealm::QuasiSpace};
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

BOOST_AUTO_TEST_CASE(encounter_projection_identity_survives_dense_record_erasure)
{
    uqm::CampaignEncounters entries;
    entries.insert(); entries.insert();
    const auto retainedId = (entries.begin() + 1)->runtime_id;
    uqm::SimulationEngine engine{1};
    uqm::SimulationBinding selected{engine};
    auto &element = engine.allocateElement()->get();
    uqm::NavigationEncounterViews views;
    views.insert(*(entries.begin() + 1), uqm::combat::EntityHandle::capture(disp_q, element));
    entries.erase(entries.begin());
    BOOST_TEST(entries.begin()->runtime_id == retainedId);
    BOOST_REQUIRE(views.find(*entries.begin(), engine));
    BOOST_TEST(&views.find(*entries.begin(), engine)->get() == &element);
}

BOOST_AUTO_TEST_CASE(navigation_reads_its_own_catalogue_and_fleets_with_another_session_active)
{
    uqm::GameSession owner{std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Owner"}), uqm::SinglePlayerId};
    uqm::GameSession other{std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Other"}), uqm::SinglePlayerId};
    std::ifstream input{UQM_TEST_STARMAP_PATH};
    uqm::detail::DefinitionsAccess::loadStars(uqm::detail::DefinitionsAccess::storage(owner), input);
    QUEUE *ownedQueue;
    {
        uqm::SessionActivation active{owner};
        ownedQueue = &GLOBAL(avail_race_q); // Test setup at the C projection boundary.
        InitQueue(ownedQueue, 1, QUEUE_FLEET_INFO);
        auto *fleet = static_cast<FLEET_INFO *>(AllocLink(ownedQueue));
        *fleet = {};
        fleet->crew_level = 9;
        fleet->actual_strength = 42;
        PutQueue(ownedQueue, fleet);
    }
    struct Release { QUEUE *queue; ~Release() { UninitQueue(queue); } } release{ownedQueue};
    uqm::SimulationEngine engine;
    uqm::SpaceNavigation hyper{owner, engine, uqm::NavigationRealm::HyperSpace};
    uqm::SpaceNavigation quasi{owner, engine, uqm::NavigationRealm::QuasiSpace};
    const auto &catalogue = owner.definitions().stars();
    const auto &first = catalogue.at(StarId{0});
    const POINT position{static_cast<COORD>(first.position().x), static_cast<COORD>(first.position().y)};
    {
        uqm::SessionActivation wrongActive{other};
        BOOST_TEST(&uqm::gameSession() == &other);
        BOOST_TEST(owner.raceFleets().at(0).crew_level == 9u);
        BOOST_TEST(owner.raceFleets().at(0).actual_strength == 42u);
        BOOST_CHECK_THROW(owner.raceFleets().at(1), std::out_of_range);
        BOOST_CHECK_THROW(other.raceFleets(), std::logic_error);
        const auto matches = hyper.starsNear(position, 0, 0);
        BOOST_REQUIRE(!matches.empty());
        BOOST_TEST(matches.front().value == first.id().value);
        const auto quasiMatches = quasi.starsNear(position, -1, -1);
        BOOST_REQUIRE(!quasiMatches.empty());
        for (const auto id : quasiMatches)
            BOOST_CHECK(catalogue.at(id).space() == StarSpace::QuasiSpace);
    }
}
