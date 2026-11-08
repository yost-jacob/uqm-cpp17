#ifndef UQM_ENERGY_BEHAVIOR_HPP_
#define UQM_ENERGY_BEHAVIOR_HPP_

#include "discovery_state.hpp"
#include <cstddef>
#include <optional>

namespace uqm {
class CampaignState;
// A decision for the caller, not a UI callback. Physical removal/reward is
// committed by the authoritative interaction path after acceptance. Reports run
// outside state locks. report is an index in this world's report table.
struct EnergyInteraction {
    bool accepted = false;
    bool removeNode = false;
    std::optional<std::size_t> report;
};
// Concrete no-op default. Behaviors own rules/configuration, never player
// progress. World owns this object; each call supplies the interacting campaign.
class EnergyBehavior {
public:
    virtual ~EnergyBehavior() = default;
    // site is 0..31. physicallyRetrieved is the authoritative physical status,
    // not personal visit history. All interactions run on the simulation thread.
    virtual EnergyInteraction interact(CampaignState &campaign, const WorldId &world,
            std::size_t site, bool physicallyRetrieved) const;
};
class ArtifactEnergyBehavior final : public EnergyBehavior {
public:
    explicit ArtifactEnergyBehavior(std::optional<std::size_t> report = 0);
    EnergyInteraction interact(CampaignState &, const WorldId &,
            std::size_t site, bool physicallyRetrieved) const override;
private:
    std::optional<std::size_t> report_;
};
class PersonalDiscoveryEnergyBehavior final : public EnergyBehavior {
public:
    // sites is 1..32; reports may be fewer than sites. Distinct accepted visits
    // advance the personal sequence. After reports run out, new sites still
    // count as visited. A physical site is never removed by this behavior.
    PersonalDiscoveryEnergyBehavior(std::size_t sites, std::size_t reports);
    EnergyInteraction interact(CampaignState &, const WorldId &,
            std::size_t site, bool physicallyRetrieved) const override;
private:
    std::size_t sites_, reports_;
};
} // namespace uqm
#endif
