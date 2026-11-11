#ifndef UQM_PLANET_RENDER_HPP_
#define UQM_PLANET_RENDER_HPP_
#include "planet_render_data.h"
#include <memory>
struct solarsys_state;
struct planet_desc;
namespace uqm {
namespace detail { struct PlanetRenderAccess; }
// One body's generated graphics and backing buffers. Owned lazily by World,
// independent of the current orbit/scan/lander view. Never writes game state.
class PlanetRenderResources {
public:
    PlanetRenderResources();
    ~PlanetRenderResources();
    PlanetRenderResources(const PlanetRenderResources &) = delete;
    PlanetRenderResources &operator=(const PlanetRenderResources &) = delete;
    // Release prior generated art before regenerating this body on a later visit.
    // Existing frame/buffer borrows must have ended. Safe for partial generation.
    void reset() noexcept;
    // Construct owned terrain/globe resources after orbit rules select the
    // surface definition. system must currently borrow this graphics record;
    // descriptor and source art are borrowed only during generation.
    void createSurface(solarsys_state &system, planet_desc &descriptor, FRAME surfaceDefinition);
    bool hasSurface() const noexcept { return data_.TopoFrame != nullptr; }
private:
    friend struct detail::PlanetRenderAccess;
    class Surface;
    PLANET_RENDER_DATA data_{}; // Outlives Surface, including failed construction.
    std::unique_ptr<Surface> surface_;
};
}
#endif
