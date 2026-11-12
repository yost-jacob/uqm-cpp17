#ifndef UQM_COLLECTION_STORAGE_HPP_
#define UQM_COLLECTION_STORAGE_HPP_
#include <algorithm>
#include <cassert>
#include <vector>
namespace uqm {
// Fixed-address storage for collections whose C clients retain object addresses.
// Storage is sized once; active order is a separate vector. No allocation occurs
// on insertion/removal. Iterators visit active pointers and end on order mutation;
// object borrows end only on release/reset/destruction, not on another erase.
template<class T, class Storage = std::vector<T>>
class CollectionStorage {
public:
    using value_type = T;
    using iterator = typename std::vector<T *>::iterator;
    explicit CollectionStorage(std::size_t capacity = 0) : slots_(capacity) {
        free_.reserve(capacity); order_.reserve(capacity); clear();
    }
    CollectionStorage(const CollectionStorage &) = delete;
    CollectionStorage &operator=(const CollectionStorage &) = delete;
    CollectionStorage(CollectionStorage &&) = default;
    CollectionStorage &operator=(CollectionStorage &&) = default;
    T *data() noexcept { return slots_.data(); }
    std::size_t capacity() const noexcept { return slots_.size(); }
    std::size_t size() const noexcept { return order_.size(); }
    T &at(std::size_t index) { return *order_.at(index); }
    const T &at(std::size_t index) const { return *order_.at(index); }
    iterator begin() noexcept { return order_.begin(); }
    iterator end() noexcept { return order_.end(); }
    auto begin() const noexcept { return order_.begin(); }
    auto end() const noexcept { return order_.end(); }
    // Reserve an inactive slot; payload is intentionally retained for C callers.
    T *allocate() noexcept {
        if (free_.empty()) return nullptr;
        auto *slot = free_.back(); free_.pop_back(); return slot;
    }
    void release(T *slot) noexcept {
        assert(std::find(order_.begin(), order_.end(), slot) == order_.end());
        assert(free_.size() < slots_.size());
        free_.push_back(slot);
    }
    iterator insert(iterator position, T *slot) { return order_.insert(position, slot); }
    iterator erase(iterator position) { return order_.erase(position); }
    // No gameplay hooks or payload destruction: native resource owners are
    // released separately by the engine before returning its slots.
    void clear() noexcept {
        order_.clear(); free_.clear();
        for (auto it = slots_.rbegin(); it != slots_.rend(); ++it) free_.push_back(&*it);
    }
private:
    Storage slots_;
    std::vector<T *> free_, order_;
};
}
#endif
