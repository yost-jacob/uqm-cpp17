#ifndef UQM_COMBAT_PLANET_HPP_
#define UQM_COMBAT_PLANET_HPP_
#include "combat_entity.hpp"
#include "gravity_field.hpp"
#include <functional>
#include <optional>
namespace uqm { class SimulationEngine; }
namespace uqm::combat {
struct PlanetProperties {
    GravityField gravity{};
    COUNT spriteIndex = 0;
};
class Planet final : public CombatEntity {
public:
    explicit Planet(PlanetProperties properties = {}) : properties_(properties) {}
    const PlanetProperties &properties() const noexcept { return properties_; }
    void update(ELEMENT &) override {} // Static body; physics runs after motion.
    void postprocess(ELEMENT &self) override;
    void collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) override;
private:
    PlanetProperties properties_;
};
// Creates and enqueues a pool-owned planet using the loaded combat planet art.
// Art supplies its pixel collision mask; spriteIndex selects a valid frame.
// engine and loaded art must outlive the planet. Returns a borrow until
// release/reset/shutdown, or nullopt on pool exhaustion.
std::optional<std::reference_wrapper<ELEMENT>> spawnPlanet(SimulationEngine &engine, PlanetProperties properties = {});
}
#endif
