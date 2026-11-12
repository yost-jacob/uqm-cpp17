//Copyright Paul Reiche, Fred Ford. 1992-2002

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#include "displist.h"
#include "game_collections.hpp"
#include "fleet_legacy.hpp"
#include "combat_entity_legacy.hpp"
#include "entity_handle.hpp"
#include <atomic>
#include "element.h"
#include "races.h"
#include "grpinfo.h"
#include "encount.h"
#include "master.h"

struct QueueStorage {
    // Concrete objects have their actual C types and alignment, not byte-array
    // storage. Each vector is sized once, before any handles are published.
    std::variant<uqm::SimulationElements<>, uqm::BattleParticipants<>,
            uqm::FleetShips<>, uqm::RaceFleets<>, uqm::SystemGroups<>,
            uqm::MasterShips<>> objects;
    std::vector<std::unique_ptr<uqm::combat::CombatEntity>> entities;
    std::vector<std::uint64_t> generations;
    uintptr_t begin = 0;
    size_t capacity = 0;
};

namespace {
std::atomic<std::uint64_t> nextEntityGeneration{1};
// Access only the common two-handle prefix, avoiding C++ type-aliasing rules.
void setPred(void *object, HLINK pred) { std::memcpy(object, &pred, sizeof pred); }
void setSucc(void *object, HLINK succ) {
    std::memcpy(static_cast<unsigned char *>(object) + sizeof(HLINK), &succ, sizeof succ);
}
[[maybe_unused]] bool contains(const QUEUE *queue, HLINK link) {
    if (!queue || !queue->storage)
        return false;
    const auto address = reinterpret_cast<uintptr_t>(link);
    const auto &pool = *queue->storage;
    return address >= pool.begin && address - pool.begin < pool.capacity * queue->object_size
            && (address - pool.begin) % queue->object_size == 0;
}
}

BOOLEAN InitQueue(QUEUE *queue, const COUNT capacity, const QueueKind kind) try {
    assert(queue && !queue->storage);
    auto pool = std::make_unique<QueueStorage>();
    switch (kind) {
        case QUEUE_ELEMENT: pool->objects.emplace<uqm::SimulationElements<>>(capacity); break;
        case QUEUE_STARSHIP: pool->objects.emplace<uqm::BattleParticipants<>>(capacity); break;
        case QUEUE_SHIP_FRAGMENT: pool->objects.emplace<uqm::FleetShips<>>(capacity); break;
        case QUEUE_FLEET_INFO: pool->objects.emplace<uqm::RaceFleets<>>(capacity); break;
        case QUEUE_IP_GROUP: pool->objects.emplace<uqm::SystemGroups<>>(capacity); break;
        case QUEUE_MASTER_SHIP: pool->objects.emplace<uqm::MasterShips<>>(capacity); break;
        default: std::terminate();
    }
    if (kind == QUEUE_ELEMENT) {
        pool->entities.resize(capacity);
        pool->generations.resize(capacity);
    }
    pool->capacity = capacity;
    std::visit([&](auto &objects) {
        using Stored = typename std::decay_t<decltype(objects)>::value_type;
        using Object = std::conditional_t<std::is_same_v<Stored, uqm::Ship>, SHIP_FRAGMENT, Stored>;
        static_assert(std::is_standard_layout_v<Object> && std::is_trivially_copyable_v<Object>);
        static_assert(offsetof(Object, pred) == 0 && offsetof(Object, succ) == sizeof(HLINK));
        queue->object_size = sizeof(Object);
        pool->begin = reinterpret_cast<uintptr_t>(objects.data());
    }, pool->objects);
    queue->head = queue->tail = nullptr;
    queue->storage = pool.release();
    return TRUE;
} catch (...) { std::terminate(); }

BOOLEAN UninitQueue(QUEUE *queue) {
    assert(queue);
    delete queue->storage;
    *queue = {};
    return TRUE;
}
void SwapQueues(QUEUE *left, QUEUE *right) {
    assert(left && right);
    std::swap(*left, *right);
}
void ReinitQueue(QUEUE *queue) {
    assert(queue && queue->storage);
    queue->head = queue->tail = nullptr;
    auto &pool = *queue->storage;
    for (auto &entity : pool.entities) entity.reset();
    std::fill(pool.generations.begin(), pool.generations.end(), 0);
    std::visit([](auto &objects) { objects.clear(); }, pool.objects);
}
HLINK AllocLink(QUEUE *queue) {
    assert(queue && queue->storage);
    const HLINK link = std::visit([](auto &objects) -> HLINK { return objects.allocate(); }, queue->storage->objects);
    if (!link) return nullptr;
    auto &pool = *queue->storage;
    if (!pool.generations.empty()) {
        const auto generation = nextEntityGeneration.fetch_add(1, std::memory_order_relaxed);
        if (!generation) std::terminate();
        pool.generations[(reinterpret_cast<uintptr_t>(link) - pool.begin) / queue->object_size] = generation;
    }
    if (std::holds_alternative<uqm::FleetShips<>>(queue->storage->objects))
        uqm::detail::FleetAccess::activate(uqm::detail::FleetAccess::ship(*static_cast<SHIP_FRAGMENT *>(link)));
    return link;
}
void FreeLink(QUEUE *queue, HLINK link) {
    assert(link && contains(queue, link));
    auto &pool = *queue->storage;
    if (!pool.entities.empty()) {
        const auto index = (reinterpret_cast<uintptr_t>(link) - pool.begin) / queue->object_size;
        pool.generations[index] = 0;
        pool.entities[index].reset();
    }
    std::visit([&](auto &objects) {
        using T = typename std::decay_t<decltype(objects)>::value_type;
        objects.release(static_cast<T *>(link));
    }, pool.objects);
}
LINK *LockLink(const QUEUE *queue, HLINK link) {
    assert(!link || contains(queue, link));
    return static_cast<LINK *>(link);
}
void UnlockLink(const QUEUE *queue, HLINK link) {
    assert(!link || contains(queue, link));
}
void PutQueue(QUEUE *queue, HLINK link) { InsertQueue(queue, link, nullptr); }
void InsertQueue(QUEUE *queue, HLINK link, HLINK ref) {
    assert(link && contains(queue, link) && (!ref || contains(queue, ref)));
    std::visit([&](auto &objects) {
        using T = typename std::decay_t<decltype(objects)>::value_type;
        auto position = std::find(objects.begin(), objects.end(), static_cast<T *>(ref));
        assert(!ref || position != objects.end());
        const HLINK pred = position == objects.begin() ? nullptr : *(position - 1);
        objects.insert(position, static_cast<T *>(link));
        // C links are projections of vector order, not the ordering authority.
        setPred(link, pred); setSucc(link, ref);
        if (pred) setSucc(pred, link); else queue->head = link;
        if (ref) setPred(ref, link); else queue->tail = link;
    }, queue->storage->objects);
}
void RemoveQueue(QUEUE *queue, HLINK link) {
    assert(link && contains(queue, link));
    std::visit([&](auto &objects) {
        using T = typename std::decay_t<decltype(objects)>::value_type;
        const auto position = std::find(objects.begin(), objects.end(), static_cast<T *>(link));
        assert(position != objects.end());
        const HLINK pred = position == objects.begin() ? nullptr : *(position - 1);
        const HLINK succ = position + 1 == objects.end() ? nullptr : *(position + 1);
        objects.erase(position);
        if (pred) setSucc(pred, succ); else queue->head = succ;
        if (succ) setPred(succ, pred); else queue->tail = pred;
    }, queue->storage->objects);
}
COUNT CountLinks(const QUEUE *queue) {
    assert(queue);
    if (!queue->storage) return 0;
    return std::visit([](const auto &objects) { return static_cast<COUNT>(objects.size()); }, queue->storage->objects);
}

void ForAllLinks(QUEUE *queue, void (*callback)(LINK *, void *), void *arg) try {
    assert(queue && callback);
    for (auto link = queue->head; link;) {
        const auto next = _GetSuccLink(link);
        callback(LockLink(queue, link), arg);
        link = next;
    }
} catch (...) { std::terminate(); }

namespace uqm::detail {
void bindEntity(QUEUE &queue, HLINK link, std::unique_ptr<combat::CombatEntity> entity) {
    assert(contains(&queue, link) && !queue.storage->entities.empty());
    auto &owner = queue.storage->entities[(reinterpret_cast<uintptr_t>(link) - queue.storage->begin) / queue.object_size];
    assert(!owner);
    owner = std::move(entity);
}
combat::CombatEntity *borrowEntity(const QUEUE &queue, HLINK link) noexcept {
    if (!contains(&queue, link) || queue.storage->entities.empty()) return nullptr;
    return queue.storage->entities[(reinterpret_cast<uintptr_t>(link) - queue.storage->begin) / queue.object_size].get();
}
}

namespace uqm::combat {
EntityHandle EntityHandle::capture(const QUEUE &queue, const ELEMENT &element) noexcept {
    EntityHandle result;
    auto *slot = const_cast<ELEMENT *>(&element);
    if (!contains(&queue, slot) || queue.storage->generations.empty()) return result;
    result.slot_ = slot;
    result.generation_ = queue.storage->generations[(reinterpret_cast<uintptr_t>(slot) - queue.storage->begin) / queue.object_size];
    return result;
}
std::optional<std::reference_wrapper<ELEMENT>> EntityHandle::resolve(const QUEUE &queue) const noexcept {
    if (!generation_ || !contains(&queue, slot_) || queue.storage->generations.empty()) return std::nullopt;
    const auto generation = queue.storage->generations[(reinterpret_cast<uintptr_t>(slot_) - queue.storage->begin) / queue.object_size];
    if (generation != generation_) return std::nullopt;
    return std::ref(*slot_);
}
}

namespace uqm::detail {
const FleetShips<> &fleetShips(QUEUE &queue) { return std::get<FleetShips<>>(queue.storage->objects); }
const FleetShips<> &fleetShips(const QUEUE &queue) { return std::get<FleetShips<>>(queue.storage->objects); }
const BattleParticipants<> &battleParticipants(QUEUE &queue) { return std::get<BattleParticipants<>>(queue.storage->objects); }
const BattleParticipants<> &battleParticipants(const QUEUE &queue) { return std::get<BattleParticipants<>>(queue.storage->objects); }
const RaceFleets<> &raceFleets(QUEUE &queue) { return std::get<RaceFleets<>>(queue.storage->objects); }
const SystemGroups<> &systemGroups(QUEUE &queue) { return std::get<SystemGroups<>>(queue.storage->objects); }
}
