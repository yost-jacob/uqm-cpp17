#include "world.hpp"
#include "game_state.hpp"
#include "world_c.h"
#include "starmap.h"
#include "planets/planets.h"
#include <cstdlib>
#include <stdexcept>
#include <limits>
#include <utility>

struct UQM_SYSTEM_RUNTIME { std::unique_ptr<uqm::SolarSystem> system; };
namespace {
uqm::SolarSystem &runtime(SOLARSYS_STATE &system) {
    if (!system.worlds) throw std::logic_error("system worlds have not been built");
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
    return {id, kind, world.rand_seed, shielded, {}};
}
}
extern "C" {
void BuildSystemWorlds(SOLARSYS_STATE *system, const STAR_DESC *star) {
    try {
        if (!system || !star || system->SunDesc[0].NumPlanets > MAX_PLANETS)
            throw std::invalid_argument("invalid system descriptor");
        const StarId starId{GetStarIndex(star)};
        if (starId.value >= NUM_SOLAR_SYSTEMS)
            throw std::out_of_range("world system requires a real catalogue star");
        auto state = uqm::gameSession().world().id();
        std::vector<uqm::WorldDefinition> planets;
        for (COUNT i = 0; i < system->SunDesc[0].NumPlanets; ++i)
            planets.push_back(definition({state, starId, i, std::nullopt},
                    uqm::WorldKind::Planet, system->PlanetDesc[i]));
        auto next = std::make_unique<UQM_SYSTEM_RUNTIME>();
        // Native owners receive C++ values and shared state; only this bridge
        // reads the borrowed C descriptor arrays.
        next->system = std::make_unique<uqm::SolarSystem>(starId,
                uqm::gameSession().worldOwner(), std::move(planets));
        delete system->worlds;
        system->worlds = next.release();
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
                    uqm::WorldKind::Moon, system->MoonDesc[i]), uqm::gameSession().worldOwner()));
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
                uqm::gameSession().player().campaign(), site, retrieved);
        if (result.report && *result.report > std::numeric_limits<COUNT>::max())
            throw std::out_of_range("report index exceeds C table capacity");
        return {static_cast<BOOLEAN>(result.accepted), static_cast<BOOLEAN>(result.removeNode),
                static_cast<BOOLEAN>(result.report.has_value()),
                static_cast<COUNT>(result.report.value_or(0))};
    } catch (...) { std::abort(); }
}
}
