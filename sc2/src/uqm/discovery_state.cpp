#include "game_state.hpp"
#include "starmap.h"
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace uqm {
DiscoveryProgress CampaignState::discovery(const WorldId &world) const {
    const auto found = discoveries_.find(world);
    return found == discoveries_.end() ? DiscoveryProgress{} : found->second;
}
bool CampaignState::visitDiscovery(const WorldId &world, std::size_t site) {
    if (site >= 32)
        throw std::out_of_range("discovery site must be in 0..31");
    if (legacyDiscoveries_.find(world) != legacyDiscoveries_.end() && site >= 16)
        throw std::out_of_range("legacy discovery world supports only 16 sites");
    auto &progress = discoveries_[world];
    if (progress.visitedSites.test(site)) return false;
    progress.visitedSites.set(site);
    return true;
}
DiscoverySnapshot CampaignState::discoverySnapshot() const { return discoveries_; }
void CampaignState::restoreDiscoveries(DiscoverySnapshot snapshot) {
    for (const auto &[world, progress] : snapshot)
        if (legacyDiscoveries_.find(world) != legacyDiscoveries_.end()
                && progress.visitedSites.to_ulong() > std::numeric_limits<std::uint16_t>::max())
            throw std::out_of_range("legacy discovery world supports only 16 sites");
    discoveries_.swap(snapshot);
    // Keep bindings for unchanged world IDs: they describe generated layout,
    // not old personal progress. Drop only worlds absent from the replacement.
    for (auto it = legacyDiscoveries_.begin(); it != legacyDiscoveries_.end(); ) {
        if (discoveries_.find(it->first) == discoveries_.end())
            it = legacyDiscoveries_.erase(it);
        else
            ++it;
    }
}
void CampaignState::clearDiscoveries() noexcept {
    discoveries_.clear();
    legacyDiscoveries_.clear();
}
void CampaignState::importLegacyDiscovery(LegacyDiscoveryLocation location, std::uint16_t visited) {
    if (location.world.system.value >= NUM_SOLAR_SYSTEMS
            || location.energyWord >= location.systemWords
            || location.systemWords == 0 || location.systemWords % 3 != 0)
        throw std::invalid_argument("invalid legacy discovery layout");
    if (discovery(location.world).visitedSites.to_ulong() > std::numeric_limits<std::uint16_t>::max())
        throw std::out_of_range("legacy discovery world supports only 16 sites");
    const auto old = legacyDiscoveries_.find(location.world);
    if (old != legacyDiscoveries_.end()) {
        if (old->second.energyWord != location.energyWord || old->second.systemWords != location.systemWords)
            throw std::invalid_argument("discovery layout changed within a campaign");
        return;
    }
    // Allocate both records before updating progress. Existing native personal
    // history wins over an old archive; only absent histories are imported.
    const auto [binding, inserted] = legacyDiscoveries_.emplace(location.world, location);
    try {
        discoveries_.try_emplace(location.world, DiscoveryProgress{std::bitset<32>{visited}});
    } catch (...) {
        if (inserted) legacyDiscoveries_.erase(binding);
        throw;
    }
}
std::vector<std::uint32_t> CampaignState::legacyScanSnapshot(UniverseId universe,
        const std::vector<std::uint8_t> &archive) const {
    if (archive.size() % sizeof(std::uint32_t) != 0)
        throw std::invalid_argument("legacy scan archive is not word-aligned");
    for (const auto &[world, progress] : discoveries_)
        if (world.universe == universe && progress.visitedSites.any()
                && legacyDiscoveries_.find(world) == legacyDiscoveries_.end())
            throw std::logic_error("discovery world has no legacy save layout");
    std::vector<std::uint32_t> words(archive.size() / sizeof(std::uint32_t));
    if (!archive.empty()) std::memcpy(words.data(), archive.data(), archive.size());
    for (const auto &[world, location] : legacyDiscoveries_) {
        if (world.universe != universe) continue;
        const auto visited = discovery(world).visitedSites.to_ulong();
        if (visited > std::numeric_limits<std::uint16_t>::max())
            throw std::out_of_range("legacy discovery history supports only 16 sites");
        if (words.size() < NUM_SOLAR_SYSTEMS)
            throw std::invalid_argument("legacy scan archive has no star offset table");
        const auto offset = words.at(world.system.value);
        if (offset % sizeof(std::uint32_t) != 0)
            throw std::invalid_argument("unaligned legacy system block");
        auto start = static_cast<std::size_t>(offset / sizeof(std::uint32_t));
        if (offset == 0) {
            // A personal visit can precede the first physical pickup. Materialize
            // its zero-filled legacy system block only in this detached save.
            if (words.size() > std::numeric_limits<std::uint32_t>::max() / sizeof(std::uint32_t)
                    || location.systemWords > std::numeric_limits<std::uint32_t>::max() / sizeof(std::uint32_t) - words.size())
                throw std::length_error("legacy scan archive exceeds 32-bit offsets");
            start = words.size();
            words.resize(start + location.systemWords);
            words.at(world.system.value) = static_cast<std::uint32_t>(start * sizeof(std::uint32_t));
        }
        if (start < NUM_SOLAR_SYSTEMS || start > words.size()
                || location.systemWords > words.size() - start)
            throw std::invalid_argument("truncated legacy discovery system block");
        auto &energy = words.at(start + location.energyWord);
        energy = (energy & UINT32_C(0x0000ffff)) | (static_cast<std::uint32_t>(visited) << 16);
    }
    return words;
}
} // namespace uqm
