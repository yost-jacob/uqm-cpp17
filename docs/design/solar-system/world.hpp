#ifndef UQM_DESIGN_WORLD_HPP_
#define UQM_DESIGN_WORLD_HPP_

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "world_state.hpp"

namespace uqm::design {
class WorldBehavior;
class GameSession;
class RandomStream; // Future adapter over the existing compatible RNG.
struct GenerationContext {
    const StarDesc &star;
    const WorldState &universe;
    RandomStream &random;
    // Borrowed for a generation call only; handlers must not retain this context.
};
enum class WorldKind { Planet, Moon, Starbase, FinalBattleSite };
struct WorldDefinition {
    WorldId id;
    WorldKind kind;
    std::string name;
    std::uint32_t seed;
    bool shielded;
    // Static properties/layout will come from data or existing generation code.
};
struct MineralNode { std::size_t index; unsigned type, amount; };
struct CreatureState {
    std::size_t index;
    unsigned species, hitPoints; // Zero hit points means stunned/canned.
};
struct DiscoverySite { std::size_t index; unsigned contentId; };
struct SurfaceState {
    std::vector<MineralNode> minerals;
    std::vector<CreatureState> creatures;
    std::vector<DiscoverySite> discoveries;
    // Positions/terrain and other simulation fields are omitted from this sketch.
};

// Owns one visitable world's local data, working progress and runtime behavior.
// Holds shared universe ownership so queued/local worlds cannot outlive their
// state. This does not make World itself thread-safe: its runtime data belongs
// to its simulation thread. Network threads submit commands, not method calls.
class World {
public:
    // universe and behavior must be non-null. Physical state belongs to this
    // shared universe; no player's private campaign is retained by a World.
    // Construction establishes ownership without running a story encounter.
    World(std::shared_ptr<WorldState> universe, WorldDefinition definition,
            std::unique_ptr<WorldBehavior> behavior);
    // Cleanup only. Persistent claims were already committed through StateActions;
    // explicit checkpointing happens before teardown, never a throwing final
    // state write from a destructor. Defined out of line for incomplete behavior.
    ~World() noexcept;
    World(const World &) = delete;
    World &operator=(const World &) = delete;
    World(World &&) = delete;
    World &operator=(World &&) = delete;

    const WorldId &id() const noexcept;
    const WorldDefinition &definition() const noexcept;
    bool canLand() const noexcept; // Planet/moon and not shielded.
    void setShielded(bool shielded) noexcept;
    // Checked child access; generation owns additions, never caller raw pointers.
    std::size_t childCount() const noexcept;
    World &child(std::size_t index);
    const World &child(std::size_t index) const;
    void addChild(std::unique_ptr<World> child);
    // Generate physical details without launching conversations or battles.
    void generate(GenerationContext &context);
    // session must be attached to this universe. Handlers consult shared world
    // facts AND the interacting player's private campaign through that session.
    void enterOrbit(GameSession &session);
    bool collected(NodeId node) const;
    // Submit pickup through the authoritative interaction path. Resolve story
    // eligibility on the simulation thread, then atomically claim/reward through
    // StateActions across session.playerEntity() and the universe. Never award twice
    // or treat a local mask as authority.
    void collect(GameSession &session, NodeId node);
    // Merge only confirmed progress for self/children; never replace masks or
    // grant cargo here. May throw on lock/allocation failure. Call explicitly
    // for save/leave; never rely on destruction to publish the first pickup.
    void commitProgress();
    WorldProgress progress() const; // Fresh shared-world snapshot, not an unsafe ref.
    // Surface exists only when generated. Access without one throws logic_error.
    const SurfaceState &surface() const;
    void setSurface(SurfaceState surface);

private:
    WorldDefinition definition_;
    std::shared_ptr<WorldState> universe_;
    WorldProgress confirmedProgress_; // Local cache/checkpoint, never authority.
    std::unique_ptr<WorldBehavior> behavior_;
    std::vector<std::unique_ptr<World>> children_;
    std::optional<SurfaceState> surface_;
};

class SolarSystem {
public:
    explicit SolarSystem(StarId star);
    std::size_t worldCount() const noexcept;
    World &world(std::size_t index); // Checked access.
    const World &world(std::size_t index) const;
    void addWorld(std::unique_ptr<World> world);
    void commitProgress(); // Checkpoint every live world; may throw.
private:
    StarId star_;
    std::vector<std::unique_ptr<World>> worlds_;
};
// Build the system before handing it to the session. Initially this can adapt
// existing generation algorithms. Existing seed coercion/draw order stay intact.
std::unique_ptr<SolarSystem> generateSystem(const StarDesc &star,
        std::shared_ptr<WorldState> universe, RandomStream &random);
} // namespace uqm::design
#endif
