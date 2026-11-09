#ifndef UQM_WORLD_HPP_
#define UQM_WORLD_HPP_

#include "world_types.hpp"
#include "world_state.hpp"
#include "energy_behavior.hpp"
#include <memory>
#include <string>
#include <vector>

namespace uqm {
class StoryState;
class SystemBehavior;
enum class WorldKind { Planet, Moon, Starbase, FinalBattleSite };
struct WorldDefinition {
    WorldId id;
    WorldKind kind;
    std::uint32_t seed;
    bool shielded;
    std::string name;
    std::uint16_t moonCount = 0; // Generated layout, before moon descriptors are loaded.
};
// Owns a runtime world's behavior and generated children. Physical persistence
// stays in the shared state; personal progress stays in the interacting story state.
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
    WorldProgress progress() const; // Requires layout attachment by SolarSystem; fresh copy.
    void replaceProgress(const WorldProgress &progress); // Explicit import/reset only.
    void mergeProgress(const WorldProgress &confirmed); // Permanent bits; no cargo award.
    void setShielded(bool shielded) noexcept;
    // Reject null behavior. Configuration is runtime-only and owns no progress.
    void setEnergyBehavior(std::unique_ptr<EnergyBehavior> behavior);
    EnergyInteraction interactEnergy(StoryState &story, std::size_t site,
            bool physicallyRetrieved);
    std::size_t childCount() const noexcept;
    World &child(std::size_t index);
    const World &child(std::size_t index) const;
    // Moon slots must be dense and match this universe/system/planet. Validate
    // the whole replacement before mutation. Borrowed child references expire.
    void replaceChildren(std::vector<std::unique_ptr<World>> children);
private:
    friend class SolarSystem;
    void attachLayout(const SystemLayout &layout);
    SystemLayout layout_;
    WorldDefinition definition_;
    std::shared_ptr<WorldState> state_;
    std::unique_ptr<EnergyBehavior> energy_;
    std::vector<std::unique_ptr<World>> children_;
};
class SolarSystem {
public:
    // Detached data-only view, without behavior. state must be non-null;
    // definitions use dense zero-based planet slots. behavior() throws on this view.
    SolarSystem(StarId star, std::shared_ptr<WorldState> state,
            std::vector<WorldDefinition> definitions);
    // story state must outlive this system; the selected behavior borrows it.
    SolarSystem(StarId star, std::shared_ptr<WorldState> state,
            StoryState &story, std::uint8_t specialSystem);
    ~SolarSystem();
    SystemBehavior &behavior();
    const SystemBehavior &behavior() const;
    // Attach one complete generated planet layout. Validate before replacing it;
    // behavior identity and borrowed story-state reference remain unchanged.
    void replacePlanets(std::vector<WorldDefinition> definitions);
    StarId star() const noexcept;
    UniverseId universe() const noexcept;
    std::size_t worldCount() const noexcept;
    const SystemLayout &layout() const noexcept; // Borrowed generated moon counts.
    WorldProgress progress(const WorldId &id) const; // Includes not-yet-generated moons.
    void replaceProgress(const WorldId &id, const WorldProgress &progress);
    void mergeProgress(const WorldId &id, const WorldProgress &confirmed);
    World &world(std::size_t planet);
    const World &world(std::size_t planet) const;
    World &at(const WorldId &id); // Checked universe/system/planet/moon lookup.
    const World &at(const WorldId &id) const;
private:
    std::unique_ptr<SystemBehavior> behavior_;
    StarId star_;
    std::shared_ptr<WorldState> state_;
    SystemLayout layout_;
    std::vector<std::unique_ptr<World>> worlds_;
};
} // namespace uqm
#endif
