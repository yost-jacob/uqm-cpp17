#include "simulation_engine_legacy.hpp"
#include "runtime_ship_roster_legacy.hpp"
#include "game_state.hpp"
#include "globdata.h"
#include <utility>
#include <stdexcept>
extern "C" {
PRIMITIVE *DisplayArray = nullptr;
COUNT DisplayFreeList = 0;
PRIM_LINKS DisplayLinks{};
}
namespace uqm {
namespace {
thread_local SimulationEngine *boundEngine = nullptr;
// Startup/title/Super Melee C paths have not all become GameModes yet. This
// compatibility owner is used only when the dispatcher has no active mode.
SimulationEngine &legacyEngine() { static SimulationEngine value; return value; }
}
SimulationEngine::~SimulationEngine() { shutdown(); }
void SimulationEngine::shutdown() noexcept {
    if (entities_.storage) UninitQueue(&entities_);
    ships_.clear(); // Resource cleanup only; combat completion is explicit.
    ticks_ = 0;
}
void SimulationEngine::initialize(COUNT capacity) {
    if (entities_.storage) throw std::logic_error("simulation engine is already initialized");
    if (!capacity) throw std::invalid_argument("simulation capacity must be positive");
    InitQueue(&entities_, capacity, QUEUE_ELEMENT);
    reset();
}
void SimulationEngine::reset() {
    if (entities_.storage) ReinitQueue(&entities_);
    ticks_ = 0;
    for (COUNT i = 0; i < MAX_DISPLAY_PRIMS; ++i)
        SetPrimLinks(&primitives_[i], END_OF_LIST, i + 1);
    SetPrimLinks(&primitives_.back(), END_OF_LIST, END_OF_LIST);
    freePrimitive_ = 0;
    primitiveLinks_ = MakeLinks(END_OF_LIST, END_OF_LIST);
    if (boundEngine == this || (!boundEngine && this == &legacyEngine()))
        detail::SimulationAccess::bindProjection(*this);
}
COUNT SimulationEngine::size() const noexcept { return entities_.storage ? CountLinks(&entities_) : 0; }
std::optional<std::reference_wrapper<ELEMENT>> SimulationEngine::resolve(combat::EntityHandle handle) noexcept {
    return handle.resolve(entities_);
}
std::optional<std::reference_wrapper<const ELEMENT>> SimulationEngine::resolve(combat::EntityHandle handle) const noexcept {
    if (const auto result = handle.resolve(entities_)) return std::cref(result->get());
    return std::nullopt;
}
SimulationBinding::SimulationBinding(SimulationEngine &engine) noexcept : randomBinding_(engine.random()), previous_(boundEngine), selected_(&engine) {
    auto &previous = simulationEngine();
    if (&previous != &engine) {
        detail::SimulationAccess::saveProjection(previous);
        detail::SimulationAccess::bindProjection(engine);
    }
    boundEngine = &engine;
}
SimulationBinding::~SimulationBinding() {
    auto *previous = previous_ ? previous_ : &legacyEngine();
    if (selected_ != previous) {
        if (!discard_) detail::SimulationAccess::saveProjection(*selected_);
        detail::SimulationAccess::bindProjection(*previous);
    }
    boundEngine = previous_;
}
SimulationEngine &simulationEngine() {
    if (boundEngine) {
        GLOBAL(DisplayArray) = DisplayArray;
        return *boundEngine;
    }
    auto &engine = legacyEngine();
    if (DisplayArray != detail::SimulationAccess::primitives(engine)) detail::SimulationAccess::bindProjection(engine);
    GLOBAL(DisplayArray) = DisplayArray;
    return engine;
}
void detail::SimulationAccess::saveProjection(SimulationEngine &engine) noexcept {
    engine.freePrimitive_ = DisplayFreeList;
    engine.primitiveLinks_ = DisplayLinks;
}
void detail::SimulationAccess::bindProjection(SimulationEngine &engine) noexcept {
    DisplayArray = engine.primitives_.data();
    GLOBAL(DisplayArray) = DisplayArray;
    DisplayFreeList = engine.freePrimitive_;
    DisplayLinks = engine.primitiveLinks_;
}
void SimulationEngine::swapScene(SimulationEngine &other) noexcept {
    auto &active = simulationEngine();
    if (&active == this || &active == &other) detail::SimulationAccess::saveProjection(active);
    SwapQueues(&entities_, &other.entities_);
    primitives_.swap(other.primitives_);
    std::swap(freePrimitive_, other.freePrimitive_);
    std::swap(primitiveLinks_, other.primitiveLinks_);
    std::swap(ticks_, other.ticks_);
    if (&active == this || &active == &other) detail::SimulationAccess::bindProjection(active);
}
}
// C buffer borrow from the active mode, or the startup compatibility owner.
QUEUE *GetSimulationQueue(void) { return &uqm::detail::SimulationAccess::queue(uqm::simulationEngine()); }

BOOLEAN InitializeSimulationEngine(COUNT capacity) {
    try { uqm::simulationEngine().initialize(capacity); return TRUE; }
    catch (...) { return FALSE; }
}
void ShutdownSimulationEngine(void) { uqm::simulationEngine().shutdown(); }

extern "C" QUEUE *GetRuntimeShipQueues(void) {
    return uqm::detail::RuntimeRosterAccess::queues(uqm::simulationEngine().shipRoster());
}

extern "C" void ClearRuntimeShipRoster(COUNT side) {
    if (side >= 2) std::terminate();
    uqm::simulationEngine().shipRoster().clear(static_cast<uqm::RosterSide>(side));
}
