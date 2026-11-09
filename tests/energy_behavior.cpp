#include "uqm/energy_behavior.hpp"
#include "uqm/world.hpp"
#include "uqm/game_state.hpp"
#include <boost/test/unit_test.hpp>
#include <stdexcept>
#include <type_traits>

namespace {
const uqm::WorldId planetId{uqm::UniverseId{1}, StarId{42}, 0, std::nullopt};
uqm::WorldDefinition planet() { return {planetId, uqm::WorldKind::Planet, 123, false, "World"}; }
}
static_assert(std::is_same_v<decltype(std::declval<const uqm::SolarSystem &>().world(0)), const uqm::World &>);
static_assert(!std::is_move_constructible_v<uqm::World>);

BOOST_AUTO_TEST_CASE(polymorphic_discovery_behavior_advances_each_players_own_reports)
{
    auto state = std::make_shared<uqm::WorldState>();
    uqm::World world{planet(), state, std::make_unique<uqm::PersonalDiscoveryEnergyBehavior>(3, 2)};
    uqm::GameState alice{{uqm::PlayerId{1}, "Alice"}, state}, bob{{uqm::PlayerId{2}, "Bob"}, state};
    auto &first = alice.player(uqm::PlayerId{1}).story();
    auto &second = bob.player(uqm::PlayerId{2}).story();
    const auto ignored = world.interactEnergy(first, 0, true);
    BOOST_TEST(!ignored.accepted);
    BOOST_TEST(first.discovery(planetId).visitedSites.none());
    const auto a = world.interactEnergy(first, 2, false);
    BOOST_REQUIRE(a.report);
    BOOST_TEST(*a.report == 0u);
    BOOST_TEST(!a.removeNode);
    BOOST_TEST(!world.interactEnergy(first, 2, false).accepted);
    const auto b = world.interactEnergy(first, 0, false);
    BOOST_REQUIRE(b.report);
    BOOST_TEST(*b.report == 1u);
    const auto exhausted = world.interactEnergy(first, 1, false);
    BOOST_TEST(exhausted.accepted);
    BOOST_TEST(!exhausted.report.has_value());
    BOOST_TEST(!exhausted.removeNode);
    const auto other = world.interactEnergy(second, 2, false);
    BOOST_REQUIRE(other.report);
    BOOST_TEST(*other.report == 0u);
    BOOST_CHECK_THROW(world.interactEnergy(first, 3, false), std::out_of_range);
}
BOOST_AUTO_TEST_CASE(artifact_behavior_requests_physical_removal_without_personal_visit_flags)
{
    auto state = std::make_shared<uqm::WorldState>();
    uqm::SolarSystem system{StarId{42}, state, {planet()}};
    auto &world = system.world(0);
    world.setEnergyBehavior(std::make_unique<uqm::ArtifactEnergyBehavior>());
    uqm::GameState player{{uqm::PlayerId{1}, "Player"}, state};
    auto &story = player.player(uqm::PlayerId{1}).story();
    const auto result = world.interactEnergy(story, 0, false);
    BOOST_TEST(result.accepted);
    BOOST_TEST(result.removeNode);
    BOOST_REQUIRE(result.report);
    BOOST_TEST(*result.report == 0u);
    uqm::WorldProgress confirmed;
    confirmed.energy.set(0);
    world.mergeProgress(confirmed);
    const auto collected = world.interactEnergy(story, 0, false); // Stale C view cannot re-award it.
    BOOST_TEST(!collected.accepted);
    BOOST_TEST(!collected.removeNode);
    BOOST_TEST(story.discovery(planetId).visitedSites.none());
    BOOST_CHECK_THROW(world.interactEnergy(story, 1, false), std::out_of_range);
}
BOOST_AUTO_TEST_CASE(world_tree_owns_checked_children_and_keeps_shielding_separate_from_behavior)
{
    auto state = std::make_shared<uqm::WorldState>();
    auto parent = planet();
    parent.moonCount = 1;
    uqm::SolarSystem system{StarId{42}, state, {parent}};
    auto &world = system.at(planetId);
    BOOST_TEST(world.canLand());
    world.setShielded(true);
    BOOST_TEST(!world.canLand());
    auto moonDefinition = planet();
    moonDefinition.id.moon = 0;
    moonDefinition.kind = uqm::WorldKind::Moon;
    std::vector<std::unique_ptr<uqm::World>> moons;
    moons.push_back(std::make_unique<uqm::World>(moonDefinition, state));
    world.replaceChildren(std::move(moons));
    BOOST_TEST(&system.at(moonDefinition.id) == &world.child(0));
    BOOST_CHECK_THROW(world.child(1), std::out_of_range);
    auto wrong = moonDefinition;
    wrong.id.planet = 1;
    std::vector<std::unique_ptr<uqm::World>> invalid;
    invalid.push_back(std::make_unique<uqm::World>(wrong, state));
    BOOST_CHECK_THROW(world.replaceChildren(std::move(invalid)), std::invalid_argument);
    BOOST_TEST(world.childCount() == 1u);
    wrong = moonDefinition;
    wrong.id.universe = uqm::UniverseId{2};
    BOOST_CHECK_THROW(system.at(wrong.id), std::out_of_range);
    auto starbase = planet();
    starbase.kind = uqm::WorldKind::Starbase;
    uqm::World base{starbase, state};
    BOOST_TEST(!base.canLand());
    BOOST_CHECK_THROW(world.setEnergyBehavior(nullptr), std::invalid_argument);
}
BOOST_AUTO_TEST_CASE(world_and_energy_ownership_are_raii_without_destructor_state_writes)
{
    struct Tracked final : uqm::EnergyBehavior {
        explicit Tracked(unsigned &destroyed) : destroyed_(destroyed) {}
        ~Tracked() override { ++destroyed_; }
        unsigned &destroyed_;
    };
    unsigned destroyed = 0;
    std::weak_ptr<uqm::WorldState> lifetime;
    {
        auto state = std::make_shared<uqm::WorldState>();
        lifetime = state;
        uqm::World world{planet(), state, std::make_unique<Tracked>(destroyed)};
        state.reset();
        BOOST_TEST(!lifetime.expired());
        world.setEnergyBehavior(std::make_unique<Tracked>(destroyed));
        BOOST_TEST(destroyed == 1u);
    }
    BOOST_TEST(destroyed == 2u);
    BOOST_TEST(lifetime.expired());
    BOOST_CHECK_THROW((uqm::PersonalDiscoveryEnergyBehavior{0, 1}), std::invalid_argument);
    BOOST_CHECK_THROW((uqm::PersonalDiscoveryEnergyBehavior{33, 1}), std::invalid_argument);
}
