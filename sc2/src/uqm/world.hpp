#ifndef UQM_WORLD_HPP_
#define UQM_WORLD_HPP_

#include "discovery_state.hpp"
#include "energy_behavior.hpp"
#include <memory>
#include <string>
#include <vector>

namespace uqm {
class CampaignState;
enum class WorldKind { Planet, Moon, Starbase, FinalBattleSite };
struct WorldDefinition {
    WorldId id;
    WorldKind kind;
    std::uint32_t seed;
    bool shielded;
    std::string name;
};
// Owns a runtime world's behavior and generated children. Physical persistence
// stays in the shared state; personal progress stays in the interacting campaign.
// This runtime is simulation-thread confined. No state is written in destruction.
class World {
public:
    World(WorldDefinition definition, std::shared_ptr<WorldState> state);
    World(WorldDefinition definition, std::shared_ptr<WorldState> state,
            std::unique_ptr<EnergyBehavior> energy);
    ~World();
    World(const World &) = delete;
    World &operator=(const World &) = delete;
    World(World &&) = delete;
    World &operator=(World &&) = delete;
    const WorldDefinition &definition() const noexcept;
    const WorldId &id() const noexcept;
    bool canLand() const noexcept;
    void setShielded(bool shielded) noexcept;
    // Reject null behavior. Configuration is runtime-only and owns no progress.
    void setEnergyBehavior(std::unique_ptr<EnergyBehavior> behavior);
    EnergyInteraction interactEnergy(CampaignState &campaign, std::size_t site,
            bool physicallyRetrieved);
    std::size_t childCount() const noexcept;
    World &child(std::size_t index);
    const World &child(std::size_t index) const;
    // Moon slots must be dense and match this universe/system/planet. Validate
    // the whole replacement before mutation. Borrowed child references expire.
    void replaceChildren(std::vector<std::unique_ptr<World>> children);
private:
    WorldDefinition definition_;
    std::shared_ptr<WorldState> state_;
    std::unique_ptr<EnergyBehavior> energy_;
    std::vector<std::unique_ptr<World>> children_;
};
class SolarSystem {
public:
    // state must be non-null; definitions use dense zero-based planet slots.
    SolarSystem(StarId star, std::shared_ptr<WorldState> state,
            std::vector<WorldDefinition> definitions);
    StarId star() const noexcept;
    UniverseId universe() const noexcept;
    std::size_t worldCount() const noexcept;
    World &world(std::size_t planet);
    const World &world(std::size_t planet) const;
    World &at(const WorldId &id); // Checked universe/system/planet/moon lookup.
    const World &at(const WorldId &id) const;
private:
    StarId star_;
    std::shared_ptr<WorldState> state_;
    std::vector<std::unique_ptr<World>> worlds_;
};
} // namespace uqm
#endif
