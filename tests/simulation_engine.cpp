#include "uqm/simulation_engine_legacy.hpp"
#include "uqm/game_state.hpp"
#include "uqm/races.h"
#include "uqm/globdata.h"
#include "uqm/ship.h"
#include "uqm/units.h"
#include <boost/test/unit_test.hpp>
#include <stdexcept>
#include <algorithm>
using namespace uqm;
// Device-free crew boundary: native damage/target policy and real pool storage
// run, while HUD drawing is outside this test's simulation responsibility.
extern "C" {

BOOLEAN DeltaCrew(ELEMENT *element, SIZE delta) {
    const auto remaining = std::max(0, int(element->crew_level) + delta);
    element->crew_level = static_cast<COUNT>(remaining);
    return remaining ? TRUE : FALSE;
}
}
namespace {
ELEMENT &add(SimulationEngine &engine) {
    auto &queue = detail::SimulationAccess::queue(engine);
    auto handle = AllocLink(&queue);
    auto &element = *static_cast<ELEMENT *>(handle);
    element = {};
    element.life_span = NORMAL_LIFE;
    PutQueue(&queue, handle);
    return element;
}
}
BOOST_AUTO_TEST_CASE(simulation_engines_own_separate_pools_and_restore_nested_bindings)
{
    auto *original = &simulationEngine();
    SimulationEngine first, second;
    first.initialize(3); second.initialize(2);
    auto &a = add(first); auto &b = add(second);
    const auto aId = combat::EntityHandle::capture(detail::SimulationAccess::queue(first), a);
    BOOST_REQUIRE(first.resolve(aId));
    BOOST_TEST(!second.resolve(aId));
    {
        SimulationBinding outer(first);
        BOOST_TEST(&simulationEngine() == &first);
        BOOST_TEST(GetSimulationQueue() == &detail::SimulationAccess::queue(first));
        {
            SimulationBinding inner(second);
            BOOST_TEST(&simulationEngine() == &second);
            BOOST_TEST(GetHeadElement() == &b);
        }
        BOOST_TEST(GetHeadElement() == &a);
    }
    BOOST_TEST(&simulationEngine() == original);
    first.reset();
    BOOST_TEST(first.size() == 0u);
    BOOST_TEST(second.size() == 1u);
    BOOST_TEST(!first.resolve(aId));
    first.shutdown(); first.shutdown();
    BOOST_TEST(!first.initialized());
    first.initialize(1);
    BOOST_TEST(first.size() == 0u);
    BOOST_CHECK_THROW(first.initialize(1), std::logic_error);
}
BOOST_AUTO_TEST_CASE(simulation_damage_preserves_ship_planet_and_finite_object_rules)
{
    SimulationEngine engine; engine.initialize(3);
    auto &shot = add(engine); shot.hit_points = 5; shot.mass_points = 1;
    engine.damage(shot, 2); BOOST_TEST(shot.hit_points == 3u);
    engine.damage(shot, 3); BOOST_TEST(shot.hit_points == 0u);
    BOOST_TEST(shot.life_span == 0u); BOOST_TEST((shot.state_flags & NONSOLID) != 0);
    auto &planet = add(engine); planet.hit_points = 200; planet.mass_points = 200;
    engine.damage(planet, 100); BOOST_TEST(planet.hit_points == 200u);
    auto &ship = add(engine); ship.state_flags = PLAYER_SHIP; ship.crew_level = 7;
    engine.damage(ship, 2); BOOST_TEST(ship.crew_level == 5u);
    engine.damage(ship, 8); BOOST_TEST(ship.crew_level == 0u);
    BOOST_TEST(ship.life_span == 0u); BOOST_TEST((ship.state_flags & NONSOLID) != 0);
}
BOOST_AUTO_TEST_CASE(simulation_targeting_searches_only_its_owner_and_clears_borrowed_targets)
{
    struct Primitives {
        PRIMITIVE *saved = GLOBAL(DisplayArray);
        Primitives() { GLOBAL(DisplayArray) = DisplayArray; }
        ~Primitives() { GLOBAL(DisplayArray) = saved; }
    } primitives;
    SimulationEngine engine, other; engine.initialize(3); other.initialize(1);
    RACE_DESC descriptor{}; descriptor.ship_info.crew_level = 5;
    STARSHIP vessel{}; vessel.RaceDescPtr = &descriptor;
    auto &tracker = add(engine); tracker.playerNr = 0; tracker.current.location = {0, 0};
    auto &target = add(engine); target.playerNr = 1; target.state_flags = PLAYER_SHIP;
    target.current.location = {100, 0}; target.pParent = &vessel;
    SetPrimType(&DisplayArray[0], STAMP_PRIM);
    auto &foreign = add(other); foreign.playerNr = 1; foreign.state_flags = PLAYER_SHIP; foreign.pParent = &vessel;
    COUNT facing = 0;
    BOOST_TEST(engine.track(tracker, facing) >= 0);
    BOOST_TEST(tracker.hTarget == &target);
    engine.clearTargets(target); BOOST_TEST(tracker.hTarget == nullptr);
    target.life_span = 0;
    BOOST_TEST(engine.track(tracker, facing) == -1);
    BOOST_TEST(tracker.hTarget == nullptr);
}

BOOST_AUTO_TEST_CASE(modes_own_distinct_engines_and_dispatch_binds_each_one)
{
    auto state = std::make_shared<GameState>(UserProfile{SinglePlayerId, "Engine test"});
    GameSession session(state, SinglePlayerId);
    auto *previous = &simulationEngine();
    struct Mode final : GameMode {
        Mode(std::shared_ptr<GameState> state) : GameMode(std::move(state), SinglePlayerId) {}
        ModeResult run() override {
            BOOST_TEST(&simulationEngine() == &engine());
            add(engine());
            return SessionExit::title();
        }
    };
    auto first = std::make_unique<Mode>(state), second = std::make_unique<Mode>(state);
    BOOST_TEST(&first->engine() != &second->engine());
    auto &entity = add(first->engine());
    const auto id = combat::EntityHandle::capture(detail::SimulationAccess::queue(first->engine()), entity);
    BOOST_TEST(!second->engine().resolve(id));
    const auto result = session.run(std::move(first));
    BOOST_TEST(static_cast<int>(result.kind) == static_cast<int>(SessionExitKind::ReturnToTitle));
    BOOST_TEST(second->engine().size() == 0u);
    BOOST_TEST(&simulationEngine() == previous);
}
