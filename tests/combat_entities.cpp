#include "uqm/combat_entity_legacy.hpp"
#include "uqm/entity_handle.hpp"
#include "uqm/animation_effect.hpp"
#include "uqm/gravity_field.hpp"
#include "uqm/units.h"
#include <boost/test/unit_test.hpp>
#include <stdexcept>
using namespace uqm::combat;
namespace {
struct Pool {
    QUEUE saved{};
    Pool() { SwapQueues(&saved, &disp_q); InitQueue(&disp_q, 3, QUEUE_ELEMENT); }
    ~Pool() { UninitQueue(&disp_q); SwapQueues(&saved, &disp_q); }
};
}
BOOST_FIXTURE_TEST_CASE(entity_handle_rejects_freed_reused_reset_and_destroyed_slots, Pool)
{
    const auto a = AllocLink(&disp_q);
    auto &element = *static_cast<ELEMENT *>(a);
    const auto original = EntityHandle::capture(disp_q, element);
    BOOST_REQUIRE(original.resolve(disp_q));
    BOOST_TEST(&original.resolve(disp_q)->get() == &element);
    ELEMENT prediction = element;
    BOOST_TEST(!EntityHandle::capture(disp_q, prediction).resolve(disp_q));
    FreeLink(&disp_q, a);
    BOOST_TEST(!original.resolve(disp_q));
    const auto reused = AllocLink(&disp_q);
    BOOST_TEST(reused == a);
    const auto replacement = EntityHandle::capture(disp_q, *static_cast<ELEMENT *>(reused));
    BOOST_REQUIRE(replacement.resolve(disp_q));
    BOOST_TEST(!original.resolve(disp_q));
    ReinitQueue(&disp_q);
    BOOST_TEST(!replacement.resolve(disp_q));
    const auto next = AllocLink(&disp_q);
    const auto beforeTeardown = EntityHandle::capture(disp_q, *static_cast<ELEMENT *>(next));
    UninitQueue(&disp_q);
    BOOST_TEST(!beforeTeardown.resolve(disp_q));
    InitQueue(&disp_q, 3, QUEUE_ELEMENT);
    AllocLink(&disp_q);
    BOOST_TEST(!beforeTeardown.resolve(disp_q));
    BOOST_TEST(!EntityHandle{}.resolve(disp_q));
}
BOOST_FIXTURE_TEST_CASE(entity_dispatch_handles_death_animation_and_preserves_ownership, Pool)
{
    struct Unit final : CombatEntity {
        unsigned &destroyed;
        bool dying = false;
        explicit Unit(unsigned &count) : destroyed(count) {}
        ~Unit() override { ++destroyed; }
        void update(ELEMENT &self) override { if (dying) self.hit_points = 0; }
        void expire(ELEMENT &self) override {
            dying = true;
            self.state_flags &= ~DISAPPEARING;
            self.life_span = 4;
            self.death_func = nullptr;
        }
        void collide(ELEMENT &self, POINT &, ELEMENT &, POINT &) override { self.hit_points = 7; }
    };
    unsigned destroyed = 0;
    auto handle = AllocLink(&disp_q);
    auto &self = *static_cast<ELEMENT *>(handle); self = {};
    using uqm::detail::EntityHooks;
    uqm::detail::attachEntity(self, std::make_unique<Unit>(destroyed),
        EntityHooks::Update | EntityHooks::Collision | EntityHooks::Expiration);
    auto *owner = uqm::detail::borrowEntity(disp_q, handle);
    POINT point{}; ELEMENT other{};
    self.collision_func(&self, &point, &other, &point);
    BOOST_TEST(self.hit_points == 7u);
    self.state_flags |= DISAPPEARING;
    self.death_func(&self);
    BOOST_TEST(self.life_span == 4u);
    BOOST_TEST((self.state_flags & DISAPPEARING) == 0);
    BOOST_TEST(uqm::detail::borrowEntity(disp_q, handle) == owner);
    BOOST_TEST(destroyed == 0u);
    self.preprocess_func(&self);
    BOOST_TEST(self.hit_points == 0u);
    FreeLink(&disp_q, handle);
    BOOST_TEST(destroyed == 1u);
}
BOOST_FIXTURE_TEST_CASE(shared_animation_effect_keeps_frame_delay_and_reuses_its_owner, Pool)
{
    auto handle = AllocLink(&disp_q);
    auto &self = *static_cast<ELEMENT *>(handle); self = {};
    self.current.image.frame = SetAbsFrameIndex(nullptr, 2);
    self.turn_wait = 1; self.next_turn = 2;
    attachAnimation(self);
    auto *owner = uqm::detail::borrowEntity(disp_q, handle);
    attachAnimation(self);
    BOOST_TEST(uqm::detail::borrowEntity(disp_q, handle) == owner);
    self.preprocess_func(&self);
    BOOST_TEST(self.turn_wait == 0u);
    BOOST_TEST((self.state_flags & CHANGING) == 0);
    self.preprocess_func(&self);
    BOOST_TEST(GetFrameIndex(self.next.image.frame) == 3u);
    BOOST_TEST(self.turn_wait == 2u);
    self.current = self.next;
    self.preprocess_func(&self); self.preprocess_func(&self); self.preprocess_func(&self);
    BOOST_TEST(GetFrameIndex(self.next.image.frame) == 4u);
    FreeLink(&disp_q, handle);
}
BOOST_AUTO_TEST_CASE(combat_gravity_defaults_boundaries_and_configurable_strength)
{
    GravityField stock;
    BOOST_TEST(stock.range() == 255);
    BOOST_TEST(stock.acceleration() == 1);
    BOOST_TEST(stock.contains(DISPLAY_TO_WORLD(255), 0));
    BOOST_TEST(!stock.contains(DISPLAY_TO_WORLD(256), 0));
    BOOST_TEST(stock.contains(DISPLAY_TO_WORLD(-180), DISPLAY_TO_WORLD(180)));
    BOOST_TEST(!stock.contains(DISPLAY_TO_WORLD(181), DISPLAY_TO_WORLD(181)));
    BOOST_TEST(!GravityField(0, 1).contains(0, 0));
    VELOCITY_DESC a{}, b{}, disabled{};
    stock.apply(a, 100, 0);
    GravityField(255, 3).apply(b, 100, 0);
    GravityField(255, 0).apply(disabled, 100, 0);
    SIZE ax, ay, bx, by, zx, zy;
    GetCurrentVelocityComponents(&a, &ax, &ay);
    GetCurrentVelocityComponents(&b, &bx, &by);
    GetCurrentVelocityComponents(&disabled, &zx, &zy);
    BOOST_TEST(bx == ax * 3);
    BOOST_TEST(ay == 0); BOOST_TEST(by == 0);
    BOOST_TEST(zx == 0); BOOST_TEST(zy == 0);
    BOOST_CHECK_THROW(GravityField(-1, 1), std::out_of_range);
    BOOST_CHECK_THROW(GravityField(1, -1), std::out_of_range);
    BOOST_CHECK_THROW(GravityField(1, 1024), std::out_of_range);
}
