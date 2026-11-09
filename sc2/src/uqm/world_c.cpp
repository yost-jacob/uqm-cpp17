#include "world.hpp"
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
uqm::WorldDefinition definition(uqm::WorldId id, uqm::WorldKind kind, const PLANET_DESC &world) {
    if (world.data_index == HIERARCHY_STARBASE)
        kind = uqm::WorldKind::Starbase;
    else if (world.data_index == SA_MATRA)
        kind = uqm::WorldKind::FinalBattleSite;
    const bool shielded = (kind == uqm::WorldKind::Planet || kind == uqm::WorldKind::Moon)
            && (world.data_index & PLANET_SHIELDED) != 0;
    return {id, kind, world.rand_seed, shielded, {}, world.NumPlanets};
}
uqm::WorldId orbitalId(SOLARSYS_STATE &system, uqm::SolarSystem &native) {
    const auto planet = system.pBaseDesc->pPrevDesc - system.PlanetDesc;
    if (planet < 0 || static_cast<std::size_t>(planet) >= native.worldCount())
        throw std::out_of_range("invalid orbital planet");
    const auto moon = system.pOrbitalDesc->pPrevDesc == system.SunDesc
            ? -1 : static_cast<int>(system.pOrbitalDesc - system.MoonDesc);
    return id(native, static_cast<COUNT>(planet), moon);
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
    context.campaign.importLegacyDiscovery(
            discoveryLocation(system, world), static_cast<std::uint16_t>(legacy >> 16));
    progress.energy &= std::bitset<32>{UINT32_C(0x0000ffff)};
    if (legacy >> 16) system.replaceProgress(world, progress);
}

}
extern "C" {
void InitPlanetInfo(void) {
    try {
        uqm::gameSession().world().initializePlanetProgress();
        ResetPersonalDiscoveries();
    } catch (...) { std::abort(); }
}
void UninitPlanetInfo(void) { DeleteStateFile(STARINFO_FILE); }
void GetPlanetInfo(void) {
    try {
        auto &native = runtime(*pSolarSysState);
        const auto world = orbitalId(*pSolarSysState, native);
        importSystemDiscoveries(native, *GetStar(native.star().value), *pSolarSysState->worlds); // Recheck after in-place checkpoint load.
        const auto progress = native.progress(world);
        auto &masks = pSolarSysState->SysInfo.PlanetInfo.ScanRetrieveMask;
        masks[MINERAL_SCAN] = static_cast<DWORD>(progress.minerals.to_ulong());
        masks[BIOLOGICAL_SCAN] = static_cast<DWORD>(progress.biologicals.to_ulong());
        masks[ENERGY_SCAN] = static_cast<DWORD>(progress.energy.to_ulong());
    } catch (...) { std::abort(); }
}
void PutPlanetInfo(void) {
    try {
        auto &native = runtime(*pSolarSysState);
        const auto &masks = pSolarSysState->SysInfo.PlanetInfo.ScanRetrieveMask;
        const uqm::WorldProgress progress{std::bitset<32>{masks[MINERAL_SCAN]},
                std::bitset<32>{masks[BIOLOGICAL_SCAN]}, std::bitset<32>{masks[ENERGY_SCAN]}};
        native.mergeProgress(orbitalId(*pSolarSysState, native), progress);
    } catch (...) { std::abort(); }
}

void PrepareSystemWorlds(SOLARSYS_STATE *system, const STAR_DESC *star) {
    try {
        if (!system || !system->worlds || !star) throw std::invalid_argument("missing exploration context");
        const StarId id{GetStarIndex(star)};
        if (id.value >= NUM_SOLAR_SYSTEMS) throw std::out_of_range("invalid system star");
        auto &context = *system->worlds;
        context.system = std::make_unique<uqm::SolarSystem>(id, context.world,
                context.campaign, star->Index);
        context.system->behavior().attachStar(*star);
        if (context.session) context.system->behavior().attachSession(*context.session);
    } catch (...) { std::abort(); }
}
// Legacy debug tooling still owns an opaque heap handle. Production exploration
// instead supplies a stack-owned context carrying its explicit campaign borrow.
void CreateSystemWorlds(SOLARSYS_STATE *system, const STAR_DESC *star) {
    try {
        auto next = std::make_unique<UQM_SYSTEM_RUNTIME>(UQM_SYSTEM_RUNTIME{
                uqm::gameSession().player().campaign(), uqm::gameSession().worldOwner(), nullptr});
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
                    uqm::WorldKind::Planet, system->PlanetDesc[i]));
        auto &native = runtime(*system);
        if (native.star() != starId) throw std::logic_error("wrong active system");
        native.replacePlanets(std::move(planets));
        importSystemDiscoveries(native, *star, *system->worlds);
    } catch (...) { std::abort(); }
}
void BuildMoonWorlds(SOLARSYS_STATE *system, const PLANET_DESC *planet) {
    try {
        auto &native = runtime(*system);
        const auto slot = planet - system->PlanetDesc;
        if (slot < 0 || static_cast<std::size_t>(slot) >= native.worldCount() || planet->NumPlanets > MAX_MOONS)
            throw std::out_of_range("invalid moon parent");
        std::vector<std::unique_ptr<uqm::World>> moons;
        for (COUNT i = 0; i < planet->NumPlanets; ++i)
            moons.push_back(std::make_unique<uqm::World>(definition(id(native, static_cast<COUNT>(slot), i),
                    uqm::WorldKind::Moon, system->MoonDesc[i]), system->worlds->world));
        native.world(static_cast<std::size_t>(slot)).replaceChildren(std::move(moons));
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
                system->worlds->campaign, site, retrieved);
        if (result.report && *result.report > std::numeric_limits<COUNT>::max())
            throw std::out_of_range("report index exceeds C table capacity");
        return {static_cast<BOOLEAN>(result.accepted), static_cast<BOOLEAN>(result.removeNode),
                static_cast<BOOLEAN>(result.report.has_value()),
                static_cast<COUNT>(result.report.value_or(0))};
    } catch (...) { std::abort(); }
}
}
