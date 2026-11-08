#include "game_state.hpp"
#include "state.h"
#include "starmap.h"
#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace {
uqm::WorldId worldId(COUNT system, COUNT planet, int moon) {
    if (system >= NUM_SOLAR_SYSTEMS || moon < -1 || moon > 65535)
        throw std::out_of_range("invalid legacy discovery identity");
    return {uqm::gameSession().world().id(), StarId{system}, planet,
            moon == -1 ? std::nullopt : std::optional<std::uint16_t>{static_cast<std::uint16_t>(moon)}};
}

}
struct PLANET_INFO_SNAPSHOT { std::vector<std::uint32_t> words; };
extern "C" {
void ResetPersonalDiscoveries(void) {
    try { uqm::gameSession().player().campaign().clearDiscoveries(); }
    catch (...) { std::abort(); }
}
void ImportPersonalDiscovery(COUNT system, COUNT planet, int moon,
        DWORD energyWord, DWORD systemWords, DWORD legacyEnergy) {
    try {
        uqm::gameSession().player().campaign().importLegacyDiscovery(
                {worldId(system, planet, moon), energyWord, systemWords},
                static_cast<std::uint16_t>(legacyEnergy >> 16));
    } catch (...) { std::abort(); }
}
COUNT PersonalDiscoveryCount(COUNT system, COUNT planet, int moon) {
    try {
        return static_cast<COUNT>(uqm::gameSession().player().campaign().discovery(
                worldId(system, planet, moon)).visitedSites.count());
    } catch (...) { std::abort(); }
}
BOOLEAN VisitPersonalDiscovery(COUNT system, COUNT planet, int moon, COUNT site) {
    try {
        return static_cast<BOOLEAN>(uqm::gameSession().player().campaign().visitDiscovery(
                worldId(system, planet, moon), site));
    } catch (...) { std::abort(); }
}
PLANET_INFO_SNAPSHOT *CapturePlanetInfoForSave(void) {
    try {
        auto &session = uqm::gameSession();
        auto words = session.player().campaign().legacyScanSnapshot(
                session.world().id(), session.world().snapshot().planetScans);
        return new PLANET_INFO_SNAPSHOT{std::move(words)};
    } catch (...) { return nullptr; }
}
DWORD PlanetInfoSnapshotLength(const PLANET_INFO_SNAPSHOT *snapshot) {
    return static_cast<DWORD>(snapshot->words.size() * sizeof(std::uint32_t));
}
const DWORD *PlanetInfoSnapshotData(const PLANET_INFO_SNAPSHOT *snapshot) { return snapshot->words.data(); }
void ReleasePlanetInfoSnapshot(PLANET_INFO_SNAPSHOT *snapshot) { delete snapshot; }
}
