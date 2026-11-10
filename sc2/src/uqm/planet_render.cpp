//Copyright Paul Reiche, Fred Ford. 1992-2002
// SPDX-License-Identifier: GPL-2.0-or-later
#include "planet_render_legacy.hpp"
#include "world.hpp"
#include "libs/memlib.h"
#include <stdexcept>
#include <utility>
namespace uqm {
namespace {
void releaseFrame(FRAME frame) noexcept { if (frame) DestroyDrawable(ReleaseDrawable(frame)); }
}
PlanetRenderResources::~PlanetRenderResources() { reset(); }
void PlanetRenderResources::reset() noexcept {
    const auto old = std::exchange(data_, PLANET_RENDER_DATA{});
    for (auto frame : old.PlanetSideFrame) releaseFrame(frame);
    if (old.XlatRef) DestroyStringTable(ReleaseStringTable(old.XlatRef));
    releaseFrame(old.TopoFrame);
    if (old.OrbitalCMap) DestroyColorMap(ReleaseColorMap(old.OrbitalCMap));
    HFree(old.Orbit.lpTopoData);
    releaseFrame(old.Orbit.TopoZoomFrame);
    releaseFrame(old.Orbit.SphereFrame);
    releaseFrame(old.Orbit.TintFrame);
    releaseFrame(old.Orbit.ObjectFrame);
    releaseFrame(old.Orbit.WorkFrame);
    HFree(old.Orbit.TopoColors);
    HFree(old.Orbit.ScratchArray);
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
