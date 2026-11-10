#ifndef UQM_PLANET_RENDER_HPP_
#define UQM_PLANET_RENDER_HPP_
#include "planet_render_data.h"
namespace uqm {
namespace detail { struct PlanetRenderAccess; }
// One body's generated graphics and backing buffers. Owned lazily by World,
// independent of the current orbit/scan/lander view. Never writes game state.
class PlanetRenderResources {
public:
    PlanetRenderResources() = default;
    ~PlanetRenderResources();
    PlanetRenderResources(const PlanetRenderResources &) = delete;
    PlanetRenderResources &operator=(const PlanetRenderResources &) = delete;
    // Release prior generated art before regenerating this body on a later visit.
    // Existing frame/buffer borrows must have ended. Safe for partial generation.
    void reset() noexcept;
    bool hasSurface() const noexcept { return data_.TopoFrame != nullptr; }
private:
    friend struct detail::PlanetRenderAccess;
    PLANET_RENDER_DATA data_{};
};
}
#endif
