#ifndef UQM_BEAM_HPP_
#define UQM_BEAM_HPP_
#include "combat_entity.hpp"
#include <memory>
namespace uqm::combat {
// One-tick beam launch. origin/displacement use world units; displacement is
// measured from origin, before the muzzle offset. facing is a 16-step heading
// and muzzleOffset uses display pixels. All fields are value-initialized.
struct BeamLaunch {
    POINT origin{}, displacement{};
    ELEMENT_FLAGS flags = 0;
    SIZE player = 0;
    SIZE muzzleOffset = 0;
    COUNT facing = 0;
    Color color{};
};
// No autonomous steering: the engine expires the beam after one collision pass.
class Beam : public CombatEntity {
public:
    void update(ELEMENT &) override {}
    virtual bool hasExpiration() const noexcept { return false; }
    void collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) override;
};
// Publishing boundary for the existing engine/sprite representation. The active
// engine owns the returned allocation and its Beam behavior. The caller must
// enqueue it or free an unused/speculative shot. Exhaustion returns null.
// A supplied behavior transfers ownership even on exhaustion. Derived beams
// opt into expiration without replacing the owner after publication.
HELEMENT publishBeam(const BeamLaunch &launch, std::unique_ptr<Beam> behavior = {});
}
#endif
