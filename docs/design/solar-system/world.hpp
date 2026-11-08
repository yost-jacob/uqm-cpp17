#ifndef UQM_DESIGN_WORLD_HPP_
#define UQM_DESIGN_WORLD_HPP_

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "campaign_state.hpp"

namespace uqm::design {
class WorldBehavior;
class GameSession;
class RandomStream; // Future adapter over the existing compatible RNG.
struct GenerationContext {
    const StarDesc &star;
    const CampaignState &campaign;
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
// The supplied campaign must outlive this object and every owned child.
class World {
public:
    // behavior must be non-null (ordinary worlds get the concrete base behavior).
    // Only one live World may hold a given WorldId. Allocate/find its record first,
    // bind committedProgress_ and copy its masks. Construction never runs a story.
    World(CampaignState &campaign, WorldDefinition definition,
            std::unique_ptr<WorldBehavior> behavior);
    // Commit progress without throwing/allocating, then release local resources.
    // Defined out of line so WorldBehavior may remain an incomplete type here.
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
    // Explicit interaction. session must own this world's campaign; the handler
    // reads that live campaign rather than a construction-time snapshot.
    void enterOrbit(GameSession &session);
    bool collected(NodeId node) const;
    // Run the pickup handler first; mark only if it accepts the pickup. Invalid
    // node kinds/indices are rejected. Cargo changes remain campaign operations.
    void collect(GameSession &session, NodeId node);
    // Idempotent flush of self and children. Copy fixed-size masks to records
    // reserved during construction: no insertion, I/O or story callbacks here.
    void commitProgress() noexcept;
    const WorldProgress &progress() const noexcept;
    // Surface exists only when generated. Access without one throws logic_error.
    const SurfaceState &surface() const;
    void setSurface(SurfaceState surface);

private:
    WorldDefinition definition_;
    WorldProgress &committedProgress_; // Borrowed stable record; campaign owns it.
    WorldProgress workingProgress_;
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
    void commitProgress() noexcept; // Flush every live world, without destruction.
private:
    StarId star_;
    std::vector<std::unique_ptr<World>> worlds_;
};
// Build the system before handing it to the session. Initially this can adapt
// existing generation algorithms. Existing seed coercion/draw order stay intact.
std::unique_ptr<SolarSystem> generateSystem(const StarDesc &star,
        CampaignState &campaign, RandomStream &random);
} // namespace uqm::design
#endif
