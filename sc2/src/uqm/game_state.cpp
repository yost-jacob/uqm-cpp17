#include "game_state.hpp"
#include "globdata.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace uqm {
namespace detail {
struct PlayerStorage { GLOBDATA legacy{}; };
// Only the C boundary sees a mutable legacy aggregate. Native callers use the
// player/campaign methods and detached ship/location values.
struct LegacyStateAccess {
    static GLOBDATA &data(PlayerEntity &player) noexcept {
        return player.storage_->legacy;
    }
};
}
namespace {
struct Field { int first, last; };
Field field(StoryFlag flag) {
    switch (flag) {
        case StoryFlag::SupoxUltronHelp: return {SUPOX_ULTRON_HELP, END_SUPOX_ULTRON_HELP};
        case StoryFlag::ShipToCompel: return {SHIP_TO_COMPEL, END_SHIP_TO_COMPEL};
        case StoryFlag::ReadyToConfuseUrquan: return {READY_TO_CONFUSE_URQUAN, END_READY_TO_CONFUSE_URQUAN};
        case StoryFlag::PortalSpawnerOnShip: return {PORTAL_SPAWNER_ON_SHIP, END_PORTAL_SPAWNER_ON_SHIP};
        case StoryFlag::DiscussedUltron: return {DISCUSSED_ULTRON, END_DISCUSSED_ULTRON};
        case StoryFlag::ChmmrEmerging: return {CHMMR_EMERGING, END_CHMMR_EMERGING};
        case StoryFlag::Artifact3OnShip: return {ARTIFACT_3_ON_SHIP, END_ARTIFACT_3_ON_SHIP};
        case StoryFlag::Artifact2OnShip: return {ARTIFACT_2_ON_SHIP, END_ARTIFACT_2_ON_SHIP};
        case StoryFlag::BattleSegue: return {BATTLE_SEGUE, END_BATTLE_SEGUE};
        case StoryFlag::BombCarrier: return {BOMB_CARRIER, END_BOMB_CARRIER};
        case StoryFlag::ThraddManner: return {THRADD_MANNER, END_THRADD_MANNER};
        case StoryFlag::TalkingPetOnShip: return {TALKING_PET_ON_SHIP, END_TALKING_PET_ON_SHIP};
        case StoryFlag::KohrAhKilledAll: return {KOHR_AH_KILLED_ALL, END_KOHR_AH_KILLED_ALL};
        case StoryFlag::PlanetaryLanding: return {PLANETARY_LANDING, END_PLANETARY_LANDING};
        case StoryFlag::ImprovedLanderSpeed: return {IMPROVED_LANDER_SPEED, END_IMPROVED_LANDER_SPEED};
        case StoryFlag::ImprovedLanderShot: return {IMPROVED_LANDER_SHOT, END_IMPROVED_LANDER_SHOT};
        case StoryFlag::ImprovedLanderCargo: return {IMPROVED_LANDER_CARGO, END_IMPROVED_LANDER_CARGO};
        case StoryFlag::ArilouSpace: return {ARILOU_SPACE, END_ARILOU_SPACE};
        case StoryFlag::BroadcasterResponse: return {BROADCASTER_RESPONSE, END_BROADCASTER_RESPONSE};
        case StoryFlag::UsedBroadcaster: return {USED_BROADCASTER, END_USED_BROADCASTER};

        case StoryFlag::AquaHelix: return {AQUA_HELIX, END_AQUA_HELIX};
        case StoryFlag::AquaHelixOnShip: return {AQUA_HELIX_ON_SHIP, END_AQUA_HELIX_ON_SHIP};
        case StoryFlag::BombUnprotected: return {BOMB_UNPROTECTED, END_BOMB_UNPROTECTED};
        case StoryFlag::BurvixeseBroadcasters: return {BURVIXESE_BROADCASTERS, END_BURVIXESE_BROADCASTERS};
        case StoryFlag::BurvBroadcastersOnShip: return {BURV_BROADCASTERS_ON_SHIP, END_BURV_BROADCASTERS_ON_SHIP};
        case StoryFlag::ClearSpindle: return {CLEAR_SPINDLE, END_CLEAR_SPINDLE};
        case StoryFlag::ClearSpindleOnShip: return {CLEAR_SPINDLE_ON_SHIP, END_CLEAR_SPINDLE_ON_SHIP};
        case StoryFlag::DruugeManner: return {DRUUGE_MANNER, END_DRUUGE_MANNER};
        case StoryFlag::EggCase0OnShip: return {EGG_CASE0_ON_SHIP, END_EGG_CASE0_ON_SHIP};
        case StoryFlag::EggCase1OnShip: return {EGG_CASE1_ON_SHIP, END_EGG_CASE1_ON_SHIP};
        case StoryFlag::EggCase2OnShip: return {EGG_CASE2_ON_SHIP, END_EGG_CASE2_ON_SHIP};
        case StoryFlag::HelixUnprotected: return {HELIX_UNPROTECTED, END_HELIX_UNPROTECTED};
        case StoryFlag::KnowSyreenVault: return {KNOW_SYREEN_VAULT, END_KNOW_SYREEN_VAULT};
        case StoryFlag::KohrAhFrenzy: return {KOHR_AH_FRENZY, END_KOHR_AH_FRENZY};
        case StoryFlag::MaidensOnShip: return {MAIDENS_ON_SHIP, END_MAIDENS_ON_SHIP};
        case StoryFlag::MetZoqfot: return {MET_ZOQFOT, END_MET_ZOQFOT};
        case StoryFlag::MoonbaseDestroyed: return {MOONBASE_DESTROYED, END_MOONBASE_DESTROYED};
        case StoryFlag::MoonbaseOnShip: return {MOONBASE_ON_SHIP, END_MOONBASE_ON_SHIP};
        case StoryFlag::MyconFellForAmbush: return {MYCON_FELL_FOR_AMBUSH, END_MYCON_FELL_FOR_AMBUSH};
        case StoryFlag::PlayerHypnotized: return {PLAYER_HYPNOTIZED, END_PLAYER_HYPNOTIZED};
        case StoryFlag::PortalKey: return {PORTAL_KEY, END_PORTAL_KEY};
        case StoryFlag::PortalKeyOnShip: return {PORTAL_KEY_ON_SHIP, END_PORTAL_KEY_ON_SHIP};
        case StoryFlag::ProbeMessageDelivered: return {PROBE_MESSAGE_DELIVERED, END_PROBE_MESSAGE_DELIVERED};
        case StoryFlag::RosySphere: return {ROSY_SPHERE, END_ROSY_SPHERE};
        case StoryFlag::RosySphereOnShip: return {ROSY_SPHERE_ON_SHIP, END_ROSY_SPHERE_ON_SHIP};
        case StoryFlag::ShipVaultUnlocked: return {SHIP_VAULT_UNLOCKED, END_SHIP_VAULT_UNLOCKED};
        case StoryFlag::ShofixtiBroKia: return {SHOFIXTI_BRO_KIA, END_SHOFIXTI_BRO_KIA};
        case StoryFlag::ShofixtiKia: return {SHOFIXTI_KIA, END_SHOFIXTI_KIA};
        case StoryFlag::ShofixtiMaidens: return {SHOFIXTI_MAIDENS, END_SHOFIXTI_MAIDENS};
        case StoryFlag::ShofixtiRecruited: return {SHOFIXTI_RECRUITED, END_SHOFIXTI_RECRUITED};
        case StoryFlag::SpathiCreaturesEliminated: return {SPATHI_CREATURES_ELIMINATED, END_SPATHI_CREATURES_ELIMINATED};
        case StoryFlag::SpathiCreaturesExamined: return {SPATHI_CREATURES_EXAMINED, END_SPATHI_CREATURES_EXAMINED};
        case StoryFlag::SpathiShieldedSelves: return {SPATHI_SHIELDED_SELVES, END_SPATHI_SHIELDED_SELVES};
        case StoryFlag::SunDevice: return {SUN_DEVICE, END_SUN_DEVICE};
        case StoryFlag::SunDeviceUnguarded: return {SUN_DEVICE_UNGUARDED, END_SUN_DEVICE_UNGUARDED};
        case StoryFlag::SyreenShuttleOnShip: return {SYREEN_SHUTTLE_ON_SHIP, END_SYREEN_SHUTTLE_ON_SHIP};
        case StoryFlag::TaaloProtector: return {TAALO_PROTECTOR, END_TAALO_PROTECTOR};
        case StoryFlag::TaaloProtectorOnShip: return {TAALO_PROTECTOR_ON_SHIP, END_TAALO_PROTECTOR_ON_SHIP};
        case StoryFlag::TaaloUnprotected: return {TAALO_UNPROTECTED, END_TAALO_UNPROTECTED};
        case StoryFlag::TalkingPet: return {TALKING_PET, END_TALKING_PET};
        case StoryFlag::UmgahBroadcasters: return {UMGAH_BROADCASTERS, END_UMGAH_BROADCASTERS};
        case StoryFlag::UmgahBroadcastersOnShip: return {UMGAH_BROADCASTERS_ON_SHIP, END_UMGAH_BROADCASTERS_ON_SHIP};
        case StoryFlag::UmgahZombieBlobbies: return {UMGAH_ZOMBIE_BLOBBIES, END_UMGAH_ZOMBIE_BLOBBIES};
        case StoryFlag::UrquanMessedUp: return {URQUAN_MESSED_UP, END_URQUAN_MESSED_UP};
        case StoryFlag::UrquanProtectingSamatra: return {URQUAN_PROTECTING_SAMATRA, END_URQUAN_PROTECTING_SAMATRA};
        case StoryFlag::UtwigBomb: return {UTWIG_BOMB, END_UTWIG_BOMB};
        case StoryFlag::UtwigBombOnShip: return {UTWIG_BOMB_ON_SHIP, END_UTWIG_BOMB_ON_SHIP};
        case StoryFlag::UtwigHaveUltron: return {UTWIG_HAVE_ULTRON, END_UTWIG_HAVE_ULTRON};
        case StoryFlag::VuxBeast: return {VUX_BEAST, END_VUX_BEAST};
        case StoryFlag::VuxBeastOnShip: return {VUX_BEAST_ON_SHIP, END_VUX_BEAST_ON_SHIP};
        case StoryFlag::ZexIsDead: return {ZEX_IS_DEAD, END_ZEX_IS_DEAD};

        case StoryFlag::StarbaseAccessGranted: return {STARBASE_AVAILABLE, END_STARBASE_AVAILABLE};
        case StoryFlag::YehatCivilWar: return {YEHAT_CIVIL_WAR, END_YEHAT_CIVIL_WAR};
        case StoryFlag::ChmmrFreed: return {CHMMR_UNLEASHED, END_CHMMR_UNLEASHED};
        case StoryFlag::IlwrathDeceived: return {ILWRATH_DECEIVED, END_ILWRATH_DECEIVED};
        case StoryFlag::SunDeviceOnShip: return {SUN_DEVICE_ON_SHIP, END_SUN_DEVICE_ON_SHIP};
    }
    throw std::invalid_argument("unknown story flag");
}
Field field(StoryCounter counter) {
    switch (counter) {
        case StoryCounter::ThraddashBodyCount: return {THRADDASH_BODY_COUNT, END_THRADDASH_BODY_COUNT};
        case StoryCounter::LanderShields: return {LANDER_SHIELDS, END_LANDER_SHIELDS};
        case StoryCounter::ArilouSpaceCounter: return {ARILOU_SPACE_COUNTER, END_ARILOU_SPACE_COUNTER};
        case StoryCounter::ArilouSpaceSide: return {ARILOU_SPACE_SIDE, END_ARILOU_SPACE_SIDE};
        case StoryCounter::EscapeCounter: return {ESCAPE_COUNTER, END_ESCAPE_COUNTER};
        case StoryCounter::MelnormeAnger: return {MELNORME_ANGER, END_MELNORME_ANGER};
        case StoryCounter::PortalCounter: return {PORTAL_COUNTER, END_PORTAL_COUNTER};
        case StoryCounter::SlylandroMultiplier: return {SLYLANDRO_MULTIPLIER, END_SLYLANDRO_MULTIPLIER};

        case StoryCounter::ColonyGrpoffs0: return {COLONY_GRPOFFS0, COLONY_GRPOFFS0 + 31};
        case StoryCounter::DruugeHomeVisits: return {DRUUGE_HOME_VISITS, END_DRUUGE_HOME_VISITS};
        case StoryCounter::DruugeVisits: return {DRUUGE_VISITS, END_DRUUGE_VISITS};
        case StoryCounter::FoundPlutoSpathi: return {FOUND_PLUTO_SPATHI, END_FOUND_PLUTO_SPATHI};
        case StoryCounter::HelixVisits: return {HELIX_VISITS, END_HELIX_VISITS};
        case StoryCounter::KnowAboutShattered: return {KNOW_ABOUT_SHATTERED, END_KNOW_ABOUT_SHATTERED};
        case StoryCounter::Melnorme0Grpoffs0: return {MELNORME0_GRPOFFS0, MELNORME0_GRPOFFS0 + 31};
        case StoryCounter::Melnorme1Grpoffs0: return {MELNORME1_GRPOFFS0, MELNORME1_GRPOFFS0 + 31};
        case StoryCounter::Melnorme2Grpoffs0: return {MELNORME2_GRPOFFS0, MELNORME2_GRPOFFS0 + 31};
        case StoryCounter::Melnorme3Grpoffs0: return {MELNORME3_GRPOFFS0, MELNORME3_GRPOFFS0 + 31};
        case StoryCounter::Melnorme4Grpoffs0: return {MELNORME4_GRPOFFS0, MELNORME4_GRPOFFS0 + 31};
        case StoryCounter::Melnorme5Grpoffs0: return {MELNORME5_GRPOFFS0, MELNORME5_GRPOFFS0 + 31};
        case StoryCounter::Melnorme6Grpoffs0: return {MELNORME6_GRPOFFS0, MELNORME6_GRPOFFS0 + 31};
        case StoryCounter::Melnorme7Grpoffs0: return {MELNORME7_GRPOFFS0, MELNORME7_GRPOFFS0 + 31};
        case StoryCounter::Melnorme8Grpoffs0: return {MELNORME8_GRPOFFS0, MELNORME8_GRPOFFS0 + 31};
        case StoryCounter::MyconVisits: return {MYCON_VISITS, END_MYCON_VISITS};
        case StoryCounter::RainbowWorld0: return {RAINBOW_WORLD0, END_RAINBOW_WORLD0};
        case StoryCounter::RainbowWorld1: return {RAINBOW_WORLD1, END_RAINBOW_WORLD1};
        case StoryCounter::SamatraGrpoffs0: return {SAMATRA_GRPOFFS0, SAMATRA_GRPOFFS0 + 31};
        case StoryCounter::ShofixtiGrpoffs0: return {SHOFIXTI_GRPOFFS0, SHOFIXTI_GRPOFFS0 + 31};
        case StoryCounter::SyreenHomeVisits: return {SYREEN_HOME_VISITS, END_SYREEN_HOME_VISITS};
        case StoryCounter::ThraddMission: return {THRADD_MISSION, END_THRADD_MISSION};
        case StoryCounter::UltronCondition: return {ULTRON_CONDITION, END_ULTRON_CONDITION};
        case StoryCounter::UrquanProbeGrpoffs0: return {URQUAN_PROBE_GRPOFFS0, URQUAN_PROBE_GRPOFFS0 + 31};
        case StoryCounter::ZoqfotDistress: return {ZOQFOT_DISTRESS, END_ZOQFOT_DISTRESS};
        case StoryCounter::ZoqfotGrpoffs0: return {ZOQFOT_GRPOFFS0, ZOQFOT_GRPOFFS0 + 31};

        case StoryCounter::BattlePlanet: return {BATTLE_PLANET, END_BATTLE_PLANET};
        case StoryCounter::EncounterContext: return {GLOBAL_FLAGS_AND_DATA, END_GLOBAL_FLAGS_AND_DATA};
        case StoryCounter::BombProgress: return {CHMMR_BOMB_STATE, END_CHMMR_BOMB_STATE};
        case StoryCounter::ShofixtiVisits: return {SHOFIXTI_VISITS, END_SHOFIXTI_VISITS};
    }
    throw std::invalid_argument("unknown story counter");
}
static_assert(counterMaximum(StoryCounter::BombProgress)
        == (1u << (END_CHMMR_BOMB_STATE - CHMMR_BOMB_STATE + 1)) - 1);
static_assert(counterMaximum(StoryCounter::ShofixtiVisits)
        == (1u << (END_SHOFIXTI_VISITS - SHOFIXTI_VISITS + 1)) - 1);
unsigned read(const detail::PlayerStorage &storage, Field field) {
    if (field.last - field.first + 1 == 32)
        return getGameState32(storage.legacy.Game_state.GameState, field.first);
    return getGameState(storage.legacy.Game_state.GameState, field.first, field.last);
}
void write(detail::PlayerStorage &storage, Field field, unsigned value) {
    if (field.last - field.first + 1 == 32) {
        setGameState32(storage.legacy.Game_state.GameState, field.first, value
#ifdef STATE_DEBUG
                , "typed campaign group offset"
#endif
                );
        return;
    }
    setGameState(storage.legacy.Game_state.GameState, field.first, field.last,
            static_cast<BYTE>(value)
#ifdef STATE_DEBUG
            , "typed campaign field"
#endif
            );
}
// Legacy names can be restored from a corrupt save without a terminator. Bound
// reads to the array; never construct a string by scanning past its end.
template<std::size_t N>
std::string name(const char (&text)[N]) {
    return {text, std::find(text, text + N, '\0')};
}
}

CampaignState::CampaignState(detail::PlayerStorage &storage) noexcept : storage_(storage) {}
bool CampaignState::read(StoryFlag flag) const { return uqm::read(storage_, field(flag)) != 0; }
unsigned CampaignState::read(StoryCounter counter) const { return uqm::read(storage_, field(counter)); }
void CampaignState::set(StoryFlag flag) { write(storage_, field(flag), 1); }
void CampaignState::set(StoryFlag flag, bool value) { write(storage_, field(flag), value ? 1 : 0); }
void CampaignState::clear(StoryFlag flag) { write(storage_, field(flag), 0); }
void CampaignState::set(StoryCounter counter, unsigned value) {
    const auto bits = field(counter); // Reject invalid enums before any mutation.
    if (value > counterMaximum(counter))
        throw std::out_of_range("story counter value out of range");
    write(storage_, bits, value);
}

PlayerEntity::PlayerEntity(UserProfile profile)
    : profile_(std::move(profile)), storage_(std::make_unique<detail::PlayerStorage>()),
      campaign_(*storage_) {}
PlayerEntity::~PlayerEntity() = default;
PlayerId PlayerEntity::id() const noexcept { return profile_.id; }
NavigationState &PlayerEntity::navigationState() noexcept { return navigation_; }
const NavigationState &PlayerEntity::navigationState() const noexcept { return navigation_; }
const UserProfile &PlayerEntity::profile() const noexcept { return profile_; }
std::uint32_t PlayerEntity::fuel() const noexcept { return storage_->legacy.SIS_state.FuelOnBoard; }
std::uint16_t PlayerEntity::crew() const noexcept { return storage_->legacy.SIS_state.CrewEnlisted; }
void PlayerEntity::setHyperSpacePosition(std::int32_t x, std::int32_t y) noexcept {
    storage_->legacy.SIS_state.log_x = x;
    storage_->legacy.SIS_state.log_y = y;
}
ShipState PlayerEntity::ship() const {
    const auto &sis = storage_->legacy.SIS_state;
    ShipState result{};
    result.resourceUnits = sis.ResUnits;
    result.fuel = sis.FuelOnBoard;
    result.crew = sis.CrewEnlisted;
    result.mineralMass = sis.TotalElementMass;
    result.biologicalMass = sis.TotalBioMass;
    static_assert(std::size(sis.ElementAmounts) == std::tuple_size<decltype(result.minerals)>::value);
    static_assert(std::size(sis.ModuleSlots) == std::tuple_size<decltype(result.modules)>::value);
    static_assert(std::size(sis.DriveSlots) == std::tuple_size<decltype(result.drives)>::value);
    static_assert(std::size(sis.JetSlots) == std::tuple_size<decltype(result.jets)>::value);
    std::copy(std::begin(sis.ElementAmounts), std::end(sis.ElementAmounts), result.minerals.begin());
    std::copy(std::begin(sis.ModuleSlots), std::end(sis.ModuleSlots), result.modules.begin());
    std::copy(std::begin(sis.DriveSlots), std::end(sis.DriveSlots), result.drives.begin());
    std::copy(std::begin(sis.JetSlots), std::end(sis.JetSlots), result.jets.begin());
    result.landers = sis.NumLanders;
    result.shipName = name(sis.ShipName);
    result.commanderName = name(sis.CommanderName);
    result.planetName = name(sis.PlanetName);
    return result;
}
PlayerLocation PlayerEntity::location() const noexcept {
    const auto &sis = storage_->legacy.SIS_state;
    const auto &game = storage_->legacy.Game_state;
    return {sis.log_x, sis.log_y, game.ip_location.x, game.ip_location.y,
            game.ShipFacing, game.ip_planet, game.in_orbit};
}
void PlayerEntity::setFuel(std::uint32_t fuel) noexcept { storage_->legacy.SIS_state.FuelOnBoard = fuel; }
void PlayerEntity::setCrew(std::uint16_t crew) noexcept { storage_->legacy.SIS_state.CrewEnlisted = crew; }
CampaignState &PlayerEntity::campaign() noexcept { return campaign_; }
const CampaignState &PlayerEntity::campaign() const noexcept { return campaign_; }

GameState::GameState(UserProfile profile)
    : GameState(std::move(profile), std::make_shared<WorldState>()) {}
GameState::GameState(UserProfile profile, std::shared_ptr<WorldState> world)
    : world_(std::move(world)), player_(std::move(profile)) {
    if (!world_)
        throw std::invalid_argument("game state requires world state");
}
WorldState &GameState::world() noexcept { return *world_; }
const WorldState &GameState::world() const noexcept { return *world_; }
std::shared_ptr<WorldState> GameState::worldOwner() noexcept { return world_; }
std::shared_ptr<const WorldState> GameState::worldOwner() const noexcept { return world_; }
PlayerEntity &GameState::player(PlayerId id) {
    if (id != player_.id())
        throw std::out_of_range("unknown player ID");
    return player_;
}
const PlayerEntity &GameState::player(PlayerId id) const {
    if (id != player_.id())
        throw std::out_of_range("unknown player ID");
    return player_;
}
GameSession::GameSession(std::shared_ptr<GameState> state, PlayerId player)
    : state_(std::move(state)), player_(player) {
    if (!state_)
        throw std::invalid_argument("session requires game state");
    state_->player(player_); // Validate binding now, not at the first interaction.
}
GameSession::~GameSession() = default;
std::shared_ptr<GameState> GameSession::stateOwner() noexcept { return state_; }
PlayerId GameSession::playerId() const noexcept { return player_; }
PlayerEntity &GameSession::player() { return state_->player(player_); }
const PlayerEntity &GameSession::player() const { return std::as_const(*state_).player(player_); }
std::shared_ptr<WorldState> GameSession::worldOwner() noexcept { return state_->worldOwner(); }
WorldState &GameSession::world() noexcept { return state_->world(); }
const WorldState &GameSession::world() const noexcept { return std::as_const(*state_).world(); }
GameSession &gameSession() {
    static GameSession session{std::make_shared<GameState>(UserProfile{SinglePlayerId, "Player"}), SinglePlayerId};
    return session;
}
} // namespace uqm

extern "C" GLOBDATA *GetLegacyGlobData(void) {
    try {
        return &uqm::detail::LegacyStateAccess::data(uqm::gameSession().player());
    } catch (...) {
        // Only first-use construction can fail. C has no recovery protocol for
        // missing global storage; never let a C++ exception cross its ABI.
        std::abort();
    }
}
