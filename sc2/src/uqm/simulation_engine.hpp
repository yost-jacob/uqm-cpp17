#ifndef UQM_SIMULATION_ENGINE_HPP_
#define UQM_SIMULATION_ENGINE_HPP_
#include "element.h"
#include "entity_handle.hpp"
#include "runtime_ship_roster.hpp"
#include "libs/math/random.hpp"
#include <cstdint>
#include <array>
namespace uqm {
namespace detail { struct SimulationAccess; }
// Mode-owned simulation used by combat, navigation and exploration. The
// engine owns entity lifetimes; presentation and device bindings remain external.
class SimulationEngine {
public:
    SimulationEngine() = default;
    explicit SimulationEngine(COUNT capacity, std::uint32_t seed = RandomDefaultSeed)
        : random_(seed) { initialize(capacity); }
    ~SimulationEngine();
    SimulationEngine(const SimulationEngine &) = delete;
    SimulationEngine &operator=(const SimulationEngine &) = delete;
    void initialize(COUNT capacity = MAX_DISPLAY_ELEMENTS);
    void shutdown() noexcept;
    bool initialized() const noexcept { return entities_.storage != nullptr; }
    void swapScene(SimulationEngine &other) noexcept;
    void reset(); // Invalidates every entity handle; no gameplay death events.
    COUNT size() const noexcept;
    // Reserve an unlinked, zero-initialized element and its display primitive.
    // The engine owns the slot; nullopt means pool exhaustion. The borrow ends
    // at release/reset/shutdown. Caller must enqueue it or release it unused.
    std::optional<std::reference_wrapper<ELEMENT>> allocateElement();
    // element must be this engine's reserved, unlinked live allocation. Releases
    // native behavior/assets without death hooks or gameplay result publication.
    void releaseElement(ELEMENT &element) noexcept;
    // Append this engine's reserved, unlinked allocation to simulation order.
    void enqueue(ELEMENT &element) noexcept;
    RuntimeShipRoster &shipRoster() noexcept { return ships_; }
    const RuntimeShipRoster &shipRoster() const noexcept { return ships_; }
    RandomStream &random() noexcept { return random_; }
    const RandomStream &random() const noexcept { return random_; }
    std::uint32_t battleFrame() const noexcept { return battleFrame_; }
    void resetBattleFrame() noexcept { battleFrame_ = 0; }
    void advanceBattleFrame() noexcept { ++battleFrame_; }
    std::uint64_t ticks() const noexcept { return ticks_; }
    // Exactly one existing simulation pass. The legacy scene adapter must have
    // prepared its collision sprites/status context. This does not draw a frame.
    void step();
    // Records are borrowed for this operation only and must belong to this
    // engine (or be a supported AI prediction copy with a valid ship association).
    void damage(ELEMENT &target, SIZE amount);
    void resolveCollision(ELEMENT &first, ELEMENT &second);
    void contact(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint);
    HELEMENT weaponHit(ELEMENT &weapon, POINT &point, ELEMENT &target, POINT &targetPoint);
    BOOLEAN gravity(ELEMENT &source);
    SIZE track(ELEMENT &tracker, COUNT &facing);
    void clearTargets(const ELEMENT &target);
    std::optional<std::reference_wrapper<ELEMENT>> resolve(combat::EntityHandle handle) noexcept;
    std::optional<std::reference_wrapper<const ELEMENT>> resolve(combat::EntityHandle handle) const noexcept;
private:
    friend struct detail::SimulationAccess;
    RandomStream random_;
    RuntimeShipRoster ships_;
    QUEUE entities_{};
    // Compatibility projection data stays with its queue: ELEMENT::PrimIndex
    // indexes this buffer. Actual drawing/device services remain external.
    std::array<PRIMITIVE, MAX_DISPLAY_PRIMS> primitives_{};
    COUNT freePrimitive_ = 0;
    PRIM_LINKS primitiveLinks_{};
    std::uint64_t ticks_ = 0;
    std::uint32_t battleFrame_ = 0;
};
// Synchronous compatibility binding, not cross-thread shared state. Native
// mode execution binds its engine; C callbacks borrow it until scope exit.
class SimulationBinding {
public:
    explicit SimulationBinding(SimulationEngine &engine) noexcept;
    ~SimulationBinding();
    // Owner teardown occurs while selected; skip saving its soon-dead buffer.
    void discardOnExit() noexcept { discard_ = true; }
    SimulationBinding(const SimulationBinding &) = delete;
    SimulationBinding &operator=(const SimulationBinding &) = delete;
private:
    RandomBinding randomBinding_;
    SimulationEngine *previous_;
    SimulationEngine *selected_;
    bool discard_ = false;
};
SimulationEngine &simulationEngine();
// Temporarily transfer a prepared scene (e.g. orbital surface nodes) into a
// child's distinct engine. Returns its updated scene before either owner dies.
class SimulationSceneTransfer {
public:
    SimulationSceneTransfer(SimulationEngine &source, SimulationEngine &destination) noexcept
        : source_(source), destination_(destination) { source_.swapScene(destination_); }
    ~SimulationSceneTransfer() { source_.swapScene(destination_); }
    SimulationSceneTransfer(const SimulationSceneTransfer &) = delete;
    SimulationSceneTransfer &operator=(const SimulationSceneTransfer &) = delete;
private:
    SimulationEngine &source_, &destination_;
};
}
#endif
