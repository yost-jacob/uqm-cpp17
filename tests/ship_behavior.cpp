#include "uqm/ship_behavior.hpp"
#include <boost/test/unit_test.hpp>
#include <type_traits>
#include <memory>

namespace {
using uqm::combat::Ship;
using uqm::combat::WeaponSlots;
struct Calls { unsigned before = 0, after = 0, weapons = 0, finish = 0, destroyed = 0; };
class TestShip final : public Ship {
public:
    TestShip(const RACE_DESC &definition, Calls &calls) : Ship(definition), calls_(calls) {}
    ~TestShip() override { ++calls_.destroyed; }
private:
    void beforeMotion(ELEMENT &element) noexcept override {
        ++calls_.before;
        element.turn_wait = 9;
        ++legacyData().ship_info.crew_level;
    }
    void afterMotion(ELEMENT &element) noexcept override {
        ++calls_.after;
        element.thrust_wait = 7;
    }
    COUNT createWeapons(ELEMENT &element, WeaponSlots &weapons) noexcept override {
        ++calls_.weapons;
        weapons[0] = &element; // A stable opaque test handle, never dereferenced as a queue link.
        return 1;
    }
    void onFinish() noexcept override { ++calls_.finish; }
    Calls &calls_;
};
}
BOOST_AUTO_TEST_CASE(native_ship_owns_an_independent_descriptor_with_stable_c_buffer_identity)
{
    static_assert(std::is_abstract_v<Ship>);
    static_assert(!std::is_copy_constructible_v<Ship>);
    RACE_DESC definition{};
    definition.ship_info.crew_level = 5;
    Calls firstCalls, secondCalls;
    auto first = std::make_unique<TestShip>(definition, firstCalls);
    auto second = std::make_unique<TestShip>(definition, secondCalls);
    BOOST_TEST(first->legacyData().ship_owner == static_cast<Ship *>(first.get()));
    first->legacyData().ship_info.crew_level = 3;
    BOOST_TEST(second->legacyData().ship_info.crew_level == 5u);
    BOOST_TEST(definition.ship_info.crew_level == 5u);
    BOOST_TEST(&uqm::combat::shipFor(first->legacyData()) == static_cast<Ship *>(first.get()));
}
BOOST_AUTO_TEST_CASE(native_ship_dispatch_reaches_overrides_and_clears_unused_weapon_slots)
{
    Calls calls;
    auto owner = std::make_unique<TestShip>(RACE_DESC{}, calls);
    auto *descriptor = &owner->legacyData();
    ELEMENT element{};
    WeaponSlots weapons;
    for (auto &weapon : weapons) weapon = descriptor;
    auto &ship = uqm::combat::shipFor(*descriptor);
    ship.preprocess(element);
    ship.postprocess(element);
    const auto count = ship.initializeWeapon(element, weapons);
    BOOST_TEST(count == 1u);
    BOOST_TEST(calls.before == 1u);
    BOOST_TEST(calls.after == 1u);
    BOOST_TEST(calls.weapons == 1u);
    BOOST_TEST(element.turn_wait == 9u);
    BOOST_TEST(element.thrust_wait == 7u);
    BOOST_TEST(descriptor->ship_info.crew_level == 1u);
    BOOST_TEST(weapons[0] == &element);
    for (unsigned i = 1; i < 6; ++i) BOOST_TEST(weapons[i] == nullptr);
    owner->finish();
    owner.reset();
    BOOST_TEST(calls.finish == 1u);
    BOOST_TEST(calls.destroyed == 1u);
}
BOOST_AUTO_TEST_CASE(ship_preprocessing_can_be_disabled_and_restored_without_replacing_callbacks)
{
    Calls calls;
    TestShip ship(RACE_DESC{}, calls);
    ELEMENT element{};
    ship.enablePreprocessing(false);
    ship.preprocess(element);
    BOOST_TEST(calls.before == 0u);
    BOOST_TEST(!ship.preprocessingEnabled());
    ship.enablePreprocessing(true);
    ship.preprocess(element);
    BOOST_TEST(calls.before == 1u);
    ship.postprocess(element);
    BOOST_TEST(calls.after == 1u); // Disabling preprocessing does not replace postprocessing.
}
BOOST_AUTO_TEST_CASE(ship_completion_is_idempotent_and_blocks_further_gameplay_hooks)
{
    Calls calls;
    {
        TestShip ship(RACE_DESC{}, calls);
        ELEMENT element{};
        WeaponSlots weapons;
        weapons.fill(&element);
        ship.finish();
        ship.finish();
        BOOST_TEST(calls.finish == 1u);
        ship.preprocess(element);
        ship.postprocess(element);
        BOOST_TEST(ship.initializeWeapon(element, weapons) == 0u);
        BOOST_TEST(calls.before == 0u);
        BOOST_TEST(calls.after == 0u);
        BOOST_TEST(calls.weapons == 0u);
        for (auto handle : weapons) BOOST_TEST(handle == nullptr);
    }
    BOOST_TEST(calls.finish == 1u);
    BOOST_TEST(calls.destroyed == 1u);
}

BOOST_AUTO_TEST_CASE(abandoned_ship_resource_destruction_does_not_publish_a_campaign_result)
{
    Calls calls;
    auto owner = std::make_unique<TestShip>(RACE_DESC{}, calls);
    owner.reset();
    BOOST_TEST(calls.destroyed == 1u);
    BOOST_TEST(calls.finish == 0u);
}
