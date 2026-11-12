#include "world.hpp"
#include "system_runtime.hpp"
#include "world_legacy.hpp"
#include "system_behavior.hpp"
#include "planet_render.hpp"
#include "gendef.h"
#include "starmap.h"
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace uqm {
namespace {
bool sameDefinition(const WorldDefinition &a, const WorldDefinition &b) {
    return a.id == b.id && a.kind == b.kind && a.seed == b.seed
        && a.shielded == b.shielded && a.name == b.name && a.moonCount == b.moonCount
        && a.specialSystem == b.specialSystem && a.bodyType == b.bodyType
        && a.orbitalDistance == b.orbitalDistance;
}
}
PlanetRenderResources &World::renderResources() {
    if (!rendering_) rendering_ = std::make_unique<PlanetRenderResources>();
    return *rendering_;
}
void World::refreshChildren(std::vector<std::unique_ptr<World>> children) {
    bool unchanged = children.size() == children_.size();
    for (std::size_t i = 0; unchanged && i < children.size(); ++i)
        unchanged = children[i] && sameDefinition(children[i]->definition(), children_[i]->definition());
    if (!unchanged) replaceChildren(std::move(children));
}
void SolarSystem::refreshPlanets(std::vector<WorldDefinition> definitions) {
    bool unchanged = definitions.size() == worlds_.size();
    for (std::size_t i = 0; unchanged && i < definitions.size(); ++i)
        unchanged = sameDefinition(definitions[i], worlds_[i]->definition());
    if (!unchanged) replacePlanets(std::move(definitions));
}

World::World(WorldDefinition definition, std::shared_ptr<WorldState> state)
    : World(std::move(definition), std::move(state), std::make_unique<EnergyBehavior>()) {}
World::World(WorldDefinition definition, std::shared_ptr<WorldState> state,
        std::unique_ptr<EnergyBehavior> energy)
    : definition_(std::move(definition)), orbitHandler_(selectOrbitHandler(definition_)),
      state_(std::move(state)), energy_(std::move(energy)), data_(std::make_unique<PlanetGeneratedData>()) {
    if (!state_ || !energy_) throw std::invalid_argument("world requires state and energy behavior");
    if (id().universe != state_->id()) throw std::invalid_argument("world belongs to another universe");
}
World::World(WorldDefinition description, std::shared_ptr<WorldState> state,
        SOLARSYS_STATE &system, PLANET_DESC &planet, SystemBehavior &behavior)
    : World(std::move(description), std::move(state)) {
    if (planet.NumPlanets > MAX_MOONS)
        throw std::out_of_range("declared moon count exceeds descriptor capacity");
    generation_ = std::make_unique<detail::WorldGeneration>();
    RandomContext_SeedRandom(&uqm::generationRandom(system), planet.rand_seed);
    std::fill(std::begin(system.MoonDesc), std::end(system.MoonDesc), PLANET_DESC{});
    behavior.generateMoons(system, planet);
    if (planet.NumPlanets > MAX_MOONS)
        throw std::out_of_range("generated moon count exceeds descriptor capacity");
    if (planet.NumPlanets != definition_.moonCount)
        throw std::logic_error("moon generation changed the declared layout");
    generation_->moons.reserve(planet.NumPlanets);
    children_.reserve(planet.NumPlanets);
    for (COUNT i = 0; i < planet.NumPlanets; ++i) {
        auto moon = system.MoonDesc[i];
        moon.pPrevDesc = &planet;
        auto moonId = id();
        moonId.moon = i;
        children_.push_back(std::make_unique<World>(
            detail::describeWorld(moonId, WorldKind::Moon, moon, definition_.specialSystem), state_));
        moon.pPrevDesc = nullptr; // Snapshots retain no borrow into the active view.
        generation_->moons.push_back(moon);
    }
}
void World::projectMoons(SOLARSYS_STATE &system, PLANET_DESC &planet) const {
    if (!generation_) throw std::logic_error("world has no constructed moon layout");
    std::fill(std::begin(system.MoonDesc), std::end(system.MoonDesc), PLANET_DESC{});
    std::copy(generation_->moons.begin(), generation_->moons.end(), system.MoonDesc);
    for (auto &moon : system.MoonDesc) {
        moon.pPrevDesc = &planet;
        moon.temp_color = planet.temp_color;
    }
}
SolarSystem::SolarSystem(StarId starId, std::shared_ptr<WorldState> state,
        StoryState &story, const STAR_DESC &star, SOLARSYS_STATE &system, GameSession *session)
    : SolarSystem(starId, std::move(state), story, star.Index) {
    if (!system.worlds) throw std::invalid_argument("generated system requires a generation owner");
    system.worlds->catalogueStar = &star;
    behavior_->attachStar(star);
    if (session) behavior_->attachSession(*session);
    RandomContext_SeedRandom(&uqm::generationRandom(system), MAKE_DWORD(star.star_pt.x, star.star_pt.y));
    system.SunDesc[0] = {};
    system.SunDesc[0].rand_seed = RandomContext_Random(&uqm::generationRandom(system));
    system.SunDesc[0].data_index = STAR_TYPE(star.Type);
    behavior_->generatePlanets(system);
    if (system.SunDesc[0].NumPlanets > MAX_PLANETS)
        throw std::out_of_range("generated planet count exceeds descriptor capacity");
    for (COUNT i = 0; i < system.SunDesc[0].NumPlanets; ++i) {
        system.PlanetDesc[i].pPrevDesc = &system.SunDesc[0];
        layout_.push_back(system.PlanetDesc[i].NumPlanets);
    }
    worlds_.reserve(layout_.size());
    for (COUNT i = 0; i < layout_.size(); ++i) {
        auto planet = std::make_unique<World>(
            detail::describeWorld({state_->id(), star_, i, std::nullopt}, WorldKind::Planet, system.PlanetDesc[i], star.Index),
            state_, system, system.PlanetDesc[i], *behavior_);
        planet->attachLayout(layout_);
        for (auto &moon : planet->children_) moon->attachLayout(layout_);
        worlds_.push_back(std::move(planet));
    }
}
World::~World() = default;
PlanetGeneratedData &World::generatedData() noexcept { return *data_; }
const PlanetGeneratedData &World::generatedData() const noexcept { return *data_; }
bool World::hasGeneratedData() const noexcept { return data_->PlanDataPtr != nullptr; }
const WorldDefinition &World::definition() const noexcept { return definition_; }
const WorldId &World::id() const noexcept { return definition_.id; }
bool World::canLand() const noexcept {
    return !definition_.shielded && (definition_.kind == WorldKind::Planet || definition_.kind == WorldKind::Moon);
}
void World::attachLayout(const SystemLayout &layout) { layout_ = layout; }
WorldProgress World::progress() const { return state_->planetProgress(id(), layout_); }
void World::replaceProgress(const WorldProgress &progress) { state_->replacePlanetProgress(id(), layout_, progress); }
void World::mergeProgress(const WorldProgress &confirmed) { state_->mergePlanetProgress(id(), layout_, confirmed); }
void World::setShielded(bool shielded) noexcept { definition_.shielded = shielded; }
void World::setEnergyBehavior(std::unique_ptr<EnergyBehavior> behavior) {
    if (!behavior) throw std::invalid_argument("world energy behavior must be non-null");
    energy_ = std::move(behavior);
}
EnergyInteraction World::interactEnergy(StoryState &story, std::size_t site, bool retrieved) {
    if (site >= 32) throw std::out_of_range("energy site must be in 0..31");
    if (!layout_.empty())
        retrieved = retrieved || progress().energy.test(site);
    auto result = energy_->interact(story, id(), site, retrieved);
    if (!result.accepted && (result.removeNode || result.report))
        throw std::logic_error("rejected energy interaction has side effects");
    return result;
}
std::size_t World::childCount() const noexcept { return children_.size(); }
World &World::child(std::size_t index) { return *children_.at(index); }
const World &World::child(std::size_t index) const { return *children_.at(index); }
void World::replaceChildren(std::vector<std::unique_ptr<World>> children) {
    if (!layout_.empty() && children.size() != definition_.moonCount)
        throw std::invalid_argument("moon list does not match generated layout");
    for (std::size_t i = 0; i < children.size(); ++i) {
        if (!children[i]) throw std::invalid_argument("null child world");
        const auto &other = children[i]->id();
        if (id().moon || other.universe != id().universe || other.system != id().system
                || other.planet != id().planet || !other.moon || *other.moon != i)
            throw std::invalid_argument("child world does not match parent and slot");
    }
    for (auto &child : children) child->attachLayout(layout_);
    children_ = std::move(children);
}
SolarSystem::SolarSystem(StarId star, std::shared_ptr<WorldState> state,
        std::vector<WorldDefinition> definitions) : star_(star), state_(std::move(state)) {
    if (!state_) throw std::invalid_argument("solar system requires world state");
    replacePlanets(std::move(definitions));
}
SolarSystem::SolarSystem(StarId star, std::shared_ptr<WorldState> state,
        StoryState &story, std::uint8_t specialSystem)
    : behavior_(createSystemBehavior(specialSystem, story)), star_(star), state_(std::move(state)) {
    if (!state_ || !behavior_) throw std::invalid_argument("system requires state and behavior");
}
SolarSystem::~SolarSystem() = default;
void SolarSystem::preserveGeneratedData(SolarSystem &previous) noexcept {
    if (star_ != previous.star_ || universe() != previous.universe()) return;
    for (std::size_t i = 0; i < worlds_.size() && i < previous.worlds_.size(); ++i) {
        auto &next = *worlds_[i];
        auto &old = *previous.worlds_[i];
        if (!sameDefinition(next.definition_, old.definition_)) continue;
        next.data_.swap(old.data_);
        for (std::size_t j = 0; j < next.children_.size() && j < old.children_.size(); ++j) {
            auto &nextMoon = *next.children_[j];
            auto &oldMoon = *old.children_[j];
            if (sameDefinition(nextMoon.definition_, oldMoon.definition_)) nextMoon.data_.swap(oldMoon.data_);
        }
    }
}
SystemBehavior &SolarSystem::behavior() {
    if (!behavior_) throw std::logic_error("data-only system has no runtime behavior");
    return *behavior_;
}
const SystemBehavior &SolarSystem::behavior() const {
    if (!behavior_) throw std::logic_error("data-only system has no runtime behavior");
    return *behavior_;
}
void SolarSystem::replacePlanets(std::vector<WorldDefinition> definitions) {
    SystemLayout layout;
    std::vector<std::unique_ptr<World>> worlds;
    for (const auto &definition : definitions) layout.push_back(definition.moonCount);
    for (std::size_t i = 0; i < definitions.size(); ++i) {
        const auto &id = definitions[i].id;
        if (id.system != star_ || id.universe != state_->id() || id.planet != i || id.moon)
            throw std::invalid_argument("planet definition does not match system and slot");
        auto world = std::make_unique<World>(std::move(definitions[i]), state_);
        world->attachLayout(layout);
        worlds.push_back(std::move(world));
    }
    layout_ = std::move(layout);
    worlds_ = std::move(worlds);
    enterOuterSystem();
}
void SolarSystem::enterOuterSystem() noexcept { innerPlanet_.reset(); selectedWorld_.reset(); }
void SolarSystem::enterInnerSystem(const WorldId &planet) {
    if (planet.moon) throw std::invalid_argument("inner-system parent must be a planet");
    at(planet);
    innerPlanet_ = planet;
    selectedWorld_ = planet;
}
void SolarSystem::selectWorld(const WorldId &world) {
    at(world);
    if (innerPlanet_ && world.planet != innerPlanet_->planet)
        throw std::invalid_argument("selected world is outside the inner-system view");
    selectedWorld_ = world;
}
void SolarSystem::clearSelection() noexcept { selectedWorld_.reset(); }
bool SolarSystem::inInnerSystem() const noexcept { return innerPlanet_.has_value(); }
bool SolarSystem::hasSelectedWorld() const noexcept { return selectedWorld_.has_value(); }
World &SolarSystem::innerPlanet() { return at(innerPlanet_.value()); }
const World &SolarSystem::innerPlanet() const { return at(innerPlanet_.value()); }
World &SolarSystem::selectedWorld() { return at(selectedWorld_.value()); }
const World &SolarSystem::selectedWorld() const { return at(selectedWorld_.value()); }
std::size_t SolarSystem::visibleWorldCount() const { return inInnerSystem() ? innerPlanet().childCount() : worldCount(); }
World &SolarSystem::visibleWorld(std::size_t index) { return inInnerSystem() ? innerPlanet().child(index) : world(index); }
const World &SolarSystem::visibleWorld(std::size_t index) const { return inInnerSystem() ? innerPlanet().child(index) : world(index); }
StarId SolarSystem::star() const noexcept { return star_; }
UniverseId SolarSystem::universe() const noexcept { return state_->id(); }
std::size_t SolarSystem::worldCount() const noexcept { return worlds_.size(); }
const SystemLayout &SolarSystem::layout() const noexcept { return layout_; }
WorldProgress SolarSystem::progress(const WorldId &id) const {
    if (id.system != star_) throw std::out_of_range("world belongs to another solar system");
    return state_->planetProgress(id, layout_);
}
void SolarSystem::replaceProgress(const WorldId &id, const WorldProgress &progress) {
    if (id.system != star_) throw std::out_of_range("world belongs to another solar system");
    state_->replacePlanetProgress(id, layout_, progress);
}
void SolarSystem::mergeProgress(const WorldId &id, const WorldProgress &progress) {
    if (id.system != star_) throw std::out_of_range("world belongs to another solar system");
    state_->mergePlanetProgress(id, layout_, progress);
}
World &SolarSystem::world(std::size_t planet) { return *worlds_.at(planet); }
const World &SolarSystem::world(std::size_t planet) const { return *worlds_.at(planet); }
const World &SolarSystem::at(const WorldId &id) const {
    if (id.universe != universe() || id.system != star_)
        throw std::out_of_range("world belongs to another system or universe");
    const auto &planet = world(id.planet);
    return id.moon ? planet.child(*id.moon) : planet;
}
World &SolarSystem::at(const WorldId &id) {
    if (id.universe != universe() || id.system != star_)
        throw std::out_of_range("world belongs to another system or universe");
    auto &planet = world(id.planet);
    return id.moon ? planet.child(*id.moon) : planet;
}
} // namespace uqm
