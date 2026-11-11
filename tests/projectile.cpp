#include "uqm/projectile.hpp"
#include "uqm/projectile_legacy.hpp"
#include "uqm/sis_weapons.hpp"
#include "uqm/sis_legacy.hpp"
#include "uqm/fleet.hpp"
#include "uqm/game_state.hpp"
#include "uqm/globdata.h"
#include "uqm/weapon.h"
#include <boost/test/unit_test.hpp>
#include <stdexcept>
using namespace uqm::combat;
namespace {
unsigned trackingCalls = 0, visualCalls = 0;
bool targetPresent = true;
SIZE targetDelta = 1;
ProjectileDefinition projectileDefinition(unsigned period = 2) {
    ProjectileDefinition result{};
    result.stats = {20, 1, 1, 32};
    result.stats.flight.steeringPeriod = period;
    result.stats.flight.initialDelay = period ? period - 1 : 0;
    return result;
}
void visual(ELEMENT *) { ++visualCalls; }
struct Pool {
    QUEUE saved{};
    Pool() { SwapQueues(&saved, &disp_q); InitQueue(&disp_q, 2, QUEUE_ELEMENT); }
    ~Pool() { UninitQueue(&disp_q); SwapQueues(&saved, &disp_q); }
};
struct CountingProjectile final : Projectile {
    unsigned &destroyed;
    explicit CountingProjectile(unsigned &count) : Projectile(projectileDefinition(), {}), destroyed(count) {}
    ~CountingProjectile() override { ++destroyed; }
    void update(ELEMENT &) override {}
};
}
// Simulate target selection and graphics allocation only. Real factories,
// steering timers, velocity math, publishing adapter and pool ownership run.
extern "C" {

HELEMENT weapon_collision(ELEMENT *, POINT *, ELEMENT *, POINT *) { return nullptr; }
FRAME IncFrameIndex(FRAME frame) { return reinterpret_cast<FRAME>(reinterpret_cast<std::uintptr_t>(frame) + 1); }
COUNT GetFrameIndex(FRAME frame) { return static_cast<COUNT>(reinterpret_cast<std::uintptr_t>(frame)); }
FRAME SetAbsFrameIndex(FRAME, COUNT index) { return reinterpret_cast<FRAME>(std::uintptr_t(index)); }
SIZE TrackShip(ELEMENT *, COUNT *facing) {
    ++trackingCalls;
    if (!targetPresent) return -1;
    *facing = NORMALIZE_FACING(*facing + 1);
    return targetDelta;
}
HELEMENT initialize_missile(MISSILE_BLOCK *block) {
    const auto link = AllocLink(&disp_q);
    if (!link) return nullptr;
    auto &element = *reinterpret_cast<ELEMENT *>(link);
    element = {};
    element.current.location = {block->cx, block->cy};
    element.mass_points = block->damage;
    element.hit_points = block->hit_points;
    element.life_span = block->life;
    element.preprocess_func = block->preprocess_func;
    return link;
}
}
BOOST_AUTO_TEST_CASE(optional_sis_belongs_to_fleet_without_consuming_an_escort_slot)
{
    uqm::Fleet enemy{12};
    BOOST_TEST(!enemy.sis());
    uqm::Fleet campaign{12, uqm::SISPresence::Present};
    BOOST_REQUIRE(campaign.sis());
    auto &sis = campaign.sis()->get();
    sis.setFuel(1234); sis.setCrew(7);
    BOOST_TEST(campaign.size() == 0u);
    auto &player = uqm::gameSession().player();
    auto &record = *GetPlayerSISState();
    const auto saved = record;
    record.FuelOnBoard = 999;
    BOOST_TEST(player.sis().fuel() == 999u);
    player.sis().setCrew(31);
    BOOST_TEST(record.CrewEnlisted == 31u);
    BOOST_TEST(&player.sis() == &player.fleet().sis()->get());
    BOOST_TEST(sis.crew() == 7u);
    record = saved;
    campaign.clear(); // Escort clearing must not remove a borrowed flagship.
    BOOST_TEST(campaign.sis()->get().fuel() == 1234u);
}
BOOST_AUTO_TEST_CASE(sis_factory_derives_paired_mounts_damage_and_tracking_from_loadout)
{
    uqm::SISLoadout loadout{};
    loadout.modules.fill(EMPTY_SLOT + 2);
    loadout.modules[15] = GUN_WEAPON;
    loadout.modules[14] = BLASTER_WEAPON;
    loadout.modules[13] = CANNON_WEAPON;
    loadout.modules[0] = GUN_WEAPON;
    loadout.modules[5] = loadout.modules[6] = loadout.modules[7] = TRACKING_SYSTEM;
    const auto weapons = configureSISWeapons(loadout, {20, 1, 1, 32});
    BOOST_TEST(weapons.mounts.size() == 6u);
    BOOST_TEST(weapons.trackingModules == 3u);
    BOOST_TEST(weapons.weaponEnergyCost == 14u);
    const COUNT directions[]{0, 1, 15, 4, 12, 8};
    const SIZE damage[]{1, 2, 2, 3, 3, 1};
    for (std::size_t i = 0; i < weapons.mounts.size(); ++i) {
        const auto &mount = weapons.mounts[i];
        const auto launch = mount.launch({1000, 1000}, 0);
        BOOST_TEST(launch.facing == directions[i]);
        BOOST_TEST(mount.factory().definition().stats.damage == damage[i]);
        BOOST_TEST(mount.factory().definition().stats.flight.steeringPeriod == 2u);
    }
    BOOST_TEST(weapons.mounts[3].factory().definition().stats.lifetime == 48u);
    BOOST_TEST(weapons.firingFlags == (FIRES_FORE | FIRES_AFT | FIRES_LEFT | FIRES_RIGHT));
}
BOOST_AUTO_TEST_CASE(sis_tracking_changes_interval_and_each_shot_owns_its_timer)
{
    for (unsigned trackers = 0; trackers <= 4; ++trackers) {
        uqm::SISLoadout loadout{}; loadout.modules.fill(EMPTY_SLOT + 2);
        loadout.modules[15] = GUN_WEAPON;
        for (unsigned i = 0; i < trackers; ++i) loadout.modules[4 + i] = TRACKING_SYSTEM;
        const auto mounts = configureSISWeapons(loadout, {20, 1, 1, 32});
        const auto &mount = mounts.mounts.front();
        const unsigned period = trackers ? 5 - std::min(3u, trackers) : 0;
        auto a = mount.factory().create(mount.launch({1000, 1000}, 0));
        auto b = mount.factory().create(mount.launch({1000, 1000}, 0));
        ELEMENT element{}; element.velocity = a->launch().velocity;
        trackingCalls = 0; targetPresent = true;
        for (unsigned tick = 0; tick < (period ? period - 1 : 8); ++tick) a->update(element);
        BOOST_TEST(trackingCalls == 0u);
        a->update(element);
        BOOST_TEST(trackingCalls == (period ? 1u : 0u));
        if (period) {
            BOOST_TEST(GetVelocityTravelAngle(&element.velocity) == FACING_TO_ANGLE(1));
            b->update(element);
            BOOST_TEST(trackingCalls == 1u);
        }
    }
}
BOOST_AUTO_TEST_CASE(projectile_mount_supplies_muzzle_direction_and_optional_inherited_velocity)
{
    const WeaponMount front{projectileDefinition(), 0, 28}, rear{projectileDefinition(), 8, 28};
    const auto a = front.launch({1000, 1000}, 2);
    const auto b = rear.launch({1000, 1000}, 2);
    BOOST_TEST(int(a.position.x) + b.position.x == 2000);
    BOOST_TEST(int(a.position.y) + b.position.y == 2000);
    BOOST_TEST(a.facing == 2u);
    BOOST_TEST(b.facing == 10u);
    VELOCITY_DESC shipVelocity{}; SetVelocityVector(&shipVelocity, 10, 0);
    auto inherited = front.launch({1000, 1000}, 2, shipVelocity).velocity;
    auto ordinary = a.velocity;
    SIZE dx1, dy1, dx2, dy2, sx, sy;
    GetCurrentVelocityComponents(&inherited, &dx1, &dy1);
    GetCurrentVelocityComponents(&ordinary, &dx2, &dy2);
    GetCurrentVelocityComponents(&shipVelocity, &sx, &sy);
    BOOST_TEST(dx1 == dx2 + sx);
    BOOST_TEST(dy1 == dy2 + sy);
}
BOOST_FIXTURE_TEST_CASE(projectile_pool_owns_behavior_through_free_reset_and_destruction, Pool)
{
    unsigned destroyed = 0;
    auto a = AllocLink(&disp_q), b = AllocLink(&disp_q);
    uqm::detail::bindEntity(disp_q, a, std::make_unique<CountingProjectile>(destroyed));
    uqm::detail::bindEntity(disp_q, b, std::make_unique<CountingProjectile>(destroyed));
    BOOST_REQUIRE(uqm::detail::borrowEntity(disp_q, a));
    ELEMENT prediction = *static_cast<ELEMENT *>(a);
    BOOST_TEST(uqm::detail::borrowEntity(disp_q, &prediction) == nullptr);
    FreeLink(&disp_q, a);
    BOOST_TEST(destroyed == 1u);
    BOOST_TEST(uqm::detail::borrowEntity(disp_q, a) == nullptr);
    ReinitQueue(&disp_q);
    BOOST_TEST(destroyed == 2u);
    auto c = AllocLink(&disp_q);
    uqm::detail::bindEntity(disp_q, c, std::make_unique<CountingProjectile>(destroyed));
    UninitQueue(&disp_q);
    BOOST_TEST(destroyed == 3u);
    InitQueue(&disp_q, 2, QUEUE_ELEMENT);
}
BOOST_FIXTURE_TEST_CASE(projectile_publish_preserves_motion_and_engine_updates_without_ship_dependency, Pool)
{
    std::unique_ptr<Projectile> shot;
    {
        WeaponMount mount{projectileDefinition(), 0, 28};
        shot = mount.factory().create(mount.launch({1000, 1000}, 0));
    } // Firing mount/factory can disappear before the projectile updates.
    FRAME sprites[3]{};
    const auto handle = publishProjectile(std::move(shot), sprites, 0, visual, nullptr);
    BOOST_REQUIRE(handle);
    auto &element = *static_cast<ELEMENT *>(handle);
    BOOST_TEST(element.mass_points == 1u);
    BOOST_TEST(element.life_span == 32u);
    trackingCalls = visualCalls = 0;
    element.preprocess_func(&element);
    element.preprocess_func(&element);
    BOOST_TEST(trackingCalls == 1u);
    BOOST_TEST(visualCalls == 2u);
    BOOST_TEST(GetVelocityTravelAngle(&element.velocity) == FACING_TO_ANGLE(1));
    FreeLink(&disp_q, handle);
    unsigned destroyed = 0;
    const auto a = AllocLink(&disp_q), b = AllocLink(&disp_q);
    BOOST_TEST(publishProjectile(std::make_unique<CountingProjectile>(destroyed), sprites, 0, nullptr, nullptr) == nullptr);
    BOOST_TEST(destroyed == 1u);
    FreeLink(&disp_q, a); FreeLink(&disp_q, b);
}
BOOST_AUTO_TEST_CASE(projectile_configuration_rejects_invalid_or_narrowing_tuning)
{
    auto value = projectileDefinition(); value.stats.hitPoints = 0;
    BOOST_CHECK_THROW(ProjectileFactory{value}, std::invalid_argument);
    uqm::SISLoadout loadout{}; loadout.modules.fill(EMPTY_SLOT + 2);
    loadout.modules[15] = CANNON_WEAPON;
    BOOST_CHECK_THROW(configureSISWeapons(loadout, {20, 100, 100, 32}), std::out_of_range);
}

BOOST_AUTO_TEST_CASE(sis_combat_crew_distinguishes_captain_only_survival_from_defeat)
{
    uqm::SIS sis;
    sis.recordCombatCrew(1);
    BOOST_TEST(sis.alive());
    BOOST_TEST(sis.crew() == 0u);
    sis.recordCombatCrew(0);
    BOOST_TEST(!sis.alive());
    BOOST_TEST(sis.crew() == static_cast<COUNT>(~0u));
    sis.recordCombatCrew(18);
    BOOST_TEST(sis.crew() == 17u);
}

BOOST_AUTO_TEST_CASE(configured_projectile_accelerates_to_cap_and_updates_directional_sprite)
{
    auto definition = projectileDefinition(4);
    definition.stats.flight.acceleration = 4;
    definition.stats.flight.maxSpeed = 80;
    definition.stats.flight.directionalSprite = true;
    WeaponMount mount{definition, 0, 0};
    auto shot = mount.factory().create(mount.launch({0, 0}, 0));
    ELEMENT element{};
    element.velocity = shot->launch().velocity;
    trackingCalls = 0; targetPresent = true;
    for (unsigned age = 0; age < 24; ++age) {
        element.life_span = definition.stats.lifetime - age;
        shot->update(element);
        VELOCITY_DESC expected{};
        SetVelocityVector(&expected, std::min(80, 20 + int(age) * 4), GetFrameIndex(element.next.image.frame));
        SIZE dx, dy, expectedX, expectedY;
        GetCurrentVelocityComponents(&element.velocity, &dx, &dy);
        GetCurrentVelocityComponents(&expected, &expectedX, &expectedY);
        BOOST_TEST(dx == expectedX);
        BOOST_TEST(dy == expectedY);
    }
    BOOST_TEST(trackingCalls == 6u);
    BOOST_TEST(GetFrameIndex(element.next.image.frame) == 6u);
    BOOST_TEST((element.state_flags & CHANGING) != 0);
}

BOOST_FIXTURE_TEST_CASE(projectile_virtual_postprocess_and_collision_use_the_owned_instance, Pool)
{
    struct Hooks final : Projectile {
        unsigned &posts, &collisions;
        Hooks(unsigned &p, unsigned &c) : Projectile(projectileDefinition(), {}), posts(p), collisions(c) {}
        void update(ELEMENT &) override {}
        bool hasPostprocess() const noexcept override { return true; }
        void postprocess(ELEMENT &element) override { ++posts; element.life_span = 7; }
        void collide(ELEMENT &self, POINT &, ELEMENT &, POINT &) override { ++collisions; self.hit_points = 3; }
    };
    unsigned posts = 0, collisions = 0;
    FRAME sprites[3]{};
    auto handle = publishProjectile(std::make_unique<Hooks>(posts, collisions), sprites, 0, nullptr, nullptr);
    BOOST_REQUIRE(handle);
    auto &element = *static_cast<ELEMENT *>(handle);
    BOOST_REQUIRE(element.postprocess_func);
    BOOST_REQUIRE(element.collision_func);
    element.postprocess_func(&element);
    ELEMENT target{}; POINT point{};
    element.collision_func(&element, &point, &target, &point);
    BOOST_TEST(posts == 1u);
    BOOST_TEST(collisions == 1u);
    BOOST_TEST(element.life_span == 7u);
    BOOST_TEST(element.hit_points == 3u);
}

BOOST_AUTO_TEST_CASE(configured_projectile_distinguishes_direct_and_gradual_guidance)
{
    targetPresent = true; targetDelta = 5;
    for (const bool snap : {false, true}) {
        auto definition = projectileDefinition(1);
        definition.stats.flight.snapToTarget = snap;
        WeaponMount mount{definition, 0, 0};
        auto shot = mount.factory().create(mount.launch({0, 0}, 0));
        ELEMENT element{}; element.velocity = shot->launch().velocity;
        shot->update(element);
        BOOST_TEST(GetVelocityTravelAngle(&element.velocity) == FACING_TO_ANGLE(snap ? 5 : 1));
        targetPresent = false;
        shot->update(element);
        BOOST_TEST(GetVelocityTravelAngle(&element.velocity) == FACING_TO_ANGLE(snap ? 5 : 1));
        targetPresent = true;
    }
    targetDelta = 1;
}
