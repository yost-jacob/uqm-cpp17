#include "uqm/world_state.hpp"
#include "uqm/game_state.hpp"
#include "uqm/state.h"
#include <boost/test/unit_test.hpp>
#include <array>
#include <atomic>
#include <thread>
#include <climits>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

extern "C" int world_state_c_round_trip(void);
static_assert(std::is_same_v<decltype(std::declval<const uqm::GameSession &>().world()), const uqm::WorldState &>);
static_assert(std::is_same_v<decltype(std::declval<const uqm::GameState &>().worldOwner()), std::shared_ptr<const uqm::WorldState>>);

BOOST_AUTO_TEST_CASE(players_share_world_buffers_while_ships_and_typed_flags_stay_private)
{
    auto world = std::make_shared<uqm::WorldState>(uqm::UniverseId{17});
    auto alice = std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::PlayerId{1}, "Alice"}, world);
    auto bob = std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::PlayerId{2}, "Bob"}, world);
    uqm::GameSession first{alice, uqm::PlayerId{1}}, second{bob, uqm::PlayerId{2}};
    auto candidate = first.world().snapshot();
    candidate.planetScans = {1, 2, 3};
    candidate.randomGroups = {4, 5};
    candidate.scriptedGroups = {6};
    first.world().restore(candidate);
    first.player().setCrew(42);
    first.player().campaign().set(uqm::StoryFlag::SunDeviceOnShip);
    BOOST_TEST(&first.world() == &second.world());
    BOOST_TEST(second.world().snapshot().planetScans == candidate.planetScans, boost::test_tools::per_element());
    BOOST_TEST(second.player().ship().crew == 0u);
    BOOST_TEST(!second.player().campaign().read(uqm::StoryFlag::SunDeviceOnShip));
    const auto detached = second.world().snapshot();
    candidate.planetScans.clear();
    first.world().restore(candidate);
    BOOST_TEST(detached.planetScans.size() == 3u);
    BOOST_CHECK_THROW((uqm::GameState{uqm::UserProfile{uqm::PlayerId{1}, "Alice"}, nullptr}), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(world_restore_validates_universe_before_replacing_any_file)
{
    uqm::WorldState world{uqm::UniverseId{7}};
    auto candidate = world.snapshot();
    candidate.planetScans = {1};
    candidate.randomGroups = {2};
    candidate.scriptedGroups = {3};
    world.restore(candidate);
    candidate.universe = uqm::UniverseId{8};
    candidate.planetScans = {9};
    BOOST_CHECK_THROW(world.restore(candidate), std::invalid_argument);
    const auto restored = world.snapshot();
    BOOST_TEST(restored.planetScans.front() == 1u);
    BOOST_TEST(restored.randomGroups.front() == 2u);
    BOOST_TEST(restored.scriptedGroups.front() == 3u);
}

BOOST_AUTO_TEST_CASE(world_snapshots_never_mix_files_from_concurrent_restores)
{
    uqm::WorldState world;
    const auto initial = world.snapshot();
    std::atomic<bool> consistent{true};
    std::thread writer([&] {
        for (unsigned i = 0; i < 1000; ++i) {
            auto next = initial;
            const auto value = static_cast<std::uint8_t>(i);
            next.planetScans = {value};
            next.randomGroups = {value};
            next.scriptedGroups = {value};
            world.restore(std::move(next));
        }
    });
    for (unsigned i = 0; i < 1000; ++i) {
        const auto read = world.snapshot();
        if (read.planetScans != read.randomGroups || read.planetScans != read.scriptedGroups)
            consistent = false;
    }
    writer.join();
    BOOST_TEST(consistent.load());
}

struct WorldFilesFixture {
    WorldFilesFixture() {
        DeleteStateFile(STARINFO_FILE);
        DeleteStateFile(RANDGRPINFO_FILE);
        DeleteStateFile(DEFGRPINFO_FILE);
    }
    ~WorldFilesFixture() {
        DeleteStateFile(STARINFO_FILE);
        DeleteStateFile(RANDGRPINFO_FILE);
        DeleteStateFile(DEFGRPINFO_FILE);
    }
};
BOOST_FIXTURE_TEST_SUITE(shared_world_files, WorldFilesFixture)
BOOST_AUTO_TEST_CASE(c_save_bytes_and_native_snapshot_are_the_same_data)
{
    BOOST_REQUIRE(world_state_c_round_trip() == 1);
    const auto snapshot = uqm::gameSession().world().snapshot();
    std::array<std::uint32_t, 3> words{0, UINT32_C(0xfedcba98), 17};
    BOOST_REQUIRE(snapshot.planetScans.size() == sizeof words);
    BOOST_TEST(std::memcmp(snapshot.planetScans.data(), words.data(), sizeof words) == 0);
    auto candidate = snapshot;
    words = {7, 8, 9};
    std::memcpy(candidate.planetScans.data(), words.data(), sizeof words);
    uqm::gameSession().world().restore(std::move(candidate));
    auto *file = OpenStateFile(STARINFO_FILE, "rb");
    std::array<std::uint32_t, 3> result{};
    BOOST_TEST(ReadStateFile(result.data(), sizeof result[0], result.size(), file) == 3);
    CloseStateFile(file);
    BOOST_TEST(result == words, boost::test_tools::per_element());
}
BOOST_AUTO_TEST_CASE(reads_respect_logical_eof_and_complete_element_boundaries)
{
    auto *file = OpenStateFile(STARINFO_FILE, "wb");
    const std::array<BYTE, 5> bytes{1, 2, 3, 4, 5};
    BOOST_TEST(WriteStateFile(bytes.data(), 1, bytes.size(), file) == 5);
    BOOST_TEST(SeekStateFile(file, 0, SEEK_SET) == 1);
    std::array<BYTE, 8> result{};
    BOOST_TEST(ReadStateFile(result.data(), 2, 4, file) == 2);
    BOOST_TEST(ReadStateFile(result.data() + 4, 2, 1, file) == 0);
    BOOST_TEST(ReadStateFile(result.data() + 4, 1, 1, file) == 1);
    BOOST_TEST(ReadStateFile(result.data() + 5, 1, 3, file) == 0);
    BOOST_TEST(result[4] == 5u);
    BOOST_TEST(result[5] == 0u);
    BOOST_TEST(LengthStateFile(file) == 5u);
    CloseStateFile(file);
    file = OpenStateFile(STARINFO_FILE, "wb");
    BOOST_TEST(LengthStateFile(file) == 0u);
    BOOST_TEST(ReadStateFile(result.data(), 1, 1, file) == 0);
    CloseStateFile(file);
}
BOOST_AUTO_TEST_CASE(vector_growth_keeps_handles_stable_and_zero_fills_seek_holes)
{
    auto *file = OpenStateFile(RANDGRPINFO_FILE, "wb");
    BOOST_REQUIRE(file);
    const BYTE value = 42;
    BOOST_TEST(SeekStateFile(file, 100000, SEEK_SET) == 1);
    BOOST_TEST(WriteStateFile(&value, 1, 1, file) == 1);
    BOOST_TEST(LengthStateFile(file) == 100001u);
    const auto snapshot = uqm::gameSession().world().snapshot();
    BOOST_REQUIRE(snapshot.randomGroups.size() == 100001u);
    BOOST_TEST(snapshot.randomGroups.front() == 0u);
    BOOST_TEST(snapshot.randomGroups[99999] == 0u);
    BOOST_TEST(snapshot.randomGroups.back() == 42u);
    CloseStateFile(file);
    auto *reopened = OpenStateFile(RANDGRPINFO_FILE, "rb");
    BOOST_TEST(reopened == file);
    CloseStateFile(reopened);
}
BOOST_AUTO_TEST_CASE(seek_rejects_overflow_and_negative_positions_without_allocating)
{
    auto *file = OpenStateFile(DEFGRPINFO_FILE, "wb");
    const BYTE value = 42;
    BOOST_TEST(WriteStateFile(&value, 1, 1, file) == 1);
    BOOST_TEST(SeekStateFile(file, -1, SEEK_END) == 1);
    BOOST_TEST(SeekStateFile(file, LONG_MIN, SEEK_CUR) == 0);
    BOOST_TEST(SeekStateFile(file, LONG_MAX, SEEK_SET) == 0);
    BOOST_TEST(SeekStateFile(file, 0, 12345) == 0);
    BYTE read = 0;
    BOOST_TEST(ReadStateFile(&read, 1, 1, file) == 1);
    BOOST_TEST(read == value);
    BOOST_TEST(SeekStateFile(file, std::numeric_limits<DWORD>::max(), SEEK_SET) == 1);
    BOOST_TEST(WriteStateFile(&value, 1, 1, file) == 0);
    BOOST_TEST(LengthStateFile(file) == 1u);
    CloseStateFile(file);
}
BOOST_AUTO_TEST_CASE(restore_refuses_open_handles_and_changes_all_files_after_close)
{
    auto &world = uqm::gameSession().world();
    auto candidate = world.snapshot();
    candidate.planetScans = {1};
    candidate.randomGroups = {2};
    candidate.scriptedGroups = {3};
    auto *file = OpenStateFile(RANDGRPINFO_FILE, "rb");
    BOOST_CHECK_THROW(world.restore(candidate), std::logic_error);
    BOOST_TEST(world.snapshot().planetScans.empty());
    CloseStateFile(file);
    world.restore(candidate);
    BOOST_TEST(world.snapshot().planetScans.front() == 1u);
    BOOST_TEST(world.snapshot().randomGroups.front() == 2u);
    BOOST_TEST(world.snapshot().scriptedGroups.front() == 3u);
}
BOOST_AUTO_TEST_CASE(empty_invalid_and_zero_sized_operations_fail_cleanly)
{
    BOOST_TEST(OpenStateFile(-1, "rb") == nullptr);
    BOOST_TEST(OpenStateFile(3, "rb") == nullptr);
    BOOST_TEST(OpenStateFile(0, nullptr) == nullptr);
    BOOST_TEST(OpenStateFile(0, "") == nullptr);
    BOOST_TEST(OpenStateFile(0, "ab") == nullptr);
    auto *file = OpenStateFile(STARINFO_FILE, "wb");
    BYTE byte = 0;
    BOOST_TEST(ReadStateFile(&byte, 0, 1, file) == 0);
    BOOST_TEST(WriteStateFile(&byte, 1, 0, file) == 0);
    BOOST_TEST(ReadStateFile(nullptr, 1, 1, file) == 0);
    BOOST_TEST(WriteStateFile(nullptr, 1, 1, file) == 0);
    BOOST_TEST(LengthStateFile(nullptr) == 0u);
    CloseStateFile(file);
    CloseStateFile(nullptr);
    DeleteStateFile(-1);
}
BOOST_AUTO_TEST_SUITE_END()
