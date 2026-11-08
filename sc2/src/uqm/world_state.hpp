#ifndef UQM_WORLD_STATE_HPP_
#define UQM_WORLD_STATE_HPP_

#include <cstdint>
#include "world_types.hpp"
#include <memory>
#include <vector>

namespace uqm {
// Detached legacy-format state bytes, NOT a new portable save encoding. Current
// save/load code handles byte order. No file cursors, locks or handles are saved.
struct WorldSnapshot {
    UniverseId universe;
    std::vector<std::uint8_t> planetScans;
    std::vector<std::uint8_t> randomGroups;
    std::vector<std::uint8_t> scriptedGroups;
};
namespace detail { struct WorldStorage; struct LegacyWorldAccess; }

// Shared owner of legacy scan/NPC storage and checked physical progress access.
// Old checkpoints may retain personal report bits until on-demand import; this
// is not a completed multiplayer schema. It owns no player/session back. Legacy handles borrow its stable file records, not vector data pointers.
// Individual operations and native snapshots are locked; the C seek/read/write
// cursor protocol still requires simulation-thread confinement across calls.
class WorldState {
public:
    explicit WorldState(UniverseId id = SinglePlayerUniverse);
    ~WorldState();
    WorldState(const WorldState &) = delete;
    WorldState &operator=(const WorldState &) = delete;
    WorldState(WorldState &&) = delete;
    WorldState &operator=(WorldState &&) = delete;
    UniverseId id() const noexcept;
    // Typed world records are addressed by identity and generated layout. Queries
    // return copies; absent records are empty. Invalid IDs/layouts throw, malformed
    // archives throw invalid_argument, and open C scan handles throw logic_error.
    WorldProgress planetProgress(const WorldId &world, const SystemLayout &layout) const;
    void initializePlanetProgress(); // Empty offset table; caller resets personal state.
    // Replacement supports legacy import/reset; merge is for confirmed permanent
    // collection bits and cannot erase another accepted claim. Neither awards cargo.
    void replacePlanetProgress(const WorldId &world, const SystemLayout &layout, const WorldProgress &progress);
    void mergePlanetProgress(const WorldId &world, const SystemLayout &layout, const WorldProgress &confirmed);
    WorldSnapshot snapshot() const; // Coherent copied bytes under a shared lock.
    // Authoritative checkpoint restore only; never a per-player rollback online.
    // Validate ID/sizes before replacing anything. Throws invalid_argument for
    // another universe, length_error above the legacy 32-bit limit, logic_error
    // while any legacy file is open. Failed validation leaves all data unchanged.
    void restore(WorldSnapshot candidate);
private:
    friend struct detail::LegacyWorldAccess;
    void updatePlanetProgress(const WorldId &, const SystemLayout &, const WorldProgress &, bool merge);
    UniverseId id_;
    std::unique_ptr<detail::WorldStorage> storage_;
};
} // namespace uqm
#endif
