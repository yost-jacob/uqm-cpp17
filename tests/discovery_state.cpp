#include "uqm/game_state.hpp"
#include "uqm/state.h"
#include <boost/test/unit_test.hpp>
#include <cstring>
#include <stdexcept>

namespace {
const uqm::WorldId ruin{uqm::UniverseId{1}, StarId{42}, 1, std::nullopt};
const uqm::LegacyDiscoveryLocation binding{ruin, 10, 12};
std::vector<std::uint8_t> bytes(const std::vector<std::uint32_t> &words) {
    std::vector<std::uint8_t> result(words.size() * sizeof words[0]);
    if (!words.empty()) std::memcpy(result.data(), words.data(), result.size());
    return result;
}
}
BOOST_AUTO_TEST_CASE(discovery_visits_are_personal_idempotent_and_scoped_by_world_identity)
{
    auto world = std::make_shared<uqm::WorldState>();
    uqm::GameState alice{{uqm::PlayerId{1}, "Alice"}, world};
    uqm::GameState bob{{uqm::PlayerId{2}, "Bob"}, world};
    auto &first = alice.player(uqm::PlayerId{1}).story();
    auto &second = bob.player(uqm::PlayerId{2}).story();
    BOOST_TEST(first.visitDiscovery(ruin, 3));
    BOOST_TEST(!first.visitDiscovery(ruin, 3));
    BOOST_TEST(second.discovery(ruin).visitedSites.none());
    BOOST_TEST(second.visitDiscovery(ruin, 3));
    auto another = ruin;
    another.universe = uqm::UniverseId{2};
    BOOST_TEST(first.discovery(another).visitedSites.none());
    another = ruin;
    another.moon = 0;
    BOOST_TEST(first.discovery(another).visitedSites.none());
    BOOST_CHECK_THROW(first.visitDiscovery(ruin, 32), std::out_of_range);
    BOOST_TEST(first.discovery(ruin).visitedSites.count() == 1u);
}
BOOST_AUTO_TEST_CASE(legacy_discovery_import_preserves_newer_personal_visits)
{
    uqm::GameState state{{uqm::PlayerId{1}, "Player"}};
    auto &story = state.player(uqm::PlayerId{1}).story();
    story.importLegacyDiscovery(binding, 5); // Legacy sites 0 and 2.
    BOOST_TEST(story.discovery(ruin).visitedSites.count() == 2u);
    BOOST_TEST(story.visitDiscovery(ruin, 4));
    story.importLegacyDiscovery(binding, 0); // Cleared physical archive on revisit.
    BOOST_TEST(story.discovery(ruin).visitedSites.to_ulong() == 21u);
    BOOST_CHECK_THROW(story.visitDiscovery(ruin, 16), std::out_of_range);
    const auto detached = story.discoverySnapshot();
    story.clearDiscoveries();
    story.restoreDiscoveries(detached);
    story.importLegacyDiscovery(binding, 1); // Restored personal history wins.
    BOOST_TEST(story.discovery(ruin).visitedSites.to_ulong() == 21u);
    auto bad = binding;
    bad.energyWord = 11;
    BOOST_CHECK_THROW(story.importLegacyDiscovery(bad, 0), std::invalid_argument);
    BOOST_TEST(story.discovery(ruin).visitedSites.to_ulong() == 21u);
}
BOOST_AUTO_TEST_CASE(save_composes_personal_history_without_mutating_the_world_archive)
{
    uqm::GameState state{{uqm::PlayerId{1}, "Player"}};
    auto &story = state.player(uqm::PlayerId{1}).story();
    story.importLegacyDiscovery(binding, 5);
    std::vector<std::uint32_t> physical(502 + 12);
    physical[42] = 502 * 4;
    physical[502 + 10] = 3;
    const auto archive = bytes(physical);
    const auto exported = story.legacyScanSnapshot(uqm::UniverseId{1}, archive);
    BOOST_TEST(exported[502 + 10] == UINT32_C(0x00050003));
    BOOST_TEST(bytes(physical) == archive, boost::test_tools::per_element());
    BOOST_TEST(physical[502 + 10] == 3u);
    // Offsets depend on block placement in the current archive, not cached addresses.
    physical.resize(502 + 24);
    physical[42] = (502 + 12) * 4;
    physical[502 + 12 + 10] = 9;
    const auto reordered = story.legacyScanSnapshot(uqm::UniverseId{1}, bytes(physical));
    BOOST_TEST(reordered[502 + 12 + 10] == UINT32_C(0x00050009));
}
BOOST_AUTO_TEST_CASE(personal_only_progress_materializes_a_detached_legacy_block)
{
    uqm::GameState state{{uqm::PlayerId{1}, "Player"}};
    auto &story = state.player(uqm::PlayerId{1}).story();
    story.importLegacyDiscovery(binding, 0);
    story.visitDiscovery(ruin, 15);
    const std::vector<std::uint32_t> physical(502);
    const auto archive = bytes(physical);
    const auto exported = story.legacyScanSnapshot(uqm::UniverseId{1}, archive);
    BOOST_TEST(exported.size() == 514u);
    BOOST_TEST(exported[42] == 2008u);
    BOOST_TEST(exported[512] == UINT32_C(0x80000000));
    BOOST_TEST(archive.size() == 2008u);
}
BOOST_AUTO_TEST_CASE(discovery_save_rejects_missing_layout_and_malformed_archives)
{
    uqm::GameState state{{uqm::PlayerId{1}, "Player"}};
    auto &story = state.player(uqm::PlayerId{1}).story();
    story.visitDiscovery(ruin, 0);
    BOOST_CHECK_THROW(story.legacyScanSnapshot(uqm::UniverseId{1}, bytes(std::vector<std::uint32_t>(502))), std::logic_error);
    story.importLegacyDiscovery(binding, 0);
    BOOST_CHECK_THROW(story.legacyScanSnapshot(uqm::UniverseId{1}, {1}), std::invalid_argument);
    std::vector<std::uint32_t> physical(502);
    physical[42] = 2008;
    BOOST_CHECK_THROW(story.legacyScanSnapshot(uqm::UniverseId{1}, bytes(physical)), std::invalid_argument);
    physical[42] = 2009;
    BOOST_CHECK_THROW(story.legacyScanSnapshot(uqm::UniverseId{1}, bytes(physical)), std::invalid_argument);
    auto bad = binding;
    bad.systemWords = 11;
    BOOST_CHECK_THROW(story.importLegacyDiscovery(bad, 0), std::invalid_argument);
}
