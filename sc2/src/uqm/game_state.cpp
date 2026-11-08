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
        case StoryFlag::StarbaseAccessGranted: return {STARBASE_AVAILABLE, END_STARBASE_AVAILABLE};
        case StoryFlag::SunDeviceOnShip: return {SUN_DEVICE_ON_SHIP, END_SUN_DEVICE_ON_SHIP};
    }
    throw std::invalid_argument("unknown story flag");
}
Field field(StoryCounter counter) {
    switch (counter) {
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
    return getGameState(storage.legacy.Game_state.GameState, field.first, field.last);
}
void write(detail::PlayerStorage &storage, Field field, unsigned value) {
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
const UserProfile &PlayerEntity::profile() const noexcept { return profile_; }
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
