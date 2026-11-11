#ifndef UQM_GAME_STATE_HPP_
#define UQM_GAME_STATE_HPP_

#include <array>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include "world_state.hpp"
#include "world_types.hpp"
#include "game_mode.hpp"
#include "ship_state.hpp"


namespace uqm {
class Fleet;
class SIS;
// Transient per-player navigation memory, retained across encounter mode swaps.
// Portal coordinates were process statics and are not part of the legacy save.
struct NavigationState { std::int16_t portalX = 0, portalY = 0; };
struct UserProfile { PlayerId id; std::string displayName; };

// Coordinates retain legacy units. logX/Y locate the flagship in HyperSpace;
// interplanetaryX/Y use the current system frame. Planet/orbit retain legacy
// sentinel values; this value alone does not establish the current game mode.
struct PlayerLocation {
    std::int32_t logX, logY;
    std::int16_t interplanetaryX, interplanetaryY;
    std::uint16_t facing;
    std::uint8_t planet, orbit;
};
enum class StoryFlag {
    SupoxUltronHelp,
    ShipToCompel,
    ReadyToConfuseUrquan,
    PortalSpawnerOnShip,
    DiscussedUltron,
    ChmmrEmerging,
    Artifact3OnShip,
    Artifact2OnShip,
    BattleSegue,
    BombCarrier,
    ThraddManner,
    TalkingPetOnShip,
    KohrAhKilledAll,
    PlanetaryLanding,
    ImprovedLanderSpeed,
    ImprovedLanderShot,
    ImprovedLanderCargo,
    ArilouSpace,
    BroadcasterResponse,
    UsedBroadcaster,

    StarbaseAccessGranted,
    SunDeviceOnShip,
    ChmmrFreed,
    IlwrathDeceived,
    YehatCivilWar,
    AquaHelix,
    AquaHelixOnShip,
    BombUnprotected,
    BurvixeseBroadcasters,
    BurvBroadcastersOnShip,
    ClearSpindle,
    ClearSpindleOnShip,
    DruugeManner,
    EggCase0OnShip,
    EggCase1OnShip,
    EggCase2OnShip,
    HelixUnprotected,
    KnowSyreenVault,
    KohrAhFrenzy,
    MaidensOnShip,
    MetZoqfot,
    MoonbaseDestroyed,
    MoonbaseOnShip,
    MyconFellForAmbush,
    PlayerHypnotized,
    PortalKey,
    PortalKeyOnShip,
    ProbeMessageDelivered,
    RosySphere,
    RosySphereOnShip,
    ShipVaultUnlocked,
    ShofixtiBroKia,
    ShofixtiKia,
    ShofixtiMaidens,
    ShofixtiRecruited,
    SpathiCreaturesEliminated,
    SpathiCreaturesExamined,
    SpathiShieldedSelves,
    SunDevice,
    SunDeviceUnguarded,
    SyreenShuttleOnShip,
    TaaloProtector,
    TaaloProtectorOnShip,
    TaaloUnprotected,
    TalkingPet,
    UmgahBroadcasters,
    UmgahBroadcastersOnShip,
    UmgahZombieBlobbies,
    UrquanMessedUp,
    UrquanProtectingSamatra,
    UtwigBomb,
    UtwigBombOnShip,
    UtwigHaveUltron,
    VuxBeast,
    VuxBeastOnShip,
    ZexIsDead,
};
enum class StoryCounter {
    ThraddashBodyCount,
    LanderShields,
    ArilouSpaceCounter,
    ArilouSpaceSide,
    EscapeCounter,
    MelnormeAnger,
    PortalCounter,
    SlylandroMultiplier,

    BombProgress,
    ShofixtiVisits,
    EncounterContext,
    BattlePlanet,
    ColonyGrpoffs0,
    DruugeHomeVisits,
    DruugeVisits,
    FoundPlutoSpathi,
    HelixVisits,
    KnowAboutShattered,
    Melnorme0Grpoffs0,
    Melnorme1Grpoffs0,
    Melnorme2Grpoffs0,
    Melnorme3Grpoffs0,
    Melnorme4Grpoffs0,
    Melnorme5Grpoffs0,
    Melnorme6Grpoffs0,
    Melnorme7Grpoffs0,
    Melnorme8Grpoffs0,
    MyconVisits,
    RainbowWorld0,
    RainbowWorld1,
    SamatraGrpoffs0,
    ShofixtiGrpoffs0,
    SyreenHomeVisits,
    ThraddMission,
    UltronCondition,
    UrquanProbeGrpoffs0,
    ZoqfotDistress,
    ZoqfotGrpoffs0,
};
constexpr unsigned counterMaximum(StoryCounter counter) noexcept {
    switch (counter) {
        case StoryCounter::ThraddashBodyCount: return 31u;
        case StoryCounter::LanderShields: return 15u;
        case StoryCounter::ArilouSpaceCounter: return 15u;
        case StoryCounter::ArilouSpaceSide: return 3u;
        case StoryCounter::EscapeCounter: return 255u;
        case StoryCounter::MelnormeAnger: return 3u;
        case StoryCounter::PortalCounter: return 15u;
        case StoryCounter::SlylandroMultiplier: return 7u;

        case StoryCounter::ColonyGrpoffs0: return 4294967295u;
        case StoryCounter::DruugeHomeVisits: return 7u;
        case StoryCounter::DruugeVisits: return 7u;
        case StoryCounter::FoundPlutoSpathi: return 3u;
        case StoryCounter::HelixVisits: return 7u;
        case StoryCounter::KnowAboutShattered: return 3u;
        case StoryCounter::Melnorme0Grpoffs0: return 4294967295u;
        case StoryCounter::Melnorme1Grpoffs0: return 4294967295u;
        case StoryCounter::Melnorme2Grpoffs0: return 4294967295u;
        case StoryCounter::Melnorme3Grpoffs0: return 4294967295u;
        case StoryCounter::Melnorme4Grpoffs0: return 4294967295u;
        case StoryCounter::Melnorme5Grpoffs0: return 4294967295u;
        case StoryCounter::Melnorme6Grpoffs0: return 4294967295u;
        case StoryCounter::Melnorme7Grpoffs0: return 4294967295u;
        case StoryCounter::Melnorme8Grpoffs0: return 4294967295u;
        case StoryCounter::MyconVisits: return 7u;
        case StoryCounter::RainbowWorld0: return 255u;
        case StoryCounter::RainbowWorld1: return 3u;
        case StoryCounter::SamatraGrpoffs0: return 4294967295u;
        case StoryCounter::ShofixtiGrpoffs0: return 4294967295u;
        case StoryCounter::SyreenHomeVisits: return 7u;
        case StoryCounter::ThraddMission: return 7u;
        case StoryCounter::UltronCondition: return 7u;
        case StoryCounter::UrquanProbeGrpoffs0: return 4294967295u;
        case StoryCounter::ZoqfotDistress: return 3u;
        case StoryCounter::ZoqfotGrpoffs0: return 4294967295u;

        case StoryCounter::BombProgress: return 3;
        case StoryCounter::ShofixtiVisits: return 7;
        case StoryCounter::EncounterContext: return 255;
        case StoryCounter::BattlePlanet: return 255;
    }
    return 0; // Invalid enum values are rejected by the runtime overload.
}
namespace detail { struct PlayerStorage; struct LegacyStateAccess; }

// Personal story flags, counters and discovery progress, owned by value within
// PlayerEntity. Fleet and navigation are separate responsibilities. During migration
// this views the existing packed bytes; there is no parallel bitset.
// All operations run on the simulation thread. Unknown enums throw invalid_argument.
class StoryState {
public:
    StoryState(const StoryState &) = delete;
    StoryState &operator=(const StoryState &) = delete;
    bool read(StoryFlag flag) const;
    unsigned read(StoryCounter counter) const;
    void set(StoryFlag flag);
    void set(StoryFlag flag, bool value);
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
        static_assert(counterMaximum(Counter) > 0, "unknown story counter");
        static_assert(Value <= counterMaximum(Counter), "story counter value out of range");
        set(Counter, Value);
    }
private:
    friend class PlayerEntity;
    explicit StoryState(detail::PlayerStorage &storage) noexcept;
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
    NavigationState &navigationState() noexcept;
    const NavigationState &navigationState() const noexcept;
    const UserProfile &profile() const noexcept; // Borrowed; ID is immutable.
    std::uint32_t fuel() const noexcept;
    std::uint16_t crew() const noexcept;
    // Raw legacy logical coordinates; navigation enforces universe bounds.
    void setHyperSpacePosition(std::int32_t x, std::int32_t y) noexcept;
    ShipState ship() const; // Detached copy, including equipment and names.
    PlayerLocation location() const noexcept;
    // Storage quantities only; gameplay callers must enforce capacity/cost rules.
    void setFuel(std::uint32_t fuel) noexcept;
    void setCrew(std::uint16_t crew) noexcept;
    // Borrow owned escorts; valid for this player's lifetime.
    SIS &sis() noexcept;
    const SIS &sis() const noexcept;
    Fleet &fleet() noexcept;
    const Fleet &fleet() const noexcept;
    // Borrow personal story progress; valid for this player's lifetime.
    StoryState &story() noexcept;
    const StoryState &story() const noexcept;
private:
    friend struct detail::LegacyStateAccess;
    NavigationState navigation_;
    UserProfile profile_;
    std::unique_ptr<detail::PlayerStorage> storage_;
    StoryState story_;
    std::unique_ptr<Fleet> fleet_;
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
class HyperSpaceNavigation;
class NavigationBinding;
class GameSession {
public:
    // Reject null state (invalid_argument) and unknown player (out_of_range).
    // Default entropy is sampled once per session. An explicit seed reproduces
    // its mode-seed sequence. This transient source is not legacy save data.
    GameSession(std::shared_ptr<GameState> state, PlayerId player,
                std::uint32_t seed = std::random_device{}());
    std::uint32_t nextModeSeed() noexcept { return static_cast<std::uint32_t>(modeSeeds_()); }
    ~GameSession();
    GameSession(const GameSession &) = delete;
    GameSession &operator=(const GameSession &) = delete;
    // Flat synchronous dispatcher. Releases the current mode before returning.
    // Null modes, invalid transitions and C++ exceptions become Failed exits.
    SessionExit run(std::unique_ptr<GameMode> initialMode);
    // Synchronous nested execution. Only call from the active mode, with a fresh
    // child bound to the same state/player. Parent remains alive on the stack.
    // Child returns ModeCompleted or SessionExit; a returned exit must propagate
    // outward before further gameplay. Child exceptions currently abort (TODO).
    MenuResult swapGameMode(std::unique_ptr<GameMode> child);
    bool isRunning() const noexcept { return running_; } // C/credits compatibility host selection only.
    // Production lifecycle/legacy adapters; load only at an unwound boundary.
    SessionExit runCampaign(std::optional<unsigned> loadSlot = std::nullopt);
    // Transitional C menu mailbox. Native menus return the same optional result.
    // Gameplay-thread only. Shutdown > failure > load > title; first slot wins.
    void requestMenuExit(SessionExit exit);
    void requestLoad(unsigned slot);
    MenuResult menuExit() const;
    HyperSpaceNavigation &navigation(); // Checked C callback borrow; simulation thread only.
    std::shared_ptr<GameState> stateOwner() noexcept;
    PlayerId playerId() const noexcept;
    PlayerEntity &player();
    const PlayerEntity &player() const;
    WorldState &world() noexcept;
    const WorldState &world() const noexcept;
    std::shared_ptr<WorldState> worldOwner() noexcept;
private:
    std::shared_ptr<GameState> state_;
    PlayerId player_;
    std::minstd_rand0 modeSeeds_;
    std::unique_ptr<GameMode> currentMode_;
    MenuResult pendingExit_;
    bool running_ = false;
    friend class NavigationBinding;
    HyperSpaceNavigation *navigation_ = nullptr;
};

// Process-lifetime single-player session used by the C compatibility adapter.
// Do not replace its owner: legacy ship/resource code retains payload addresses.
// The payload is zero-initialized once; InitGlobData continues to reset it in place.
GameSession &gameSession();
} // namespace uqm
#endif
