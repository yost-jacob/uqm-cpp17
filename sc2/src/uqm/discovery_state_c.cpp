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
    try { uqm::gameSession().player().story().clearDiscoveries(); }
    catch (...) { std::abort(); }
}
COUNT PersonalDiscoveryCount(COUNT system, COUNT planet, int moon) {
    try {
        return static_cast<COUNT>(uqm::gameSession().player().story().discovery(
                worldId(system, planet, moon)).visitedSites.count());
    } catch (...) { std::abort(); }
}
PLANET_INFO_SNAPSHOT *CapturePlanetInfoForSave(void) {
    try {
        auto &session = uqm::gameSession();
        auto words = session.player().story().legacyScanSnapshot(
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
