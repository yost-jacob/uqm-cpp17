#include "world.hpp"
#include "world_legacy.hpp"
#include "system_runtime.hpp"
#include "system_behavior.hpp"
#include "game_state.hpp"
#include "world_c.h"
#include "state.h"
#include "gendef.h"
#include "starmap.h"
#include "planets/planets.h"
#include <cstdlib>
#include <stdexcept>
#include <limits>
#include <utility>

namespace {
uqm::SolarSystem &runtime(const SOLARSYS_STATE &system) {
    if (!system.worlds || !system.worlds->system) throw std::logic_error("system worlds have not been built");
    return *system.worlds->system;
}
uqm::WorldId id(uqm::SolarSystem &system, COUNT planet, int moon) {
    if (moon < -1 || moon > 65535) throw std::out_of_range("invalid moon slot");
    return {system.universe(), system.star(), planet,
            moon == -1 ? std::nullopt : std::optional<std::uint16_t>{static_cast<std::uint16_t>(moon)}};
}
uqm::WorldDefinition definition(uqm::WorldId id, uqm::WorldKind kind, const PLANET_DESC &world, std::uint8_t specialSystem = 0) {
    if (world.data_index == HIERARCHY_STARBASE)
        kind = uqm::WorldKind::Starbase;
    else if (world.data_index == SA_MATRA)
        kind = uqm::WorldKind::FinalBattleSite;
    const bool shielded = (kind == uqm::WorldKind::Planet || kind == uqm::WorldKind::Moon)
            && (world.data_index & PLANET_SHIELDED) != 0;
    const auto distance = id.moon ? (world.pPrevDesc ? world.pPrevDesc->radius : 0) : world.radius;
    return {id, kind, world.rand_seed, shielded, {}, world.NumPlanets, specialSystem, world.data_index, distance, {world.radius, world.location.x, world.location.y}};
}
uqm::LegacyDiscoveryLocation discoveryLocation(uqm::SolarSystem &system, const uqm::WorldId &world) {
    std::uint32_t record = 0, records = 0;
    for (std::size_t i = 0; i < system.layout().size(); ++i) {
        if (i < world.planet) record += 1 + system.layout()[i];
        records += 1 + system.layout()[i];
    }
    if (world.moon) record += 1 + *world.moon;
    return {world, record * NUM_SCAN_TYPES + ENERGY_SCAN, records * NUM_SCAN_TYPES};
}

void importSystemDiscoveries(uqm::SolarSystem &system, const STAR_DESC &star, UQM_SYSTEM_RUNTIME &context) {
    if (star.Index != ANDROSYNTH_DEFINED) return;
    const auto world = id(system, 1, -1);
    auto progress = system.progress(world);
    const auto legacy = progress.energy.to_ulong();
    context.story.importLegacyDiscovery(
            discoveryLocation(system, world), static_cast<std::uint16_t>(legacy >> 16));
    progress.energy &= std::bitset<32>{UINT32_C(0x0000ffff)};
    if (legacy >> 16) system.replaceProgress(world, progress);
}

}
namespace uqm {
void constructSystemWorlds(SOLARSYS_STATE &system, const STAR_DESC &star) {
    auto &context = *system.worlds;
    const StarId starId{GetStarIndex(&star)};
    if (starId.value >= NUM_SOLAR_SYSTEMS) throw std::out_of_range("invalid system star");
    auto generated = std::make_unique<SolarSystem>(starId, context.world, context.story,
            star, system, context.session);
    importSystemDiscoveries(*generated, star, context);
    if (context.system) generated->preserveGeneratedData(*context.system);
    context.system = std::move(generated);
    context.projectedMoonParent.reset();
    context.system->project(system);
}
World &orbitalWorld(SOLARSYS_STATE &system) {
    auto &native = runtime(system);
    return native.selectedWorld();
}
void loadPlanetProgress(SOLARSYS_STATE &system) {
    auto &native = runtime(system);
    const auto world = native.selectedWorld().id();
    importSystemDiscoveries(native, systemStar(system), *system.worlds);
    const auto progress = native.progress(world);
    auto &masks = system.SysInfo.PlanetInfo.ScanRetrieveMask;
    masks[MINERAL_SCAN] = static_cast<DWORD>(progress.minerals.to_ulong());
    masks[BIOLOGICAL_SCAN] = static_cast<DWORD>(progress.biologicals.to_ulong());
    masks[ENERGY_SCAN] = static_cast<DWORD>(progress.energy.to_ulong());
}
void publishPlanetProgress(SOLARSYS_STATE &system) {
    auto &native = runtime(system);
    const auto &masks = system.SysInfo.PlanetInfo.ScanRetrieveMask;
    const WorldProgress progress{std::bitset<32>{masks[MINERAL_SCAN]},
            std::bitset<32>{masks[BIOLOGICAL_SCAN]}, std::bitset<32>{masks[ENERGY_SCAN]}};
    native.mergeProgress(native.selectedWorld().id(), progress);
}
} // namespace uqm

namespace uqm::detail {
void projectWorld(const World &world, PLANET_DESC &output, PLANET_DESC &parent) {
    const auto &definition = world.definition();
    output.rand_seed = definition.seed;
    output.data_index = definition.bodyType;
    output.NumPlanets = static_cast<BYTE>(definition.moonCount);
    output.radius = static_cast<SIZE>(definition.orbit.radius);
    output.location = {definition.orbit.x, definition.orbit.y};
    output.pPrevDesc = &parent;
}
WorldDefinition describeWorld(WorldId world, WorldKind kind, const PLANET_DESC &descriptor, std::uint8_t specialSystem) {
    return definition(world, kind, descriptor, specialSystem);
}
const PLANET_DESC &worldDescriptor(const SOLARSYS_STATE &system, const World &world) {
    const auto &id = world.id();
    if (id.moon && (!system.worlds->projectedMoonParent
            || system.worlds->projectedMoonParent->planet != id.planet))
        throw std::logic_error("moon view must be projected explicitly");
    return id.moon ? system.MoonDesc[*id.moon] : system.PlanetDesc[id.planet];
}
PLANET_DESC &worldDescriptor(SOLARSYS_STATE &system, const World &world) {
    const auto &id = world.id();
    auto &parent = id.moon ? system.PlanetDesc[id.planet] : system.SunDesc[0];
    if (id.moon && (!system.worlds->projectedMoonParent
            || system.worlds->projectedMoonParent->planet != id.planet))
        throw std::logic_error("moon descriptor belongs to a different projected parent");
    if (id.moon) projectWorld(runtime(system).world(id.planet), parent, system.SunDesc[0]);
    auto &output = id.moon ? system.MoonDesc[*id.moon] : system.PlanetDesc[id.planet];
    projectWorld(world, output, parent);
    return output;
}
PLANET_DESC &selectedDescriptor(SOLARSYS_STATE &system) { return worldDescriptor(system, runtime(system).selectedWorld()); }
const PLANET_DESC &selectedDescriptor(const SOLARSYS_STATE &system) { return worldDescriptor(system, runtime(system).selectedWorld()); }
World &worldFromDescriptor(SOLARSYS_STATE &system, const PLANET_DESC &descriptor) {
    auto &native = runtime(system);
    for (std::size_t i = 0; i < native.worldCount(); ++i)
        if (&descriptor == &system.PlanetDesc[i]) return native.world(i);
    for (std::size_t i = 0; i < MAX_MOONS; ++i) {
        if (&descriptor != &system.MoonDesc[i]) continue;
        if (system.worlds->projectedMoonParent) {
            const auto parent = system.worlds->projectedMoonParent->planet;
            if (descriptor.pPrevDesc == &system.PlanetDesc[parent]) return native.world(parent).child(i);
        }
    }
    throw std::invalid_argument("body descriptor does not belong to the generated system");
}
}

extern "C" {
PLANET_DESC *GetSystemSelectedDescriptor(SOLARSYS_STATE *system) {
    try {
        auto &native = runtime(*system);
        return native.hasSelectedWorld() ? &uqm::detail::worldDescriptor(*system, native.selectedWorld()) : nullptr;
    } catch (...) { std::abort(); }
}
void SelectSystemWorld(SOLARSYS_STATE *system, const PLANET_DESC *descriptor) {
    try {
        auto &native = runtime(*system);
        if (descriptor) native.selectWorld(uqm::detail::worldFromDescriptor(*system, *descriptor).id());
        else native.clearSelection();
    } catch (...) { std::abort(); }
}
void InitPlanetInfo(void) {
    try {
        uqm::gameSession().world().initializePlanetProgress();
        ResetPersonalDiscoveries();
    } catch (...) { std::abort(); }
}
void UninitPlanetInfo(void) { DeleteStateFile(STARINFO_FILE); }

void PrepareSystemWorlds(SOLARSYS_STATE *system, const STAR_DESC *star) {
    try {
        if (!system || !system->worlds || !star) throw std::invalid_argument("missing exploration context");
        const StarId id{GetStarIndex(star)};
        if (id.value >= NUM_SOLAR_SYSTEMS) throw std::out_of_range("invalid system star");
        auto &context = *system->worlds;
        context.catalogueStar = star;
        context.system = std::make_unique<uqm::SolarSystem>(id, context.world,
                context.story, star->Index);
        context.system->behavior().attachStar(*star);
        if (context.session) context.system->behavior().attachSession(*context.session);
    } catch (...) { std::abort(); }
}
// Legacy debug tooling still owns an opaque heap handle. Production exploration
// instead supplies a stack-owned context carrying its explicit campaign borrow.
void CreateSystemWorlds(SOLARSYS_STATE *system, const STAR_DESC *star) {
    try {
        auto next = std::make_unique<UQM_SYSTEM_RUNTIME>(UQM_SYSTEM_RUNTIME{
                uqm::gameSession().player().story(), uqm::simulationEngine(), uqm::gameSession().worldOwner(), nullptr});
        system->worlds = next.get();
        PrepareSystemWorlds(system, star);
        next.release();
    } catch (...) { std::abort(); }
}
void BuildSystemWorlds(SOLARSYS_STATE *system, const STAR_DESC *star) {
    try {
        if (!system || !star || system->SunDesc[0].NumPlanets > MAX_PLANETS)
            throw std::invalid_argument("invalid system descriptor");
        const StarId starId{GetStarIndex(star)};
        if (starId.value >= NUM_SOLAR_SYSTEMS)
            throw std::out_of_range("world system requires a real catalogue star");
        auto state = system->worlds->world->id();
        std::vector<uqm::WorldDefinition> planets;
        for (COUNT i = 0; i < system->SunDesc[0].NumPlanets; ++i)
            planets.push_back(definition({state, starId, i, std::nullopt},
                    uqm::WorldKind::Planet, system->PlanetDesc[i], star->Index));
        auto &native = runtime(*system);
        if (native.star() != starId) throw std::logic_error("wrong active system");
        native.adoptStar(system->SunDesc[0]);
        native.refreshPlanets(std::move(planets));
        system->worlds->projectedMoonParent.reset();
        importSystemDiscoveries(native, *star, *system->worlds);
    } catch (...) { std::abort(); }
}
void BuildMoonWorlds(SOLARSYS_STATE *system, const PLANET_DESC *planet) {
    try {
        auto &native = runtime(*system);
        const auto slot = uqm::detail::worldFromDescriptor(*system, *planet).id().planet;
        if (static_cast<std::size_t>(slot) >= native.worldCount() || planet->NumPlanets > MAX_MOONS)
            throw std::out_of_range("invalid moon parent");
        std::vector<std::unique_ptr<uqm::World>> moons;
        for (COUNT i = 0; i < planet->NumPlanets; ++i) {
            auto moon = system->MoonDesc[i];
            moon.pPrevDesc = &system->PlanetDesc[slot];
            moons.push_back(std::make_unique<uqm::World>(definition(id(native, static_cast<COUNT>(slot), i),
                    uqm::WorldKind::Moon, moon, native.world(slot).definition().specialSystem), system->worlds->world));
            system->MoonDesc[i].pPrevDesc = moon.pPrevDesc;
        }
        native.world(static_cast<std::size_t>(slot)).refreshChildren(std::move(moons));
        system->worlds->projectedMoonParent = native.world(slot).id();
    } catch (...) { std::abort(); }
}
void DestroySystemWorlds(SOLARSYS_STATE *system) {
    delete system->worlds;
    system->worlds = nullptr;
}
void ConfigurePersonalEnergyBehavior(SOLARSYS_STATE *system, COUNT planet, int moon, COUNT sites, COUNT reports) {
    try {
        auto &native = runtime(*system);
        native.at(id(native, planet, moon)).setEnergyBehavior(
                std::make_unique<uqm::PersonalDiscoveryEnergyBehavior>(sites, reports));
    } catch (...) { std::abort(); }
}
void ConfigureArtifactEnergyBehavior(SOLARSYS_STATE *system, COUNT planet, int moon) {
    try {
        auto &native = runtime(*system);
        native.at(id(native, planet, moon)).setEnergyBehavior(std::make_unique<uqm::ArtifactEnergyBehavior>());
    } catch (...) { std::abort(); }
}
ENERGY_INTERACTION InteractWorldEnergy(SOLARSYS_STATE *system, COUNT planet, int moon, COUNT site, BOOLEAN retrieved) {
    try {
        auto &native = runtime(*system);
        const auto result = native.at(id(native, planet, moon)).interactEnergy(
                system->worlds->story, site, retrieved);
        if (result.report && *result.report > std::numeric_limits<COUNT>::max())
            throw std::out_of_range("report index exceeds C table capacity");
        return {static_cast<BOOLEAN>(result.accepted), static_cast<BOOLEAN>(result.removeNode),
                static_cast<BOOLEAN>(result.report.has_value()),
                static_cast<COUNT>(result.report.value_or(0))};
    } catch (...) { std::abort(); }
}
}
