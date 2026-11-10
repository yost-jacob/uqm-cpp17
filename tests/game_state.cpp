#include "uqm/globdata.h"
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
static_assert(std::is_same_v<decltype(std::declval<const PlayerEntity &>().story()), const StoryState &>);
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
        first.player().story().set<StoryCounter::BombProgress, 3>();
        BOOST_TEST(second.player().ship().fuel == 654321u);
        BOOST_TEST(second.player().story().read(StoryCounter::BombProgress) == 3u);
        BOOST_TEST(!lifetime.expired());
    }
    BOOST_TEST(lifetime.expired());
}

BOOST_AUTO_TEST_CASE(independent_players_have_independent_personal_state)
{
    GameState alice{UserProfile{PlayerId{1}, "Alice"}};
    GameState bob{UserProfile{PlayerId{2}, "Bob"}};
    alice.player(PlayerId{1}).story().set(StoryFlag::SunDeviceOnShip);
    alice.player(PlayerId{1}).setCrew(50);
    BOOST_TEST(!bob.player(PlayerId{2}).story().read(StoryFlag::SunDeviceOnShip));
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
    auto &story = player.story();
    BOOST_TEST(story.read(StoryFlag::StarbaseAccessGranted));
    BOOST_TEST(story.read(StoryFlag::SunDeviceOnShip));
    BOOST_TEST(story.read(StoryCounter::BombProgress) == 2u);
    BOOST_TEST(story.read(StoryCounter::ShofixtiVisits) == 5u);
    player.setFuel(999);
    player.setCrew(65535); // Preserve the legacy death sentinel.
    story.clear(StoryFlag::StarbaseAccessGranted);
    story.set<StoryCounter::BombProgress, 3>();
    story.set(StoryCounter::ShofixtiVisits, 7);
    BOOST_TEST(game_state_c_observes_native_updates() == 1);
    BOOST_TEST(ship.fuel == 123456u); // Detached copies are not live state.
}

BOOST_AUTO_TEST_CASE(location_uses_the_existing_coordinate_frames)
{
    game_state_c_seed();
    const auto position = gameSession().player().travel();
    BOOST_TEST(position.position.x == -12345);
    BOOST_TEST(position.position.y == 67890);
    BOOST_TEST(position.flight.position.x == -321);
    BOOST_TEST(position.flight.position.y == 654);
    BOOST_TEST(position.flight.facing == 15u);
    BOOST_TEST(position.flight.planet == 3u);
    BOOST_TEST(position.flight.loadOrbit == 1u);
}

BOOST_AUTO_TEST_CASE(rejected_counters_leave_state_and_neighbors_unchanged)
{
    game_state_c_seed();
    auto &story = gameSession().player().story();
    BOOST_CHECK_THROW(story.set(StoryCounter::BombProgress, 4), std::out_of_range);
    BOOST_CHECK_THROW(story.set(StoryCounter::ShofixtiVisits, 8), std::out_of_range);
    BOOST_CHECK_THROW(story.set(static_cast<StoryCounter>(999), 1), std::invalid_argument);
    BOOST_CHECK_THROW(story.read(static_cast<StoryCounter>(999)), std::invalid_argument);
    BOOST_CHECK_THROW(story.set(static_cast<StoryFlag>(999)), std::invalid_argument);
    BOOST_CHECK_THROW(story.clear(static_cast<StoryFlag>(999)), std::invalid_argument);
    BOOST_CHECK_THROW(story.read(static_cast<StoryFlag>(999)), std::invalid_argument);
    BOOST_TEST(story.read(StoryCounter::BombProgress) == 2u);
    BOOST_TEST(story.read(StoryCounter::ShofixtiVisits) == 5u);
    for (unsigned value = 0; value <= 7; ++value) {
        story.set(StoryCounter::ShofixtiVisits, value);
        BOOST_TEST(story.read(StoryCounter::ShofixtiVisits) == value);
        BOOST_TEST(game_state_c_counter_neighbors_intact() == 1);
    }
}

BOOST_AUTO_TEST_CASE(legacy_reset_and_restore_preserve_borrowed_storage_addresses)
{
    game_state_c_seed();
    auto *const address = game_state_c_address();
    auto &player = gameSession().player();
    auto &story = player.story();
    game_state_c_save();
    game_state_c_reset();
    BOOST_TEST(player.ship().crew == 0u);
    BOOST_TEST(story.read(StoryCounter::BombProgress) == 0u);
    game_state_c_restore();
    BOOST_TEST(game_state_c_address() == address);
    BOOST_TEST(&gameSession().player() == &player);
    BOOST_TEST(player.ship().crew == 42u);
    BOOST_TEST(story.read(StoryCounter::BombProgress) == 2u);
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

BOOST_AUTO_TEST_CASE(system_story_fields_keep_the_legacy_packed_encoding)
{
    auto &story = gameSession().player().story();
    struct Restore {
        std::array<BYTE, sizeof GLOBAL(GameState)> bytes{};
        Restore() { std::copy(std::begin(GLOBAL(GameState)), std::end(GLOBAL(GameState)), bytes.begin()); }
        ~Restore() { std::copy(bytes.begin(), bytes.end(), std::begin(GLOBAL(GameState))); }
    } restore;
    struct Flag { StoryFlag id; int start, end; };
    const Flag flags[] = {
        {StoryFlag::Artifact2OnShip, ARTIFACT_2_ON_SHIP, END_ARTIFACT_2_ON_SHIP},
        {StoryFlag::Artifact3OnShip, ARTIFACT_3_ON_SHIP, END_ARTIFACT_3_ON_SHIP},
        {StoryFlag::ChmmrEmerging, CHMMR_EMERGING, END_CHMMR_EMERGING},
        {StoryFlag::DiscussedUltron, DISCUSSED_ULTRON, END_DISCUSSED_ULTRON},
        {StoryFlag::PortalSpawnerOnShip, PORTAL_SPAWNER_ON_SHIP, END_PORTAL_SPAWNER_ON_SHIP},
        {StoryFlag::ReadyToConfuseUrquan, READY_TO_CONFUSE_URQUAN, END_READY_TO_CONFUSE_URQUAN},
        {StoryFlag::ShipToCompel, SHIP_TO_COMPEL, END_SHIP_TO_COMPEL},
        {StoryFlag::SupoxUltronHelp, SUPOX_ULTRON_HELP, END_SUPOX_ULTRON_HELP},

        {StoryFlag::BattleSegue, BATTLE_SEGUE, END_BATTLE_SEGUE},
        {StoryFlag::BombCarrier, BOMB_CARRIER, END_BOMB_CARRIER},
        {StoryFlag::ThraddManner, THRADD_MANNER, END_THRADD_MANNER},
        {StoryFlag::TalkingPetOnShip, TALKING_PET_ON_SHIP, END_TALKING_PET_ON_SHIP},
        {StoryFlag::PlanetaryLanding, PLANETARY_LANDING, END_PLANETARY_LANDING},
        {StoryFlag::ImprovedLanderSpeed, IMPROVED_LANDER_SPEED, END_IMPROVED_LANDER_SPEED},
        {StoryFlag::ImprovedLanderShot, IMPROVED_LANDER_SHOT, END_IMPROVED_LANDER_SHOT},
        {StoryFlag::ImprovedLanderCargo, IMPROVED_LANDER_CARGO, END_IMPROVED_LANDER_CARGO},
        {StoryFlag::UsedBroadcaster, USED_BROADCASTER, END_USED_BROADCASTER},
        {StoryFlag::BroadcasterResponse, BROADCASTER_RESPONSE, END_BROADCASTER_RESPONSE},
        {StoryFlag::ArilouSpace, ARILOU_SPACE, END_ARILOU_SPACE},
        {StoryFlag::KohrAhKilledAll, KOHR_AH_KILLED_ALL, END_KOHR_AH_KILLED_ALL},
        {StoryFlag::AquaHelix, AQUA_HELIX, END_AQUA_HELIX},
        {StoryFlag::AquaHelixOnShip, AQUA_HELIX_ON_SHIP, END_AQUA_HELIX_ON_SHIP},
        {StoryFlag::BombUnprotected, BOMB_UNPROTECTED, END_BOMB_UNPROTECTED},
        {StoryFlag::BurvixeseBroadcasters, BURVIXESE_BROADCASTERS, END_BURVIXESE_BROADCASTERS},
        {StoryFlag::BurvBroadcastersOnShip, BURV_BROADCASTERS_ON_SHIP, END_BURV_BROADCASTERS_ON_SHIP},
        {StoryFlag::ClearSpindle, CLEAR_SPINDLE, END_CLEAR_SPINDLE},
        {StoryFlag::ClearSpindleOnShip, CLEAR_SPINDLE_ON_SHIP, END_CLEAR_SPINDLE_ON_SHIP},
        {StoryFlag::DruugeManner, DRUUGE_MANNER, END_DRUUGE_MANNER},
        {StoryFlag::EggCase0OnShip, EGG_CASE0_ON_SHIP, END_EGG_CASE0_ON_SHIP},
        {StoryFlag::EggCase1OnShip, EGG_CASE1_ON_SHIP, END_EGG_CASE1_ON_SHIP},
        {StoryFlag::EggCase2OnShip, EGG_CASE2_ON_SHIP, END_EGG_CASE2_ON_SHIP},
        {StoryFlag::HelixUnprotected, HELIX_UNPROTECTED, END_HELIX_UNPROTECTED},
        {StoryFlag::KnowSyreenVault, KNOW_SYREEN_VAULT, END_KNOW_SYREEN_VAULT},
        {StoryFlag::KohrAhFrenzy, KOHR_AH_FRENZY, END_KOHR_AH_FRENZY},
        {StoryFlag::MaidensOnShip, MAIDENS_ON_SHIP, END_MAIDENS_ON_SHIP},
        {StoryFlag::MetZoqfot, MET_ZOQFOT, END_MET_ZOQFOT},
        {StoryFlag::MoonbaseDestroyed, MOONBASE_DESTROYED, END_MOONBASE_DESTROYED},
        {StoryFlag::MoonbaseOnShip, MOONBASE_ON_SHIP, END_MOONBASE_ON_SHIP},
        {StoryFlag::MyconFellForAmbush, MYCON_FELL_FOR_AMBUSH, END_MYCON_FELL_FOR_AMBUSH},
        {StoryFlag::PlayerHypnotized, PLAYER_HYPNOTIZED, END_PLAYER_HYPNOTIZED},
        {StoryFlag::PortalKey, PORTAL_KEY, END_PORTAL_KEY},
        {StoryFlag::PortalKeyOnShip, PORTAL_KEY_ON_SHIP, END_PORTAL_KEY_ON_SHIP},
        {StoryFlag::ProbeMessageDelivered, PROBE_MESSAGE_DELIVERED, END_PROBE_MESSAGE_DELIVERED},
        {StoryFlag::RosySphere, ROSY_SPHERE, END_ROSY_SPHERE},
        {StoryFlag::RosySphereOnShip, ROSY_SPHERE_ON_SHIP, END_ROSY_SPHERE_ON_SHIP},
        {StoryFlag::ShipVaultUnlocked, SHIP_VAULT_UNLOCKED, END_SHIP_VAULT_UNLOCKED},
        {StoryFlag::ShofixtiBroKia, SHOFIXTI_BRO_KIA, END_SHOFIXTI_BRO_KIA},
        {StoryFlag::ShofixtiKia, SHOFIXTI_KIA, END_SHOFIXTI_KIA},
        {StoryFlag::ShofixtiMaidens, SHOFIXTI_MAIDENS, END_SHOFIXTI_MAIDENS},
        {StoryFlag::ShofixtiRecruited, SHOFIXTI_RECRUITED, END_SHOFIXTI_RECRUITED},
        {StoryFlag::SpathiCreaturesEliminated, SPATHI_CREATURES_ELIMINATED, END_SPATHI_CREATURES_ELIMINATED},
        {StoryFlag::SpathiCreaturesExamined, SPATHI_CREATURES_EXAMINED, END_SPATHI_CREATURES_EXAMINED},
        {StoryFlag::SpathiShieldedSelves, SPATHI_SHIELDED_SELVES, END_SPATHI_SHIELDED_SELVES},
        {StoryFlag::SunDevice, SUN_DEVICE, END_SUN_DEVICE},
        {StoryFlag::SunDeviceOnShip, SUN_DEVICE_ON_SHIP, END_SUN_DEVICE_ON_SHIP},
        {StoryFlag::SunDeviceUnguarded, SUN_DEVICE_UNGUARDED, END_SUN_DEVICE_UNGUARDED},
        {StoryFlag::SyreenShuttleOnShip, SYREEN_SHUTTLE_ON_SHIP, END_SYREEN_SHUTTLE_ON_SHIP},
        {StoryFlag::TaaloProtector, TAALO_PROTECTOR, END_TAALO_PROTECTOR},
        {StoryFlag::TaaloProtectorOnShip, TAALO_PROTECTOR_ON_SHIP, END_TAALO_PROTECTOR_ON_SHIP},
        {StoryFlag::TaaloUnprotected, TAALO_UNPROTECTED, END_TAALO_UNPROTECTED},
        {StoryFlag::TalkingPet, TALKING_PET, END_TALKING_PET},
        {StoryFlag::UmgahBroadcasters, UMGAH_BROADCASTERS, END_UMGAH_BROADCASTERS},
        {StoryFlag::UmgahBroadcastersOnShip, UMGAH_BROADCASTERS_ON_SHIP, END_UMGAH_BROADCASTERS_ON_SHIP},
        {StoryFlag::UmgahZombieBlobbies, UMGAH_ZOMBIE_BLOBBIES, END_UMGAH_ZOMBIE_BLOBBIES},
        {StoryFlag::UrquanMessedUp, URQUAN_MESSED_UP, END_URQUAN_MESSED_UP},
        {StoryFlag::UrquanProtectingSamatra, URQUAN_PROTECTING_SAMATRA, END_URQUAN_PROTECTING_SAMATRA},
        {StoryFlag::UtwigBomb, UTWIG_BOMB, END_UTWIG_BOMB},
        {StoryFlag::UtwigBombOnShip, UTWIG_BOMB_ON_SHIP, END_UTWIG_BOMB_ON_SHIP},
        {StoryFlag::UtwigHaveUltron, UTWIG_HAVE_ULTRON, END_UTWIG_HAVE_ULTRON},
        {StoryFlag::VuxBeast, VUX_BEAST, END_VUX_BEAST},
        {StoryFlag::VuxBeastOnShip, VUX_BEAST_ON_SHIP, END_VUX_BEAST_ON_SHIP},
        {StoryFlag::YehatCivilWar, YEHAT_CIVIL_WAR, END_YEHAT_CIVIL_WAR},
        {StoryFlag::ZexIsDead, ZEX_IS_DEAD, END_ZEX_IS_DEAD},
    };
    for (const auto &flag : flags) {
        story.set(flag.id, true);
        BOOST_TEST(getGameState(GLOBAL(GameState), flag.start, flag.end) == 1u);
        story.set(flag.id, false);
        BOOST_TEST(getGameState(GLOBAL(GameState), flag.start, flag.end) == 0u);
    }
    struct Counter { StoryCounter id; int start, bits; };
    const Counter counters[] = {
        {StoryCounter::ThraddashBodyCount, THRADDASH_BODY_COUNT, 5},
        {StoryCounter::LanderShields, LANDER_SHIELDS, 4},
        {StoryCounter::SlylandroMultiplier, SLYLANDRO_MULTIPLIER, 3},
        {StoryCounter::PortalCounter, PORTAL_COUNTER, 4},
        {StoryCounter::MelnormeAnger, MELNORME_ANGER, 2},
        {StoryCounter::EscapeCounter, ESCAPE_COUNTER, 8},
        {StoryCounter::ArilouSpaceSide, ARILOU_SPACE_SIDE, 2},
        {StoryCounter::ArilouSpaceCounter, ARILOU_SPACE_COUNTER, 4},
        {StoryCounter::BattlePlanet, BATTLE_PLANET, 8},
        {StoryCounter::BombProgress, CHMMR_BOMB_STATE, 2},
        {StoryCounter::ColonyGrpoffs0, COLONY_GRPOFFS0, 32},
        {StoryCounter::DruugeHomeVisits, DRUUGE_HOME_VISITS, 3},
        {StoryCounter::DruugeVisits, DRUUGE_VISITS, 3},
        {StoryCounter::FoundPlutoSpathi, FOUND_PLUTO_SPATHI, 2},
        {StoryCounter::EncounterContext, GLOBAL_FLAGS_AND_DATA, 8},
        {StoryCounter::HelixVisits, HELIX_VISITS, 3},
        {StoryCounter::KnowAboutShattered, KNOW_ABOUT_SHATTERED, 2},
        {StoryCounter::Melnorme0Grpoffs0, MELNORME0_GRPOFFS0, 32},
        {StoryCounter::Melnorme1Grpoffs0, MELNORME1_GRPOFFS0, 32},
        {StoryCounter::Melnorme2Grpoffs0, MELNORME2_GRPOFFS0, 32},
        {StoryCounter::Melnorme3Grpoffs0, MELNORME3_GRPOFFS0, 32},
        {StoryCounter::Melnorme4Grpoffs0, MELNORME4_GRPOFFS0, 32},
        {StoryCounter::Melnorme5Grpoffs0, MELNORME5_GRPOFFS0, 32},
        {StoryCounter::Melnorme6Grpoffs0, MELNORME6_GRPOFFS0, 32},
        {StoryCounter::Melnorme7Grpoffs0, MELNORME7_GRPOFFS0, 32},
        {StoryCounter::Melnorme8Grpoffs0, MELNORME8_GRPOFFS0, 32},
        {StoryCounter::MyconVisits, MYCON_VISITS, 3},
        {StoryCounter::RainbowWorld0, RAINBOW_WORLD0, 8},
        {StoryCounter::RainbowWorld1, RAINBOW_WORLD1, 2},
        {StoryCounter::SamatraGrpoffs0, SAMATRA_GRPOFFS0, 32},
        {StoryCounter::ShofixtiGrpoffs0, SHOFIXTI_GRPOFFS0, 32},
        {StoryCounter::ShofixtiVisits, SHOFIXTI_VISITS, 3},
        {StoryCounter::SyreenHomeVisits, SYREEN_HOME_VISITS, 3},
        {StoryCounter::ThraddMission, THRADD_MISSION, 3},
        {StoryCounter::UltronCondition, ULTRON_CONDITION, 3},
        {StoryCounter::UrquanProbeGrpoffs0, URQUAN_PROBE_GRPOFFS0, 32},
        {StoryCounter::ZoqfotDistress, ZOQFOT_DISTRESS, 2},
        {StoryCounter::ZoqfotGrpoffs0, ZOQFOT_GRPOFFS0, 32},
    };
    for (const auto &counter : counters) {
        const unsigned value = counter.bits == 32 ? UINT32_C(0xfedcba98) : counterMaximum(counter.id);
        const auto before = counter.start ? getGameState(GLOBAL(GameState), counter.start - 1, counter.start - 1) : 0;
        const auto after = counter.start + counter.bits < NUM_GAME_STATE_BITS ? getGameState(GLOBAL(GameState), counter.start + counter.bits, counter.start + counter.bits) : 0;
        story.set(counter.id, value);
        const unsigned encoded = counter.bits == 32 ? getGameState32(GLOBAL(GameState), counter.start)
                : getGameState(GLOBAL(GameState), counter.start, counter.start + counter.bits - 1);
        BOOST_TEST(encoded == value);
        BOOST_TEST(story.read(counter.id) == value);
        if (counter.start) BOOST_TEST(getGameState(GLOBAL(GameState), counter.start - 1, counter.start - 1) == before);
        if (counter.start + counter.bits < NUM_GAME_STATE_BITS) BOOST_TEST(getGameState(GLOBAL(GameState), counter.start + counter.bits, counter.start + counter.bits) == after);
    }
}

BOOST_AUTO_TEST_CASE(location_alternatives_preserve_travel_without_sharing_players)
{
    PlayerEntity first{{PlayerId{101}, "First"}}, second{{PlayerId{102}, "Second"}};
    first.setMapPosition(-12345, 67890);
    std::get<HyperSpaceLocation>(first.location()).flight = {{-321, 654}, 15, 3, 1};
    BOOST_CHECK_THROW(std::get<SolarSystemLocation>(first.location()), std::bad_variant_access);
    first.enterNavigation(true);
    BOOST_TEST(std::get<QuasiSpaceLocation>(first.location()).position.x == -12345);
    auto &system = first.enterSolarSystem();
    system.system = StarId{7};
    BOOST_TEST(system.flight.position.y == 654);
    BOOST_TEST(system.flight.facing == 15u);
    BOOST_TEST(system.flight.planet == 3u);
    BOOST_TEST(&first.enterSolarSystem() == &system); // Re-entry cannot invalidate child borrows.
    BOOST_TEST(std::get<HyperSpaceLocation>(second.location()).position.x == 0);
    BOOST_TEST(std::get<HyperSpaceLocation>(second.location()).flight.planet == 0u);
    first.enterNavigation(false);
    BOOST_TEST(std::get<HyperSpaceLocation>(first.location()).position.y == 67890);
    BOOST_CHECK_THROW(std::get<SolarSystemLocation>(first.location()), std::bad_variant_access);
}

BOOST_AUTO_TEST_CASE(exploration_children_preserve_the_parent_positions)
{
    PlayerEntity player{{PlayerId{103}, "Explorer"}};
    auto &location = player.enterSolarSystem();
    location.flight.position = {12, 34};
    const WorldId moon{SinglePlayerUniverse, StarId{7}, 2, 0};
    BOOST_CHECK_THROW(location.land({1, 2}), std::bad_optional_access);
    BOOST_TEST(!location.surface.has_value());
    location.enterOrbit(moon);
    location.land({45, 67});
    auto &surface = location.surface.value();
    surface.position.x += 10; // The lander borrows this same position.
    BOOST_TEST(location.flight.position.x == 12);
    BOOST_CHECK(location.orbit->world == moon);
    location.leaveSurface();
    BOOST_TEST(!location.surface.has_value());
    BOOST_CHECK(location.orbit->world == moon);
    location.leaveOrbit();
    BOOST_TEST(!location.orbit.has_value());
    BOOST_TEST(location.flight.position.y == 34);
}

BOOST_FIXTURE_TEST_CASE(location_save_projection_restores_markers_without_live_visit_state, LegacyPlayerFixture)
{
    auto &player = gameSession().player();
    auto &system = player.enterSolarSystem();
    system.position = {-12345, 67890};
    system.flight = {{-321, 654}, 15, 3, 2};
    system.enterOrbit({SinglePlayerUniverse, StarId{7}, 2, 0});
    system.land({45, 67});
    ExportPlayerTravelPosition();
    BOOST_TEST(GetPlayerSISState()->log_x == -12345);
    BOOST_TEST(GLOBAL(ip_location.y) == 654);
    BOOST_TEST(GLOBAL(in_orbit) == 2u);
    // Old-format buffers are explicit serialization projections, not live data.
    GLOBAL(ip_location.y) = 777;
    BOOST_TEST(system.flight.position.y == 654);
    ImportPlayerTravelPosition();
    BOOST_TEST(player.travel().position.x == -12345);
    auto &restored = player.enterSolarSystem();
    BOOST_TEST(restored.flight.position.y == 777);
    BOOST_TEST(restored.flight.loadOrbit == 2u);
    BOOST_TEST(!restored.orbit.has_value());
    BOOST_TEST(!restored.surface.has_value());
}
BOOST_AUTO_TEST_CASE(story_data_names_share_typed_storage_and_reject_bad_writes) {
    uqm::GameState state{uqm::UserProfile{{777}, "dialogue"}};
    auto &story = state.player({777}).story();
    story.setNamed("PROBE_MESSAGE_DELIVERED", 1);
    BOOST_TEST(story.read(uqm::StoryFlag::ProbeMessageDelivered));
    story.setNamed("SLYLANDRO_PROBE_VISITS", 7);
    BOOST_TEST(story.readNamed("SLYLANDRO_PROBE_VISITS") == 7u);
    BOOST_CHECK_THROW(story.setNamed("SLYLANDRO_PROBE_VISITS", 8), std::out_of_range);
    BOOST_TEST(story.readNamed("SLYLANDRO_PROBE_VISITS") == 7u);
    BOOST_CHECK_THROW(story.readNamed("UNKNOWN_STORY_FLAG"), std::invalid_argument);
    BOOST_CHECK_THROW(story.setNamed("UNKNOWN_STORY_FLAG", 1), std::invalid_argument);
}
