#ifndef UQM_GAME_STATE_HPP_
#define UQM_GAME_STATE_HPP_

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include "world_state.hpp"
#include "world_types.hpp"

namespace uqm {
struct PlayerId {
    std::uint64_t value;
    constexpr bool operator==(PlayerId other) const noexcept { return value == other.value; }
    constexpr bool operator!=(PlayerId other) const noexcept { return !(*this == other); }
};
inline constexpr PlayerId SinglePlayerId{1};
struct UserProfile { PlayerId id; std::string displayName; };

// Copied values, never another mutable ship store. Fuel uses the existing game
// units (100 units per tank unit); crew excludes the captain, 65535 means dead.
struct ShipState {
    std::uint32_t resourceUnits, fuel;
    std::uint16_t crew, mineralMass, biologicalMass;
    std::array<std::uint16_t, 8> minerals;
    std::array<std::uint8_t, 16> modules;
    std::array<std::uint8_t, 11> drives;
    std::array<std::uint8_t, 8> jets;
    std::uint8_t landers;
    std::string shipName, commanderName, planetName;
};
// Coordinates retain legacy units. logX/Y locate the flagship in HyperSpace;
// interplanetaryX/Y use the current system frame. Planet/orbit retain legacy
// sentinel values; this value alone does not establish the current game mode.
struct PlayerLocation {
    std::int32_t logX, logY;
    std::int16_t interplanetaryX, interplanetaryY;
    std::uint16_t facing;
    std::uint8_t planet, orbit;
};
enum class StoryFlag { StarbaseAccessGranted, SunDeviceOnShip };
enum class StoryCounter { BombProgress, ShofixtiVisits };
constexpr unsigned counterMaximum(StoryCounter counter) noexcept {
    switch (counter) {
        case StoryCounter::BombProgress: return 3;
        case StoryCounter::ShofixtiVisits: return 7;
    }
    return 0; // Invalid enum values are rejected by the runtime overload.
}
namespace detail { struct PlayerStorage; struct LegacyStateAccess; }

// Personal story interface, owned by value within PlayerEntity. During the
// migration it views the existing packed bytes; there is no parallel bitset.
// All operations run on the simulation thread. Unknown enums throw invalid_argument.
class CampaignState {
public:
    CampaignState(const CampaignState &) = delete;
    CampaignState &operator=(const CampaignState &) = delete;
    bool read(StoryFlag flag) const;
    unsigned read(StoryCounter counter) const;
    void set(StoryFlag flag);
    void clear(StoryFlag flag);
    // Throws out_of_range above the field maximum, leaving all bits unchanged.
    void set(StoryCounter counter, unsigned value);
    DiscoveryProgress discovery(const WorldId &world) const; // Empty if unknown.
    // site must be 0..31. True only on the first accepted visit by this player.
    // Acceptance records progress before report UI, preserving legacy behavior.
    bool visitDiscovery(const WorldId &world, std::size_t site);
    DiscoverySnapshot discoverySnapshot() const;
    // Authoritative personal restore. Validate site/world policy at the caller;
    // replaces progress and retains matching legacy-layout bindings.
    void restoreDiscoveries(DiscoverySnapshot snapshot);
    void clearDiscoveries() noexcept;
    // Compatibility import is idempotent within a loaded campaign. Only the
    // first binding imports old upper energy bits; revisits never overwrite
    // newer personal progress. The adapter checks its layout before mutation.
    void importLegacyDiscovery(LegacyDiscoveryLocation location, std::uint16_t visited);
    // Detached native-word save copy, with this player's upper energy bits
    // composed into registered worlds. Live physical bytes are never modified.
    std::vector<std::uint32_t> legacyScanSnapshot(UniverseId universe,
            const std::vector<std::uint8_t> &physicalArchive) const;
    template<StoryCounter Counter, unsigned Value>
    void set() {
        static_assert(Counter == StoryCounter::BombProgress
                || Counter == StoryCounter::ShofixtiVisits, "unknown story counter");
        static_assert(Value <= counterMaximum(Counter), "story counter value out of range");
        set(Counter, Value);
    }
private:
    friend class PlayerEntity;
    explicit CampaignState(detail::PlayerStorage &storage) noexcept;
    detail::PlayerStorage &storage_;
    DiscoverySnapshot discoveries_;
    std::map<WorldId, LegacyDiscoveryLocation> legacyDiscoveries_;
};

// Owns the one authoritative legacy payload while C callers migrate. This still
// includes unclassified world/runtime fields; those have NOT become per-player
// multiplayer state. Keep legacy queue/resource cleanup before destroying it.
class PlayerEntity {
public:
    explicit PlayerEntity(UserProfile profile);
    ~PlayerEntity();
    PlayerEntity(const PlayerEntity &) = delete;
    PlayerEntity &operator=(const PlayerEntity &) = delete;
    PlayerEntity(PlayerEntity &&) = delete;
    PlayerEntity &operator=(PlayerEntity &&) = delete;
    PlayerId id() const noexcept;
    const UserProfile &profile() const noexcept; // Borrowed; ID is immutable.
    ShipState ship() const; // Detached copy, including equipment and names.
    PlayerLocation location() const noexcept;
    // Storage quantities only; gameplay callers must enforce capacity/cost rules.
    void setFuel(std::uint32_t fuel) noexcept;
    void setCrew(std::uint16_t crew) noexcept;
    CampaignState &campaign() noexcept;
    const CampaignState &campaign() const noexcept;
private:
    friend struct detail::LegacyStateAccess;
    UserProfile profile_;
    std::unique_ptr<detail::PlayerStorage> storage_;
    CampaignState campaign_;
};

// Storage-independent lookup boundary. Initially owns one player, not a list.
// Access is simulation-thread confined; shared_ptr preserves lifetime only.
class GameState {
public:
    explicit GameState(UserProfile profile); // Creates an independent universe.
    // world must be non-null. Multiple states can share one universe while
    // keeping separate player entities; the runtime still supports one player.
    GameState(UserProfile profile, std::shared_ptr<WorldState> world);
    GameState(const GameState &) = delete;
    GameState &operator=(const GameState &) = delete;
    // Unknown IDs throw out_of_range. Borrow for one operation, not across load,
    // disconnect or later storage changes. No collection representation escapes.
    PlayerEntity &player(PlayerId id);
    const PlayerEntity &player(PlayerId id) const;
    WorldState &world() noexcept;
    const WorldState &world() const noexcept;
    std::shared_ptr<WorldState> worldOwner() noexcept;
    std::shared_ptr<const WorldState> worldOwner() const noexcept;
private:
    std::shared_ptr<WorldState> world_;
    PlayerEntity player_;
};

// Session retains its owner, not a cached player pointer or separate campaign.
class GameSession {
public:
    // Reject null state (invalid_argument) and unknown player (out_of_range).
    GameSession(std::shared_ptr<GameState> state, PlayerId player);
    PlayerId playerId() const noexcept;
    PlayerEntity &player();
    const PlayerEntity &player() const;
    WorldState &world() noexcept;
    const WorldState &world() const noexcept;
    std::shared_ptr<WorldState> worldOwner() noexcept;
private:
    std::shared_ptr<GameState> state_;
    PlayerId player_;
};

// Process-lifetime single-player session used by the C compatibility adapter.
// Do not replace its owner: legacy ship/resource code retains payload addresses.
// The payload is zero-initialized once; InitGlobData continues to reset it in place.
GameSession &gameSession();
} // namespace uqm
#endif
