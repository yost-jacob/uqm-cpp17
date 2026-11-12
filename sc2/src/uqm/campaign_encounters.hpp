#ifndef UQM_CAMPAIGN_ENCOUNTERS_HPP_
#define UQM_CAMPAIGN_ENCOUNTERS_HPP_
#include "encount.h"
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>
namespace uqm {
// Dense, bounded campaign records. Capacity is reserved once; insertion never
// reallocates. Erase invalidates borrows at/after its position. Child modes may
// update values but must not structurally mutate this collection while borrowed.
template<class Storage = std::vector<ENCOUNTER>>
class BasicCampaignEncounters {
public:
    using iterator = typename Storage::iterator;
    using const_iterator = typename Storage::const_iterator;
    static constexpr std::size_t limit = 16; // Existing gameplay encounter cap.
    BasicCampaignEncounters() { records_.reserve(limit); }
    BasicCampaignEncounters(const BasicCampaignEncounters &) = delete;
    BasicCampaignEncounters &operator=(const BasicCampaignEncounters &) = delete;
    iterator begin() noexcept { return records_.begin(); }
    iterator end() noexcept { return records_.end(); }
    const_iterator begin() const noexcept { return records_.begin(); }
    const_iterator end() const noexcept { return records_.end(); }
    bool empty() const noexcept { return records_.empty(); }
    std::size_t size() const noexcept { return records_.size(); }
    // end() means full; no existing entry or selection is changed on failure.
    iterator insert(ENCOUNTER record = {}) {
        if (size() == limit) return end();
        if (nextId_ == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("encounter identity exhausted");
        record.runtime_id = nextId_++;
        records_.push_back(record);
        return end() - 1;
    }
    iterator erase(iterator entry) {
        const auto index = static_cast<std::size_t>(entry - begin());
        if (selected_) {
            if (*selected_ == index) selected_.reset();
            else if (*selected_ > index) --*selected_;
        }
        return records_.erase(entry);
    }
    void clear() noexcept { selected_.reset(); records_.clear(); }
    // entry belongs to this collection. end() clears the selection.
    void select(iterator entry) noexcept {
        if (entry == end()) selected_.reset();
        else selected_ = static_cast<std::size_t>(entry - begin());
    }
    iterator current() noexcept { return selected_ ? begin() + *selected_ : end(); }
    const_iterator current() const noexcept { return selected_ ? begin() + *selected_ : end(); }
    // Bounds-checked C archive projection. No borrow survives structural mutation.
    ENCOUNTER &at(std::size_t index) { return records_.at(index); }
    const ENCOUNTER &at(std::size_t index) const { return records_.at(index); }
private:
    Storage records_;
    std::optional<std::size_t> selected_;
    std::uint64_t nextId_ = 1; // Not serialized; never reused within a session.
};
using CampaignEncounters = BasicCampaignEncounters<>;
}
#endif
