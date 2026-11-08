#ifndef UQM_WORLD_STATE_HPP_
#define UQM_WORLD_STATE_HPP_

#include <cstdint>
#include <memory>
#include <vector>

namespace uqm {
struct UniverseId {
    std::uint64_t value;
    constexpr bool operator==(UniverseId other) const noexcept { return value == other.value; }
    constexpr bool operator!=(UniverseId other) const noexcept { return !(*this == other); }
};
inline constexpr UniverseId SinglePlayerUniverse{1};

// Detached legacy-format state bytes, NOT a new portable save encoding. Current
// save/load code handles byte order. No file cursors, locks or handles are saved.
struct WorldSnapshot {
    UniverseId universe;
    std::vector<std::uint8_t> planetScans;
    std::vector<std::uint8_t> randomGroups;
    std::vector<std::uint8_t> scriptedGroups;
};
namespace detail { struct WorldStorage; struct LegacyWorldAccess; }

// Transitional shared owner of the legacy scan/NPC archive. Scan bytes still
// contain personal discovery bits (e.g. Androsynth reports); they must be split
// before multiple players use this archive. This is storage ownership, not a
// completed multiplayer schema. It owns no player/session
// back. Legacy handles borrow its stable file records, not vector data pointers.
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
    WorldSnapshot snapshot() const; // Coherent copied bytes under a shared lock.
    // Authoritative checkpoint restore only; never a per-player rollback online.
    // Validate ID/sizes before replacing anything. Throws invalid_argument for
    // another universe, length_error above the legacy 32-bit limit, logic_error
    // while any legacy file is open. Failed validation leaves all data unchanged.
    void restore(WorldSnapshot candidate);
private:
    friend struct detail::LegacyWorldAccess;
    UniverseId id_;
    std::unique_ptr<detail::WorldStorage> storage_;
};
} // namespace uqm
#endif
