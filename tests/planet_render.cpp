#include "uqm/world.hpp"
#include "uqm/world_legacy.hpp"
#include "uqm/system_behavior.hpp"
#include "uqm/gendef.h"
#include "uqm/game_state.hpp"
#include "uqm/system_runtime.hpp"
#include "uqm/planet_render_legacy.hpp"
#include "libs/strings/strintrn.h"
#include "libs/memlib.h"
#include <boost/test/unit_test.hpp>
#include <stdexcept>
using namespace uqm;
std::size_t liveTestDrawables() noexcept;
namespace {
FRAME frame() { return CaptureDrawable(static_cast<DRAWABLE>(LoadGraphicInstance("test.planet"))); }
WorldDefinition planet(unsigned slot = 0, unsigned moons = 0) {
    return {{UniverseId{501}, StarId{3}, static_cast<std::uint16_t>(slot), {}}, WorldKind::Planet, 123, false, "Planet", static_cast<std::uint16_t>(moons)};
}
std::unique_ptr<World> moon(const std::shared_ptr<WorldState> &state) {
    return std::make_unique<World>(WorldDefinition{{UniverseId{501}, StarId{3}, 0, 0}, WorldKind::Moon, 456, false, "Moon"}, state);
}
std::vector<std::unique_ptr<World>> moons(const std::shared_ptr<WorldState> &state) {
    std::vector<std::unique_ptr<World>> result; result.push_back(moon(state)); return result;
}
}
BOOST_AUTO_TEST_CASE(planet_graphics_release_partial_and_complete_generation_once)
{
    const auto initial = liveTestDrawables();
    PlanetRenderResources resources;
    auto &data = detail::PlanetRenderAccess::data(resources);
    BOOST_TEST(!resources.hasSurface());
    data.Orbit.WorkFrame = frame(); // Partial shield generation still has an owner.
    resources.reset();
    BOOST_TEST(liveTestDrawables() == initial);
    for (auto &sprite : data.PlanetSideFrame) sprite = frame();
    data.TopoFrame = frame();
    data.Orbit.TopoZoomFrame = frame();
    data.Orbit.SphereFrame = frame();
    data.Orbit.TintFrame = frame();
    data.Orbit.ObjectFrame = frame();
    data.Orbit.WorkFrame = frame();
    data.XlatRef = CaptureStringTable(AllocStringTable(1));
    data.OrbitalCMap = CaptureColorMap(AllocStringTable(1));
    data.XlatPtr = &data;
    data.Orbit.lpTopoData = static_cast<SBYTE *>(HMalloc(8));
    data.Orbit.TopoColors = static_cast<Color *>(HMalloc(3 * sizeof(Color)));
    data.Orbit.ScratchArray = static_cast<Color *>(HMalloc(4 * sizeof(Color)));
    BOOST_TEST(resources.hasSurface());
    BOOST_TEST(liveTestDrawables() == initial + 12u);
    resources.reset();
    BOOST_TEST(!resources.hasSurface());
    BOOST_TEST(liveTestDrawables() == initial);
    BOOST_TEST(data.XlatRef == nullptr);
    BOOST_TEST(data.XlatPtr == nullptr);
    BOOST_TEST(data.OrbitalCMap == nullptr);
    BOOST_TEST(data.Orbit.ScratchArray == nullptr);
    resources.reset();
    BOOST_TEST(liveTestDrawables() == initial);
}
BOOST_AUTO_TEST_CASE(world_owns_graphics_after_visit_and_detaches_failed_compatibility_borrows)
{
    const auto initial = liveTestDrawables();
    auto state = std::make_shared<WorldState>(UniverseId{501});
    SOLARSYS_STATE projection{};
    {
        World body{planet(), state}, other{planet(1), state};
        BOOST_TEST(!body.hasRenderResources());
        {
            detail::PlanetRenderBorrow visit{projection, body};
            BOOST_REQUIRE(projection.graphics);
            projection.graphics->TopoFrame = frame();
            BOOST_CHECK_THROW((detail::PlanetRenderBorrow{projection, other}), std::logic_error);
        }
        BOOST_TEST(projection.graphics == nullptr);
        BOOST_TEST(body.renderResources().hasSurface());
        BOOST_TEST(!other.hasRenderResources());
        BOOST_TEST(liveTestDrawables() == initial + 1u);
        try {
            detail::PlanetRenderBorrow next{projection, body};
            BOOST_TEST(!body.renderResources().hasSurface()); // Regenerate only this body.
            BOOST_TEST(liveTestDrawables() == initial);
            projection.graphics->Orbit.SphereFrame = frame();
            throw std::runtime_error("incomplete planet generation");
        } catch (const std::runtime_error &) {}
        BOOST_TEST(projection.graphics == nullptr);
        BOOST_TEST(liveTestDrawables() == initial + 1u);
    }
    BOOST_TEST(liveTestDrawables() == initial);
}
BOOST_AUTO_TEST_CASE(generated_descriptor_refresh_preserves_planet_and_moon_resource_identity)
{
    const auto initial = liveTestDrawables();
    auto state = std::make_shared<WorldState>(UniverseId{501});
    const std::vector<WorldDefinition> definitions{planet(0, 1), planet(1)};
    {
        SolarSystem system{StarId{3}, state, definitions};
        auto &body = system.world(0);
        body.replaceChildren(moons(state));
        auto *bodyIdentity = &body;
        auto *moonIdentity = &body.child(0);
        auto *graphicsIdentity = &body.renderResources();
        detail::PlanetRenderAccess::data(*graphicsIdentity).TopoFrame = frame();
        detail::PlanetRenderAccess::data(body.child(0).renderResources()).TopoFrame = frame();
        system.refreshPlanets(definitions);
        system.world(0).refreshChildren(moons(state));
        BOOST_TEST(&system.world(0) == bodyIdentity);
        BOOST_TEST(&system.world(0).child(0) == moonIdentity);
        BOOST_TEST(&system.world(0).renderResources() == graphicsIdentity);
        BOOST_TEST(!system.world(1).hasRenderResources());
        BOOST_TEST(liveTestDrawables() == initial + 2u);
        auto invalid = definitions; invalid[0].id.planet = 5;
        BOOST_CHECK_THROW(system.refreshPlanets(invalid), std::invalid_argument);
        BOOST_TEST(&system.world(0) == bodyIdentity);
        BOOST_TEST(liveTestDrawables() == initial + 2u);
        auto changed = definitions; ++changed[0].seed;
        system.refreshPlanets(changed);
        BOOST_TEST(!system.world(0).hasRenderResources());
        BOOST_TEST(liveTestDrawables() == initial);
        detail::PlanetRenderAccess::data(system.world(0).renderResources()).TopoFrame = frame();
        detail::PlanetRenderAccess::data(system.world(1).renderResources()).TopoFrame = frame();
    }
    BOOST_TEST(liveTestDrawables() == initial);
}

BOOST_AUTO_TEST_CASE(planet_constructor_owns_moons_and_projects_without_regenerating)
{
    class MoonBehavior final : public SystemBehavior {
    public:
        explicit MoonBehavior(StoryState &story) : SystemBehavior(story) {}
        unsigned calls = 0;
        bool generateMoons(SOLARSYS_STATE &system, PLANET_DESC &parent) override {
            ++calls;
            BOOST_TEST(parent.NumPlanets == 1u);
            system.MoonDesc[0].rand_seed = 456;
            system.MoonDesc[0].data_index = SELENIC_WORLD;
            system.MoonDesc[0].radius = 100;
            return true;
        }
    };
    GameState campaign{{SinglePlayerId, "Constructor test"}};
    MoonBehavior behavior{campaign.player(SinglePlayerId).story()};
    auto state = std::make_shared<WorldState>(UniverseId{501});
    SimulationEngine simulation;
    UQM_SYSTEM_RUNTIME runtime{campaign.player(SinglePlayerId).story(), simulation, state, nullptr};
    SOLARSYS_STATE projection{};
    projection.worlds = &runtime;
    auto &parent = projection.PlanetDesc[0];
    parent.NumPlanets = 1;
    parent.rand_seed = 123;
    World body{planet(0, 1), state, projection, parent, behavior};
    BOOST_TEST(body.childCount() == 1u);
    BOOST_TEST(body.child(0).definition().seed == 456u);
    projection.MoonDesc[0].rand_seed = 999; // Simulate another inner flight view.
    body.projectMoons(projection, parent);
    BOOST_TEST(projection.MoonDesc[0].rand_seed == 456u);
    BOOST_TEST(projection.MoonDesc[0].pPrevDesc == &parent);
    body.projectMoons(projection, parent);
    BOOST_TEST(behavior.calls == 1u);
}

namespace {
unsigned surfaceGenerations = 0;
bool failSurfaceGeneration = false;
}
// Device-free stand-in: exercise production construction and unwind ownership.
extern "C" void GeneratePlanetSurface(PLANET_DESC *, FRAME, RandomContext *rng) {
    ++surfaceGenerations;
    pSolarSysState->graphics->TopoFrame = frame();
    pSolarSysState->graphics->Orbit.SphereFrame = frame();
    if (failSurfaceGeneration) throw std::runtime_error("surface generation failed");
}
BOOST_AUTO_TEST_CASE(surface_constructor_owns_generation_and_unwinds_partial_failure)
{
    const auto initial = liveTestDrawables();
    GameState campaign{{SinglePlayerId, "Surface generation"}};
    SimulationEngine simulation;
    UQM_SYSTEM_RUNTIME runtime{campaign.player(SinglePlayerId).story(), simulation, campaign.worldOwner(), nullptr};
    SOLARSYS_STATE projection{};
    projection.worlds = &runtime;
    auto *previous = pSolarSysState;
    struct Restore { SOLARSYS_STATE *value; ~Restore() { pSolarSysState = value; } } restore{previous};
    pSolarSysState = &projection;
    PlanetRenderResources resources;
    projection.graphics = &detail::PlanetRenderAccess::data(resources);
    PLANET_DESC descriptor{};
    failSurfaceGeneration = true;
    BOOST_CHECK_THROW(resources.createSurface(projection, descriptor, nullptr), std::runtime_error);
    failSurfaceGeneration = false;
    BOOST_TEST(!resources.hasSurface());
    BOOST_TEST(liveTestDrawables() == initial);
    resources.createSurface(projection, descriptor, nullptr);
    BOOST_TEST(resources.hasSurface());
    BOOST_TEST(liveTestDrawables() == initial + 2u);
    BOOST_CHECK_THROW(resources.createSurface(projection, descriptor, nullptr), std::logic_error);
    resources.reset();
    BOOST_TEST(liveTestDrawables() == initial);
}

BOOST_AUTO_TEST_CASE(world_generated_values_have_one_owner_and_borrows_restore_on_unwind)
{
    auto state = std::make_shared<WorldState>(UniverseId{501});
    World first{planet(0), state}, second{planet(1), state};
    SYSTEM_INFO view{};
    view.PlanetInfo.ScanRetrieveMask[MINERAL_SCAN] = 17;
    auto &a = first.generatedData();
    auto &b = second.generatedData();
    a.PlanetRadius = 38;
    a.ScanSeed[MINERAL_SCAN] = 123;
    {
        detail::WorldDataBorrow outer{view, first};
        BOOST_TEST(view.PlanetInfo.data == &a);
        view.PlanetInfo.data->SurfaceTemperature = 165; // Script override writes the owner.
        try {
            detail::WorldDataBorrow inner{view, second};
            view.PlanetInfo.data->PlanetRadius = 100;
            view.PlanetInfo.data->ScanSeed[MINERAL_SCAN] = 456;
            throw std::runtime_error("cancelled preparation");
        } catch (const std::runtime_error &) {}
        BOOST_TEST(view.PlanetInfo.data == &a);
        BOOST_TEST(view.PlanetInfo.data->PlanetRadius == 38u);
        BOOST_TEST(view.PlanetInfo.data->ScanSeed[MINERAL_SCAN] == 123u);
    }
    BOOST_TEST(view.PlanetInfo.data == nullptr);
    BOOST_TEST(first.generatedData().SurfaceTemperature == 165);
    BOOST_TEST(second.generatedData().PlanetRadius == 100u);
    BOOST_TEST(second.generatedData().ScanSeed[MINERAL_SCAN] == 456u);
    BOOST_TEST(view.PlanetInfo.ScanRetrieveMask[MINERAL_SCAN] == 17u);
    BOOST_TEST(a.SurfaceTemperature == 165); // Ending a view did not destroy either record.
}
BOOST_AUTO_TEST_CASE(layout_reconstruction_preserves_matching_world_data_and_invalidates_changed_inputs)
{
    auto state = std::make_shared<WorldState>(UniverseId{501});
    auto description = planet(0, 1);
    description.bodyType = WATER_WORLD;
    description.orbitalDistance = EARTH_RADIUS;
    SolarSystem previous{StarId{3}, state, std::vector<WorldDefinition>{description}};
    previous.world(0).replaceChildren(moons(state));
    previous.world(0).generatedData().SurfaceTemperature = 22;
    previous.world(0).generatedData().ScanSeed[MINERAL_SCAN] = 1234;
    previous.world(0).child(0).generatedData().PlanetRadius = 25;
    auto *retained = &previous.world(0).generatedData();
    SolarSystem next{StarId{3}, state, std::vector<WorldDefinition>{description}};
    next.world(0).replaceChildren(moons(state));
    next.preserveGeneratedData(previous);
    BOOST_TEST(&next.world(0).generatedData() == retained);
    BOOST_TEST(next.world(0).generatedData().SurfaceTemperature == 22);
    BOOST_TEST(next.world(0).generatedData().ScanSeed[MINERAL_SCAN] == 1234u);
    BOOST_TEST(next.world(0).child(0).generatedData().PlanetRadius == 25u);
    description.orbitalDistance += 1;
    SolarSystem changed{StarId{3}, state, std::vector<WorldDefinition>{description}};
    changed.world(0).replaceChildren(moons(state));
    changed.preserveGeneratedData(next);
    BOOST_TEST(changed.world(0).generatedData().SurfaceTemperature == 0);
    BOOST_TEST(changed.world(0).child(0).generatedData().PlanetRadius == 0u);
}
