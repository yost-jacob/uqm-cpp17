#ifndef UQM_COMBAT_ENTITY_HPP_
#define UQM_COMBAT_ENTITY_HPP_
#include "element.h"

namespace uqm::detail { struct EntityAccess; }
namespace uqm::combat {
// Common lifecycle for objects participating in the combat simulation. The
// engine still owns live kinematics and safe removal timing in this increment.
class CombatEntity {
public:
    virtual ~CombatEntity() = default;
    CombatEntity(const CombatEntity &) = delete;
    CombatEntity &operator=(const CombatEntity &) = delete;
    // self is this entity's live engine record, borrowed only for this call.
    // Runs before movement integration; do not free self during a hook.
    virtual void update(ELEMENT &self) = 0;
    // Optional stages. Expiration may start a death animation by extending life;
    // destruction itself only releases resources and never spawns game objects.
    virtual bool hasPostprocess() const noexcept { return false; }
    virtual void postprocess(ELEMENT &) {}
    virtual void expire(ELEMENT &) {}
    // Contact records/points are borrowed for this collision only.
    virtual void collide(ELEMENT &, POINT &, ELEMENT &, POINT &) {}
protected:
    CombatEntity() = default;
private:
    friend struct uqm::detail::EntityAccess;
    // Compatibility callbacks are confined to the engine publishing boundary.
    ElementProcessFunc *visualUpdate_ = nullptr;
    ElementCollisionFunc *legacyCollision_ = nullptr;
};
}
#endif
