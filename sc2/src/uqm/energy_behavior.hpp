#ifndef UQM_ENERGY_BEHAVIOR_HPP_
#define UQM_ENERGY_BEHAVIOR_HPP_

#include "world_types.hpp"
#include <cstddef>
#include <optional>

namespace uqm {
class StoryState;
// A decision for the caller, not a UI callback. Physical removal/reward is
// committed by the authoritative interaction path after acceptance. Reports run
// outside state locks. report is an index in this world's report table.
struct EnergyInteraction {
    bool accepted = false;
    bool removeNode = false;
    std::optional<std::size_t> report;
};
// Concrete no-op default. Behaviors own rules/configuration, never player
// progress. World owns this object; each call supplies the interacting story state.
class EnergyBehavior {
public:
    virtual ~EnergyBehavior() = default;
    // site is 0..31. physicallyRetrieved is the authoritative physical status,
    // not personal visit history. All interactions run on the simulation thread.
    virtual EnergyInteraction interact(StoryState &story, const WorldId &world,
            std::size_t site, bool physicallyRetrieved) const;
};
class ArtifactEnergyBehavior final : public EnergyBehavior {
public:
    explicit ArtifactEnergyBehavior(std::optional<std::size_t> report = 0);
    EnergyInteraction interact(StoryState &, const WorldId &,
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
    EnergyInteraction interact(StoryState &, const WorldId &,
            std::size_t site, bool physicallyRetrieved) const override;
private:
    std::size_t sites_, reports_;
};
} // namespace uqm
#endif
