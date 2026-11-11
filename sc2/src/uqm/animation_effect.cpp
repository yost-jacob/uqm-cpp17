#include "animation_effect.hpp"
#include "combat_entity_legacy.hpp"
#include <cassert>
namespace uqm::combat {
void attachAnimation(ELEMENT &self) {
    auto *existing = uqm::detail::borrowEntity(disp_q, &self);
    assert(!existing || dynamic_cast<AnimationEffect *>(existing));
    if (!existing) uqm::detail::attachEntity(self, std::make_unique<AnimationEffect>());
    else uqm::detail::enableEntityHooks(self, uqm::detail::EntityHooks::Update);
}
void advanceAnimation(ELEMENT &self) {
    if (self.turn_wait) --self.turn_wait;
    else {
        self.next.image.frame = IncFrameIndex(self.current.image.frame);
        self.state_flags |= CHANGING;
        self.turn_wait = self.next_turn;
    }
}
}
