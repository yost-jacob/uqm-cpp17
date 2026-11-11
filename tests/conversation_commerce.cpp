#include "uqm/conversation_commerce.hpp"
#include "uqm/conversation_state.hpp"
#include "uqm/dialogue/engine.hpp"
#include "uqm/fleet_legacy.hpp"
#include "uqm/conversation_reports.hpp"
#include "uqm/game_state.hpp"
#include "uqm/inventory.hpp"
#include "uqm/sis_legacy.hpp"
#include "uqm/build.h"
#include "uqm/commglue.h"
#include <boost/test/unit_test.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>
namespace {
using namespace uqm;
using dialogue::Json;
Json definition(const std::string &name) {
    std::ifstream input(std::filesystem::path(UQM_TEST_SHIPS_PATH).parent_path() / "conversations" / ("comm." + name + ".dialogue.json"));
    return Json::parse(input);
}
struct CommerceFixture {
    std::shared_ptr<GameState> state = std::make_shared<GameState>(UserProfile{SinglePlayerId, "Trader"});
    GameSession session{state, SinglePlayerId, 10};
    SessionActivation activation{session};
    SimulationEngine engine{150, 10};
    SimulationBinding binding{engine};
    PlayerEntity &player = session.player();
    SIS_STATE &sis = detail::SISAccess::record(player.sis());
    Json melnorme = definition("melnorme"), druuge = definition("druuge"), starbase = definition("starbase");
    CommerceFixture() {
        std::fill(std::begin(sis.ModuleSlots), std::end(sis.ModuleSlots), EMPTY_SLOT + 2);
        std::fill(std::begin(sis.DriveSlots), std::end(sis.DriveSlots), EMPTY_SLOT);
        std::fill(std::begin(sis.JetSlots), std::end(sis.JetSlots), EMPTY_SLOT + 1);
        sis.ModuleSlots[0] = CREW_POD; sis.ModuleSlots[1] = FUEL_TANK; sis.ModuleSlots[2] = STORAGE_BAY;
        sis.CrewEnlisted = 150;
        InitQueue(&GLOBAL(avail_race_q), NUM_AVAILABLE_RACES, QUEUE_FLEET_INFO);
        for (unsigned i = 0; i < NUM_AVAILABLE_RACES; ++i) {
            auto link = AllocLink(&GLOBAL(avail_race_q));
            auto &info = *LockFleetInfo(&GLOBAL(avail_race_q), link);
            info = {}; info.SpeciesID = DRUUGE_ID; info.crew_level = info.max_crew = 14;
            PutQueue(&GLOBAL(avail_race_q), link);
        }
    }
    ~CommerceFixture() { UninitQueue(&GLOBAL(avail_race_q)); }
    void credits(unsigned n) { player.story().setNamed("MELNORME_CREDIT0", n & 255); player.story().setNamed("MELNORME_CREDIT1", n >> 8); }
    ConversationCommerce merchant(const Json &graph) { return {player, session.clock(), graph.at("markets")}; }
};
}
// Only catalogue queries are adapted for the isolated test executable; the real
// fleet pool, factory, transaction, report and rescue implementations are linked.
extern "C" HLINK GetStarShipFromIndex(QUEUE *queue, COUNT index) {
    auto link = GetHeadLink(queue);
    while (link && index--) link = _GetSuccLink(link);
    return link;
}
extern "C" COUNT CheckAlliance(COUNT race) {
    auto handle = GetStarShipFromIndex(&GLOBAL(avail_race_q), race);
    return handle ? LockFleetInfo(&GLOBAL(avail_race_q), handle)->allied_state : 0;
}
extern "C" COUNT EscortFeasibilityStudy(COUNT race) {
    return GetStarShipFromIndex(&GLOBAL(avail_race_q), race) ? MAX_BUILT_SHIPS - uqm::gameSession().player().fleet().size() : 0;
}
extern "C" COUNT CalculateEscortsWorth(void) {
    COUNT worth = 0;
    const auto &fleet = uqm::gameSession().player().fleet();
    for (auto id : fleet.ships()) worth += uqm::shipPrice(fleet.find(id)->get().race());
    return worth;
}
BOOST_FIXTURE_TEST_CASE(merchant_fuel_keeps_fractional_units_and_charges_ceiling_for_fill, CommerceFixture) {
    auto shop = merchant(melnorme); credits(100);
    sis.FuelOnBoard = player.inventory().capacity("fuel_hundredths") - 51;
    BOOST_TEST(shop.purchase("melnorme", "fuel", "fill").code == "ok");
    BOOST_TEST(player.inventory().quantity("credits") == 99);
    BOOST_TEST(sis.FuelOnBoard == player.inventory().capacity("fuel_hundredths"));
    sis.FuelOnBoard -= 51;
    BOOST_TEST(shop.purchase("melnorme", "fuel", 5).code == "no_room");
    BOOST_TEST(player.inventory().quantity("credits") == 99);
    credits(0);
    BOOST_TEST(shop.purchase("melnorme", "fuel", 1).code == "no_credit");
    BOOST_TEST(sis.FuelOnBoard == player.inventory().capacity("fuel_hundredths") - 51);
}
BOOST_FIXTURE_TEST_CASE(merchant_technology_and_information_validate_payment_and_apply_once, CommerceFixture) {
    auto shop = merchant(melnorme); credits(149);
    BOOST_TEST(shop.purchase("melnorme", "next_technology", 1).code == "no_credit");
    BOOST_TEST(player.gameData().prices.modules[BLASTER_WEAPON] == 0u);
    credits(1000);
    BOOST_TEST(shop.purchase("melnorme", "next_technology", 1).code == "ok");
    BOOST_TEST(player.gameData().prices.modules[BLASTER_WEAPON] == 80u);
    BOOST_TEST(player.inventory().quantity("credits") == 850);
    player.story().setNamed("MELNORME_ALIEN_INFO_STACK", 13);
    BOOST_TEST(shop.purchase("melnorme", "alien_races", 1).code == "ok");
    BOOST_TEST(player.story().readNamed("KNOW_SPATHI_PASSWORD") == 1u);
    BOOST_TEST(player.story().readNamed("SPATHI_HOME_VISITS") == 7u);
    BOOST_TEST(shop.purchase("melnorme", "alien_races", 1).code == "ok");
    BOOST_TEST(player.story().readNamed("KNOW_SYREEN_WORLD_SHATTERED") == 1u);
    BOOST_TEST(player.story().readNamed("MELNORME_ALIEN_INFO_STACK") == 15u);
}
BOOST_FIXTURE_TEST_CASE(merchant_sales_do_not_overflow_accounts_or_pay_for_discoveries_twice, CommerceFixture) {
    auto shop = merchant(melnorme); credits(65535); sis.TotalBioMass = 5;
    BOOST_TEST(shop.sell("melnorme", "biomass", "all").code == "no_room");
    BOOST_TEST(sis.TotalBioMass == 5u);
    credits(0);
    BOOST_TEST(shop.sell("melnorme", "biomass", "all").code == "ok");
    BOOST_TEST(sis.TotalBioMass == 0u);
    player.story().setNamed("RAINBOW_WORLD0", 3);
    BOOST_TEST(shop.unreportedRainbows() == 2);
    BOOST_TEST(shop.sell("melnorme", "rainbows", "all").code == "ok");
    BOOST_TEST(player.inventory().quantity("credits") == 1010);
    BOOST_TEST(shop.sell("melnorme", "rainbows", "all").code == "unavailable");
}
BOOST_FIXTURE_TEST_CASE(druuge_reserves_vessels_before_charging_and_batches_slavery_accounting, CommerceFixture) {
    auto shop = merchant(druuge);
    BOOST_TEST(shop.purchase("druuge", "ship", 1).code == "ok");
    BOOST_TEST(player.fleet().size() == 1u); BOOST_TEST(sis.CrewEnlisted == 50u);
    BOOST_TEST(shop.purchase("druuge", "ship", 1).code == "no_credit");
    BOOST_TEST(player.fleet().size() == 1u);
    shop.report("druuge.slavery", Json::object());
    BOOST_TEST(player.story().readNamed("CREW_SOLD_TO_DRUUGE0") == 100u);
    BOOST_TEST(player.gameData().prices.crew == 3u);
    shop.report("druuge.slavery", Json::object());
    BOOST_TEST(player.story().readNamed("CREW_SOLD_TO_DRUUGE0") == 100u);
}
BOOST_FIXTURE_TEST_CASE(druuge_full_or_unavailable_fleet_does_not_consume_payment_or_randomness, CommerceFixture) {
    auto shop = merchant(druuge);
    const auto definition = *LockFleetInfo(&GLOBAL(avail_race_q), GetStarShipFromIndex(&GLOBAL(avail_race_q), DRUUGE_SHIP));
    for (unsigned i = 0; i < MAX_BUILT_SHIPS; ++i) BOOST_REQUIRE(player.fleet().create(DRUUGE_SHIP, definition, engine.random()));
    const auto seed = engine.random().state();
    BOOST_TEST(shop.purchase("druuge", "ship", 1).code == "no_room");
    BOOST_TEST(sis.CrewEnlisted == 150u); BOOST_TEST(engine.random().state() == seed);
}
BOOST_FIXTURE_TEST_CASE(druuge_item_trade_fills_fractional_tanks_and_sphere_is_a_distinct_exchange, CommerceFixture) {
    auto shop = merchant(druuge);
    player.story().setNamed("BURV_BROADCASTERS_ON_SHIP", 1);
    sis.FuelOnBoard = player.inventory().capacity("fuel_hundredths") - 51;
    BOOST_TEST(shop.sell("druuge", "caster", "regular").code == "ok");
    BOOST_TEST(player.story().readNamed("BURV_BROADCASTERS_ON_SHIP") == 0u);
    BOOST_TEST(sis.FuelOnBoard == player.inventory().capacity("fuel_hundredths"));
    player.story().setNamed("MAIDENS_ON_SHIP", 1);
    BOOST_TEST(shop.sell("druuge", "maidens", "sphere").code == "ok");
    BOOST_TEST(player.story().readNamed("ROSY_SPHERE") == 1u);
    BOOST_TEST(player.fleet().size() == 0u);
}
BOOST_FIXTURE_TEST_CASE(rescue_preparation_is_stable_and_acceptance_rejects_stale_or_reused_offers, CommerceFixture) {
    auto shop = merchant(melnorme);
    sis.DriveSlots[0] = FUSION_THRUSTER; sis.NumLanders = 1; sis.CrewEnlisted = 20;
    const auto original = sis; const auto seed = engine.random().state();
    const auto first = shop.prepareRescue("melnorme");
    BOOST_TEST(first.code == "offer");
    BOOST_CHECK(shop.prepareRescue("melnorme").values == first.values);
    BOOST_TEST(engine.random().state() == seed); BOOST_TEST(sis.FuelOnBoard == original.FuelOnBoard);
    ++sis.CrewEnlisted;
    BOOST_TEST(shop.acceptRescue("melnorme.rescue").code == "stale");
    --sis.CrewEnlisted;
    BOOST_TEST(shop.acceptRescue("melnorme.rescue").code == "ok");
    BOOST_TEST(sis.FuelOnBoard > 0u);
    BOOST_TEST(shop.acceptRescue("melnorme.rescue").code == "unavailable");
}
BOOST_FIXTURE_TEST_CASE(starbase_reports_do_not_acknowledge_devices_until_explicit_playback_commit, CommerceFixture) {
    StarbaseReports reports{player, session.clock(), starbase.at("phrases"), true};
    player.story().setNamed("SUN_DEVICE_ON_SHIP", 1);
    BOOST_TEST(reports.available("hayes.devices"));
    BOOST_TEST(player.story().readNamed("DISCUSSED_SUN_EFFICIENCY") == 0u);
    const auto plan = reports.prepare("hayes.devices", Json::object());
    BOOST_TEST(player.story().readNamed("DISCUSSED_SUN_EFFICIENCY") == 0u);
    BOOST_TEST(reports.prepare("hayes.commit", plan.values.at("commit")).code == "ok");
    BOOST_TEST(player.story().readNamed("DISCUSSED_SUN_EFFICIENCY") == 1u);
    BOOST_TEST(!reports.available("hayes.devices"));
    BOOST_TEST(reports.prepare("hayes.commit", plan.values.at("commit")).code == "ok");
}
BOOST_FIXTURE_TEST_CASE(starbase_ready_report_commits_once_and_report_replay_never_recruits_again, CommerceFixture) {
    StarbaseReports reports{player, session.clock(), starbase.at("phrases"), false};
    player.story().setNamed("MOONBASE_ON_SHIP", 1);
    const auto ready = reports.prepare("hayes.arrival", Json::object());
    BOOST_TEST(sis.ResUnits == 0u);
    reports.prepare("hayes.commit", ready.values.at("commit"));
    BOOST_TEST(sis.ResUnits == 2500u);
    reports.prepare("hayes.commit", ready.values.at("commit"));
    BOOST_TEST(sis.ResUnits == 2500u);
    player.story().setNamed("MOONBASE_ON_SHIP", 0); player.story().setNamed("PORTAL_SPAWNER", 1);
    StarbaseReports returning{player, session.clock(), starbase.at("phrases"), true};
    const auto bulletins = returning.prepare("hayes.arrival", Json::object());
    BOOST_TEST(player.fleet().size() == 0u);
    returning.prepare("hayes.commit", bulletins.values.at("commit"));
    BOOST_TEST(player.fleet().size() == 3u);
    returning.prepare("hayes.repeat_bulletins", Json::object());
    returning.prepare("hayes.commit", bulletins.values.at("commit"));
    BOOST_TEST(player.fleet().size() == 3u);
}
BOOST_FIXTURE_TEST_CASE(starbase_mineral_sale_rejects_account_overflow_without_taking_cargo, CommerceFixture) {
    StarbaseReports reports{player, session.clock(), starbase.at("phrases"), true};
    sis.ElementAmounts[COMMON] = sis.TotalElementMass = 20; sis.ResUnits = std::numeric_limits<DWORD>::max();
    BOOST_TEST(reports.sellMinerals().code == "no_room"); BOOST_TEST(sis.TotalElementMass == 20u);
    sis.ResUnits = 0;
    BOOST_TEST(reports.sellMinerals().code == "ok"); BOOST_TEST(sis.TotalElementMass == 0u); BOOST_TEST(sis.ResUnits == 20u);
}

namespace {
Json context(const Json &graph) {
    return {{"markets", graph.value("markets", Json::object())}, {"phrases", graph.at("phrases")},
        {"using_speech", true}, {"won_last_battle", false},
        {"player", {{"captain_name", "Trader"}, {"ship_name", "Vindicator"}, {"alliance_name", "Alliance"}}}};
}
void settle(dialogue::Conversation &graph) {
    for (unsigned i = 0; i < 256 && !graph.ended(); ++i) {
        const auto entry = graph.view().entry;
        graph.finishDialogue(entry);
        if (!graph.ended() && graph.view().entry == entry) return;
    }
    if (!graph.ended()) throw std::logic_error("dialogue failed to reach a response menu");
}
void choose(dialogue::Conversation &graph, const std::string &response) {
    graph.choose(graph.view().entry, response); settle(graph);
}
CampaignDialogueState::Report noOtherReports = [](const std::string &, const Json &) -> dialogue::Result {
    throw std::logic_error("unexpected external report");
};
}
BOOST_FIXTURE_TEST_CASE(live_druuge_graph_uses_native_state_for_crew_purchase_and_departure, CommerceFixture) {
    player.story().setNamed("GLOBAL_FLAGS_AND_DATA", 128);
    CampaignDialogueState adapter{player, session.clock(), noOtherReports, context(druuge)};
    auto graph = dialogue::Conversation::load(druuge, adapter);
    graph->advance(); settle(*graph);
    choose(*graph, "want_to_buy"); choose(*graph, "im_ready_to_buy"); choose(*graph, "buy_art_1");
    BOOST_TEST(sis.CrewEnlisted == 50u); BOOST_TEST(player.story().readNamed("ARTIFACT_2_ON_SHIP") == 1u);
    choose(*graph, "done_buying"); choose(*graph, "bye");
    BOOST_TEST(graph->ended()); BOOST_TEST(player.story().readNamed("CREW_SOLD_TO_DRUUGE0") == 100u);
}
BOOST_FIXTURE_TEST_CASE(live_melnorme_graph_preserves_topic_priority_and_exhausted_account_routing, CommerceFixture) {
    CampaignDialogueState adapter{player, session.clock(), noOtherReports, context(melnorme)};
    auto graph = dialogue::Conversation::load(melnorme, adapter);
    graph->advance(); settle(*graph);
    choose(*graph, "what_about_universe");
    BOOST_TEST(graph->responses().front().id == "giving_is_good_1");
    choose(*graph, "what_about_yourselves");
    BOOST_TEST(graph->responses().front().id == "what_factors");
    credits(1);
    choose(*graph, "get_on_with_business"); choose(*graph, "buy"); choose(*graph, "buy_fuel");
    choose(*graph, "buy_1_fuel");
    BOOST_TEST(player.inventory().quantity("credits") == 0);
    choose(*graph, "done_buying_fuel");
    const auto responses = graph->responses();
    BOOST_CHECK(std::none_of(responses.begin(), responses.end(), [](const auto &r) { return r.id == "buy"; }));
    choose(*graph, "goodbye"); BOOST_TEST(graph->ended());
}
BOOST_FIXTURE_TEST_CASE(live_starbase_graph_commits_report_after_playback_and_retains_all_information_menus, CommerceFixture) {
    player.story().setNamed("SUN_DEVICE_ON_SHIP", 1);
    CampaignDialogueState adapter{player, session.clock(), noOtherReports, context(starbase)};
    auto graph = dialogue::Conversation::load(starbase, adapter);
    graph->advance(); settle(*graph);
    graph->choose(graph->view().entry, "new_devices");
    BOOST_TEST(player.story().readNamed("DISCUSSED_SUN_EFFICIENCY") == 0u);
    settle(*graph);
    BOOST_TEST(player.story().readNamed("DISCUSSED_SUN_EFFICIENCY") == 1u);
    choose(*graph,"need_info"); choose(*graph,"starbase_functions");
    BOOST_TEST(graph->responses().front().id == "tell_me_about_modules0");
    choose(*graph,"tell_me_about_fuel0"); choose(*graph,"tell_me_about_ships"); choose(*graph,"tell_me_about_crew");
    choose(*graph,"enough_starbase"); choose(*graph,"history"); choose(*graph,"alien_races");
    choose(*graph,"what_about_alliance"); choose(*graph,"shofixti"); choose(*graph,"enough_alliance");
    choose(*graph,"enough_aliens"); choose(*graph,"enough_history"); choose(*graph,"our_mission");
    choose(*graph,"what_do_now"); choose(*graph,"what_about_urquan"); choose(*graph,"how_defeat"); choose(*graph,"how_find_urquan");
    choose(*graph,"enough_defeat"); choose(*graph,"enough_mission"); choose(*graph,"no_need_info"); choose(*graph,"goodbye_commander");
    BOOST_TEST(graph->ended());
}
// Unused legacy action paths fail loudly in this integration fixture. The three
// migrated graphs use the real native commerce/report implementation above.
extern "C" void DeltaSISGauges(SIZE crew, SIZE fuel, int ru) {
    if (crew != UNDEFINED_DELTA || fuel != UNDEFINED_DELTA || ru != UNDEFINED_DELTA)
        throw std::logic_error("unexpected legacy gauge mutation in graph test");
}
extern "C" void DrawLanders(void) {}
extern "C" void setSegue(Segue value) {
    if (value != Segue_peace && value != Segue_hostile) throw std::logic_error("unexpected test segue");
    uqm::gameSession().player().story().setNamed("BATTLE_SEGUE", value == Segue_hostile);
}
extern "C" COUNT CountEscortShips(COUNT race) {
    const auto &fleet = uqm::gameSession().player().fleet(); unsigned count = 0;
    for (auto id : fleet.ships()) if (fleet.find(id)->get().race() == race) ++count;
    return count;
}
extern "C" COUNT AddEscortShips(COUNT, SIZE) { throw std::logic_error("unexpected legacy recruitment"); }
extern "C" int SetEscortCrewComplement(COUNT, COUNT, BYTE) { throw std::logic_error("unexpected legacy crew patch"); }
extern "C" void RemoveEscortShips(COUNT) { throw std::logic_error("unexpected legacy removal"); }
extern "C" COUNT RemoveSomeEscortShips(COUNT, COUNT) { throw std::logic_error("unexpected legacy removal"); }
extern "C" COUNT SetRaceAllied(COUNT, BOOLEAN) { throw std::logic_error("unexpected legacy alliance write"); }
extern "C" void ApplyChmmrBombRefit(int) { throw std::logic_error("unexpected refit"); }
BOOST_FIXTURE_TEST_CASE(druuge_allocation_failure_does_not_charge_or_consume_random_defaults, CommerceFixture) {
    auto shop = merchant(druuge);
    auto &pool = detail::FleetAccess::queue(player.fleet());
    std::vector<HLINK> reservations;
    for (unsigned i = 0; i < MAX_BUILT_SHIPS; ++i) reservations.push_back(AllocLink(&pool));
    const auto seed = engine.random().state();
    BOOST_TEST(shop.purchase("druuge", "ship", 1).code == "no_room");
    BOOST_TEST(sis.CrewEnlisted == 150u); BOOST_TEST(player.fleet().size() == 0u); BOOST_TEST(engine.random().state() == seed);
    for (auto slot : reservations) FreeShipFrag(&pool, slot);
}
BOOST_FIXTURE_TEST_CASE(rescue_charity_and_penance_preserve_required_crew_capacity, CommerceFixture) {
    sis.CrewEnlisted = 20; sis.NumLanders = 0;
    std::fill(std::begin(sis.ModuleSlots), std::end(sis.ModuleSlots), EMPTY_SLOT + 2);
    sis.ModuleSlots[0] = CREW_POD;
    auto shop = merchant(melnorme);
    BOOST_TEST(shop.prepareRescue("melnorme").code == "free");
    const auto fuel = sis.FuelOnBoard;
    BOOST_TEST(fuel > 0u);
    BOOST_TEST(shop.prepareRescue("melnorme").code == "unavailable");
    BOOST_TEST(sis.FuelOnBoard == fuel);
    sis.ModuleSlots[1] = BLASTER_WEAPON; sis.TotalBioMass = 10;
    player.gameData().prices.modules[BLASTER_WEAPON] = 80;
    BOOST_TEST(shop.stripShip("melnorme.penance").code == "ok");
    BOOST_TEST(sis.ModuleSlots[0] == CREW_POD); BOOST_TEST(sis.ModuleSlots[1] == EMPTY_SLOT + 2);
    BOOST_TEST(sis.CrewEnlisted == 20u); BOOST_TEST(sis.TotalBioMass == 0u);
}
BOOST_FIXTURE_TEST_CASE(starbase_timed_bulletins_use_strict_boundaries_and_pending_reports_expire, CommerceFixture) {
    player.story().setNamed("STARBASE_VISITED", 1);
    player.story().setNamed("STARBASE_MONTH", 2); player.story().setNamed("STARBASE_DAY", 17);
    session.clock().advance(7);
    StarbaseReports sameDay{player, session.clock(), starbase.at("phrases"), true};
    const auto atBoundary = sameDay.prepare("hayes.arrival", Json::object());
    const auto contains = [](const Json &speech, const std::string &id) {
        return std::find(speech.begin(), speech.end(), Json(id)) != speech.end();
    };
    BOOST_TEST(!contains(atBoundary.values.at("speech"), "STARBASE_BULLETIN_10"));
    session.clock().advance(1);
    StarbaseReports after{player, session.clock(), starbase.at("phrases"), true};
    const auto report = after.prepare("hayes.arrival", Json::object());
    BOOST_TEST(contains(report.values.at("speech"), "STARBASE_BULLETIN_10"));
    after.prepare("hayes.devices", Json::object());
    BOOST_TEST(after.prepare("hayes.commit", report.values.at("commit")).code == "stale");
    BOOST_TEST(player.story().readNamed("STARBASE_BULLETS1") == 0u);
}
BOOST_FIXTURE_TEST_CASE(invalid_merchant_definitions_fail_before_any_gameplay_mutation, CommerceFixture) {
    auto bad = melnorme.at("markets");
    bad["melnorme"]["technology"][0]["grant"]["module"] = "UNKNOWN_MODULE";
    BOOST_CHECK_THROW((ConversationCommerce{player, session.clock(), bad}), std::invalid_argument);
    bad = melnorme.at("markets"); bad["melnorme"]["fuel"]["unit_price"] = -1;
    BOOST_CHECK_THROW((ConversationCommerce{player, session.clock(), bad}), std::invalid_argument);
    BOOST_TEST(sis.CrewEnlisted == 150u); BOOST_TEST(player.gameData().prices.modules[BLASTER_WEAPON] == 0u);
}
BOOST_FIXTURE_TEST_CASE(live_druuge_bomb_space_and_salvage_entries_keep_distinct_outcomes, CommerceFixture) {
    player.story().setNamed("GLOBAL_FLAGS_AND_DATA", 64);
    CampaignDialogueState adapter{player, session.clock(), noOtherReports, context(druuge)};
    auto bomb = dialogue::Conversation::load(druuge, adapter);
    bomb->advance(); settle(*bomb);
    choose(*bomb,"whats_up_at_bomb_planet"); choose(*bomb,"we_get_bomb"); choose(*bomb,"then_we_take_bomb");
    BOOST_TEST(bomb->ended()); BOOST_TEST(player.story().readNamed("BATTLE_SEGUE") == 1u);
    player.story().setNamed("GLOBAL_FLAGS_AND_DATA", 0);
    auto salvage = dialogue::Conversation::load(druuge, adapter);
    salvage->advance(); settle(*salvage);
    BOOST_TEST(salvage->ended()); BOOST_CHECK(salvage->memory().local.at("attempted_salvage") == true);
    sis.ModuleSlots[3] = GUN_WEAPON;
    auto space = dialogue::Conversation::load(druuge, adapter);
    space->advance(); settle(*space);
    choose(*space,"whats_up_in_space"); choose(*space,"bye");
    BOOST_TEST(space->ended()); BOOST_TEST(player.story().readNamed("BATTLE_SEGUE") == 0u);
}
BOOST_FIXTURE_TEST_CASE(live_melnorme_rescue_and_hostility_branches_use_bounded_native_operations, CommerceFixture) {
    player.story().setNamed("MET_MELNORME", 1); player.story().setNamed("MELNORME_YACK_STACK2", 6);
    sis.DriveSlots[0] = FUSION_THRUSTER;
    CampaignDialogueState adapter{player, session.clock(), noOtherReports, context(melnorme)};
    auto rescue = dialogue::Conversation::load(melnorme, adapter);
    rescue->advance(); settle(*rescue);
    choose(*rescue,"yes_help"); choose(*rescue,"take_it");
    BOOST_TEST(rescue->ended()); BOOST_TEST(sis.FuelOnBoard > 0u);
    player.story().setNamed("MELNORME_ANGER", 1);
    auto angry = dialogue::Conversation::load(melnorme, adapter);
    angry->advance(); settle(*angry);
    choose(*angry,"explore_relationship"); choose(*angry,"so_we_can_attack");
    BOOST_TEST(angry->ended()); BOOST_TEST(player.story().readNamed("MELNORME_ANGER") == 2u);
    BOOST_TEST(player.story().readNamed("BATTLE_SEGUE") == 1u);
}
BOOST_FIXTURE_TEST_CASE(device_reports_keep_device_order_and_restore_the_commander_after_the_scientist, CommerceFixture) {
    player.story().setNamed("PORTAL_SPAWNER_ON_SHIP", 1);
    player.story().setNamed("ARTIFACT_2_ON_SHIP", 1);
    player.story().setNamed("VUX_BEAST_ON_SHIP", 1);
    player.story().setNamed("DESTRUCT_CODE_ON_SHIP", 1);
    StarbaseReports reports{player, session.clock(), starbase.at("phrases"), true};
    const auto plan = reports.prepare("hayes.devices", Json::object());
    const auto &speech = plan.values.at("speech");
    BOOST_CHECK(speech.at(1) == "ABOUT_PORTAL");
    auto scientist = std::find_if(speech.begin(), speech.end(), [](const Json &line) {
        return line.is_object() && line.value("on_start", "") == "starbase.scientist";
    });
    BOOST_REQUIRE(scientist != speech.end());
    BOOST_CHECK((scientist + 1)->at("on_start") == "starbase.commander");
    BOOST_CHECK(speech.at(speech.size() - 2) == "ABOUT_ARTIFACT_2");
}
