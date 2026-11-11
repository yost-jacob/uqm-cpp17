#ifndef UQM_WORLD_LEGACY_HPP_
#define UQM_WORLD_LEGACY_HPP_
#include "world.hpp"
#include "planets/planets.h"
namespace uqm::detail {
// Owned generated data; the legacy MoonDesc array is only a projected view.
struct WorldGeneration { std::vector<PLANET_DESC> moons; };
WorldDefinition describeWorld(WorldId id, WorldKind kind, const PLANET_DESC &descriptor);
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
