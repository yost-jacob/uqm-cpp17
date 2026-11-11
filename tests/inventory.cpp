#include "uqm/inventory.hpp"
#include "uqm/game_state.hpp"
#include "uqm/sis_legacy.hpp"
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <limits>
#include <type_traits>
namespace {
struct InventoryFixture {
    uqm::GameState state{{uqm::SinglePlayerId, "Inventory"}};
    uqm::PlayerEntity &player = state.player(uqm::SinglePlayerId);
    uqm::Inventory &inventory = player.inventory();
    SIS_STATE &sis = uqm::detail::SISAccess::record(player.sis());
    InventoryFixture() {
        std::fill(std::begin(sis.ModuleSlots), std::end(sis.ModuleSlots), EMPTY_SLOT);
        sis.ModuleSlots[0] = CREW_POD;
        sis.ModuleSlots[1] = CREW_POD;
        sis.ModuleSlots[2] = FUEL_TANK;
        sis.ModuleSlots[3] = STORAGE_BAY;
    }
    void credits(unsigned amount) {
        player.story().setNamedBatch({{"MELNORME_CREDIT0", amount & 255}, {"MELNORME_CREDIT1", amount >> 8}});
    }
};
}
static_assert(std::is_same_v<decltype(std::declval<const uqm::PlayerEntity &>().inventory()), const uqm::Inventory &>);

BOOST_FIXTURE_TEST_CASE(inventory_owns_the_same_fleet_and_flagship_used_by_legacy_callers, InventoryFixture) {
    BOOST_TEST(&inventory.fleet() == &player.fleet());
    BOOST_TEST(&inventory.fleet().sis()->get() == &player.sis());
    auto &other = uqm::gameSession().player();
    BOOST_TEST(&other.inventory().fleet() != &inventory.fleet());
}
BOOST_FIXTURE_TEST_CASE(inventory_grants_crew_and_an_item_as_one_exact_package, InventoryFixture) {
    const auto result = inventory.transact({{}, {{"crew",100}, {"items.sun_device",1}}});
    BOOST_CHECK(result.status == uqm::InventoryStatus::Completed);
    BOOST_TEST(sis.CrewEnlisted == 100u);
    BOOST_TEST(inventory.quantity("items.sun_device") == 1);
    BOOST_TEST(player.story().readNamed("SUN_DEVICE_ON_SHIP") == 1u);
}
BOOST_FIXTURE_TEST_CASE(inventory_capacity_failure_neither_charges_nor_grants_any_part, InventoryFixture) {
    credits(100); sis.CrewEnlisted = 1;
    const auto result = inventory.transact({{{"credits",50}}, {{"crew",100}, {"items.sun_device",1}}});
    BOOST_CHECK(result.status == uqm::InventoryStatus::NoCapacity);
    BOOST_TEST(inventory.quantity("credits") == 100);
    BOOST_TEST(sis.CrewEnlisted == 1u);
    BOOST_TEST(inventory.quantity("items.sun_device") == 0);
}
BOOST_FIXTURE_TEST_CASE(inventory_pays_before_receiving_and_preserves_fractional_fuel, InventoryFixture) {
    credits(100); sis.FuelOnBoard = 2001;
    auto result = inventory.transact({{{"credits",100}}, {{"fuel",10}}});
    BOOST_CHECK(result.status == uqm::InventoryStatus::Completed);
    BOOST_TEST(inventory.quantity("credits") == 0);
    BOOST_TEST(sis.FuelOnBoard == 3001u);
    result = inventory.transact({{{"credits",1}}, {{"credits",1}, {"items.sun_device",1}}});
    BOOST_CHECK(result.status == uqm::InventoryStatus::Insufficient);
    BOOST_TEST(inventory.quantity("items.sun_device") == 0);
    sis.FuelOnBoard = 5501;
    result = inventory.transact({{}, {{"fuel",5}}});
    BOOST_CHECK(result.status == uqm::InventoryStatus::NoCapacity);
    BOOST_TEST(sis.FuelOnBoard == 5501u);
}
BOOST_FIXTURE_TEST_CASE(inventory_minerals_share_capacity_and_keep_cached_mass_consistent, InventoryFixture) {
    auto result = inventory.transact({{}, {{"minerals.common",300}, {"minerals.radioactive",200}}});
    BOOST_CHECK(result.status == uqm::InventoryStatus::Completed);
    BOOST_TEST(sis.TotalElementMass == 500u);
    result = inventory.transact({{}, {{"minerals.exotic",1}, {"items.sun_device",1}}});
    BOOST_CHECK(result.status == uqm::InventoryStatus::NoCapacity);
    BOOST_TEST(inventory.quantity("items.sun_device") == 0);
    result = inventory.transact({{{"minerals.common",20}}, {{"minerals.exotic",20}}});
    BOOST_CHECK(result.status == uqm::InventoryStatus::Completed);
    BOOST_TEST(sis.TotalElementMass == 500u);
    BOOST_TEST(inventory.quantity("minerals.common") == 280);
}
BOOST_FIXTURE_TEST_CASE(inventory_validates_the_complete_request_before_mutation, InventoryFixture) {
    credits(100);
    BOOST_CHECK_THROW(inventory.transact({{{"credits",1}}, {{"items.unknown",1}}}), std::invalid_argument);
    BOOST_CHECK_THROW(inventory.transact({{}, {{"crew",-1}}}), std::invalid_argument);
    BOOST_CHECK_THROW(inventory.transact({{}, {{"fuel",std::numeric_limits<std::int64_t>::max()}}}), std::out_of_range);
    BOOST_CHECK_THROW(inventory.transact({{}, {}}), std::invalid_argument);
    BOOST_TEST(inventory.quantity("credits") == 100);
    BOOST_TEST(sis.CrewEnlisted == 0u);
    sis.CrewEnlisted = static_cast<COUNT>(~0u);
    BOOST_CHECK(inventory.transact({{}, {{"crew",1}}}).status == uqm::InventoryStatus::Unavailable);
}
BOOST_FIXTURE_TEST_CASE(inventory_accounts_and_unique_items_do_not_overflow, InventoryFixture) {
    credits(65535); sis.ResUnits = std::numeric_limits<DWORD>::max();
    BOOST_CHECK(inventory.transact({{}, {{"credits",1}}}).status == uqm::InventoryStatus::NoCapacity);
    BOOST_CHECK(inventory.transact({{}, {{"ru",1}}}).status == uqm::InventoryStatus::NoCapacity);
    BOOST_CHECK(inventory.transact({{}, {{"items.sun_device",1}}}).status == uqm::InventoryStatus::Completed);
    BOOST_CHECK(inventory.transact({{}, {{"items.sun_device",1}}}).status == uqm::InventoryStatus::NoCapacity);
    BOOST_CHECK(inventory.transact({{{"items.sun_device",1}}, {}}).status == uqm::InventoryStatus::Completed);
    BOOST_TEST(inventory.quantity("items.sun_device") == 0);
}
BOOST_FIXTURE_TEST_CASE(story_batches_reject_bad_or_duplicate_fields_before_writing_any, InventoryFixture) {
    auto &story = player.story();
    BOOST_CHECK_THROW(story.setNamedBatch({{"CHMMR_BOMB_STATE",2},{"SHOFIXTI_VISITS",256}}), std::out_of_range);
    BOOST_TEST(story.readNamed("CHMMR_BOMB_STATE") == 0u);
    BOOST_CHECK_THROW(story.setNamedBatch({{"CHMMR_BOMB_STATE",2},{"NOT_A_FIELD",1}}), std::invalid_argument);
    BOOST_TEST(story.readNamed("CHMMR_BOMB_STATE") == 0u);
    BOOST_CHECK_THROW(story.setNamedBatch({{"CHMMR_BOMB_STATE",1},{"CHMMR_BOMB_STATE",2}}), std::invalid_argument);
    story.setNamedBatch({{"CHMMR_BOMB_STATE",2},{"SHOFIXTI_VISITS",3}});
    BOOST_TEST(story.readNamed("CHMMR_BOMB_STATE") == 2u);
    BOOST_TEST(story.readNamed("SHOFIXTI_VISITS") == 3u);
    BOOST_TEST(uqm::Inventory::ownsLegacyField("SUN_DEVICE_ON_SHIP"));
    BOOST_TEST(!uqm::Inventory::ownsLegacyField("CHMMR_BOMB_STATE"));
}

BOOST_AUTO_TEST_CASE(legacy_capacity_queries_use_the_inventory_rules_and_selected_sis_storage) {
    auto &player = uqm::gameSession().player();
    auto &sis = uqm::detail::SISAccess::record(player.sis());
    struct Restore {
        SIS_STATE &live;
        SIS_STATE before;
        ~Restore() { live = before; }
    } restore{sis, sis};
    std::fill(std::begin(sis.ModuleSlots), std::end(sis.ModuleSlots), EMPTY_SLOT);
    sis.ModuleSlots[0] = CREW_POD;
    sis.ModuleSlots[1] = STORAGE_BAY;
    sis.ModuleSlots[2] = HIGHEFF_FUELSYS;
    BOOST_TEST(GetCrewPodCapacity() == 50u);
    BOOST_TEST(GetStorageBayCapacity() == 500u);
    BOOST_TEST(GetFuelTankCapacity() == FUEL_RESERVE + HEFUEL_TANK_CAPACITY);
    BOOST_TEST(player.inventory().capacity("crew") == GetCrewPodCapacity());
    BOOST_TEST(player.inventory().capacity("fuel") * FUEL_TANK_SCALE == GetFuelTankCapacity());
    BOOST_TEST(player.inventory().capacity("minerals.radioactive") == GetStorageBayCapacity());
}
