#include "uqm/game_state.hpp"
#include <boost/test/unit_test.hpp>
#include <stdexcept>
#include <type_traits>
#include <utility>

extern "C" {
void game_state_c_reset(void);
void *game_state_c_address(void);
void game_state_c_seed(void);
int game_state_c_observes_native_updates(void);
void game_state_c_save(void);
void game_state_c_restore(void);
void game_state_c_unterminated_name(void);
unsigned game_state_c_name_capacity(void);
int game_state_c_counter_neighbors_intact(void);
int game_state_c_packed_words(void);
}
using namespace uqm;
static_assert(std::is_same_v<decltype(std::declval<const GameState &>().player(SinglePlayerId)), const PlayerEntity &>);
static_assert(std::is_same_v<decltype(std::declval<const GameSession &>().player()), const PlayerEntity &>);
static_assert(std::is_same_v<decltype(std::declval<const PlayerEntity &>().campaign()), const CampaignState &>);
static_assert(!std::is_copy_constructible_v<PlayerEntity>);
static_assert(!std::is_move_constructible_v<PlayerEntity>);

BOOST_AUTO_TEST_CASE(player_lookup_hides_storage_and_validates_session_binding)
{
    auto state = std::make_shared<GameState>(UserProfile{PlayerId{17}, "Alice"});
    GameSession session{state, PlayerId{17}};
    BOOST_TEST(session.playerId().value == 17u);
    BOOST_TEST(session.player().profile().displayName == "Alice");
    BOOST_TEST(&session.player() == &state->player(PlayerId{17}));
    BOOST_CHECK_THROW(state->player(PlayerId{18}), std::out_of_range);
    BOOST_CHECK_THROW(std::as_const(*state).player(PlayerId{18}), std::out_of_range);
    BOOST_CHECK_THROW((GameSession{state, PlayerId{18}}), std::out_of_range);
    BOOST_CHECK_THROW((GameSession{nullptr, PlayerId{17}}), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(sessions_retain_state_and_share_the_same_player)
{
    auto state = std::make_shared<GameState>(UserProfile{PlayerId{17}, "Alice"});
    std::weak_ptr<GameState> lifetime = state;
    {
        GameSession first{state, PlayerId{17}}, second{state, PlayerId{17}};
        state.reset();
        first.player().setFuel(654321);
        first.player().campaign().set<StoryCounter::BombProgress, 3>();
        BOOST_TEST(second.player().ship().fuel == 654321u);
        BOOST_TEST(second.player().campaign().read(StoryCounter::BombProgress) == 3u);
        BOOST_TEST(!lifetime.expired());
    }
    BOOST_TEST(lifetime.expired());
}

BOOST_AUTO_TEST_CASE(independent_players_have_independent_personal_state)
{
    GameState alice{UserProfile{PlayerId{1}, "Alice"}};
    GameState bob{UserProfile{PlayerId{2}, "Bob"}};
    alice.player(PlayerId{1}).campaign().set(StoryFlag::SunDeviceOnShip);
    alice.player(PlayerId{1}).setCrew(50);
    BOOST_TEST(!bob.player(PlayerId{2}).campaign().read(StoryFlag::SunDeviceOnShip));
    BOOST_TEST(bob.player(PlayerId{2}).ship().crew == 0u);
}

struct LegacyPlayerFixture {
    LegacyPlayerFixture() { game_state_c_reset(); }
    ~LegacyPlayerFixture() { game_state_c_reset(); }
};
BOOST_FIXTURE_TEST_SUITE(legacy_player_state, LegacyPlayerFixture)
BOOST_AUTO_TEST_CASE(c_and_cpp_observe_one_authoritative_ship_and_campaign)
{
    game_state_c_seed();
    auto &player = gameSession().player();
    const auto ship = player.ship();
    BOOST_TEST(ship.fuel == 123456u);
    BOOST_TEST(ship.crew == 42u);
    BOOST_TEST(ship.resourceUnits == 98765u);
    BOOST_TEST(ship.mineralMass == 28u);
    BOOST_TEST(ship.biologicalMass == 9u);
    BOOST_TEST(ship.minerals.back() == 28u);
    BOOST_TEST(ship.modules.back() == 12u);
    BOOST_TEST(ship.drives.back() == 3u);
    BOOST_TEST(ship.jets.back() == 4u);
    BOOST_TEST(ship.landers == 2u);
    BOOST_TEST(ship.shipName == "Vindicator");
    BOOST_TEST(ship.commanderName == "Captain");
    BOOST_TEST(ship.planetName == "Earth");
    auto &campaign = player.campaign();
    BOOST_TEST(campaign.read(StoryFlag::StarbaseAccessGranted));
    BOOST_TEST(campaign.read(StoryFlag::SunDeviceOnShip));
    BOOST_TEST(campaign.read(StoryCounter::BombProgress) == 2u);
    BOOST_TEST(campaign.read(StoryCounter::ShofixtiVisits) == 5u);
    player.setFuel(999);
    player.setCrew(65535); // Preserve the legacy death sentinel.
    campaign.clear(StoryFlag::StarbaseAccessGranted);
    campaign.set<StoryCounter::BombProgress, 3>();
    campaign.set(StoryCounter::ShofixtiVisits, 7);
    BOOST_TEST(game_state_c_observes_native_updates() == 1);
    BOOST_TEST(ship.fuel == 123456u); // Detached copies are not live state.
}

BOOST_AUTO_TEST_CASE(location_uses_the_existing_coordinate_frames)
{
    game_state_c_seed();
    const auto position = gameSession().player().location();
    BOOST_TEST(position.logX == -12345);
    BOOST_TEST(position.logY == 67890);
    BOOST_TEST(position.interplanetaryX == -321);
    BOOST_TEST(position.interplanetaryY == 654);
    BOOST_TEST(position.facing == 15u);
    BOOST_TEST(position.planet == 3u);
    BOOST_TEST(position.orbit == 1u);
}

BOOST_AUTO_TEST_CASE(rejected_counters_leave_state_and_neighbors_unchanged)
{
    game_state_c_seed();
    auto &campaign = gameSession().player().campaign();
    BOOST_CHECK_THROW(campaign.set(StoryCounter::BombProgress, 4), std::out_of_range);
    BOOST_CHECK_THROW(campaign.set(StoryCounter::ShofixtiVisits, 8), std::out_of_range);
    BOOST_CHECK_THROW(campaign.set(static_cast<StoryCounter>(999), 1), std::invalid_argument);
    BOOST_CHECK_THROW(campaign.read(static_cast<StoryCounter>(999)), std::invalid_argument);
    BOOST_CHECK_THROW(campaign.set(static_cast<StoryFlag>(999)), std::invalid_argument);
    BOOST_CHECK_THROW(campaign.clear(static_cast<StoryFlag>(999)), std::invalid_argument);
    BOOST_CHECK_THROW(campaign.read(static_cast<StoryFlag>(999)), std::invalid_argument);
    BOOST_TEST(campaign.read(StoryCounter::BombProgress) == 2u);
    BOOST_TEST(campaign.read(StoryCounter::ShofixtiVisits) == 5u);
    for (unsigned value = 0; value <= 7; ++value) {
        campaign.set(StoryCounter::ShofixtiVisits, value);
        BOOST_TEST(campaign.read(StoryCounter::ShofixtiVisits) == value);
        BOOST_TEST(game_state_c_counter_neighbors_intact() == 1);
    }
}

BOOST_AUTO_TEST_CASE(legacy_reset_and_restore_preserve_borrowed_storage_addresses)
{
    game_state_c_seed();
    auto *const address = game_state_c_address();
    auto &player = gameSession().player();
    auto &campaign = player.campaign();
    game_state_c_save();
    game_state_c_reset();
    BOOST_TEST(player.ship().crew == 0u);
    BOOST_TEST(campaign.read(StoryCounter::BombProgress) == 0u);
    game_state_c_restore();
    BOOST_TEST(game_state_c_address() == address);
    BOOST_TEST(&gameSession().player() == &player);
    BOOST_TEST(player.ship().crew == 42u);
    BOOST_TEST(campaign.read(StoryCounter::BombProgress) == 2u);
}

BOOST_AUTO_TEST_CASE(names_are_bounded_even_without_legacy_null_termination)
{
    game_state_c_unterminated_name();
    BOOST_TEST(gameSession().player().ship().shipName
            == std::string(game_state_c_name_capacity(), 'X'));
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(legacy_unaligned_32bit_fields_survive_copy)
{
    BOOST_TEST(game_state_c_packed_words() == 1);
}
