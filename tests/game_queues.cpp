#include <boost/test/unit_test.hpp>
#include <cstdint>
#include <vector>
#include "uqm/displist.h"
#include "uqm/element.h"
#include "uqm/races.h"
#include "uqm/grpinfo.h"
#include "uqm/encount.h"
#include "uqm/master.h"
#include "uqm/globdata.h"

extern "C" int game_queue_c_contract(void);

namespace {
struct OwnedQueue {
    QUEUE value{};
    OwnedQueue(QueueKind kind, COUNT capacity) { InitQueue(&value, capacity, kind); }
    ~OwnedQueue() { UninitQueue(&value); }
};
// Observe order only through the public interface, including reverse links.
std::vector<HLINK> order(const QUEUE &queue) {
    std::vector<HLINK> result;
    HLINK previous = nullptr;
    for (auto link = GetHeadLink(&queue); link; link = _GetSuccLink(link)) {
        BOOST_TEST(_GetPredLink(link) == previous);
        result.push_back(link);
        previous = link;
    }
    BOOST_TEST(previous == GetTailLink(&queue));
    BOOST_TEST(result.size() == CountLinks(&queue));
    return result;
}
struct VisitState { QUEUE *queue; std::vector<HLINK> visited; };
void removeCurrent(LINK *link, void *arg) {
    auto &state = *static_cast<VisitState *>(arg);
    state.visited.push_back(link);
    RemoveQueue(state.queue, link);
    FreeLink(state.queue, link);
}
}

BOOST_AUTO_TEST_CASE(game_queue_typed_storage_capacity_alignment_and_stability) {
    struct Type { QueueKind kind; size_t size, alignment; };
    for (const auto type : {Type{QUEUE_ELEMENT, sizeof(ELEMENT), alignof(ELEMENT)},
            {QUEUE_STARSHIP, sizeof(STARSHIP), alignof(STARSHIP)},
            {QUEUE_SHIP_FRAGMENT, sizeof(SHIP_FRAGMENT), alignof(SHIP_FRAGMENT)},
            {QUEUE_FLEET_INFO, sizeof(FLEET_INFO), alignof(FLEET_INFO)},
            {QUEUE_ENCOUNTER, sizeof(ENCOUNTER), alignof(ENCOUNTER)},
            {QUEUE_IP_GROUP, sizeof(IP_GROUP), alignof(IP_GROUP)},
            {QUEUE_MASTER_SHIP, sizeof(MASTER_SHIP_INFO), alignof(MASTER_SHIP_INFO)}}) {
        OwnedQueue owned(type.kind, 3);
        auto &queue = owned.value;
        BOOST_TEST(GetLinkSize(&queue) == type.size);
        const auto first = AllocLink(&queue);
        const auto second = AllocLink(&queue);
        const auto third = AllocLink(&queue);
        BOOST_REQUIRE(first);
        BOOST_REQUIRE(second);
        BOOST_REQUIRE(third);
        BOOST_TEST(reinterpret_cast<uintptr_t>(first) % type.alignment == 0u);
        BOOST_TEST(reinterpret_cast<uintptr_t>(second) - reinterpret_cast<uintptr_t>(first) == type.size);
        BOOST_TEST(AllocLink(&queue) == nullptr);
        BOOST_TEST(static_cast<void *>(LockLink(&queue, first)) == first);
        UnlockLink(&queue, first);
        FreeLink(&queue, second);
        BOOST_TEST(AllocLink(&queue) == second);
        BOOST_TEST(static_cast<void *>(LockLink(&queue, first)) == first);
    }
}

BOOST_AUTO_TEST_CASE(game_queue_encounter_limit_and_zero_capacity) {
    static_assert(MAX_ENCOUNTERS == 16);
    OwnedQueue encounters(QUEUE_ENCOUNTER, MAX_ENCOUNTERS);
    for (int i = 0; i < MAX_ENCOUNTERS; ++i) {
        const auto link = AllocLink(&encounters.value);
        BOOST_REQUIRE(link);
        PutQueue(&encounters.value, link);
    }
    BOOST_TEST(CountLinks(&encounters.value) == MAX_ENCOUNTERS);
    BOOST_TEST(AllocLink(&encounters.value) == nullptr);
    const auto first = GetHeadLink(&encounters.value);
    RemoveQueue(&encounters.value, first);
    // Unlinking does not return capacity until explicitly released.
    BOOST_TEST(AllocLink(&encounters.value) == nullptr);
    FreeLink(&encounters.value, first);
    BOOST_TEST(AllocLink(&encounters.value) == first);
    OwnedQueue empty(QUEUE_IP_GROUP, 0);
    BOOST_TEST(AllocLink(&empty.value) == nullptr);
    ReinitQueue(&empty.value);
    BOOST_TEST(CountLinks(&empty.value) == 0u);
    BOOST_TEST(LockLink(&empty.value, nullptr) == nullptr);
}

BOOST_AUTO_TEST_CASE(game_queue_insertion_removal_and_lifo_slot_reuse) {
    OwnedQueue owned(QUEUE_IP_GROUP, 3);
    auto &q = owned.value;
    const auto a = AllocLink(&q), b = AllocLink(&q), c = AllocLink(&q);
    PutQueue(&q, a);
    InsertQueue(&q, c, nullptr);
    InsertQueue(&q, b, c);
    auto expected = std::vector<HLINK>{a, b, c};
    BOOST_TEST(order(q) == expected, boost::test_tools::per_element());
    RemoveQueue(&q, b);
    BOOST_TEST(_GetSuccLink(b) == c);
    InsertQueue(&q, b, a);
    expected = {b, a, c};
    BOOST_TEST(order(q) == expected, boost::test_tools::per_element());
    RemoveQueue(&q, b);
    RemoveQueue(&q, c);
    RemoveQueue(&q, a);
    BOOST_TEST(order(q).empty());
    FreeLink(&q, a);
    FreeLink(&q, b);
    FreeLink(&q, c);
    BOOST_TEST(AllocLink(&q) == c);
    BOOST_TEST(AllocLink(&q) == b);
    BOOST_TEST(AllocLink(&q) == a);
}

BOOST_AUTO_TEST_CASE(game_queue_reinit_returns_linked_and_reserved_slots_without_clearing_payload) {
    OwnedQueue owned(QUEUE_IP_GROUP, 3);
    auto &q = owned.value;
    const auto a = AllocLink(&q), b = AllocLink(&q), c = AllocLink(&q);
    static_cast<IP_GROUP *>(a)->race_id = 27;
    PutQueue(&q, b);
    PutQueue(&q, a);
    ReinitQueue(&q);
    BOOST_TEST(order(q).empty());
    BOOST_TEST(AllocLink(&q) == a);
    BOOST_TEST(AllocLink(&q) == b);
    BOOST_TEST(AllocLink(&q) == c);
    BOOST_TEST(static_cast<IP_GROUP *>(a)->race_id == 27u);
    BOOST_TEST(AllocLink(&q) == nullptr);
}

BOOST_AUTO_TEST_CASE(game_queue_preview_swap_preserves_original_owner_and_handles) {
    OwnedQueue original(QUEUE_SHIP_FRAGMENT, 1);
    const auto ship = AllocLink(&original.value);
    PutQueue(&original.value, ship);
    QUEUE saved{};
    SwapQueues(&saved, &original.value);
    BOOST_TEST(original.value.storage == nullptr);
    InitQueue(&original.value, 1, QUEUE_SHIP_FRAGMENT);
    const auto preview = AllocLink(&original.value);
    PutQueue(&original.value, preview);
    BOOST_TEST(preview != ship);
    UninitQueue(&original.value);
    SwapQueues(&original.value, &saved);
    BOOST_TEST(saved.storage == nullptr);
    BOOST_TEST(GetHeadLink(&original.value) == ship);
    BOOST_TEST(GetTailLink(&original.value) == ship);
    BOOST_TEST(CountLinks(&original.value) == 1u);
    BOOST_TEST(AllocLink(&original.value) == nullptr);
}

BOOST_AUTO_TEST_CASE(game_queue_visitation_can_remove_current_and_c_api_works) {
    OwnedQueue owned(QUEUE_IP_GROUP, 3);
    std::vector<HLINK> expected;
    for (int i = 0; i < 3; ++i) {
        const auto link = AllocLink(&owned.value);
        expected.push_back(link);
        PutQueue(&owned.value, link);
    }
    VisitState state{&owned.value, {}};
    ForAllLinks(&owned.value, removeCurrent, &state);
    BOOST_TEST(state.visited == expected, boost::test_tools::per_element());
    BOOST_TEST(order(owned.value).empty());
    BOOST_TEST(AllocLink(&owned.value) == expected.back());
    BOOST_TEST(game_queue_c_contract() == 1);
}
