#ifndef UQM_ANIMATION_EFFECT_HPP_
#define UQM_ANIMATION_EFFECT_HPP_
#include "combat_entity.hpp"
namespace uqm::combat {
// Advances the next sprite at the existing engine delay. self must have valid
// sprite frames; turn_wait is the countdown and next_turn is its reset value.
// This also lets an autonomous entity reuse animation during its dying phase.
void advanceAnimation(ELEMENT &self);
// Binds an unowned effect to the native pool (or reuses its AnimationEffect).
// Other callbacks, sprite resources and lifetime remain untouched.
void attachAnimation(ELEMENT &self);
class AnimationEffect final : public CombatEntity {
public:
    void update(ELEMENT &self) override { advanceAnimation(self); }
};
}
#endif
