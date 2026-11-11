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
#include "fleet_legacy.hpp"
#include "projectile_legacy.hpp"
#include "element.h"
#include "races.h"
#include "grpinfo.h"
#include "encount.h"
#include "clock.h"
#include "master.h"

struct QueueStorage {
    // Concrete objects have their actual C types and alignment, not byte-array
    // storage. Each vector is sized once, before any handles are published.
    std::variant<std::vector<ELEMENT>, std::vector<STARSHIP>,
            std::vector<uqm::Ship>, std::vector<FLEET_INFO>,
            std::vector<IP_GROUP>, std::vector<ENCOUNTER>,
            std::vector<EVENT>, std::vector<MASTER_SHIP_INFO>> objects;
    std::vector<HLINK> free;
    std::vector<std::unique_ptr<uqm::combat::Projectile>> projectiles;
    uintptr_t begin = 0;
    size_t capacity = 0;
};

namespace {
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
        case QUEUE_ELEMENT: pool->objects.emplace<std::vector<ELEMENT>>(capacity); break;
        case QUEUE_STARSHIP: pool->objects.emplace<std::vector<STARSHIP>>(capacity); break;
        case QUEUE_SHIP_FRAGMENT: pool->objects.emplace<std::vector<uqm::Ship>>(capacity); break;
        case QUEUE_FLEET_INFO: pool->objects.emplace<std::vector<FLEET_INFO>>(capacity); break;
        case QUEUE_IP_GROUP: pool->objects.emplace<std::vector<IP_GROUP>>(capacity); break;
        case QUEUE_ENCOUNTER: pool->objects.emplace<std::vector<ENCOUNTER>>(capacity); break;
        case QUEUE_EVENT: pool->objects.emplace<std::vector<EVENT>>(capacity); break;
        case QUEUE_MASTER_SHIP: pool->objects.emplace<std::vector<MASTER_SHIP_INFO>>(capacity); break;
        default: std::terminate();
    }
    if (kind == QUEUE_ELEMENT) pool->projectiles.resize(capacity);
    pool->capacity = capacity;
    pool->free.reserve(capacity);
    std::visit([&](auto &objects) {
        using Stored = typename std::decay_t<decltype(objects)>::value_type;
        using Object = std::conditional_t<std::is_same_v<Stored, uqm::Ship>, SHIP_FRAGMENT, Stored>;
        static_assert(std::is_standard_layout_v<Object> && std::is_trivially_copyable_v<Object>);
        static_assert(offsetof(Object, pred) == 0 && offsetof(Object, succ) == sizeof(HLINK));
        queue->object_size = sizeof(Object);
        pool->begin = reinterpret_cast<uintptr_t>(objects.data());
        for (auto it = objects.rbegin(); it != objects.rend(); ++it)
            pool->free.push_back(&*it);
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
    for (auto &projectile : pool.projectiles) projectile.reset();
    pool.free.clear();
    std::visit([&](auto &objects) {
        for (auto it = objects.rbegin(); it != objects.rend(); ++it)
            pool.free.push_back(&*it);
    }, pool.objects);
}
HLINK AllocLink(QUEUE *queue) {
    assert(queue && queue->storage);
    auto &free = queue->storage->free;
    if (free.empty())
        return nullptr;
    const auto link = free.back();
    free.pop_back();
    if (std::holds_alternative<std::vector<uqm::Ship>>(queue->storage->objects))
        uqm::detail::FleetAccess::activate(uqm::detail::FleetAccess::ship(*static_cast<SHIP_FRAGMENT *>(link)));
    return link;
}
void FreeLink(QUEUE *queue, HLINK link) {
    assert(link && contains(queue, link));
    auto &pool = *queue->storage;
    assert(pool.free.size() < pool.capacity);
    if (!pool.projectiles.empty())
        pool.projectiles[(reinterpret_cast<uintptr_t>(link) - pool.begin) / queue->object_size].reset();
    pool.free.push_back(link); // Capacity was reserved in InitQueue.
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
    const auto pred = ref ? _GetPredLink(ref) : queue->tail;
    setPred(link, pred);
    setSucc(link, ref);
    if (pred) setSucc(pred, link); else queue->head = link;
    if (ref) setPred(ref, link); else queue->tail = link;
}
void RemoveQueue(QUEUE *queue, HLINK link) {
    assert(link && contains(queue, link));
    const auto pred = _GetPredLink(link), succ = _GetSuccLink(link);
    if (pred) setSucc(pred, succ); else queue->head = succ;
    if (succ) setPred(succ, pred); else queue->tail = pred;
}
COUNT CountLinks(const QUEUE *queue) {
    assert(queue);
    COUNT count = 0;
    for (auto link = queue->head; link; link = _GetSuccLink(link))
        ++count;
    return count;
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
void bindProjectile(QUEUE &queue, HLINK link, std::unique_ptr<combat::Projectile> projectile) {
    assert(contains(&queue, link) && !queue.storage->projectiles.empty());
    auto &owner = queue.storage->projectiles[(reinterpret_cast<uintptr_t>(link) - queue.storage->begin) / queue.object_size];
    assert(!owner);
    owner = std::move(projectile);
}
combat::Projectile *borrowProjectile(const QUEUE &queue, HLINK link) noexcept {
    if (!contains(&queue, link) || queue.storage->projectiles.empty()) return nullptr;
    return queue.storage->projectiles[(reinterpret_cast<uintptr_t>(link) - queue.storage->begin) / queue.object_size].get();
}
}
