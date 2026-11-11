//Copyright Paul Reiche, Fred Ford. 1992-2002
// SPDX-License-Identifier: GPL-2.0-or-later
#include "planet_render_legacy.hpp"
#include "world.hpp"
#include "world_legacy.hpp"
#include "system_runtime.hpp"
#include "libs/memlib.h"
#include <stdexcept>
#include <utility>
#include <cstdlib>
namespace uqm {
namespace {
void releaseFrame(FRAME frame) noexcept { if (frame) DestroyDrawable(ReleaseDrawable(frame)); }
void releaseSurface(PLANET_RENDER_DATA &data) noexcept {
    releaseFrame(std::exchange(data.TopoFrame, nullptr));
    const auto orbit = std::exchange(data.Orbit, PLANET_ORBIT{});
    HFree(orbit.lpTopoData);
    releaseFrame(orbit.TopoZoomFrame);
    releaseFrame(orbit.SphereFrame);
    releaseFrame(orbit.TintFrame);
    releaseFrame(orbit.ObjectFrame);
    releaseFrame(orbit.WorkFrame);
    HFree(orbit.TopoColors);
    HFree(orbit.ScratchArray);
}
}
// The record is a legacy projection owned by the enclosing graphics owner.
// This scope owns its generated surface allocations; later rotation updates
// the same projection, so cleanup observes the final frame/buffer handles.
class PlanetRenderResources::Surface {
public:
    Surface(SOLARSYS_STATE &system, PLANET_DESC &descriptor, FRAME definition)
        : data_(*system.graphics) {
        try { GeneratePlanetSurface(&descriptor, definition); }
        catch (...) { releaseSurface(data_); throw; }
    }
    ~Surface() { releaseSurface(data_); }
    Surface(const Surface &) = delete;
    Surface &operator=(const Surface &) = delete;
private:
    PLANET_RENDER_DATA &data_;
};
PlanetRenderResources::PlanetRenderResources() = default;
void PlanetRenderResources::createSurface(SOLARSYS_STATE &system, PLANET_DESC &descriptor, FRAME definition) {
    if (system.graphics != &data_ || pSolarSysState != &system)
        throw std::logic_error("surface generation requires this owner's active graphics borrow");
    if (surface_ || hasSurface()) throw std::logic_error("planet surface already constructed");
    surface_ = std::make_unique<Surface>(system, descriptor, definition);
}
PlanetRenderResources::~PlanetRenderResources() { reset(); }
void PlanetRenderResources::reset() noexcept {
    surface_.reset();
    // Also release partially filled compatibility records (including test/debug callers).
    releaseSurface(data_);
    const auto old = std::exchange(data_, PLANET_RENDER_DATA{});
    for (auto frame : old.PlanetSideFrame) releaseFrame(frame);
    if (old.XlatRef) DestroyStringTable(ReleaseStringTable(old.XlatRef));
    if (old.OrbitalCMap) DestroyColorMap(ReleaseColorMap(old.OrbitalCMap));
}
namespace detail {
PlanetRenderBorrow::PlanetRenderBorrow(SOLARSYS_STATE &system, World &world) : system_(system) {
    if (system.graphics) throw std::logic_error("planet graphics already borrowed");
    auto &owner = world.renderResources();
    owner.reset();
    system.graphics = &PlanetRenderAccess::data(owner);
}
PlanetRenderBorrow::~PlanetRenderBorrow() { system_.graphics = nullptr; }
}
}

// C orbit loader boundary. Surface construction remains with the selected
// native world's graphics owner; no gameplay side effects occur in cleanup.
extern "C" void ConstructPlanetSurface(FRAME definition) {
    try {
        auto &system = *pSolarSysState;
        auto &world = uqm::orbitalWorld(system);
        world.renderResources().createSurface(system,
            uqm::detail::selectedDescriptor(system), definition);
    } catch (...) { std::abort(); }
}
