#ifndef UQM_COMBAT_ENTITY_LEGACY_HPP_
#define UQM_COMBAT_ENTITY_LEGACY_HPP_
#include "combat_entity.hpp"
#include <memory>
namespace uqm::detail {
// Internal C-buffer services. Slots must belong to a live ELEMENT queue.
// A borrow ends at FreeLink/ReinitQueue/UninitQueue. ELEMENT copies do not copy
// ownership; relocations must explicitly bind a new native object.
void bindEntity(QUEUE &queue, HLINK link, std::unique_ptr<combat::CombatEntity> entity);
combat::CombatEntity *borrowEntity(const QUEUE &queue, HLINK link) noexcept;
enum class EntityHooks : unsigned { Update = 1, Postprocess = 2, Collision = 4, Expiration = 8 };
constexpr EntityHooks operator|(EntityHooks a, EntityHooks b) noexcept {
    return static_cast<EntityHooks>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}
// Install only selected stages, preserving other engine callbacks. element must
// have a native owner before the engine invokes these callbacks.
void enableEntityHooks(ELEMENT &element, EntityHooks hooks) noexcept;
// Transfers ownership to element's pool slot; the slot must not already have an
// owner. Bind only after successful engine allocation, before enqueueing it.
void attachEntity(ELEMENT &element, std::unique_ptr<combat::CombatEntity> entity,
    EntityHooks hooks = EntityHooks::Update);
struct EntityAccess {
    static void visual(combat::CombatEntity &, ElementProcessFunc *) noexcept;
    static void collision(combat::CombatEntity &, ElementCollisionFunc *) noexcept;
    static void advance(combat::CombatEntity &, ELEMENT &);
    static void collide(combat::CombatEntity &, ELEMENT &, POINT &, ELEMENT &, POINT &);
};
}
#endif
