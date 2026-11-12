#include "uqm/world_visit.hpp"
#include "uqm/world.hpp"
#include "uqm/system_runtime.hpp"
#include "uqm/game_definitions_legacy.hpp"
#include "uqm/gendef.h"
#include "uqm/setup.h"
#include "uqm/world_legacy.hpp"
#include "uqm/planet_render_legacy.hpp"
#include <boost/test/unit_test.hpp>
#include <functional>
#include <fstream>
using namespace uqm;
extern std::function<void(SOLARSYS_STATE &)> testOrbitGeneration;
namespace {
std::function<void()> orbitMenu;
unsigned orbitFinishes = 0;
struct Hooks {
    ~Hooks() { testOrbitGeneration = {}; orbitMenu = {}; }
};
WorldDefinition definition(UniverseId universe, BYTE special, unsigned planet, int moon = -1) {
    return {{universe, StarId{0}, static_cast<std::uint16_t>(planet),
        moon < 0 ? std::nullopt : std::optional<std::uint16_t>{static_cast<std::uint16_t>(moon)}},
        moon < 0 ? WorldKind::Planet : WorldKind::Moon, 12, false, {}, 0, special};
}
}
extern "C" void PlanetOrbitMenu() { if (orbitMenu) orbitMenu(); }
extern "C" void FinishPlanetOrbit() { ++orbitFinishes; }
BOOST_AUTO_TEST_CASE(world_construction_selects_only_applicable_orbit_events)
{
    auto state = std::make_shared<WorldState>(UniverseId{900});
    const auto selected = [&](BYTE special, unsigned planet, int moon = -1) {
        World world{definition(state->id(), special, planet, moon), state};
        return world.orbitHandler().has_value();
    };
    BOOST_TEST(!selected(0, 0));
    BOOST_TEST(!selected(MELNORME0_DEFINED, 0));
    BOOST_TEST(!selected(CHMMR_DEFINED, 0));
    BOOST_TEST(selected(CHMMR_DEFINED, 1));
    BOOST_TEST(selected(CHMMR_DEFINED, 1, 0));
    BOOST_TEST(!selected(CHMMR_DEFINED, 1, 1));
    BOOST_TEST(selected(SOL_DEFINED, 7, 0)); // Sol customizes even ordinary analysis.
    BOOST_TEST(selected(TAALO_PROTECTOR_DEFINED, 1, 2));
    BOOST_TEST(!selected(TAALO_PROTECTOR_DEFINED, 1));
    BOOST_TEST(selected(BOMB_DEFINED, 5, 1));
    BOOST_TEST(!selected(BOMB_DEFINED, 0));
    BOOST_TEST(selected(SAMATRA_DEFINED, 4, 0));
    BOOST_TEST(!selected(SAMATRA_DEFINED, 4));
    BOOST_TEST(selected(SPATHI_DEFINED, 0, 0));
    BOOST_TEST(!selected(SPATHI_DEFINED, 0, 1));
    BOOST_TEST(selected(URQUAN_WRECK_DEFINED, 6));
    BOOST_TEST(!selected(URQUAN_WRECK_DEFINED, 5));
}
BOOST_AUTO_TEST_CASE(ordinary_orbit_child_returns_its_scene_and_unwinds_visits_on_load_or_failure)
{
    for (const auto action : {0u, 1u, 2u}) {
        Hooks hooks;
        GameSession session{std::make_shared<GameState>(UserProfile{SinglePlayerId, "Orbit pilot"}), SinglePlayerId};
        std::ifstream stars{UQM_TEST_STARMAP_PATH};
        detail::DefinitionsAccess::loadStars(detail::DefinitionsAccess::storage(session), stars);
        SessionActivation activation{session};
        struct Explore final : GameMode {
            GameSession &session;
            unsigned action;
            unsigned resumes = 0;
            Explore(GameSession &s, unsigned a) : GameMode(s.dispatcher()), session(s), action(a) {}
            ModeResult run() override {
                GLOBAL(CurrentActivity) = IN_INTERPLANETARY;
                session.player().enterSolarSystem();
                UQM_SYSTEM_RUNTIME runtime{session.player().story(), engine(), session.worldOwner(), nullptr, &session};
                auto &system = runtime.engine;
                system.worlds = &runtime;
                runtime.system = std::make_unique<SolarSystem>(StarId{0}, session.worldOwner(),
                    std::vector<WorldDefinition>{definition(session.world().id(), 0, 0)});
                runtime.system->selectWorld(runtime.system->world(0).id());
                auto *old = pSolarSysState;
                struct Restore { SOLARSYS_STATE *old; ~Restore() { pSolarSysState = old; } } restore{old};
                pSolarSysState = &system;
                auto *source = &engine();
                unsigned generations = 0;
                testOrbitGeneration = [&](SOLARSYS_STATE &prepared) {
                    ++generations;
                    prepared.graphics->TopoFrame = CaptureDrawable(static_cast<DRAWABLE>(LoadGraphicInstance("test.orbit")));
                    auto element = engine().allocateElement();
                    BOOST_REQUIRE(element);
                    engine().enqueue(element->get());
                };
                orbitMenu = [&] {
                    BOOST_TEST(&simulationEngine() != source);
                    BOOST_TEST(simulationEngine().size() == 1u);
                    BOOST_TEST(source->size() == 0u);
                    BOOST_CHECK(runtime.view == ExplorationView::Orbit);
                    if (action == 0) {
                        struct SurfaceChild final : GameMode {
                            SimulationEngine &source;
                            std::optional<SimulationSceneTransfer> transfer;
                            SurfaceChild(GameSession &session, SimulationEngine &engine)
                                : GameMode(session.dispatcher()), source(engine) {}
                            void initialize() override { transfer.emplace(source, engine()); }
                            ModeResult run() override {
                                BOOST_TEST(source.size() == 0u);
                                BOOST_TEST(engine().size() == 1u);
                                return ModeCompleted{};
                            }
                        };
                        auto &orbitalEngine = simulationEngine();
                        BOOST_TEST(!session.swapGameMode(std::make_unique<SurfaceChild>(session, orbitalEngine)));
                        BOOST_TEST(&simulationEngine() == &orbitalEngine);
                        BOOST_TEST(orbitalEngine.size() == 1u);
                    }
                    if (action == 1) session.requestLoad(2);
                    if (action == 2) throw std::runtime_error("orbit menu failure");
                };
                const auto finished = orbitFinishes;
                try {
                    { WorldVisit visit{session, system, runtime.system->world(0)}; BOOST_TEST(visit.run() == (action == 0)); }
                } catch (...) {
                    BOOST_TEST(system.graphics == nullptr);
                    BOOST_TEST(engine().size() == 1u);
                    BOOST_TEST(orbitFinishes == finished + 1);
                    BOOST_TEST(!std::get<SolarSystemLocation>(session.player().location()).orbit);
                    throw;
                }
                BOOST_TEST(generations == 1u);
                BOOST_TEST(engine().size() == 1u);
                BOOST_TEST(system.graphics == nullptr);
                BOOST_TEST(orbitFinishes == finished + 1);
                BOOST_TEST(!std::get<SolarSystemLocation>(session.player().location()).orbit);
                BOOST_TEST(resumes == (action == 0 ? 1u : 0u));
                testOrbitGeneration = {}; orbitMenu = {};
                if (auto exit = session.menuExit()) return *exit;
                return ModeCompleted{};
            }
            void resume() override { ++resumes; BOOST_TEST(&simulationEngine() == &engine()); }
        };
        auto result = session.run(std::make_unique<Explore>(session, action));
        const auto expected = action == 1 ? SessionExitKind::LoadGame : action == 2 ? SessionExitKind::Failed : SessionExitKind::ReturnToTitle;
        BOOST_CHECK(result.kind == expected);
    }
}

// Device-free dependencies for the actual inline event scripts. These tests
// exercise event routing and ownership, not terrain algorithms or UI drawing.
extern "C" {
SIZE EncounterRace = 0;
BYTE EncounterGroup = 0;
void LoadPlanet(FRAME) {}
void NotifyOthers(COUNT, BYTE) {}
void RepairSISBorder() {}
void DrawHyperCoords(POINT) {}
void DrawSISMessage(const UNICODE *) {}
DWORD FadeScreen(ScreenFadeType, SIZE) { return 0; }
void DoPlanetaryAnalysis(SYSTEM_INFO *, PLANET_DESC *) {}
COUNT GenerateLifeForms(const SYSTEM_INFO *, COUNT, NODE_INFO *) { return 0; }
COUNT GenerateMineralDeposits(const SYSTEM_INFO *, COUNT, NODE_INFO *) { return 0; }
UWORD CalcGravity(const PLANET_INFO *) { return 0; }
bool worldIsPlanet(const SOLARSYS_STATE *system, const PLANET_DESC *world) {
    return world->pPrevDesc == system->SunDesc;
}
COUNT planetIndex(const SOLARSYS_STATE *system, const PLANET_DESC *world) {
    return (worldIsPlanet(system, world) ? world : world->pPrevDesc) - system->PlanetDesc;
}
COUNT moonIndex(const SOLARSYS_STATE *system, const PLANET_DESC *world) { return world - system->MoonDesc; }
bool isNodeRetrieved(const PLANET_INFO *planet, BYTE scan, BYTE node) {
    return (planet->ScanRetrieveMask[scan] & (DWORD{1} << node)) != 0;
}
}

BOOST_AUTO_TEST_CASE(debug_world_inspection_uses_scripts_without_modes_or_player_location_changes)
{
    for (BYTE special : {BYTE{CHMMR_DEFINED}, BYTE{SOL_DEFINED}}) {
        Hooks hooks;
        GameSession session{std::make_shared<GameState>(UserProfile{SinglePlayerId, "Inspector"}), SinglePlayerId};
        std::ifstream stars{UQM_TEST_STARMAP_PATH};
        detail::DefinitionsAccess::loadStars(detail::DefinitionsAccess::storage(session), stars);
        SessionActivation activation{session};
        SimulationEngine engine{16};
        SimulationBinding simulation{engine};
        session.player().story().set(StoryFlag::ChmmrFreed);
        UQM_SYSTEM_RUNTIME runtime{session.player().story(), engine, session.worldOwner(), nullptr};
        auto &system = runtime.engine;
        system.worlds = &runtime;
        std::vector<WorldDefinition> definitions{definition(session.world().id(), special, 0)};
        if (special == CHMMR_DEFINED) definitions.push_back(definition(session.world().id(), special, 1));
        runtime.system = std::make_unique<SolarSystem>(StarId{0}, session.worldOwner(), definitions);
        auto &world = runtime.system->world(special == CHMMR_DEFINED ? 1 : 0);
        runtime.system->selectWorld(world.id());
        system.PlanetDesc[world.id().planet].pPrevDesc = system.SunDesc;
        auto *previous = pSolarSysState;
        struct Restore { SOLARSYS_STATE *value; ~Restore() { pSolarSysState = value; } } restore{previous};
        pSolarSysState = &system;
        const auto locationKind = session.player().location().index();
        const auto finishes = orbitFinishes;
        orbitMenu = [] { BOOST_FAIL("inspection opened an orbital menu"); };
        {
            detail::PlanetRenderBorrow graphics{system, world};
            WorldVisit::inspectPreparedWorld(session, system);
            BOOST_TEST(system.graphics != nullptr);
            BOOST_TEST(!world.renderResources().hasSurface());
        }
        BOOST_TEST(session.player().location().index() == locationKind);
        BOOST_TEST(engine.size() == 0u);
        BOOST_TEST(orbitFinishes == finishes);
        if (special == SOL_DEFINED) {
            BOOST_TEST(system.SysInfo.PlanetInfo.PlanetRadius == 38u);
            BOOST_TEST(system.SysInfo.PlanetInfo.SurfaceTemperature == 165);
        }
    }
}
