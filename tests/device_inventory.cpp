#include "uqm/device_menu.hpp"
#include "uqm/game_state.hpp"
#include "uqm/globdata.h"
#include "uqm/shipcont.h"
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <array>

BOOST_AUTO_TEST_CASE(device_inventory_is_ordered_and_belongs_to_the_supplied_player)
{
    uqm::GameState first{{uqm::SinglePlayerId, "First"}}, second{{uqm::SinglePlayerId, "Second"}};
    auto &campaign = first.player(uqm::SinglePlayerId).campaign();
    campaign.set(uqm::StoryFlag::TalkingPetOnShip);
    campaign.set(uqm::StoryFlag::RosySphereOnShip);
    campaign.set(uqm::StoryFlag::PortalSpawnerOnShip);
    campaign.set(uqm::StoryCounter::UltronCondition, 2);
    const std::vector<BYTE> expected{PORTAL_SPAWNER_DEVICE, TALKING_PET_DEVICE, ROSY_SPHERE_DEVICE, ULTRON_1_DEVICE};
    BOOST_TEST(uqm::inventoryDevices(campaign) == expected, boost::test_tools::per_element());
    BOOST_TEST(uqm::inventoryDevices(second.player(uqm::SinglePlayerId).campaign()).empty());
    for (unsigned condition = 0; condition <= 4; ++condition) {
        campaign.set(uqm::StoryCounter::UltronCondition, condition);
        const auto devices = uqm::inventoryDevices(campaign);
        for (unsigned stage = 0; stage < 4; ++stage) {
            const auto device = static_cast<BYTE>(ULTRON_0_DEVICE + stage);
            BOOST_TEST((std::find(devices.begin(), devices.end(), device) != devices.end()) == (condition == stage + 1));
        }
    }
}
BOOST_AUTO_TEST_CASE(legacy_device_inventory_copies_only_the_prefix_that_fits)
{
    auto &bytes = GLOBAL(GameState);
    std::array<BYTE, sizeof GLOBAL(GameState)> saved{};
    std::copy(std::begin(bytes), std::end(bytes), saved.begin());
    struct Restore {
        const decltype(saved) &backup;
        ~Restore() { std::copy(backup.begin(), backup.end(), std::begin(GLOBAL(GameState))); }
    } restore{saved};
    std::fill(std::begin(bytes), std::end(bytes), 0);
    auto &campaign = uqm::gameSession().player().campaign();
    campaign.set(uqm::StoryFlag::TalkingPetOnShip);
    campaign.set(uqm::StoryFlag::UtwigBombOnShip);
    std::array<BYTE, 3> output{255, 255, 255};
    BOOST_TEST(InventoryDevices(output.data(), 1) == 1);
    BOOST_TEST(output[0] == TALKING_PET_DEVICE);
    BOOST_TEST(output[1] == 255u);
    BOOST_TEST(InventoryDevices(output.data(), output.size()) == 2);
    BOOST_TEST(output[1] == UTWIG_BOMB_DEVICE);
    BOOST_TEST(output[2] == 255u);
    BOOST_TEST(InventoryDevices(nullptr, 0) == 0);
}
