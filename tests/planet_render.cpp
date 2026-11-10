#include "uqm/world.hpp"
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
