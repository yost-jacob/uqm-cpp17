#include "uqm/world_visit.hpp"
#include "uqm/orbit_presentation.hpp"
#include "uqm/orbit_runtime.hpp"
#include "uqm/planet_sphere.hpp"
#include "libs/strings/strintrn.h"
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
extern std::function<void *(RESOURCE)> testResourceLookup;
extern std::function<void(FONT, FRAME, STRING)> testDiscoveryReport;
extern std::size_t liveTestDrawables() noexcept;
extern unsigned destroyedTestFonts() noexcept;
namespace {
std::function<void()> orbitMenu, surfaceLoader;
unsigned orbitFinishes = 0;
int batchDepth = 1;
struct Hooks {
    ~Hooks() { testOrbitGeneration = {}; orbitMenu = {}; surfaceLoader = {}; testResourceLookup = {}; testDiscoveryReport = {}; batchDepth = 1; }
};
WorldDefinition definition(UniverseId universe, BYTE special, unsigned planet, int moon = -1) {
    return {{universe, StarId{0}, static_cast<std::uint16_t>(planet),
        moon < 0 ? std::nullopt : std::optional<std::uint16_t>{static_cast<std::uint16_t>(moon)}},
        moon < 0 ? WorldKind::Planet : WorldKind::Moon, 12, false, {}, 0, special};
}
}
extern "C" void BatchGraphics() { ++batchDepth; }
extern "C" void UnbatchGraphics() { --batchDepth; }
void uqm::OrbitRuntime::run() { if (orbitMenu) orbitMenu(); }
void uqm::UninitSphereRotation(PLANET_ORBIT &) { ++orbitFinishes; }
void uqm::loadPlanetSurface(SOLARSYS_STATE &, World &, FRAME) { if (surfaceLoader) surfaceLoader(); }
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
    for (const auto action : {0u, 1u, 2u, 3u}) {
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
                runtime.catalogueStar = GetStar(0);
                runtime.system = std::make_unique<SolarSystem>(StarId{0}, session.worldOwner(),
                    std::vector<WorldDefinition>{definition(session.world().id(), 0, 0)});
                runtime.system->selectWorld(runtime.system->world(0).id());
                auto *source = &engine();
                unsigned generations = 0;
                testOrbitGeneration = [&](SOLARSYS_STATE &prepared) {
                    ++generations;
                    if (action != 3) prepared.graphics->TopoFrame = CaptureDrawable(static_cast<DRAWABLE>(LoadGraphicInstance("test.orbit")));
                    auto element = engine().allocateElement();
                    BOOST_REQUIRE(element);
                    engine().enqueue(element->get());
                };
                surfaceLoader = [&] { if (action == 3) throw std::runtime_error("partial surface failure"); };
                orbitMenu = [&] {
                    BOOST_TEST(&simulationEngine() != source);
                    BOOST_TEST(simulationEngine().size() == 1u);
                    BOOST_TEST(source->size() == 0u);
                    BOOST_CHECK(runtime.view == ExplorationView::Orbit);
                    BOOST_TEST(runtime.visit != nullptr);
                    BOOST_TEST(&orbitRuntime(system).session() == &session);
                    BOOST_TEST(&orbitRuntime(system).engine() == &simulationEngine());
                    {
                        GameSession other{std::make_shared<GameState>(UserProfile{SinglePlayerId, "Other"}), SinglePlayerId};
                        SessionActivation unrelated{other};
                        BOOST_TEST(&orbitRuntime(system).session() == &session);
                        BOOST_TEST(&orbitRuntime(system).campaign().activity != &GLOBAL(CurrentActivity));
                        BOOST_TEST(orbitRuntime(system).campaign().activity == IN_INTERPLANETARY);
                    }
                    BOOST_TEST(orbitRuntime(system).scan().location.x == 0);
                    orbitRuntime(system).scan().location = {17, 23};
                    orbitRuntime(system).scan().eraseFrame.reset(CaptureDrawable(static_cast<DRAWABLE>(LoadGraphicInstance("test.cursor"))));
                    BOOST_TEST(&systemVisit(system) == runtime.visit);
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
                        BOOST_TEST(orbitRuntime(system).scan().location.x == 17);
                    }
                    if (action == 1) session.requestLoad(2);
                    if (action == 2) throw std::runtime_error("orbit menu failure");
                };
                const auto finished = orbitFinishes;
                const auto initialFrames = liveTestDrawables();
                try {
                    { WorldVisit visit{session, system, runtime.system->world(0)}; BOOST_TEST(visit.run() == (action == 0)); }
                } catch (...) {
                    BOOST_TEST(system.graphics == nullptr);
                    BOOST_TEST(system.SysInfo.PlanetInfo.data == nullptr);
                    BOOST_TEST(runtime.visit == nullptr);
                    BOOST_CHECK_THROW(orbitRuntime(system), std::logic_error);
                    BOOST_TEST(engine().size() == 1u);
                    BOOST_TEST(orbitFinishes == finished + 1);
                    BOOST_TEST(liveTestDrawables() == initialFrames + (action == 3 ? 0u : 1u));
                    BOOST_TEST(!std::get<SolarSystemLocation>(session.player().location()).orbit);
                    throw;
                }
                BOOST_TEST(generations == 1u);
                BOOST_TEST(engine().size() == 1u);
                BOOST_TEST(system.graphics == nullptr);
                BOOST_TEST(system.SysInfo.PlanetInfo.data == nullptr);
                BOOST_TEST(runtime.visit == nullptr);
                BOOST_TEST(orbitFinishes == finished + 1);
                BOOST_TEST(liveTestDrawables() == initialFrames + 1u);
                BOOST_CHECK_THROW(systemVisit(system), std::logic_error);
                BOOST_TEST(!std::get<SolarSystemLocation>(session.player().location()).orbit);
                BOOST_TEST(resumes == (action == 0 ? 1u : 0u));
                testOrbitGeneration = {}; orbitMenu = {};
                if (auto exit = session.menuExit()) return *exit;
                return ModeCompleted{};
            }
            void resume() override { ++resumes; BOOST_TEST(&simulationEngine() == &engine()); }
        };
        auto result = session.run(std::make_unique<Explore>(session, action));
        const auto expected = action == 1 ? SessionExitKind::LoadGame : action >= 2 ? SessionExitKind::Failed : SessionExitKind::ReturnToTitle;
        BOOST_CHECK(result.kind == expected);
    }
}

// Device-free dependencies for the actual inline event scripts. These tests
// exercise event routing and ownership, not terrain algorithms or UI drawing.
extern "C" {
const PlanetFrame *PlanData = nullptr;
SIZE EncounterRace = 0;
BYTE EncounterGroup = 0;
void RepairSISBorder() {}
void DrawHyperCoords(POINT) {}
void DrawSISMessage(const UNICODE *) {}
DWORD FadeScreen(ScreenFadeType, SIZE) { return 0; }
void DoPlanetaryAnalysis(SYSTEM_INFO *, const PLANET_DESC *, RandomContext *, const PlanetAnalysisInput *) {}
COUNT GenerateLifeForms(const SYSTEM_INFO *, COUNT, NODE_INFO *, RandomContext *rng, EXTENT mapSize) { return 0; }
COUNT GenerateMineralDeposits(const SYSTEM_INFO *, COUNT, NODE_INFO *, RandomContext *rng, EXTENT mapSize) { return 0; }
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
        runtime.catalogueStar = GetStar(0);
        std::vector<WorldDefinition> definitions{definition(session.world().id(), special, 0)};
        if (special == CHMMR_DEFINED) definitions.push_back(definition(session.world().id(), special, 1));
        runtime.system = std::make_unique<SolarSystem>(StarId{0}, session.worldOwner(), definitions);
        auto &world = runtime.system->world(special == CHMMR_DEFINED ? 1 : 0);
        runtime.system->selectWorld(world.id());
        system.PlanetDesc[world.id().planet].pPrevDesc = system.SunDesc;
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
            BOOST_TEST(world.generatedData().PlanetRadius == 38u);
            BOOST_TEST(world.generatedData().SurfaceTemperature == 165);
        }
    }
}

BOOST_AUTO_TEST_CASE(planet_reports_cycle_consume_and_release_resources_even_on_display_failure)
{
    Hooks hooks;
    const auto frames = liveTestDrawables();
    const auto fonts = destroyedTestFonts();
    testResourceLookup = [](RESOURCE) -> void * { return AllocStringTable(3); };
    std::vector<unsigned> seen;
    testDiscoveryReport = [&](FONT font, FRAME effect, STRING text) {
        BOOST_TEST(font != nullptr);
        BOOST_TEST(effect != nullptr);
        BOOST_TEST(batchDepth == 0);
        seen.push_back(GetStringTableIndex(text));
    };
    {
        PlanetReport report;
        report.load("test.report");
        BOOST_TEST(report.size() == 3u);
        report.select(1);
        BOOST_TEST(report.showLander(nullptr, true));
        BOOST_TEST(report.showLander(nullptr, true));
        BOOST_TEST(report.empty());
        BOOST_TEST(!report.showLander(nullptr, true));
        BOOST_TEST(batchDepth == 1);
        BOOST_REQUIRE(seen.size() == 2u);
        BOOST_TEST(seen[0] == 1u);
        BOOST_TEST(seen[1] == 2u);
        report.load("test.report");
        BOOST_TEST(report.showLander(nullptr, false));
        BOOST_TEST(report.empty());
        // Replacing text keeps one font/effect pair for this visit.
        report.load("test.report");
        report.load("test.report");
        BOOST_TEST(liveTestDrawables() == frames + 1);
        testDiscoveryReport = [](FONT, FRAME, STRING) { throw std::runtime_error("display failure"); };
        BOOST_CHECK_THROW(report.showLander(nullptr, false), std::runtime_error);
        BOOST_TEST(batchDepth == 1);
        BOOST_TEST(!report.empty()); // A failed display did not consume the report.
    }
    BOOST_TEST(liveTestDrawables() == frames);
    BOOST_TEST(destroyedTestFonts() == fonts + 1);
}

BOOST_AUTO_TEST_CASE(report_only_orbit_script_releases_its_assets_without_a_surface_or_state_publication)
{
    for (const auto outcome : {0u, 1u, 2u}) {
        Hooks hooks;
        GameSession session{std::make_shared<GameState>(UserProfile{SinglePlayerId, "Visitor"}), SinglePlayerId};
        std::ifstream stars{UQM_TEST_STARMAP_PATH};
        detail::DefinitionsAccess::loadStars(detail::DefinitionsAccess::storage(session), stars);
        SessionActivation activation{session};
        GLOBAL(CurrentActivity) = IN_INTERPLANETARY;
        session.player().enterSolarSystem();
        SimulationEngine engine;
        UQM_SYSTEM_RUNTIME runtime{session.player().story(), engine, session.worldOwner(), nullptr, &session};
        auto &system = runtime.engine;
        system.worlds = &runtime;
        runtime.catalogueStar = GetStar(0);
        auto first = definition(session.world().id(), CHMMR_DEFINED, 0);
        auto parent = definition(session.world().id(), CHMMR_DEFINED, 1);
        parent.moonCount = 1;
        runtime.system = std::make_unique<SolarSystem>(StarId{0}, session.worldOwner(), std::vector<WorldDefinition>{first, parent});
        std::vector<std::unique_ptr<World>> moons;
        moons.push_back(std::make_unique<World>(definition(session.world().id(), CHMMR_DEFINED, 1, 0), session.worldOwner()));
        runtime.system->world(1).replaceChildren(std::move(moons));
        auto &world = runtime.system->world(1).child(0);
        runtime.system->selectWorld(world.id());
        system.PlanetDesc[1].pPrevDesc = system.SunDesc;
        runtime.system->world(1).projectMoons(system, system.PlanetDesc[1]);
        testResourceLookup = [](RESOURCE) -> void * { return AllocStringTable(1); };
        const auto frames = liveTestDrawables();
        const auto fonts = destroyedTestFonts();
        const auto finishes = orbitFinishes;
        unsigned shown = 0;
        testDiscoveryReport = [&](FONT, FRAME, STRING) {
            ++shown;
            BOOST_TEST(runtime.visit != nullptr);
            BOOST_TEST(session.player().story().read(StoryFlag::ChmmrFreed) == false);
            if (outcome == 1) session.requestLoad(2);
            if (outcome == 2) throw std::runtime_error("report cancelled");
        };
        if (outcome == 2) {
            BOOST_CHECK_THROW((WorldVisit{session, system, world}.run()), std::runtime_error);
        } else {
            WorldVisit visit{session, system, world};
            BOOST_TEST(visit.run() == (outcome == 0));
            BOOST_TEST(!world.renderResources().hasSurface());
        }
        BOOST_TEST(shown == 1u);
        BOOST_TEST(liveTestDrawables() == frames);
        BOOST_TEST(destroyedTestFonts() == fonts + 1);
        BOOST_TEST(orbitFinishes == finishes); // No orbit display was created.
        BOOST_TEST(runtime.visit == nullptr);
        BOOST_TEST(system.SysInfo.PlanetInfo.data == nullptr);
        BOOST_TEST(!std::get<SolarSystemLocation>(session.player().location()).orbit);
        BOOST_TEST(session.player().story().read(StoryFlag::ChmmrFreed) == false);
    }
}
