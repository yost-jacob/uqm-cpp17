#include "../docs/design/combat/battle.hpp"
#include <boost/test/unit_test.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
using namespace uqm::design::combat;
class TestShip final : public Ship {
public:
    explicit TestShip(unsigned &destroyed) : destroyed_(destroyed) {}
    ~TestShip() override { ++destroyed_; }
    const ShipDefinition &definition() const noexcept override {
        std::terminate(); // Definition loading is not part of this ownership fixture.
    }
    ShipSnapshot snapshot() const override { return {}; }
    void beginTick(BattleContext &, const ShipCommands &) override {}
    void endTick(BattleContext &) override {}
    DamageResult receiveHit(BattleContext &, const Hit &) override { return {}; }
private:
    unsigned &destroyed_;
};
std::shared_ptr<Ship> heldFireShip(unsigned &destroyed) {
    auto ship = std::make_shared<TestShip>(destroyed);
    ShipCommands commands;
    commands.firePrimary = true;
    ship->acceptCommands(commands);
    return ship;
}
ProjectileState crystalState() {
    return {{1, 7}, {0}, {100, 200}, {4, -2}, 6, std::nullopt};
}
CombatView view() { return {0, {1, 7}, {}, {}}; }
}

BOOST_AUTO_TEST_CASE(crystal_follows_held_commands_and_release_spawns_independent_fragments_once)
{
    unsigned destroyed = 0;
    auto battleOwner = heldFireShip(destroyed);
    Projectile crystal(crystalState(),
        std::make_unique<ChenjesuCrystalBehavior>(battleOwner, CrystalFragments{8, 10, 2, 16}));
    BOOST_TEST(crystal.step(view()).empty());
    BOOST_TEST(crystal.active());
    BOOST_TEST(crystal.state().position.x == 104);
    BOOST_TEST(crystal.state().position.y == 198);
    BOOST_TEST(!crystal.state().remainingTicks.has_value());

    battleOwner->acceptCommands({}); // Identical path for human, network or AI commands.
    auto fragments = crystal.step(view());
    BOOST_TEST(!crystal.active());
    BOOST_REQUIRE_EQUAL(fragments.size(), 8u);
    BOOST_TEST(crystal.step(view()).empty());
    BOOST_TEST(crystal.detonate().empty());
    std::weak_ptr<Ship> source = battleOwner;
    battleOwner.reset();
    BOOST_TEST(source.expired()); // Neither retired parent nor fragments keep the source.
    BOOST_TEST(destroyed == 1u);
    for (auto &fragment : fragments) {
        BOOST_TEST(fragment->state().source.serial == 7u);
        BOOST_TEST(fragment->state().team.value == 0u);
        BOOST_TEST(fragment->state().position.x == 104);
        BOOST_TEST(fragment->state().position.y == 198);
        BOOST_TEST(fragment->state().damage == 2u);
        BOOST_REQUIRE(fragment->state().remainingTicks);
        BOOST_TEST(*fragment->state().remainingTicks == 10u);
        const auto velocity = fragment->state().velocity;
        BOOST_TEST(std::hypot(velocity.x, velocity.y) == 16, boost::test_tools::tolerance(1e-12));
        for (unsigned tick = 0; tick < 10; ++tick) fragment->step(view());
        BOOST_TEST(!fragment->active());
    }
}

BOOST_AUTO_TEST_CASE(crystal_retains_its_source_until_the_next_tick_after_battle_releases_ownership)
{
    unsigned destroyed = 0;
    auto battleOwner = heldFireShip(destroyed);
    std::weak_ptr<Ship> observed = battleOwner; // Observation must not add a strong owner.
    Projectile crystal(crystalState(),
        std::make_unique<ChenjesuCrystalBehavior>(battleOwner, CrystalFragments{}));
    battleOwner.reset(); // Ship still has held-fire commands, but Battle has retired it.
    BOOST_TEST(!observed.expired());
    BOOST_TEST(destroyed == 0u);
    const auto fragments = crystal.step(view());
    BOOST_TEST(fragments.size() == 8u);
    BOOST_TEST(observed.expired());
    BOOST_TEST(destroyed == 1u);
}

BOOST_AUTO_TEST_CASE(projectile_collision_removal_and_teardown_release_source_without_spawning)
{
    for (const bool removeFirst : {false, true}) {
        unsigned destroyed = 0;
        auto battleOwner = heldFireShip(destroyed);
        std::weak_ptr<Ship> observed = battleOwner;
        {
            Projectile crystal(crystalState(),
                std::make_unique<ChenjesuCrystalBehavior>(battleOwner, CrystalFragments{}));
            battleOwner.reset();
            if (removeFirst) {
                crystal.remove();
                BOOST_TEST(crystal.detonate().empty());
                BOOST_TEST(crystal.step(view()).empty());
            }
        }
        BOOST_TEST(observed.expired());
        BOOST_TEST(destroyed == 1u);
    }
}

BOOST_AUTO_TEST_CASE(straight_projectile_expires_independently_without_source_or_control_access)
{
    auto state = crystalState();
    state.remainingTicks = 2;
    Projectile projectile(state, std::make_unique<StraightFlight>());
    projectile.step(view());
    BOOST_TEST(projectile.active());
    projectile.step(view());
    BOOST_TEST(!projectile.active());
    BOOST_TEST(projectile.state().position.x == 108);
    projectile.step(view());
    BOOST_TEST(projectile.state().position.x == 108); // No updates after removal.
}

BOOST_AUTO_TEST_CASE(projectile_configuration_rejects_missing_owners_and_nonfinite_motion)
{
    BOOST_CHECK_THROW((Projectile{crystalState(), nullptr}), std::invalid_argument);
    auto state = crystalState();
    state.position.x = std::numeric_limits<double>::infinity();
    BOOST_CHECK_THROW((Projectile{state, std::make_unique<StraightFlight>()}), std::invalid_argument);
    state = crystalState();
    state.remainingTicks = 0;
    BOOST_CHECK_THROW((Projectile{state, std::make_unique<StraightFlight>()}), std::invalid_argument);
    BOOST_CHECK_THROW((ChenjesuCrystalBehavior{nullptr, {}}), std::invalid_argument);
    unsigned destroyed = 0;
    auto source = heldFireShip(destroyed);
    auto fragments = CrystalFragments{};
    fragments.lifetime = 0;
    BOOST_CHECK_THROW((ChenjesuCrystalBehavior{source, fragments}), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(seeking_projectiles_turn_independently_and_keep_flying_when_the_target_disappears)
{
    auto state = crystalState();
    state.position = {};
    state.velocity = {4, 0};
    state.remainingTicks = 20;
    auto observation = view();
    ShipSnapshot target{};
    target.id = {1, 9};
    target.position = {0, 100};
    target.crew = 1;
    observation.ships.push_back(target);
    Projectile projectile(state, std::make_unique<SeekingFlight>(target.id, 0.1));
    projectile.step(observation);
    auto velocity = projectile.state().velocity;
    BOOST_TEST(std::atan2(velocity.y, velocity.x) == 0.1, boost::test_tools::tolerance(1e-12));
    BOOST_TEST(std::hypot(velocity.x, velocity.y) == 4, boost::test_tools::tolerance(1e-12));
    observation.ships.clear();
    projectile.step(observation);
    BOOST_TEST(projectile.state().velocity.x == velocity.x);
    BOOST_TEST(projectile.state().velocity.y == velocity.y);
    BOOST_TEST(projectile.active());
}

BOOST_AUTO_TEST_CASE(seeking_chooses_the_short_turn_across_the_angle_boundary)
{
    constexpr double pi = 3.14159265358979323846;
    auto state = crystalState();
    state.position = {};
    state.velocity = {std::cos(pi - 0.05), std::sin(pi - 0.05)};
    auto observation = view();
    ShipSnapshot target{};
    target.id = {1, 9};
    target.position = {100 * std::cos(-pi + 0.05), 100 * std::sin(-pi + 0.05)};
    target.crew = 1;
    observation.ships.push_back(target);
    Projectile projectile(state, std::make_unique<SeekingFlight>(target.id, 0.02));
    projectile.step(observation);
    const auto velocity = projectile.state().velocity;
    BOOST_TEST(std::atan2(velocity.y, velocity.x) == pi - 0.03, boost::test_tools::tolerance(1e-12));
    BOOST_CHECK_THROW((SeekingFlight{target.id, -1}), std::invalid_argument);
    BOOST_CHECK_THROW((SeekingFlight{target.id, std::numeric_limits<double>::quiet_NaN()}), std::invalid_argument);
    Projectile foreign(state, std::make_unique<SeekingFlight>(ShipId{2, 9}, 0.1));
    BOOST_CHECK_THROW(foreign.step(observation), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(crystal_reports_unexpected_strong_owners_instead_of_silently_losing_retirement_detection)
{
    unsigned destroyed = 0;
    auto battleOwner = heldFireShip(destroyed);
    auto accidentalOwner = battleOwner;
    Projectile crystal(crystalState(),
        std::make_unique<ChenjesuCrystalBehavior>(battleOwner, CrystalFragments{}));
    BOOST_CHECK_THROW(crystal.step(view()), std::logic_error);
}

BOOST_AUTO_TEST_CASE(failed_detonation_cannot_run_the_effect_twice)
{
    class FailingDetonation final : public ProjectileBehavior {
    public:
        explicit FailingDetonation(unsigned &calls) : calls_(calls) {}
        ProjectileAction update(ProjectileState &, const CombatView &) override {
            return ProjectileAction::Detonate;
        }
        std::vector<std::unique_ptr<Projectile>> detonate(const ProjectileState &) override {
            ++calls_;
            throw std::runtime_error("failed to create fragments");
        }
    private:
        unsigned &calls_;
    };
    unsigned calls = 0;
    Projectile projectile(crystalState(), std::make_unique<FailingDetonation>(calls));
    BOOST_CHECK_THROW(projectile.step(view()), std::runtime_error);
    BOOST_TEST(!projectile.active());
    BOOST_TEST(projectile.detonate().empty());
    BOOST_TEST(calls == 1u);
}

BOOST_AUTO_TEST_CASE(default_retirement_cleanup_removes_only_its_sources_projectiles_without_detonation)
{
    static_assert(!BattleRules::persistentProjectiles);
    unsigned destroyed = 0;
    auto source = heldFireShip(destroyed);
    Projectile crystal(crystalState(),
        std::make_unique<ChenjesuCrystalBehavior>(source, CrystalFragments{}));
    crystal.retireSource({2, 7}); // Same serial in another battle is unrelated.
    BOOST_TEST(crystal.active());
    source.reset();
    crystal.retireSource({1, 7});
    BOOST_TEST(!crystal.active());
    BOOST_TEST(destroyed == 1u);
    BOOST_TEST(crystal.detonate().empty());
}
