#ifndef UQM_DESIGN_WORLD_BEHAVIOR_HPP_
#define UQM_DESIGN_WORLD_BEHAVIOR_HPP_

#include "world.hpp"

namespace uqm::design {
// Concrete default behavior: normal generation and the normal orbital interface.
// Overrides contain exceptions. Behavior objects own no campaign progress and
// retain no references to temporary generation contexts or replaced sessions.
class WorldBehavior {
public:
    virtual ~WorldBehavior() = default;
    virtual void generateWorld(World &world, GenerationContext &context) const;
    virtual void generateMoons(World &world, GenerationContext &context) const;
    virtual void generateMinerals(World &world, GenerationContext &context) const;
    virtual void generateLife(World &world, GenerationContext &context) const;
    virtual void generateEnergy(World &world, GenerationContext &context) const;
    // Default opens normal orbit; lander availability is checked separately.
    virtual void enterOrbit(GameSession &session, World &world);
    // Return true if the node should be removed and its progress bit marked.
    // Overrides may perform story actions; the World applies the progress change.
    virtual bool pickup(GameSession &session, World &world, NodeId node);
};
class SolBehavior final : public WorldBehavior {
public:
    void generateWorld(World &, GenerationContext &) const override;
    void generateMoons(World &, GenerationContext &) const override;
    void enterOrbit(GameSession &, World &) override;
};
class ChmmrBehavior final : public WorldBehavior {
public:
    void generateWorld(World &, GenerationContext &) const override;
    void generateMoons(World &, GenerationContext &) const override;
    void enterOrbit(GameSession &, World &) override;
};
class ThraddashBehavior final : public WorldBehavior {
public:
    void enterOrbit(GameSession &, World &) override;
    bool pickup(GameSession &, World &, NodeId) override;
};
class StarbaseBehavior final : public WorldBehavior {
public:
    void enterOrbit(GameSession &, World &) override;
};
class SaMatraBehavior final : public WorldBehavior {
public:
    void enterOrbit(GameSession &, World &) override;
};
// The switch maps StarDesc's specialSystem ID (the existing gendef.h values)
// and this world/location to a derived behavior. Zero uses ordinary behavior.
// Return exclusive non-null ownership. No explicit std::move is needed at return.
// These sample subclasses are not an exhaustive migration of every generator.
std::unique_ptr<WorldBehavior> makeWorldBehavior(unsigned specialSystem,
        const WorldDefinition &world);
} // namespace uqm::design
#endif
