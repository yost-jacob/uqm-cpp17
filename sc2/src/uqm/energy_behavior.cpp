#include "energy_behavior.hpp"
#include "game_state.hpp"
#include <stdexcept>

namespace uqm {
EnergyInteraction EnergyBehavior::interact(CampaignState &, const WorldId &,
        std::size_t site, bool) const {
    if (site >= 32) throw std::out_of_range("energy site must be in 0..31");
    return {};
}
ArtifactEnergyBehavior::ArtifactEnergyBehavior(std::optional<std::size_t> report) : report_(report) {}
EnergyInteraction ArtifactEnergyBehavior::interact(CampaignState &, const WorldId &,
        std::size_t site, bool retrieved) const {
    if (site != 0) throw std::out_of_range("artifact has only site zero");
    if (retrieved) return {};
    return {true, true, report_};
}
PersonalDiscoveryEnergyBehavior::PersonalDiscoveryEnergyBehavior(std::size_t sites, std::size_t reports)
    : sites_(sites), reports_(reports) {
    if (sites == 0 || sites > 32) throw std::invalid_argument("discovery site count must be 1..32");
}
EnergyInteraction PersonalDiscoveryEnergyBehavior::interact(CampaignState &campaign,
        const WorldId &world, std::size_t site, bool retrieved) const {
    if (site >= sites_) throw std::out_of_range("unknown discovery site");
    if (retrieved) return {};
    const auto report = campaign.discovery(world).visitedSites.count();
    if (!campaign.visitDiscovery(world, site)) return {};
    return {true, false, report < reports_ ? std::optional<std::size_t>{report} : std::nullopt};
}
} // namespace uqm
