#ifndef UQM_WORLD_HPP_
#define UQM_WORLD_HPP_

#include "world_types.hpp"
#include "world_state.hpp"
#include "energy_behavior.hpp"
#include <memory>
#include <string>
#include <vector>

struct solarsys_state;
struct planet_desc;
struct star_desc;
namespace uqm {
namespace detail { struct WorldGeneration; }
class StoryState;
class WorldVisit;
using OrbitHandler = void (*)(WorldVisit &);
class SystemBehavior;
class PlanetRenderResources;
enum class WorldKind { Planet, Moon, Starbase, FinalBattleSite };
struct WorldDefinition {
    WorldId id;
    WorldKind kind;
    std::uint32_t seed;
    bool shielded;
    std::string name;
    std::uint16_t moonCount = 0; // Declared moon count in the generated layout.
    std::uint8_t specialSystem = 0; // Immutable script identity, not story state.
};
// Owns a runtime world's behavior and generated children. Physical persistence
// stays in the shared state; personal progress stays in the interacting story state.
// This runtime is simulation-thread confined. No state is written in destruction.
class World {
public:
    World(WorldDefinition definition, std::shared_ptr<WorldState> state);
    World(WorldDefinition definition, std::shared_ptr<WorldState> state,
            std::unique_ptr<EnergyBehavior> energy);
    // Generate this planet's moon layout during construction. The prepared
    // system descriptors and behavior are borrowed only for this call.
    World(WorldDefinition definition, std::shared_ptr<WorldState> state,
            solarsys_state &system, planet_desc &planet, SystemBehavior &behavior);
    // Copy owned moon descriptors into the active legacy flight view.
    void projectMoons(solarsys_state &system, planet_desc &planet) const;
    ~World();
    World(const World &) = delete;
    World &operator=(const World &) = delete;
    World(World &&) = delete;
    World &operator=(World &&) = delete;
    const WorldDefinition &definition() const noexcept;
    const WorldId &id() const noexcept;
    bool canLand() const noexcept;
    std::optional<OrbitHandler> orbitHandler() const noexcept { return orbitHandler_; }
    WorldProgress progress() const; // Requires layout attachment by SolarSystem; fresh copy.
    void replaceProgress(const WorldProgress &progress); // Explicit import/reset only.
    void mergeProgress(const WorldProgress &confirmed); // Permanent bits; no cargo award.
    void setShielded(bool shielded) noexcept;
    // Reject null behavior. Configuration is runtime-only and owns no progress.
    void setEnergyBehavior(std::unique_ptr<EnergyBehavior> behavior);
    EnergyInteraction interactEnergy(StoryState &story, std::size_t site,
            bool physicallyRetrieved);
    // Lazily create this body's graphics owner. No assets are loaded by this
    // accessor; generators fill its scoped compatibility view on orbit entry.
    PlanetRenderResources &renderResources();
    bool hasRenderResources() const noexcept { return static_cast<bool>(rendering_); }
    std::size_t childCount() const noexcept;
    World &child(std::size_t index);
    const World &child(std::size_t index) const;
    // Moon slots must be dense and match this universe/system/planet. Validate
    // the whole replacement before mutation. Borrowed child references expire.
    void replaceChildren(std::vector<std::unique_ptr<World>> children);
    // Repeated descriptor generation preserves existing children and their
    // resources when definitions match. Changed definitions replace the layout.
    void refreshChildren(std::vector<std::unique_ptr<World>> children);
private:
    friend class SolarSystem;
    static std::optional<OrbitHandler> selectOrbitHandler(const WorldDefinition &definition);
    void attachLayout(const SystemLayout &layout);
    SystemLayout layout_;
    WorldDefinition definition_;
    std::optional<OrbitHandler> orbitHandler_;
    std::shared_ptr<WorldState> state_;
    std::unique_ptr<EnergyBehavior> energy_;
    std::unique_ptr<PlanetRenderResources> rendering_;
    std::vector<std::unique_ptr<World>> children_;
    std::unique_ptr<detail::WorldGeneration> generation_;
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
    // Construct a complete generated layout. system must be the active legacy
    // generation view; star/session outlive the selected behavior. No orbit
    // handlers, encounters or presentation are run by this constructor.
    SolarSystem(StarId star, std::shared_ptr<WorldState> state, StoryState &story,
            const star_desc &description, solarsys_state &system, class GameSession *session);
    ~SolarSystem();
    SystemBehavior &behavior();
    const SystemBehavior &behavior() const;
    // Attach one complete generated planet layout. Validate before replacing it;
    // behavior identity and borrowed story-state reference remain unchanged.
    void replacePlanets(std::vector<WorldDefinition> definitions);
    // Regeneration of an unchanged layout preserves World/behavior/resource
    // identity. Changed definitions use the validated replacement path.
    void refreshPlanets(std::vector<WorldDefinition> definitions);
    // Visit selection uses identities, never pointers into generated C buffers.
    // enterInnerSystem requires a planet ID in this system; selectWorld accepts
    // a generated planet or moon in the active inner view, when present.
    // Invalid identities throw before mutation. Missing selection getters throw
    // bad_optional_access; modes establish their required view on entry.
    void enterOuterSystem() noexcept;
    void enterInnerSystem(const WorldId &planet);
    void selectWorld(const WorldId &world);
    void clearSelection() noexcept;
    bool inInnerSystem() const noexcept;
    bool hasSelectedWorld() const noexcept;
    World &innerPlanet();
    const World &innerPlanet() const;
    World &selectedWorld();
    const World &selectedWorld() const;
    // Current flight view: planets outside, selected planet's moons inside.
    std::size_t visibleWorldCount() const;
    World &visibleWorld(std::size_t index);
    const World &visibleWorld(std::size_t index) const;
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
    std::optional<WorldId> innerPlanet_, selectedWorld_;
};
} // namespace uqm
#endif
