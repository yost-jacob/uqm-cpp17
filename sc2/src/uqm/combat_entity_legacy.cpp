#include "combat_entity_legacy.hpp"
#include <utility>
namespace uqm::detail {
void EntityAccess::visual(combat::CombatEntity &entity, ElementProcessFunc *callback) noexcept { entity.visualUpdate_ = callback; }
void EntityAccess::collision(combat::CombatEntity &entity, ElementCollisionFunc *callback) noexcept { entity.legacyCollision_ = callback; }
void EntityAccess::advance(combat::CombatEntity &entity, ELEMENT &element) {
    if (entity.visualUpdate_) entity.visualUpdate_(&element);
    entity.update(element);
}
void EntityAccess::collide(combat::CombatEntity &entity, ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) {
    if (entity.legacyCollision_) entity.legacyCollision_(&self, &point, &other, &otherPoint);
    else entity.collide(self, point, other, otherPoint);
}
namespace {
void update(ELEMENT *element) {
    if (auto *entity = borrowEntity(disp_q, element)) EntityAccess::advance(*entity, *element);
}
void postprocess(ELEMENT *element) {
    if (auto *entity = borrowEntity(disp_q, element)) entity->postprocess(*element);
}
void expire(ELEMENT *element) {
    if (auto *entity = borrowEntity(disp_q, element)) entity->expire(*element);
}
void collide(ELEMENT *self, POINT *point, ELEMENT *other, POINT *otherPoint) {
    if (auto *entity = borrowEntity(disp_q, self)) EntityAccess::collide(*entity, *self, *point, *other, *otherPoint);
}
}
void enableEntityHooks(ELEMENT &element, EntityHooks hooks) noexcept {
    const auto has = [hooks](EntityHooks flag) { return (static_cast<unsigned>(hooks) & static_cast<unsigned>(flag)) != 0; };
    if (has(EntityHooks::Update)) element.preprocess_func = update;
    if (has(EntityHooks::Postprocess)) element.postprocess_func = postprocess;
    if (has(EntityHooks::Collision)) element.collision_func = collide;
    if (has(EntityHooks::Expiration)) element.death_func = expire;
}
void attachEntity(ELEMENT &element, std::unique_ptr<combat::CombatEntity> entity, EntityHooks hooks) {
    bindEntity(disp_q, &element, std::move(entity));
    enableEntityHooks(element, hooks);
}
}
