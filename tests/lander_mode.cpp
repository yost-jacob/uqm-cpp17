#include "uqm/lander_runtime.hpp"
#include "uqm/inventory.hpp"
#include "uqm/sis_legacy.hpp"
#include "uqm/simulation_engine_legacy.hpp"
#include "uqm/resinst.h"
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <functional>
#include <stdexcept>
extern bool allowTestLanderSound;
extern std::string failedTestGraphic;
std::size_t liveTestDrawables() noexcept;
namespace {
using namespace uqm;
std::function<void(LanderVisit &)> kernel;
struct Fixture {
    GameSession session{std::make_shared<GameState>(UserProfile{SinglePlayerId, "Pilot"}), SinglePlayerId};
    SimulationEngine engine{16};
    UQM_SYSTEM_RUNTIME runtime{session.player().story(), engine, session.worldOwner(), nullptr, &session};
    Fixture() {
        allowTestLanderSound = true;
        auto &sis = detail::SISAccess::record(session.player().sis());
        std::fill(std::begin(sis.ModuleSlots), std::end(sis.ModuleSlots), EMPTY_SLOT + 2);
        sis.ModuleSlots[0] = CREW_POD;
        sis.ModuleSlots[1] = STORAGE_BAY;
        sis.CrewEnlisted = 20;
        sis.NumLanders = 2;
        session.player().enterSolarSystem();
        std::get<SolarSystemLocation>(session.player().location()).enterOrbit(
            {session.world().id(), StarId{0}, 0, {}});
        runtime.engine.worlds = &runtime;
    }
    ~Fixture() { allowTestLanderSound = false; failedTestGraphic.clear(); kernel = {}; }
};
}
// Real mode, runtime, resource leases, state transactions and scene handback;
// only the animation/input kernel is replaced.
void uqm::runLander(LanderVisit &visit) { if (kernel) kernel(visit); }

BOOST_FIXTURE_TEST_CASE(lander_recovery_is_owner_explicit_atomic_and_at_most_once, Fixture)
{
    GameSession other{std::make_shared<GameState>(UserProfile{SinglePlayerId, "Other"}), SinglePlayerId};
    SessionActivation unrelated{other};
    const auto before = liveTestDrawables();
    {
        LanderVisit visit{session, runtime.engine, engine, {10, 20}};
        BOOST_TEST(visit.embarkCrew());
        BOOST_TEST(visit.embarkCrew());
        BOOST_TEST(session.player().sis().crew() == 18u);
        BOOST_TEST(other.player().sis().crew() == 0u);
        visit.ElementAmounts[0] = 4; visit.ElementLevel = 4; visit.BiologicalLevel = 7;
        BOOST_TEST(session.player().sis().mineralMass() == 0u);
        BOOST_CHECK(visit.finish() == LanderOutcome::Returned);
        BOOST_TEST(session.player().sis().crew() == 20u);
        BOOST_TEST(session.player().sis().mineral(0) == 4u);
        BOOST_TEST(session.player().sis().mineralMass() == 4u);
        BOOST_TEST(session.player().sis().biomass() == 7u);
        BOOST_TEST(session.player().sis().landers() == 2u);
        BOOST_TEST(other.player().sis().mineralMass() == 0u);
        BOOST_CHECK_THROW(visit.finish(), std::logic_error);
        BOOST_TEST(liveTestDrawables() == before + 7u);
    }
    BOOST_TEST(!std::get<SolarSystemLocation>(session.player().location()).surface);
    BOOST_TEST(session.assets().size() == 0u);
    BOOST_TEST(liveTestDrawables() == before);
}
BOOST_FIXTURE_TEST_CASE(lander_loss_and_cancellation_do_not_award_the_collected_hold, Fixture)
{
    for (unsigned action : {0u, 1u, 2u, 3u}) {
        auto &sis = session.player().sis();
        sis.setCrew(20); sis.setLanders(2);
        detail::encounterLegacy(session.player()).activity = action == 1 ? CHECK_ABORT : action == 2 ? CHECK_LOAD : 0;
        {
            LanderVisit visit{session, runtime.engine, engine, {1, 2}};
            BOOST_TEST(visit.embarkCrew());
            visit.ElementAmounts[0] = 5; visit.ElementLevel = 5; visit.BiologicalLevel = 6;
            visit.crew_left = 0;
            if (action == 3) session.requestLoad(2);
            BOOST_CHECK(visit.finish() == (action ? LanderOutcome::Cancelled : LanderOutcome::Lost));
            BOOST_TEST(sis.landers() == (action ? 2u : 1u));
        }
        BOOST_TEST(sis.crew() == 19u);
        BOOST_TEST(sis.mineralMass() == 0u);
        BOOST_TEST(sis.biomass() == 0u);
    }
}
BOOST_FIXTURE_TEST_CASE(lander_failed_return_leaves_the_inventory_unchanged, Fixture)
{
    LanderVisit visit{session, runtime.engine, engine, {3, 4}};
    for (unsigned i = 0; i < LanderCrewColumns * LanderCrewRows; ++i) BOOST_TEST(visit.embarkCrew());
    BOOST_TEST(!visit.embarkCrew());
    BOOST_TEST(session.player().sis().crew() == 8u);
    visit.ElementAmounts[0] = 60000; visit.ElementLevel = 60000;
    visit.BiologicalLevel = 3;
    BOOST_CHECK_THROW(visit.finish(), std::logic_error);
    BOOST_TEST(session.player().sis().crew() == 8u);
    BOOST_TEST(session.player().sis().biomass() == 0u);
    BOOST_TEST(session.player().sis().mineralMass() == 0u);
    visit.ElementAmounts[0] = 2; visit.ElementLevel = 2;
    BOOST_CHECK(visit.finish() == LanderOutcome::Returned);
    BOOST_TEST(session.player().sis().crew() == 20u);
}
BOOST_FIXTURE_TEST_CASE(lander_mode_restores_location_assets_and_persistent_scene_on_every_exit, Fixture)
{
    for (unsigned action = 0; action < 5; ++action) {
        struct Parent final : GameMode {
            GameSession &session;
            unsigned action;
            Parent(GameSession &s, unsigned a) : GameMode(s.dispatcher()), session(s), action(a) {}
            ModeResult run() override {
                UQM_SYSTEM_RUNTIME context{session.player().story(), engine(), session.worldOwner(), nullptr, &session};
                auto &system = context.engine;
                system.worlds = &context;
                auto &location = std::get<SolarSystemLocation>(session.player().location());
                auto &node = engine().allocateElement()->get(); engine().enqueue(node);
                const auto before = liveTestDrawables();
                auto child = createLanderMode(session, system, engine(), {31, 41});
                BOOST_TEST(!location.surface); // Construction has no player-state effects.
                kernel = [&](LanderVisit &visit) {
                    BOOST_TEST(location.surface.has_value());
                    BOOST_TEST(visit.curLanderLoc.x == 31);
                    BOOST_TEST(&visit.simulation != &engine());
                    BOOST_TEST(engine().size() == 0u);
                    auto &effect = visit.simulation.allocateElement()->get();
                    effect.state_flags = FINITE_LIFE;
                    visit.simulation.enqueue(effect);
                    BOOST_TEST(visit.embarkCrew());
                    visit.ElementAmounts[0] = 3; visit.ElementLevel = 3;
                    if (action == 1) visit.crew_left = 0;
                    if (action == 2) session.requestLoad(3);
                    if (action == 3) throw std::runtime_error("lander kernel failed");
                    visit.finish();
                };
                if (action == 4) failedTestGraphic = LANDER_SHIELD_MASK_ANIM;
                MenuResult exit;
                try { exit = session.swapGameMode(std::move(child)); }
                catch (...) {
                    BOOST_TEST(engine().size() == 1u);
                    BOOST_TEST(!location.surface);
                    BOOST_TEST(liveTestDrawables() == before);
                    throw;
                }
                BOOST_TEST(engine().size() == 1u);
                BOOST_TEST(GetHeadLink(&detail::SimulationAccess::queue(engine())) == &node);
                BOOST_TEST(!location.surface);
                BOOST_TEST(location.orbit.has_value());
                BOOST_TEST(liveTestDrawables() == before);
                BOOST_TEST(session.assets().size() == 0u);
                if (exit) return *exit;
                return ModeCompleted{};
            }
        };
        // A separate session per case keeps a load/failure mailbox from one case
        // from steering the next expedition.
        Fixture scenario;
        const auto result = scenario.session.run(std::make_unique<Parent>(scenario.session, action));
        BOOST_CHECK(result.kind == (action == 2 ? SessionExitKind::LoadGame
            : action >= 3 ? SessionExitKind::Failed : SessionExitKind::ReturnToTitle));
    }
}
BOOST_FIXTURE_TEST_CASE(lander_effect_cleanup_uses_its_engine_with_another_engine_selected, Fixture)
{
    SimulationEngine other{8};
    SimulationBinding unrelated{other};
    auto &node = engine.allocateElement()->get(); engine.enqueue(node);
    auto &hazard = engine.allocateElement()->get(); hazard.state_flags = FINITE_LIFE; engine.enqueue(hazard);
    auto &untouched = other.allocateElement()->get(); untouched.state_flags = FINITE_LIFE; other.enqueue(untouched);
    removeLanderEffects(engine);
    BOOST_TEST(engine.size() == 1u); BOOST_TEST(other.size() == 1u);
    BOOST_TEST(&simulationEngine() == &other);
}
