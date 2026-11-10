#ifndef UQM_PLANET_RENDER_LEGACY_HPP_
#define UQM_PLANET_RENDER_LEGACY_HPP_
#include "planet_render.hpp"
#include "planets/planets.h"
namespace uqm { class World; }
namespace uqm::detail {
// Only generation/drawing compatibility code may mutate the owning record.
struct PlanetRenderAccess {
    static PLANET_RENDER_DATA &data(PlanetRenderResources &owner) noexcept { return owner.data_; }
};
// system and world must outlive this borrow; do not replace their layout while
// it is active. Regenerates on entry because story/scan/lighting
// services still rebuild per visit. Destruction detaches the borrow, not assets.
class PlanetRenderBorrow {
public:
    PlanetRenderBorrow(SOLARSYS_STATE &system, World &world);
    ~PlanetRenderBorrow();
    PlanetRenderBorrow(const PlanetRenderBorrow &) = delete;
    PlanetRenderBorrow &operator=(const PlanetRenderBorrow &) = delete;
private:
    SOLARSYS_STATE &system_;
};
}
#endif
