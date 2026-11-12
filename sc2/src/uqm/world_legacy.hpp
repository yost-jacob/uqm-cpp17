#ifndef UQM_WORLD_LEGACY_HPP_
#define UQM_WORLD_LEGACY_HPP_
#include "world.hpp"
#include "planets/planets.h"
namespace uqm::detail {
// Replace only physical fields; frame/color state belongs to the view.
void projectWorld(const World &world, PLANET_DESC &output, PLANET_DESC &parent);
WorldDefinition describeWorld(WorldId id, WorldKind kind, const PLANET_DESC &descriptor, std::uint8_t specialSystem = 0);
// Borrow one world's generated values into the C work record. The world and
// output outlive the scope; destruction restores only the prior pointer.
class WorldDataBorrow {
public:
    WorldDataBorrow(SYSTEM_INFO &output, World &world) noexcept
        : output_(output), previous_(output.PlanetInfo.data) { output_.PlanetInfo.data = &world.generatedData(); }
    ~WorldDataBorrow() { output_.PlanetInfo.data = previous_; }
    WorldDataBorrow(const WorldDataBorrow &) = delete;
    WorldDataBorrow &operator=(const WorldDataBorrow &) = delete;
private:
    SYSTEM_INFO &output_;
    PLANET_GENERATED_DATA *previous_;
};
// Compatibility views of generated body data, not owners. Moons must belong to
// the currently generated moon buffer. Borrow only while that view is loaded.
PLANET_DESC &worldDescriptor(SOLARSYS_STATE &system, const World &world);
const PLANET_DESC &worldDescriptor(const SOLARSYS_STATE &system, const World &world);
// Expected focus is a mode contract; absence throws bad_optional_access.
PLANET_DESC &selectedDescriptor(SOLARSYS_STATE &system);
const PLANET_DESC &selectedDescriptor(const SOLARSYS_STATE &system);
// Resolve a C callback argument once at the boundary. Foreign descriptors and
// unknown/generated-out-of-range bodies throw; no cross-array subtraction.
World &worldFromDescriptor(SOLARSYS_STATE &system, const PLANET_DESC &descriptor);
}
#endif
