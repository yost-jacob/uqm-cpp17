#include "uqm/dialogue/legacy_turn.hpp"
#include "uqm/dialogue/text.hpp"
#include <boost/test/unit_test.hpp>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
namespace dd = uqm::dialogue;
namespace {
dd::Json read(const std::filesystem::path &path) {
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("missing dialogue fixture: " + path.string());
    return dd::Json::parse(input);
}
dd::Result offered(std::size_t count) {
    dd::Json values = {{"has_responses", count != 0}};
    for (std::size_t slot = 0; slot < 8; ++slot) {
        values["slot_" + std::to_string(slot)] = slot < count;
        if (slot < count)
            values["text_" + std::to_string(slot)] = "constructed answer " + std::to_string(slot);
    }
    return {"ok", std::move(values)};
}
} // namespace
BOOST_AUTO_TEST_CASE(all_live_graphs_preserve_turn_order_completion_and_all_eight_response_slots) {
    unsigned files = 0;
    for (const auto &file : std::filesystem::directory_iterator(LIVE_DIALOGUE_ROOT)) {
        if (file.path().extension() != ".json")
            continue;
        const auto data = read(file.path());
        dd::validateLiveDefinition(data, data.at("resource"), data.at("phrases").size());
        ++files;
        if (data.at("_migration").at("status") != "native_handlers") {
            struct DefinitionState final : dd::State {
                dd::Integer storyMaximum(const std::string &name) const override {
                    if (name != "LANDERS_LOST") throw std::invalid_argument("unexpected story field");
                    return 255;
                }
                void validateInventoryKey(const std::string &key) const override {
                    if (key != "landers") throw std::invalid_argument("unexpected inventory key");
                }
            } validationState;
            const auto graph = dd::Conversation::load(data, validationState);
            BOOST_TEST(graph->nodeCount() == data.at("node_count").get<std::size_t>());
            continue;
        }
        const auto id = data.at("id").get<std::string>();
        for (std::size_t selected = 0; selected < 8; ++selected) {
            unsigned introCalls = 0, responseCalls = 0;
            dd::LegacyTurnState state{id, [&](std::optional<std::size_t> selection) {
                                          if (!selection) {
                                              ++introCalls;
                                              return offered(8);
                                          }
                                          BOOST_TEST(*selection == selected);
                                          ++responseCalls;
                                          return offered(0);
                                      }};
            auto graph = dd::Conversation::load(data, state);
            BOOST_TEST(introCalls == 0u);
            graph->advance();
            BOOST_TEST(introCalls == 1u);
            auto entry = graph->view().entry;
            BOOST_CHECK_THROW(graph->responses(), std::logic_error);
            graph->finishDialogue(entry);
            const auto menu = graph->responses();
            BOOST_TEST(menu.size() == 8u);
            for (std::size_t slot = 0; slot < menu.size(); ++slot) {
                BOOST_TEST(menu[slot].id == "slot_" + std::to_string(slot));
                BOOST_TEST(menu[slot].text.get<std::string>() ==
                           "constructed answer " + std::to_string(slot));
            }
            graph->choose(entry, "slot_" + std::to_string(selected));
            BOOST_TEST(responseCalls == 1u);
            BOOST_TEST(!graph->ended()); // Final queued speech completes before exit.
            BOOST_CHECK_THROW(graph->choose(entry, "slot_0"), std::invalid_argument);
            graph->finishDialogue(graph->view().entry);
            BOOST_TEST(graph->ended());
            BOOST_TEST(responseCalls == 1u);
        }
    }
    BOOST_TEST(files == 27u);
}
BOOST_AUTO_TEST_CASE(native_handlers_fail_closed_on_unknown_script_bad_slots_and_partial_commit) {
    unsigned calls = 0;
    dd::LegacyTurnState state{"arilou", [&](std::optional<std::size_t>) {
                                  ++calls;
                                  return offered(0);
                              }};
    BOOST_CHECK_THROW(state.legacyTurn("unknown", nullptr), std::invalid_argument);
    BOOST_CHECK_THROW(state.legacyTurn("arilou", 0), std::invalid_argument);
    state.legacyTurn("arilou", nullptr);
    BOOST_CHECK_THROW(state.legacyTurn("arilou", nullptr), std::logic_error);
    BOOST_CHECK_THROW(state.legacyTurn("arilou", -1), std::invalid_argument);
    BOOST_CHECK_THROW(state.legacyTurn("arilou", 8), std::invalid_argument);
    BOOST_CHECK_THROW(state.legacyTurn("arilou", 1.0), std::invalid_argument);
    BOOST_TEST(calls == 1u);
    const dd::Json data = {
        {"version", 1},
        {"id", "failure"},
        {"entry", "intro"},
        {"node_count", 3},
        {"nodes",
         dd::Json::array(
             {{{"id", "intro"},
               {"kind", "action"},
               {"action", {{"verb", "handlers.legacy_turn"}, {"args", {"failure", nullptr}}}},
               {"save_as", "turn"},
               {"next", "farewell"}},
              {{"id", "farewell"},
               {"kind", "dialogue"},
               {"variants", dd::Json::array({{{"speech", dd::Json::array()}}})},
               {"next", "end"}},
              {{"id", "end"}, {"kind", "end"}, {"outcome", "native_segue"}}})}};
    dd::LegacyTurnState failure{"failure", [&](std::optional<std::size_t>) -> dd::Result {
                                    ++calls;
                                    throw std::runtime_error("failure after story update");
                                }};
    auto graph = dd::Conversation::load(data, failure);
    BOOST_CHECK_THROW(graph->advance(), std::runtime_error);
    BOOST_CHECK_THROW(graph->advance(), std::logic_error);
    BOOST_TEST(calls == 2u);
}
BOOST_AUTO_TEST_CASE(live_phrase_metadata_rejects_bad_indices_duplicate_names_and_embedded_nuls) {
    const auto original =
        read(std::filesystem::path(LIVE_DIALOGUE_ROOT) / "comm.arilou.dialogue.json");
    const auto count = original.at("phrases").size();
    BOOST_CHECK_THROW(dd::validateLiveDefinition(original, "wrong.resource", count),
                      std::invalid_argument);
    auto bad = original;
    bad["phrases"][0]["index"] = 1;
    BOOST_CHECK_THROW(dd::validateLiveDefinition(bad, original.at("resource"), count),
                      std::invalid_argument);
    bad = original;
    bad["phrases"][1]["id"] = bad["phrases"][0]["id"];
    BOOST_CHECK_THROW(dd::validateLiveDefinition(bad, original.at("resource"), count),
                      std::invalid_argument);
    bad = original;
    bad["phrases"][0]["text"] = std::string("a\0b", 3);
    BOOST_CHECK_THROW(dd::validateLiveDefinition(bad, original.at("resource"), count),
                      std::invalid_argument);
    for (const auto &phrase : original.at("phrases")) {
        const auto text = phrase.at("text").get<std::string>();
        BOOST_TEST(dd::textFingerprint(text) ==
                   phrase.at("original_text_hash").get<std::uint64_t>());
        BOOST_CHECK(dd::textFingerprint(text + " poop") !=
                    phrase.at("original_text_hash").get<std::uint64_t>());
    }
}
namespace {
struct ProbeState : dd::State {
    std::map<std::string, dd::Integer> fields;
    std::string outcome;
    unsigned coordinates = 0, fades = 0, calendarEvents = 0, randomDraws = 0;
    dd::Integer biomass = 0, escorts = 0, slots = 12;
    dd::Integer shipFuel = 5, landers = 1, crew = 18, radioactive = 0;
    dd::Integer storyMaximum(const std::string &name) const override {
        if (name != "LANDERS_LOST") throw std::invalid_argument("unexpected story batch key");
        return 255;
    }
    dd::Result setStoryBatch(const std::vector<std::pair<std::string, dd::Json>> &changes) override {
        auto staged = fields;
        for (const auto &[key, value] : changes) staged[key] = value.get<dd::Integer>();
        fields.swap(staged);
        return {};
    }
    void validateInventoryKey(const std::string &key) const override {
        if (key != "landers") throw std::invalid_argument("unexpected transaction key");
    }
    dd::Result transact(const uqm::InventoryTransaction &request) override {
        if (!request.give.empty() || request.receive != uqm::InventoryPackage{{"landers",1}})
            throw std::invalid_argument("unexpected transaction");
        ++landers;
        return {};
    }
    dd::Integer fuel() const override {
        return shipFuel;
    }
    dd::Integer quantity(const std::string &item) const override {
        if (item == "landers")
            return landers;
        if (item == "radioactive")
            return radioactive;
        if (item == "biomass")
            return biomass;
        throw std::invalid_argument("unexpected quantity");
    }
    bool allied = false, shofixtiEscort = false, umgahAvailable = true;
    bool hasShip(const std::string &type) const override {
        if (type == "spathi")
            return escorts != 0;
        if (type != "shofixti")
            throw std::invalid_argument("unexpected escort query");
        return shofixtiEscort;
    }
    unsigned reveals = 0, refits = 0;
    dd::Result configureShip(const std::string &operation, const dd::Json &parameters) override {
        if (operation != "chmmr_bomb_refit" || !parameters.is_object() || !parameters.empty())
            throw std::invalid_argument("unexpected refit");
        ++refits;
        return {};
    }
    std::vector<std::string> alliances;
    dd::Integer shipCount(const std::string &race) const override {
        if (race != "orz")
            throw std::invalid_argument("unexpected fleet count");
        return escorts;
    }
    dd::Result removeShips(const std::string &race, dd::Integer count) override {
        if (race != "orz")
            throw std::invalid_argument("unexpected fleet withdrawal");
        const auto removed = std::min(escorts, count);
        escorts -= removed;
        return {"ok", {{"count", removed}}};
    }
    dd::Result setRelationship(const std::string &race, const std::string &relation) override {
        if ((race != "utwig" && race != "supox" && race != "orz" && race != "syreen" &&
             race != "spathi" && race != "chmmr" && race != "zoqfot" && race != "thradd") ||
            (relation != "allied" && relation != "neutral"))
            throw std::invalid_argument("unexpected alliance");
        alliances.push_back(race);
        allied = relation == "allied";
        return {};
    }
    dd::Integer availableSlots(const std::string &) const override {
        return slots;
    }
    std::string relationship(const std::string &) const override {
        return allied ? "allied" : "neutral";
    }
    dd::Result revealRace(const std::string &race) override {
        if (race != "utwig" && race != "supox" && race != "umgah")
            throw std::invalid_argument("unexpected reveal");
        ++reveals;
        return {"ok", {{"available", umgahAvailable}}};
    }
    dd::Result adjustShip(const std::string &property, dd::Integer amount) override {
        if (property == "biomass")
            biomass += amount;
        else if (property == "fuel")
            shipFuel += amount;
        else if (property == "crew")
            crew += amount;
        else if (property == "landers")
            landers += amount;
        else
            throw std::invalid_argument("unexpected cargo");
        return {};
    }
    dd::Result createShip(const std::string &type, const std::string &captain,
                          dd::Integer crew) override {
        if (type != "spathi" || captain != "fwiffo" || crew != 1)
            throw std::invalid_argument("unexpected named captain");
        ++escorts;
        return {"ok", {{"count", 1}}};
    }
    dd::Result giveShips(const std::string &type, dd::Integer count) override {
        if (type != "umgah" && type != "supox" && type != "yehat" && type != "yehat_rebel" &&
            type != "pkunk" && type != "utwig" && type != "zoqfot")
            throw std::invalid_argument("unexpected escorts");
        escorts += count;
        return {"ok", {{"count", count}}};
    }
    std::vector<dd::Integer> randomValues;
    dd::Integer randomBelow(dd::Integer bound) override {
        if (bound != 256)
            throw std::invalid_argument("unexpected random bound");
        const auto result = randomDraws < randomValues.size() ? randomValues[randomDraws] : 255;
        ++randomDraws;
        return result;
    }
    dd::Json context = {{"won_last_battle", false},
                        {"at_samatra", false},
                        {"at_probe_message", false},
                        {"at_colony", false},
                        {"in_last_battle", false},
                        {"cyborg_enabled", false},
                        {"using_speech", true},
                        {"has_npc_ships", false},
                        {"calendar_campaign_year", 0},
                        {"years_to_kohrah_victory", 4},
                        {"thraddash_body_threshold", 25},
                        {"calendar_month", 2},
                        {"calendar_day", 17}};
    dd::Json read(const std::string &scope, const std::string &key) const override {
        if (scope == "player") {
            if (key == "captain_name")
                return "Zelnick";
            if (key == "ship_name")
                return "Vindicator";
            if (key == "alliance_name")
                return "New Alliance";
            throw std::invalid_argument("unexpected player field");
        }
        if (scope == "encounter")
            return context.at(key);
        if (scope != "story")
            throw std::invalid_argument("unexpected scope");
        const auto found = fields.find(key);
        return found == fields.end() ? dd::Json(0) : dd::Json(found->second);
    }
    dd::Result set(const std::string &scope, const std::string &key,
                   const dd::Json &value) override {
        if (scope != "story")
            throw std::invalid_argument("unexpected scope");
        fields[key] = value.get<dd::Integer>();
        return {};
    }
    dd::Result setOutcome(const std::string &value) override {
        outcome = value;
        return {};
    }
    dd::Result scheduleEvent(const std::string &name, dd::Integer months, dd::Integer days,
                             dd::Integer years) override {
        const bool shofixti = name == "SHOFIXTI_RETURN_EVENT" && months == 2 && days == 0;
        const bool arilou = name == "ARILOU_UMGAH_CHECK" && months == 0 && days == 10;
        const bool mission =
            (name == "ADVANCE_ILWRATH_MISSION" || name == "ADVANCE_MYCON_MISSION" ||
             name == "YEHAT_REBEL_EVENT" || name == "SLYLANDRO_RAMP_DOWN" ||
             name == "ADVANCE_UTWIG_SUPOX_MISSION" || name == "ADVANCE_THRADD_MISSION") &&
            months == 0 && days == 0;
        const bool zoqfot = name == "ZOQFOT_DISTRESS_EVENT" && months == 3 && days == 0;
        const bool pkunk = (name == "ADVANCE_PKUNK_MISSION" || name == "SPATHI_SHIELD_EVENT") &&
                           (months == 0 || months == 6) && days == 0;
        if (years != 0 || !(shofixti || arilou || mission || pkunk || zoqfot))
            throw std::invalid_argument("unexpected calendar action");
        ++calendarEvents;
        return {};
    }
    dd::Result prepareReport(const std::string &name, const dd::Json &) override {
        if (name == "mycon.ramble")
            return {"ok", {{"speech", dd::Json::array({"RAMBLE_1"})}, {"made_choice", false}}};
        if (name == "probe.coordinates")
            ++coordinates;
        else if (name == "urquan.hypno_intro")
            ++fades;
        else
            throw std::invalid_argument("unexpected report");
        return {};
    }
};
std::unique_ptr<dd::Conversation> probeMenu(ProbeState &state) {
    const auto data = read(std::filesystem::path(LIVE_DIALOGUE_ROOT) / "comm.probe.dialogue.json");
    auto graph = dd::Conversation::load(data, state);
    graph->advance();
    graph->finishDialogue(graph->view().entry); // Greeting -> menu.
    graph->finishDialogue(graph->view().entry); // Empty menu speech -> responses.
    return graph;
}
} // namespace
BOOST_AUTO_TEST_CASE(probe_greeting_sequences_clamp_at_eighth_visit_and_preserve_response_order) {
    for (dd::Integer visit = 0; visit < 8; ++visit) {
        ProbeState state;
        state.fields["SLYLANDRO_PROBE_VISITS"] = visit;
        const auto data =
            read(std::filesystem::path(LIVE_DIALOGUE_ROOT) / "comm.probe.dialogue.json");
        auto graph = dd::Conversation::load(data, state);
        graph->advance();
        BOOST_TEST(graph->view().speech[0].get<std::string>() ==
                   "WE_COME_IN_PEACE_" + std::to_string(visit + 1));
        BOOST_TEST(state.fields.at("SLYLANDRO_PROBE_VISITS") ==
                   std::min<dd::Integer>(visit + 1, 7));
        graph->finishDialogue(graph->view().entry);
        graph->finishDialogue(graph->view().entry);
        const auto responses = graph->responses();
        BOOST_TEST(responses.size() == 5u);
        BOOST_TEST(responses[0].id == "threat_1");
        BOOST_TEST(responses[1].id == "something_wrong_1");
        BOOST_TEST(responses[2].id == "we_are_us_1");
        BOOST_TEST(responses[3].id == "why_attack_1");
        BOOST_TEST(responses[4].id == "bye_1");
    }
}
BOOST_AUTO_TEST_CASE(
    probe_responses_commit_counters_bug_and_hostility_and_use_one_coordinate_handler) {
    for (const auto &group :
         std::vector<std::pair<std::string, std::string>>{{"threat", "THREAT"},
                                                          {"something_wrong", "WRONG"},
                                                          {"we_are_us", "ID"},
                                                          {"why_attack", "INFO"},
                                                          {"bye", "EXIT"}}) {
        for (dd::Integer stage = 0; stage < 4; ++stage) {
            ProbeState state;
            const auto field = "SLYLANDRO_PROBE_" + group.second;
            state.fields[field] = stage;
            auto graph = probeMenu(state);
            graph->choose(graph->view().entry, group.first + "_" + std::to_string(stage + 1));
            BOOST_TEST(state.fields.at(field) == std::min<dd::Integer>(stage + 1, 3));
            BOOST_TEST(state.fields.at("PROBE_EXHIBITED_BUG") == 1);
            BOOST_TEST(state.outcome == "hostile");
            BOOST_TEST(state.coordinates == (group.second == "ID" && stage == 3 ? 1u : 0u));
            BOOST_TEST(graph->view().speech.back().get<std::string>() == "HOSTILE");
            BOOST_TEST(!graph->ended());
            graph->finishDialogue(graph->view().entry);
            BOOST_TEST(graph->ended());
        }
    }
}
BOOST_AUTO_TEST_CASE(probe_destruct_code_is_available_only_when_owned_and_preserves_victory) {
    ProbeState state;
    state.fields["DESTRUCT_CODE_ON_SHIP"] = 1;
    auto graph = probeMenu(state);
    BOOST_TEST(graph->responses().size() == 6u);
    BOOST_TEST(graph->responses().front().id == "destruct_code");
    graph->choose(graph->view().entry, "destruct_code");
    BOOST_TEST(state.outcome == "victory");
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "DESTRUCT_SEQUENCE");
    BOOST_TEST(state.fields["PROBE_EXHIBITED_BUG"] == 0);
}
namespace {
std::unique_ptr<dd::Conversation> kohrah(ProbeState &state) {
    return dd::Conversation::load(
        read(std::filesystem::path(LIVE_DIALOGUE_ROOT) / "comm.kohrah.dialogue.json"), state);
}
void finishToMenu(dd::Conversation &graph) {
    graph.finishDialogue(graph.view().entry);
    graph.finishDialogue(graph.view().entry);
}
} // namespace
BOOST_AUTO_TEST_CASE(kohrah_keeps_the_missing_third_pleading_record_and_stage_two_behavior) {
    ProbeState state;
    state.fields["KOHR_AH_PLEAD"] = 2;
    auto graph = kohrah(state);
    graph->advance();
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "HELLO_AND_DIE_1");
    BOOST_TEST(state.fields.at("KOHR_AH_VISITS") == 1);
    finishToMenu(*graph);
    const auto responses = graph->responses();
    bool fourth = false, third = false;
    for (const auto &response : responses) {
        fourth = fourth || response.id == "plead_4";
        third = third || response.id == "plead_3";
    }
    BOOST_CHECK(fourth && !third);
    graph->choose(graph->view().entry, "plead_4");
    BOOST_TEST(state.fields.at("KOHR_AH_PLEAD") == 2);
    BOOST_TEST(state.outcome == "hostile");
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "PLEADING_IS_USELESS_4");
}
BOOST_AUTO_TEST_CASE(kohrah_story_questions_are_visit_local_and_key_phrase_changes_campaign_state) {
    ProbeState state;
    state.fields["KNOW_KOHR_AH_STORY"] = 1;
    auto graph = kohrah(state);
    graph->advance();
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "key_phrase");
    BOOST_TEST(state.fields.at("KNOW_KOHR_AH_STORY") == 2);
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "RESPONSE_TO_KEY_PHRASE");
    finishToMenu(*graph);
    BOOST_TEST(graph->responses().size() == 5u);
    graph->choose(graph->view().entry, "why_do_you_destroy");
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "WE_WERE_SLAVES");
    finishToMenu(*graph);
    BOOST_TEST(graph->responses().size() == 4u);
    BOOST_CHECK_THROW(graph->choose(graph->view().entry, "why_do_you_destroy"),
                      std::invalid_argument);
    graph->choose(graph->view().entry, "guess_thats_all");
    BOOST_TEST(state.outcome == "hostile");
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "THEN_DIE");
}
BOOST_AUTO_TEST_CASE(kohrah_outtakes_defeat_and_samatra_have_their_original_special_paths) {
    for (const auto &context : {"outtakes", "killed_all", "samatra"}) {
        ProbeState state;
        const std::string mode = context;
        if (mode == "outtakes")
            state.context["won_last_battle"] = true;
        else if (mode == "killed_all")
            state.fields["KOHR_AH_KILLED_ALL"] = 1;
        else
            state.context["at_samatra"] = true;
        auto graph = kohrah(state);
        graph->advance();
        const std::string expected = mode == "outtakes"     ? "OUT_TAKES"
                                     : mode == "killed_all" ? "GAME_OVER_DUDE"
                                                            : "HELLO_SAMATRA";
        BOOST_TEST(graph->view().speech[0].get<std::string>() == expected);
        BOOST_TEST(state.outcome == (mode == "samatra" ? "hostile" : "peace"));
        BOOST_TEST(state.fields["KOHR_AH_VISITS"] == 0);
        graph->finishDialogue(graph->view().entry);
        BOOST_TEST(graph->ended());
    }
    ProbeState state;
    state.fields["TALKING_PET_ON_SHIP"] = 1;
    auto graph = kohrah(state);
    graph->advance();
    BOOST_TEST(state.fields.at("KOHR_AH_SENSES_EVIL") == 1);
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "SENSE_EVIL");
    BOOST_TEST(graph->view().speech[1][0].get<std::string>() == "HELLO_AND_DIE_1");
}
namespace {
std::unique_ptr<dd::Conversation> urquan(ProbeState &state) {
    return dd::Conversation::load(
        read(std::filesystem::path(LIVE_DIALOGUE_ROOT) / "comm.urquan.dialogue.json"), state);
}
} // namespace
BOOST_AUTO_TEST_CASE(urquan_surrender_confirmations_keep_defeat_and_hostility_distinct) {
    for (const auto willing : {true, false}) {
        ProbeState state;
        auto graph = urquan(state);
        graph->advance();
        BOOST_TEST(graph->view().speech[0].get<std::string>() == "INIT_HELLO");
        finishToMenu(*graph);
        graph->choose(graph->view().entry, "i_surrender");
        BOOST_TEST(graph->view().speech[0].get<std::string>() == "DISOBEDIENCE_PUNISHED");
        graph->finishDialogue(graph->view().entry);
        BOOST_TEST(graph->responses().size() == 2u);
        graph->choose(graph->view().entry, willing ? "i_will_surrender" : "i_wont_surrender");
        BOOST_TEST(state.outcome == (willing ? "defeat" : "hostile"));
        BOOST_TEST(!graph->ended());
        graph->finishDialogue(graph->view().entry);
        BOOST_TEST(graph->ended());
    }
}
BOOST_AUTO_TEST_CASE(urquan_hypnosis_pet_description_uses_graph_topics_and_persistent_flags) {
    ProbeState state;
    state.fields["PLAYER_HYPNOTIZED"] = 1;
    auto graph = urquan(state);
    graph->advance();
    BOOST_TEST(state.fades == 1u);
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "INIT_URQUAN_WAKE_UP");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "compulsion");
    BOOST_TEST(state.fields.at("MENTIONED_PET_COMPULSION") == 1);
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "wascally_little_guy");
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "WHAT_IT_LOOK_LIKE");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "turd_and_toad");
    finishToMenu(*graph);
    BOOST_TEST(graph->responses().size() == 2u);
    BOOST_CHECK_THROW(graph->choose(graph->view().entry, "turd_and_toad"), std::invalid_argument);
    graph->choose(graph->view().entry, "terran_amphibian");
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "BAD_NEWS");
    BOOST_TEST(state.outcome == "peace");
    BOOST_TEST(state.fields.at("URQUAN_HYPNO_VISITS") == 1);
}
BOOST_AUTO_TEST_CASE(urquan_frenzy_subsequent_visit_and_probe_message_complete_without_a_menu) {
    ProbeState state;
    state.fields["KOHR_AH_FRENZY"] = 1;
    state.fields["URQUAN_VISITS"] = 1;
    auto graph = urquan(state);
    graph->advance();
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "SUBSEQUENT_FLEE_HUMAN");
    BOOST_TEST(state.outcome == "peace");
    BOOST_TEST(state.fields.at("URQUAN_VISITS") == 1);
    graph->finishDialogue(graph->view().entry);
    BOOST_TEST(graph->ended());
    ProbeState messenger;
    messenger.context["at_probe_message"] = true;
    graph = urquan(messenger);
    graph->advance();
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "SEND_MESSAGE");
    BOOST_TEST(messenger.fields.at("PROBE_MESSAGE_DELIVERED") == 1);
    graph->finishDialogue(graph->view().entry);
    BOOST_TEST(graph->ended());
}
namespace {
std::unique_ptr<dd::Conversation> arilou(ProbeState &state) {
    return dd::Conversation::load(
        read(std::filesystem::path(LIVE_DIALOGUE_ROOT) / "comm.arilou.dialogue.json"), state);
}
} // namespace
BOOST_AUTO_TEST_CASE(
    arilou_first_homeworld_contact_prioritizes_selected_topic_and_exchanges_portal) {
    ProbeState state;
    state.fields["ARILOU_SPACE_SIDE"] = 2;
    state.fields["PORTAL_KEY_ON_SHIP"] = 1;
    auto graph = arilou(state);
    graph->advance();
    BOOST_TEST(state.fields.at("MET_ARILOU") == 1);
    BOOST_TEST(state.fields.at("ARILOU_HOME_VISITS") == 1);
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "FRDLY_HOMEWORLD_HELLO_1");
    graph->finishDialogue(graph->view().entry);
    BOOST_TEST(graph->responses().size() == 3u);
    graph->choose(graph->view().entry, "happy_by_hello");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "what_give_me");
    BOOST_TEST(state.fields.at("KNOW_ARILOU_WANT_WRECK") == 1);
    finishToMenu(*graph);
    BOOST_TEST(graph->responses()[0].id == "what_about_tpet");
    graph->choose(graph->view().entry, "got_it");
    BOOST_TEST(state.fields.at("PORTAL_KEY_ON_SHIP") == 0);
    BOOST_TEST(state.fields.at("PORTAL_SPAWNER") == 1);
    BOOST_TEST(state.fields.at("PORTAL_SPAWNER_ON_SHIP") == 1);
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "CLEVER_HUMAN");
    BOOST_TEST(graph->view().speech[1].get<std::string>() == "GIVE_PORTAL");
}
BOOST_AUTO_TEST_CASE(arilou_hints_have_campaign_effects_and_are_pruned_for_one_visit) {
    ProbeState state;
    state.fields["MET_ARILOU"] = 1;
    state.fields["ARILOU_HINTS"] = 1;
    auto graph = arilou(state);
    graph->advance();
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "give_me_info_2");
    BOOST_TEST(state.fields.at("KNOW_ABOUT_SHATTERED") == 2);
    BOOST_TEST(state.fields.at("ARILOU_HINTS") == 2);
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "ARILOU_HINTS_2");
    finishToMenu(*graph);
    BOOST_CHECK_THROW(graph->choose(graph->view().entry, "give_me_info_2"), std::invalid_argument);
    graph = arilou(state); // A fresh visit resets the topic mask, not campaign progress.
    graph->advance();
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "give_me_info_2");
    BOOST_TEST(state.fields.at("KNOW_URQUAN_STORY") == 1);
    BOOST_TEST(state.fields.at("KNOW_KOHR_AH_STORY") == 1);
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "ARILOU_HINTS_3");
}
BOOST_AUTO_TEST_CASE(arilou_umgah_check_schedules_once_and_retains_investigation_topic_priority) {
    ProbeState state;
    state.fields["MET_ARILOU"] = 1;
    state.fields["ARILOU_SPACE_SIDE"] = 2;
    state.fields["KNOW_UMGAH_ZOMBIES"] = 1;
    auto graph = arilou(state);
    graph->advance();
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "umgah_acting_weird");
    BOOST_TEST(state.calendarEvents == 1u);
    BOOST_TEST(state.fields.at("ARILOU_CHECKED_UMGAH") == 1);
    finishToMenu(*graph);
    BOOST_CHECK_THROW(graph->choose(graph->view().entry, "learned_about_umgah"),
                      std::invalid_argument);
    state.fields["ARILOU_CHECKED_UMGAH"] = 2;
    graph = arilou(state);
    graph->advance();
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "learned_about_umgah");
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "UMGAH_UNDER_COMPULSION");
    finishToMenu(*graph);
    BOOST_TEST(graph->responses()[0].id == "what_do_now");
    graph->choose(graph->view().entry, "what_do_now");
    BOOST_TEST(state.fields.at("ARILOU_CHECKED_UMGAH") == 3);
    BOOST_TEST(state.calendarEvents == 1u);
}
namespace {
std::unique_ptr<dd::Conversation> liveGraph(const char *resource, ProbeState &state) {
    return dd::Conversation::load(
        read(std::filesystem::path(LIVE_DIALOGUE_ROOT) / (std::string(resource) + ".json")), state);
}
} // namespace
BOOST_AUTO_TEST_CASE(ilwrath_gods_questions_unlock_channel_44_followup_and_prune_visit_topics) {
    ProbeState state;
    state.fields["GLOBAL_FLAGS_AND_DATA"] = 128;
    auto graph = liveGraph("comm.ilwrath.dialogue", state);
    graph->advance();
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "want_info_on_gods");
    finishToMenu(*graph);
    BOOST_TEST(graph->responses().size() == 4u);
    BOOST_CHECK_THROW(graph->choose(graph->view().entry, "why_44"), std::invalid_argument);
    graph->choose(graph->view().entry, "how_talk_with_gods");
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "CHANNEL_44");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "why_44");
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "BECAUSE_44");
    finishToMenu(*graph);
    BOOST_TEST(graph->responses().size() == 3u);
}
BOOST_AUTO_TEST_CASE(ilwrath_divine_orders_schedule_the_mission_once_and_end_peacefully) {
    ProbeState state;
    state.fields["GLOBAL_FLAGS_AND_DATA"] = 16;
    auto graph = liveGraph("comm.ilwrath.dialogue", state);
    graph->advance();
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "ILWRATH_BELIEVE");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "go_kill_thraddash");
    BOOST_TEST(state.fields.at("ILWRATH_DECEIVED") == 1);
    BOOST_TEST(state.calendarEvents == 1u);
    finishToMenu(*graph);
    BOOST_CHECK_THROW(graph->choose(graph->view().entry, "go_kill_thraddash"),
                      std::invalid_argument);
    graph->choose(graph->view().entry, "worship_us");
    finishToMenu(*graph);
    BOOST_CHECK_THROW(graph->choose(graph->view().entry, "worship_us"), std::invalid_argument);
    graph->choose(graph->view().entry, "bye_gods");
    BOOST_TEST(state.outcome == "peace");
    BOOST_TEST(state.fields.at("ILWRATH_GODS_SPOKEN") == 1);
    BOOST_TEST(state.calendarEvents == 1u);
}
BOOST_AUTO_TEST_CASE(mycon_caches_random_callback_choice_and_draws_only_on_new_normal_menus) {
    ProbeState state;
    state.randomValues = {255, 0};
    auto graph = liveGraph("comm.mycon.dialogue", state);
    graph->advance();
    BOOST_TEST(state.randomDraws == 1u);
    finishToMenu(*graph);
    graph->responses();
    graph->responses();
    BOOST_TEST(state.randomDraws == 1u);
    graph->choose(graph->view().entry, "come_in_peace");
    BOOST_TEST(state.randomDraws == 2u);
    BOOST_TEST(graph->view().speech[1].get<std::string>() == "RAMBLE_TAIL");
    finishToMenu(*graph);
    BOOST_CHECK_THROW(graph->choose(graph->view().entry, "come_in_peace"), std::invalid_argument);
    graph->choose(graph->view().entry, "gonna_die");
    BOOST_TEST(state.randomDraws == 2u);
    BOOST_TEST(state.outcome == "hostile");
    BOOST_TEST(graph->view().speech[1].get<std::string>() == "BYE_AND_DIE_SPACE");
}
BOOST_AUTO_TEST_CASE(
    mycon_ambush_bypasses_random_hostility_and_commits_only_after_all_three_clues) {
    ProbeState state;
    state.fields["GLOBAL_FLAGS_AND_DATA"] = 128;
    state.fields["MYCON_AMBUSH"] = 1;
    state.randomValues = {0};
    auto graph = liveGraph("comm.mycon.dialogue", state);
    graph->advance();
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "i_have_a_cunning_plan");
    finishToMenu(*graph);
    for (const auto clue : {3, 1}) {
        graph->choose(graph->view().entry, "clue_" + std::to_string(clue));
        BOOST_TEST(state.fields["MYCON_FELL_FOR_AMBUSH"] == 0);
        BOOST_TEST(state.calendarEvents == 0u);
        finishToMenu(*graph);
    }
    graph->choose(graph->view().entry, "clue_2");
    BOOST_TEST(state.fields.at("MYCON_FELL_FOR_AMBUSH") == 1);
    BOOST_TEST(state.calendarEvents == 1u);
    BOOST_TEST(state.outcome == "peace");
    BOOST_TEST(state.randomDraws == 1u);
    BOOST_TEST(graph->view().speech[1].get<std::string>() == "WE_GO_TO_IMPLANT");
}
BOOST_AUTO_TEST_CASE(
    mycon_shattered_question_uses_its_forced_normal_callback_even_on_hostile_roll) {
    ProbeState state;
    state.fields["GLOBAL_FLAGS_AND_DATA"] = 128;
    state.fields["KNOW_ABOUT_SHATTERED"] = 1;
    state.randomValues = {0, 255};
    auto graph = liveGraph("comm.mycon.dialogue", state);
    graph->advance();
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "what_about_shattered");
    BOOST_TEST(state.fields.at("KNOW_ABOUT_SHATTERED") == 2);
    BOOST_TEST(state.randomDraws == 2u);
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "ABOUT_SHATTERED");
    BOOST_TEST(!graph->ended());
}
BOOST_AUTO_TEST_CASE(
    resolved_response_composition_keeps_insertions_literal_and_rejects_unknown_shapes) {
    const dd::PhraseText lookup = [](const std::string &id) {
        if (id == "prefix")
            return std::string("Captain ");
        if (id == "suffix")
            return std::string(", fuel ");
        throw std::invalid_argument("unknown test phrase");
    };
    const auto segments = dd::Json::parse(
        R"({"compose":[{"phrase":"prefix"},{"insert":"Poop $() Captain"},"suffix",{"number":25}]})");
    BOOST_TEST(dd::formatText(segments, lookup) == "Captain Poop $() Captain, fuel 25");
    BOOST_CHECK_THROW(dd::formatText({{"unknown", "prefix"}}, lookup), std::invalid_argument);
    BOOST_CHECK_THROW(
        dd::formatText({{"number", std::numeric_limits<std::uint64_t>::max()}}, lookup),
        std::invalid_argument);
}
BOOST_AUTO_TEST_CASE(umgah_reward_grants_biomass_once_and_delivers_escorts_after_the_farewell) {
    ProbeState state;
    state.fields["TALKING_PET"] = 1;
    state.fields["GLOBAL_FLAGS_AND_DATA"] = 128;
    auto graph = liveGraph("comm.umgah.dialogue", state);
    graph->advance();
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "our_largesse");
    BOOST_TEST(state.biomass == 500);
    finishToMenu(*graph);
    BOOST_CHECK_THROW(graph->choose(graph->view().entry, "owe_me_big_time"), std::invalid_argument);
    graph->choose(graph->view().entry, "bye_post_zombie");
    BOOST_TEST(state.outcome == "hostile");
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "FUNNY_IDEA");
    BOOST_TEST(state.escorts == 0);
    BOOST_TEST(state.fields["UMGAH_HOSTILE"] == 0);
    graph->advance();
    BOOST_TEST(state.escorts == 0);
    graph->finishDialogue(graph->view().entry);
    BOOST_TEST(state.escorts == 4);
    BOOST_TEST(state.fields.at("UMGAH_HOSTILE") == 1);
    BOOST_TEST(graph->ended());
    graph->advance();
    BOOST_TEST(state.escorts == 4);
}
BOOST_AUTO_TEST_CASE(umgah_known_zombie_topics_change_order_when_the_pet_denial_is_exposed) {
    ProbeState state;
    state.fields["UMGAH_ZOMBIE_BLOBBIES"] = 1;
    state.fields["MET_NORMAL_UMGAH"] = 1;
    auto graph = liveGraph("comm.umgah.dialogue", state);
    graph->advance();
    finishToMenu(*graph);
    BOOST_TEST(graph->responses()[0].id == "whats_up_zombies");
    BOOST_TEST(graph->responses()[1].id == "how_goes_tpet");
    graph->choose(graph->view().entry, "how_goes_tpet");
    finishToMenu(*graph);
    BOOST_TEST(graph->responses()[0].id == "you_told_us");
    BOOST_TEST(graph->responses()[1].id == "whats_up_zombies");
    graph->choose(graph->view().entry, "you_told_us");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "dont_believe");
    BOOST_TEST(state.fields.at("KNOW_UMGAH_ZOMBIES") == 1);
    BOOST_TEST(state.fields.at("UMGAH_VISITS") == 0);
    BOOST_TEST(state.outcome == "hostile");
}
BOOST_AUTO_TEST_CASE(umgah_constructed_introduction_uses_owned_player_text_and_topic_focus) {
    ProbeState state;
    state.fields["UMGAH_ZOMBIE_BLOBBIES"] = 1;
    auto graph = liveGraph("comm.umgah.dialogue", state);
    graph->advance();
    finishToMenu(*graph);
    const auto responses = graph->responses();
    const auto intro =
        std::find_if(responses.begin(), responses.end(), [](const dd::ResponseView &response) {
            return response.id == "we_vindicator0";
        });
    BOOST_REQUIRE(intro != responses.end());
    BOOST_TEST(intro->text.at("compose")[1].at("insert").get<std::string>() == "Zelnick");
    BOOST_TEST(intro->text.at("compose")[3].at("insert").get<std::string>() == "Vindicator");
    graph->choose(graph->view().entry, "we_vindicator0");
    finishToMenu(*graph);
    BOOST_TEST(graph->responses()[0].id == "come_in_peace");
    BOOST_CHECK_THROW(graph->choose(graph->view().entry, "we_vindicator0"), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(supox_introductions_and_ultron_topics_preserve_focus_and_reveal_utwig) {
    ProbeState state;
    auto graph = liveGraph("comm.supox.dialogue", state);
    graph->advance();
    finishToMenu(*graph);
    BOOST_TEST(graph->responses()[0].id == "i_am0");
    BOOST_TEST(dd::formatText(graph->responses()[0].text,
                              [](const std::string &id) { return id; }) == "i_am0Zelnicki_am1");
    graph->choose(graph->view().entry, "anyone_around_here");
    BOOST_TEST(state.reveals == 1u);
    finishToMenu(*graph);
    BOOST_TEST(graph->responses()[0].id == "whats_wrong_with_utwig");
    graph->choose(graph->view().entry, "whats_wrong_with_utwig");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "whats_ultron");
    BOOST_TEST(state.fields.at("ULTRON_CONDITION") == 1);
    finishToMenu(*graph);
    BOOST_TEST(graph->responses().size() == 2u);
    graph->choose(graph->view().entry, "thanks_now_we_eat_you");
    BOOST_TEST(state.fields.at("SUPOX_HOSTILE") == 1);
    BOOST_TEST(state.outcome == "peace");
}
BOOST_AUTO_TEST_CASE(supox_gifts_wait_for_speech_and_full_fleets_receive_no_ships) {
    for (const auto slots : {0, 2, 12}) {
        ProbeState state;
        state.allied = true;
        state.slots = slots;
        state.fields["GLOBAL_FLAGS_AND_DATA"] = 128;
        auto graph = liveGraph("comm.supox.dialogue", state);
        graph->advance();
        finishToMenu(*graph);
        graph->choose(graph->view().entry, "can_you_help");
        BOOST_TEST(state.escorts == 0);
        BOOST_TEST(graph->view().speech[0].get<std::string>() == "HOW_HELP");
        BOOST_TEST(graph->view().speech[1].get<std::string>() ==
                   (slots ? "HAVE_4_SHIPS" : "DONT_NEED"));
        graph->finishDialogue(graph->view().entry);
        BOOST_TEST(state.escorts ==
                   (slots ? 4 : 0)); // The native fleet adapter clamps to capacity.
        BOOST_TEST(graph->ended());
    }
}
BOOST_AUTO_TEST_CASE(supox_prewar_report_preserves_the_legacy_news_counter_and_prunes_per_visit) {
    ProbeState state;
    state.allied = true;
    state.fields["GLOBAL_FLAGS_AND_DATA"] = 128;
    state.fields["UTWIG_SUPOX_MISSION"] = 1;
    auto graph = liveGraph("comm.supox.dialogue", state);
    graph->advance();
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "how_goes_war");
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "FLEET_ON_WAY");
    BOOST_TEST(state.fields["SUPOX_WAR_NEWS"] == 0);
    finishToMenu(*graph);
    BOOST_CHECK_THROW(graph->choose(graph->view().entry, "how_goes_war"), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(graph_owned_scripts_reference_existing_native_phrases) {
    for (const auto &file : std::filesystem::directory_iterator(LIVE_DIALOGUE_ROOT)) {
        const auto data = read(file.path());
        if (data.at("_migration").at("status") == "native_handlers")
            continue;
        std::set<std::string> phrases;
        for (const auto &phrase : data.at("phrases"))
            phrases.insert(phrase.at("id"));
        std::function<void(const dd::Json &)> check = [&](const dd::Json &value) {
            if (value.is_string()) {
                const auto name = value.get<std::string>();
                BOOST_TEST_CONTEXT(file.path().filename().string() << ": " << name) {
                    BOOST_CHECK(phrases.count(name) != 0);
                }
            } else if (value.is_array()) {
                for (const auto &part : value)
                    check(part);
            } else if (value.is_object()) {
                if (value.contains("compose"))
                    check(value.at("compose"));
                else if (value.contains("phrase"))
                    check(value.at("phrase"));
                // get/insert/number are resolved values, not phrase identifiers.
            }
        };
        for (const auto &node : data.at("nodes")) {
            if (node.at("kind") == "dialogue") {
                for (const auto &variant : node.at("variants"))
                    check(variant.at("speech"));
                if (node.contains("responses"))
                    for (const auto &response : node.at("responses"))
                        check(response.at("text"));
            } else if (node.at("kind") == "action" &&
                       node.at("action").value("action", node.at("action").value("verb", "")) == "sequence.next") {
                check(node.at("action").at("args")[2]);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(rebel_ship_gifts_respect_calendar_boundaries_and_deliver_after_speech) {
    for (const auto month : {11, 12}) {
        ProbeState state;
        state.slots = 3;
        state.context["calendar_month"] = month;
        state.context["calendar_day"] = 31;
        auto graph = liveGraph("comm.yehat.rebel.dialogue", state);
        graph->advance();
        finishToMenu(*graph);
        graph->choose(graph->view().entry, "any_ships");
        BOOST_TEST(graph->view().speech[0].get<std::string>() == "HAVE_FEW_SHIPS");
        BOOST_TEST(state.escorts == 0);
        finishToMenu(*graph);
        BOOST_TEST(state.escorts == 3);
        BOOST_TEST(state.fields.at("YEHAT_SHIP_MONTH") == (month == 12 ? 1 : 12));
        BOOST_TEST(state.fields.at("YEHAT_SHIP_YEAR") == (month == 12 ? 1 : 0));
        BOOST_TEST(state.fields.at("YEHAT_SHIP_DAY") == 28);
        BOOST_CHECK_THROW(graph->choose(graph->view().entry, "any_ships"), std::invalid_argument);
        auto revisit = liveGraph("comm.yehat.rebel.dialogue", state);
        revisit->advance();
        finishToMenu(*revisit);
        revisit->choose(revisit->view().entry, "any_ships");
        BOOST_TEST(revisit->view().speech[0].get<std::string>() == "NO_SHIPS_YET");
        const auto giftYear = state.fields.at("YEHAT_SHIP_YEAR");
        state.context["calendar_campaign_year"] = giftYear;
        state.context["calendar_month"] = state.fields.at("YEHAT_SHIP_MONTH");
        state.context["calendar_day"] = 28;
        auto ready = liveGraph("comm.yehat.rebel.dialogue", state);
        ready->advance();
        finishToMenu(*ready);
        ready->choose(ready->view().entry, "any_ships");
        BOOST_TEST(ready->view().speech[0].get<std::string>() == "HAVE_FEW_SHIPS");
    }
}
BOOST_AUTO_TEST_CASE(rebel_information_is_exhausted_per_visit_and_cavalry_caps_the_combined_gift) {
    ProbeState state;
    auto graph = liveGraph("comm.yehat.rebel.dialogue", state);
    graph->advance();
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "give_info_rebels");
    finishToMenu(*graph);
    for (const auto topic : {"what_about_urquan", "what_about_royalty", "what_about_war",
                             "what_about_vux", "what_about_clue"}) {
        graph->choose(graph->view().entry, topic);
        finishToMenu(*graph);
    }
    BOOST_TEST(graph->responses().size() == 1u);
    graph->choose(graph->view().entry, "enough_info");
    finishToMenu(*graph);
    BOOST_CHECK_THROW(graph->choose(graph->view().entry, "give_info_rebels"),
                      std::invalid_argument);
    for (const auto slots : {0, 1, 7, 12}) {
        ProbeState cavalry;
        cavalry.context["in_last_battle"] = true;
        cavalry.slots = slots;
        auto visit = liveGraph("comm.yehat.rebel.dialogue", cavalry);
        visit->advance();
        BOOST_TEST(visit->view().speech[0].get<std::string>() == "YEHAT_CAVALRY");
        BOOST_TEST(cavalry.escorts == 0);
        visit->finishDialogue(visit->view().entry);
        BOOST_TEST(cavalry.escorts == std::min(slots, 8));
        BOOST_TEST(visit->ended());
    }
}

BOOST_AUTO_TEST_CASE(shofixti_recruitment_follows_recognition_consolation_and_the_two_month_event) {
    ProbeState state;
    state.fields["MAIDENS_ON_SHIP"] = 1;
    state.fields["SHOFIXTI_STACK2"] = 2;
    auto graph = liveGraph("comm.shofixti.dialogue", state);
    graph->advance();
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "donkey_breath");
    BOOST_TEST(state.fields.at("SHOFIXTI_STACK2") == 3);
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "i_am_captain0");
    BOOST_TEST(graph->view().speech[1].get<std::string>() == "IS_DEFEAT_TRUE");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "yes_and_no");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "important_duty");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "procreating_wildly");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "rat_babes");
    BOOST_TEST(state.fields.at("SHOFIXTI_RECRUITED") == 1);
    BOOST_TEST(state.fields.at("MAIDENS_ON_SHIP") == 0);
    BOOST_TEST(state.calendarEvents == 1u);
    BOOST_TEST(state.outcome == "peace");
    graph->finishDialogue(graph->view().entry);
    BOOST_TEST(graph->ended());
}
BOOST_AUTO_TEST_CASE(
    shofixti_captain_names_and_duplicate_legacy_insults_keep_stable_response_identity) {
    for (const auto kia : {0, 1}) {
        ProbeState state;
        state.fields["SHOFIXTI_KIA"] = kia;
        state.fields["SHOFIXTI_STACK2"] = 3;
        auto graph = liveGraph("comm.shofixti.dialogue", state);
        graph->advance();
        finishToMenu(*graph);
        const auto response = graph->responses()[0];
        BOOST_TEST(dd::formatText(response.text, [](const std::string &id) { return id; }) ==
                   std::string("report0") + (kia ? "katana" : "tanaka") + "report1");
        graph->choose(graph->view().entry, "why_here0");
        finishToMenu(*graph);
        BOOST_TEST(graph->responses()[0].id == "what_happened");
    }
    ProbeState state;
    state.fields["SHOFIXTI_STACK1"] = 2;
    state.fields["SHOFIXTI_STACK3"] = 2;
    auto graph = liveGraph("comm.shofixti.dialogue", state);
    graph->advance();
    finishToMenu(*graph);
    bool first = false, second = false;
    for (const auto &response : graph->responses()) {
        first |= response.id == "look_you_are";
        second |= response.id == "look_you_are_again";
    }
    BOOST_TEST(first);
    BOOST_TEST(second);
    graph->choose(graph->view().entry, "look_you_are_again");
    BOOST_TEST(state.fields.at("SHOFIXTI_STACK1") == 3);
    BOOST_TEST(state.fields.at("SHOFIXTI_STACK3") == 2);
    BOOST_TEST(state.outcome == "hostile");
}

BOOST_AUTO_TEST_CASE(yehat_farewell_draws_once_on_selection_and_replay_cannot_reroll_it) {
    for (const auto draw : {0, 1, 254, 255}) {
        ProbeState state;
        state.randomValues = {draw};
        auto graph = liveGraph("comm.yehat.dialogue", state);
        graph->advance();
        finishToMenu(*graph);
        BOOST_TEST(state.randomDraws == 0u);
        graph->choose(graph->view().entry, "bye_space");
        const auto speech = graph->view().speech;
        BOOST_TEST(speech[0].get<std::string>() ==
                   (draw & 1 ? "GOODBYE_AND_DIE_SPACE" : "GO_IN_PEACE"));
        BOOST_TEST(state.outcome == (draw & 1 ? "hostile" : "peace"));
        BOOST_TEST(graph->view().speech == speech);
        BOOST_TEST(state.randomDraws == 1u);
        graph->finishDialogue(graph->view().entry);
        BOOST_TEST(graph->ended());
    }
}
BOOST_AUTO_TEST_CASE(yehat_revolt_requires_an_escort_and_resets_rebel_story_before_scheduling) {
    for (const auto escort : {false, true}) {
        ProbeState state;
        state.shofixtiEscort = escort;
        state.fields["SHOFIXTI_VISITS"] = 1;
        auto graph = liveGraph("comm.yehat.dialogue", state);
        graph->advance();
        finishToMenu(*graph);
        graph->choose(graph->view().entry, "shofixti_alive_1");
        BOOST_TEST(state.fields.at("YEHAT_REBEL_TOLD_PKUNK") == 1);
        finishToMenu(*graph);
        BOOST_TEST(graph->responses()[0].id == (escort ? "ok_send" : "not_here"));
        graph->choose(graph->view().entry, escort ? "ok_send" : "not_here");
        if (escort) {
            BOOST_TEST(state.fields.at("YEHAT_CIVIL_WAR") == 1);
            BOOST_TEST(state.fields.at("YEHAT_VISITS") == 0);
            BOOST_TEST(state.fields.at("YEHAT_REBEL_TOLD_PKUNK") == 0);
            BOOST_TEST(state.calendarEvents == 1u);
            BOOST_TEST(state.outcome == "peace");
        } else {
            BOOST_TEST(state.fields.at("YEHAT_REBEL_VISITS") == 1);
            BOOST_TEST(state.calendarEvents == 0u);
            BOOST_TEST(state.outcome == "hostile");
        }
    }
}

BOOST_AUTO_TEST_CASE(pkunk_initial_alliance_gift_waits_for_speech_and_records_the_next_year) {
    for (const auto slots : {0, 12}) {
        ProbeState state;
        state.slots = slots;
        state.context["calendar_day"] = 31;
        state.fields["GLOBAL_FLAGS_AND_DATA"] = 128;
        auto graph = liveGraph("comm.pkunk.dialogue", state);
        graph->advance();
        BOOST_TEST(state.fields.at("CLEAR_SPINDLE_ON_SHIP") == 1);
        BOOST_TEST(graph->view().speech[0][0].get<std::string>() == "GIVE_SPINDLE");
        finishToMenu(*graph);
        graph->choose(graph->view().entry, "we_are_vindicator0");
        finishToMenu(*graph);
        graph->choose(graph->view().entry, "we_need_help");
        BOOST_TEST(state.fields.at("PKUNK_MANNER") == 3);
        BOOST_TEST(state.calendarEvents == 1u);
        BOOST_TEST(state.escorts == 0);
        BOOST_TEST(graph->view().speech[2].get<std::string>() ==
                   (slots ? "INIT_SHIP_GIFT" : "INIT_NO_ROOM"));
        graph->finishDialogue(graph->view().entry);
        BOOST_TEST(graph->ended());
        BOOST_TEST(state.escorts == (slots ? 4 : 0));
        if (slots) {
            BOOST_TEST(state.fields.at("PKUNK_SHIP_DAY") == 28);
            BOOST_TEST(state.fields.at("PKUNK_SHIP_MONTH") == 2);
            BOOST_TEST(state.fields.at("PKUNK_SHIP_YEAR") == 1);
        }
    }
}
BOOST_AUTO_TEST_CASE(
    pkunk_annual_homeworld_gift_occurs_before_playback_and_preserves_visit_counter) {
    ProbeState state;
    state.fields["GLOBAL_FLAGS_AND_DATA"] = 128;
    state.fields["PKUNK_MANNER"] = 3;
    state.fields["PKUNK_HOME_VISITS"] = 2;
    state.fields["CLEAR_SPINDLE"] = 1;
    state.fields["PKUNK_SHIP_MONTH"] = 2;
    state.fields["PKUNK_SHIP_DAY"] = 17;
    state.fields["PKUNK_SHIP_YEAR"] = 1;
    state.context["calendar_campaign_year"] = 1;
    auto graph = liveGraph("comm.pkunk.dialogue", state);
    graph->advance();
    BOOST_TEST(state.escorts == 4);
    BOOST_TEST(state.fields.at("PKUNK_HOME_VISITS") == 2);
    BOOST_TEST(state.fields.at("PKUNK_SHIP_YEAR") == 2);
    BOOST_TEST(graph->view().speech[1].get<std::string>() == "SHIP_GIFT");
}
BOOST_AUTO_TEST_CASE(
    pkunk_migration_reasons_prune_independently_and_only_good_reasons_schedule_return) {
    for (const auto good : {false, true}) {
        ProbeState state;
        state.fields["PKUNK_MISSION"] = 1;
        auto graph = liveGraph("comm.pkunk.dialogue", state);
        graph->advance();
        finishToMenu(*graph);
        graph->choose(graph->view().entry, good ? "good_reason_2" : "bad_reason_1");
        BOOST_TEST(state.fields.at("PKUNK_REASONS") == (good ? 2 : 4));
        BOOST_TEST(state.calendarEvents == (good ? 1u : 0u));
        graph->finishDialogue(graph->view().entry);
        BOOST_TEST(graph->ended());
    }
}

BOOST_AUTO_TEST_CASE(
    slylandro_program_diagnosis_awards_the_code_and_schedules_probe_ramp_down_once) {
    ProbeState state;
    state.fields["PLAYER_KNOWS_PROBE"] = 1;
    state.fields["PROBE_EXHIBITED_BUG"] = 1;
    state.fields["SLYLANDRO_STACK1"] = 2;
    state.fields["SLYLANDRO_STACK5"] = 1;
    state.fields["SLYLANDRO_STACK6"] = 1;
    state.fields["SLYLANDRO_STACK8"] = 2;
    auto graph = liveGraph("comm.slylandro.dialogue", state);
    graph->advance();
    finishToMenu(*graph);
    for (const auto response : {"know_more_probe", "talk_more_probe_attack", "tell_me_about_basics",
                                "tell_me_about_rep_1", "what_set_priority", "what_effect",
                                "probe_has_bug", "tell_me_about_rep_2", "what_about_rep_priorities",
                                "think_about_rep_priorities", "mega_self_destruct"}) {
        graph->choose(graph->view().entry, response);
        finishToMenu(*graph);
    }
    BOOST_TEST(state.fields.at("SLYLANDRO_KNOW_BROKEN") == 1);
    BOOST_TEST(state.fields.at("DESTRUCT_CODE_ON_SHIP") == 1);
    BOOST_TEST(state.fields.at("SLYLANDRO_MULTIPLIER") == 1);
    BOOST_TEST(state.calendarEvents == 1u);
    BOOST_CHECK_THROW(graph->choose(graph->view().entry, "know_more_probe"), std::invalid_argument);
    auto revisit = liveGraph("comm.slylandro.dialogue", state);
    revisit->advance();
    BOOST_TEST(revisit->view().speech[0].get<std::string>() == "RECALL_PROGRAM_1");
    BOOST_TEST(state.fields.at("RECALL_VISITS") == 1);
    BOOST_TEST(state.calendarEvents == 1u);
}
BOOST_AUTO_TEST_CASE(slylandro_human_information_exhaustion_persists_in_story_state) {
    ProbeState state;
    state.fields["SLYLANDRO_STACK1"] = 1;
    auto graph = liveGraph("comm.slylandro.dialogue", state);
    graph->advance();
    finishToMenu(*graph);
    for (const auto response : {"happy_to_tell_more", "we_come_from_earth", "we_explore",
                                "we_fight_urquan", "not_same_urquan", "we_gather"}) {
        graph->choose(graph->view().entry, response);
        finishToMenu(*graph);
    }
    BOOST_TEST(state.fields.at("SLYLANDRO_STACK4") == 2);
    BOOST_TEST(graph->responses().size() == 1u);
    graph->choose(graph->view().entry, "enough_about_me");
    finishToMenu(*graph);
    BOOST_CHECK_THROW(graph->choose(graph->view().entry, "happy_to_tell_more"),
                      std::invalid_argument);
    BOOST_CHECK_THROW(graph->choose(graph->view().entry, "would_you_like_to_know_more"),
                      std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(spathi_fwiffo_recruitment_waits_for_speech_and_preserves_his_single_crew) {
    ProbeState state;
    state.fields["FOUND_PLUTO_SPATHI"] = 1;
    state.fields["SPATHI_HOME_VISITS"] = 7;
    state.fields["KNOW_SPATHI_PASSWORD"] = 1;
    auto graph = liveGraph("comm.spathi.dialogue", state);
    graph->advance();
    BOOST_TEST(state.fields.at("KNOW_SPATHI_PASSWORD") == 0);
    graph->finishDialogue(graph->view().entry);
    graph->choose(graph->view().entry, "hi_there");
    finishToMenu(*graph);
    // Asking too early yields progressively firmer refusals without recruiting.
    for (unsigned i = 1; i <= 3; ++i) {
        graph->choose(graph->view().entry, "join_us_refused");
        BOOST_TEST(graph->view().speech[0].get<std::string>() == "WONT_JOIN_" + std::to_string(i));
        finishToMenu(*graph);
    }
    for (const auto response :
         {"what_doing_on_pluto_1", "what_doing_on_pluto_2", "what_doing_on_pluto_3",
          "what_about_moonbase", "what_about_other_spathi", "how_many_crew", "really_thousands",
          "full_of_monsters"}) {
        graph->choose(graph->view().entry, response);
        finishToMenu(*graph);
    }
    graph->choose(graph->view().entry, "join_us");
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "WILL_JOIN");
    BOOST_TEST(state.escorts == 0);
    graph->finishDialogue(graph->view().entry);
    BOOST_TEST(state.escorts == 1);
    BOOST_TEST(graph->ended());
}
BOOST_AUTO_TEST_CASE(spathi_befriending_exhaustion_returns_to_neutral_without_an_extra_greeting) {
    ProbeState state;
    auto graph = liveGraph("comm.spathi.dialogue", state);
    graph->advance();
    finishToMenu(*graph);
    for (const auto response :
         {"come_in_peace", "looking_for_a_few_good_squids", "why_slaves", "tell_us_about_you",
          "what_you_really_want", "how_about_alliance"}) {
        graph->choose(graph->view().entry, response);
        finishToMenu(*graph);
    }
    BOOST_TEST(state.fields.at("SPATHI_STACK1") == 1);
    BOOST_TEST(state.fields.at("SPATHI_VISITS") == 1);
    BOOST_CHECK_THROW(graph->choose(graph->view().entry, "come_in_peace"), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(
    utwig_restored_ultron_ends_hostility_forms_both_alliances_and_schedules_the_mission) {
    for (const auto frenzy : {0, 1}) {
        ProbeState state;
        state.fields["UTWIG_HOSTILE"] = 1;
        state.fields["SUPOX_HOSTILE"] = 1;
        state.fields["ULTRON_CONDITION"] = 4;
        state.fields["KOHR_AH_FRENZY"] = frenzy;
        auto graph = liveGraph("comm.utwig.dialogue", state);
        graph->advance();
        finishToMenu(*graph);
        graph->choose(graph->view().entry, "hey_wait_got_ultron");
        BOOST_TEST(graph->view().speech[0][0].get<std::string>() == "TAUNT_US_BUT_WE_LOOK");
        BOOST_TEST(graph->view().speech[2].get<std::string>() ==
                   (frenzy ? "TOO_LATE" : "OK_ATTACK_KOHRAH"));
        BOOST_TEST(state.fields.at("UTWIG_HOSTILE") == 0);
        BOOST_TEST(state.fields.at("SUPOX_HOSTILE") == 0);
        BOOST_TEST(state.fields.at("UTWIG_HAVE_ULTRON") == 1);
        BOOST_TEST(state.fields.at("ULTRON_CONDITION") == 5);
        BOOST_TEST(state.calendarEvents == (frenzy ? 0u : 1u));
        BOOST_TEST(state.alliances.size() == 2u);
        BOOST_TEST(state.outcome == "peace");
        graph->finishDialogue(graph->view().entry);
        BOOST_TEST(graph->ended());
    }
}
BOOST_AUTO_TEST_CASE(
    utwig_bomb_guards_refuse_the_ultron_and_neutral_topics_keep_the_source_overwrite) {
    ProbeState state;
    state.fields["ULTRON_CONDITION"] = 4;
    state.fields["GLOBAL_FLAGS_AND_DATA"] = 64;
    auto bomb = liveGraph("comm.utwig.dialogue", state);
    bomb->advance();
    finishToMenu(*bomb);
    bomb->choose(bomb->view().entry, "got_ultron");
    BOOST_TEST(bomb->view().speech[0].get<std::string>() == "NO_ULTRON_AT_BOMB");
    BOOST_TEST(state.fields.at("REFUSED_ULTRON_AT_BOMB") == 1);
    BOOST_TEST(state.fields["UTWIG_HAVE_ULTRON"] == 0);
    BOOST_TEST(state.alliances.empty());
    state.fields["GLOBAL_FLAGS_AND_DATA"] = 0;
    auto neutral = liveGraph("comm.utwig.dialogue", state);
    neutral->advance();
    finishToMenu(*neutral);
    BOOST_CHECK_THROW(neutral->choose(neutral->view().entry, "what_about_you_1"),
                      std::invalid_argument);
    neutral->choose(neutral->view().entry, "what_about_urquan_1");
    finishToMenu(*neutral);
    neutral->choose(neutral->view().entry, "what_about_urquan_2");
    finishToMenu(*neutral);
    neutral->choose(neutral->view().entry, "what_about_you_1");
    BOOST_TEST(state.fields.at("UTWIG_WAR_NEWS") == 1);
}

BOOST_AUTO_TEST_CASE(vux_beast_escape_cue_is_read_only_and_story_consequences_wait_for_playback) {
    ProbeState state;
    state.fields["GLOBAL_FLAGS_AND_DATA"] = 64;
    state.fields["KNOW_ZEX_WANTS_MONSTER"] = 1;
    state.fields["VUX_BEAST_ON_SHIP"] = 1;
    auto graph = liveGraph("comm.vux.dialogue", state);
    graph->advance();
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "i_have_beast");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "ok_take_beast");
    BOOST_TEST(graph->view().speech[1].at("on_start").get<std::string>() == "vux.beast_escape");
    BOOST_TEST(state.fields.at("VUX_BEAST_ON_SHIP") == 1);
    BOOST_TEST(state.fields["ZEX_IS_DEAD"] == 0);
    BOOST_TEST(state.outcome == "hostile");
    const auto speech = graph->view().speech;
    BOOST_TEST(graph->view().speech == speech);
    BOOST_TEST(state.fields["ZEX_IS_DEAD"] == 0);
    graph->finishDialogue(graph->view().entry);
    BOOST_TEST(state.fields.at("VUX_BEAST_ON_SHIP") == 0);
    BOOST_TEST(state.fields.at("ZEX_IS_DEAD") == 1);
    BOOST_TEST(state.outcome == "peace");
    BOOST_TEST(graph->ended());
}
BOOST_AUTO_TEST_CASE(vux_final_apology_unlocks_friendship_pleas_instead_of_repeating_the_truth) {
    ProbeState state;
    state.fields["VUX_STACK_1"] = 13;
    auto graph = liveGraph("comm.vux.dialogue", state);
    graph->advance();
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "apology_10");
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "TRUTH");
    BOOST_TEST(state.fields.at("VUX_STACK_1") == 14);
    auto revisit = liveGraph("comm.vux.dialogue", state);
    revisit->advance();
    finishToMenu(*revisit);
    BOOST_CHECK_THROW(revisit->choose(revisit->view().entry, "apology_10"), std::invalid_argument);
    revisit->choose(revisit->view().entry, "cant_we_be_friends_1");
    BOOST_TEST(state.fields.at("VUX_STACK_3") == 1);
    BOOST_TEST(state.outcome == "hostile");
}

BOOST_AUTO_TEST_CASE(
    talking_pet_onboard_skips_the_shield_complaint_and_keeps_strobe_separate_from_story) {
    ProbeState state;
    state.fields["TALKING_PET_ON_SHIP"] = 1;
    auto graph = liveGraph("comm.talkingpet.dialogue", state);
    graph->advance();
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "HELLO_AS_DEVICE_2");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "about_your_race");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "what_about_physiology");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "what_about_powers");
    BOOST_TEST(graph->view().presentation.at("after").get<std::string>() == "talkpet.strobe");
    graph->finishDialogue(graph->view().entry);
    graph->finishDialogue(graph->view().entry);
    graph->choose(graph->view().entry, "yes_flowers");
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "GOOD_HUMAN");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "wish_to_go_now");
    BOOST_TEST(state.outcome == "peace");
    BOOST_TEST(state.fields.at("SHIP_TO_COMPEL") == 0);
}
BOOST_AUTO_TEST_CASE(talking_pet_recruitment_without_a_shield_hides_the_irrelevant_trust_response) {
    ProbeState state;
    state.umgahAvailable = false;
    auto graph = liveGraph("comm.talkingpet.dialogue", state);
    graph->advance();
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "UMGAH_ALL_GONE");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "what_kind_of_deal");
    finishToMenu(*graph);
    BOOST_CHECK_THROW(graph->choose(graph->view().entry, "how_trust"), std::invalid_argument);
    graph->choose(graph->view().entry, "ok_lets_do_it");
    BOOST_TEST(state.fields.at("TALKING_PET_ON_SHIP") == 1);
    BOOST_TEST(state.fields.at("UMGAH_ZOMBIE_BLOBBIES") == 0);
    BOOST_TEST(state.fields.at("ARILOU_STACK_2") == 0);
    BOOST_TEST(state.reveals == 1u);
}

BOOST_AUTO_TEST_CASE(orz_first_contact_uses_media_cues_without_hiding_story_state_in_them) {
    ProbeState state;
    auto graph = liveGraph("comm.orz.dialogue", state);
    graph->advance();
    BOOST_TEST(state.fields.at("MET_ORZ_BEFORE") == 1);
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "INIT_HELLO");
    BOOST_TEST(graph->view().speech[1][0].at("on_start").get<std::string>() == "orz.talking_on");
    BOOST_TEST(graph->view().presentation.at("before")[0].get<std::string>() == "orz.talking_off");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "we_are_vindicator0");
    finishToMenu(*graph);
    BOOST_TEST(graph->responses()[0].id == "seem_like_nice_guys");
}
BOOST_AUTO_TEST_CASE(orz_androsynth_questions_end_the_alliance_and_remove_escorts_once) {
    ProbeState state;
    state.allied = true;
    state.escorts = 3;
    state.fields["ORZ_MANNER"] = 3;
    state.fields["MET_ORZ_BEFORE"] = 1;
    auto graph = liveGraph("comm.orz.dialogue", state);
    graph->advance();
    finishToMenu(*graph);
    for (const auto response : {"about_andro_1", "about_andro_2"}) {
        graph->choose(graph->view().entry, response);
        finishToMenu(*graph);
    }
    graph->choose(graph->view().entry, "about_andro_3");
    BOOST_TEST(state.fields.at("ORZ_MANNER") == 2);
    BOOST_TEST(state.escorts == 0);
    BOOST_TEST(!state.allied);
    BOOST_TEST(state.outcome == "hostile");
    BOOST_TEST(graph->view().presentation.at("before").get<std::string>() == "orz.anger");
    graph->finishDialogue(graph->view().entry);
    BOOST_TEST(graph->ended());
    BOOST_TEST(state.alliances.size() == 1u);
}

BOOST_AUTO_TEST_CASE(syreen_proof_awards_the_shuttle_and_vault_completion_starts_the_ambush) {
    ProbeState state;
    state.fields["KNOW_ABOUT_SHATTERED"] = 3;
    state.fields["EGG_CASE0_ON_SHIP"] = 1;
    state.fields["SYREEN_HOME_VISITS"] = 1;
    auto graph = liveGraph("comm.syreen.dialogue", state);
    graph->advance();
    finishToMenu(*graph);
    for (const auto response :
         {"have_proof", "look_at_egg_sacks", "whats_next_step", "where_is_it"}) {
        graph->choose(graph->view().entry, response);
        finishToMenu(*graph);
    }
    BOOST_TEST(state.fields.at("SYREEN_KNOW_ABOUT_MYCON") == 1);
    BOOST_TEST(state.fields.at("SYREEN_SHUTTLE_ON_SHIP") == 1);
    graph->choose(graph->view().entry, "im_on_my_way");
    BOOST_TEST(state.outcome == "peace");
    state.fields["SHIP_VAULT_UNLOCKED"] = 1;
    auto vault = liveGraph("comm.syreen.dialogue", state);
    vault->advance();
    BOOST_TEST(state.fields.at("MYCON_AMBUSH") == 1);
    BOOST_TEST(state.fields.at("SYREEN_HOME_VISITS") == 0);
    finishToMenu(*vault);
    vault->choose(vault->view().entry, "whats_my_reward");
    BOOST_TEST(vault->view().speech[1].at("on_start").get<std::string>() == "syreen.lights_off");
    BOOST_TEST(state.fields["PLAYER_HAVING_SEX"] == 0);
    finishToMenu(*vault);
    BOOST_TEST(state.fields.at("PLAYER_HAVING_SEX") == 1);
    vault->choose(vault->view().entry, "hands_off");
    BOOST_TEST(vault->view().speech[2].at("on_start").get<std::string>() == "syreen.lights_on");
    BOOST_TEST(state.fields["PLAYER_HAD_SEX"] == 0);
    vault->finishDialogue(vault->view().entry);
    BOOST_TEST(state.fields.at("PLAYER_HAD_SEX") == 1);
    BOOST_TEST(state.fields.at("PLAYER_HAVING_SEX") == 0);
}

BOOST_AUTO_TEST_CASE(spathi_council_password_from_melnorme_still_requires_authentication) {
    ProbeState state;
    state.fields["KNOW_SPATHI_PASSWORD"] = 1;
    state.fields["SPATHI_HOME_VISITS"] = 7;
    auto graph = liveGraph("comm.safeones.dialogue", state);
    graph->advance();
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "WHAT_IS_PASSWORD");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "good_password");
    BOOST_TEST(state.fields.at("SPATHI_HOME_VISITS") == 0);
    BOOST_TEST(graph->view().presentation.at("before").get<std::string>() ==
               "spahome.council_fade");
    finishToMenu(*graph);
    for (const auto response : {"we_come_in_peace", "hand_in_friendship", "stronger", "yes_we_are",
                                "how_prove", "what_test", "tell_evil"}) {
        graph->choose(graph->view().entry, response);
        finishToMenu(*graph);
    }
    graph->choose(graph->view().entry, "i_accept");
    BOOST_TEST(state.fields.at("SPATHI_QUEST") == 1);
    BOOST_TEST(state.fields.at("SPATHI_MANNER") == 3);
    BOOST_TEST(state.outcome == "peace");
}
BOOST_AUTO_TEST_CASE(spathi_creature_quest_party_leads_to_an_alliance_and_six_month_shield_event) {
    ProbeState state;
    state.fields["SPATHI_QUEST"] = 1;
    state.fields["SPATHI_MANNER"] = 3;
    state.fields["SPATHI_CREATURES_ELIMINATED"] = 1;
    auto graph = liveGraph("comm.safeones.dialogue", state);
    graph->advance();
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "killed_them_all_1");
    BOOST_TEST(state.fields.at("SPATHI_PARTY") == 1);
    auto party = liveGraph("comm.safeones.dialogue", state);
    party->advance();
    finishToMenu(*party);
    for (const auto response : {"deals_a_deal", "how_long", "reneging", "return_beasts"}) {
        party->choose(party->view().entry, response);
        finishToMenu(*party);
    }
    party->choose(party->view().entry, "do_as_we_say");
    BOOST_TEST(state.allied);
    BOOST_TEST(state.calendarEvents == 1u);
    BOOST_TEST(state.fields.at("SPATHI_HOME_VISITS") == 0);
}

BOOST_AUTO_TEST_CASE(chmmr_emergence_clears_only_after_speech_and_analysis_prepares_the_refit) {
    ProbeState state;
    state.fields["CHMMR_EMERGING"] = 1;
    auto graph = liveGraph("comm.chmmr.dialogue", state);
    graph->advance();
    BOOST_TEST(graph->view().speech[0].get<std::string>() == "WE_ARE_FREE");
    BOOST_TEST(state.fields.at("CHMMR_UNLEASHED") == 1);
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "i_am_captain0");
    BOOST_TEST(state.fields.at("CHMMR_EMERGING") == 1);
    finishToMenu(*graph);
    BOOST_TEST(state.fields.at("CHMMR_EMERGING") == 0);
    graph->choose(graph->view().entry, "serious_1");
    BOOST_TEST(state.fields.at("AWARE_OF_SAMATRA") == 1);
    BOOST_TEST(graph->view().speech[1][0].get<std::string>() == "DONT_KNOW_ABOUT_SAMATRA");
    finishToMenu(*graph);
    BOOST_TEST(state.refits == 0u);
    state.fields["TALKING_PET_ON_SHIP"] = 1;
    state.fields["UTWIG_BOMB_ON_SHIP"] = 1;
    auto ready = liveGraph("comm.chmmr.dialogue", state);
    ready->advance();
    finishToMenu(*ready);
    ready->choose(ready->view().entry, "proceed");
    BOOST_TEST(state.refits == 1u);
    BOOST_TEST(state.fields.at("CHMMR_BOMB_STATE") == 2);
    BOOST_TEST(state.fields.at("UTWIG_BOMB_ON_SHIP") == 0);
    BOOST_TEST(state.allied);
    BOOST_TEST(ready->view().speech[0].get<std::string>() == "TAKE_2_WEEKS");
    ready->finishDialogue(ready->view().entry);
    BOOST_TEST(ready->ended());
}

BOOST_AUTO_TEST_CASE(zoqfot_speaker_cues_preserve_alternation_and_alliance_effects_follow_speech) {
    ProbeState state;
    state.fields["GLOBAL_FLAGS_AND_DATA"] = 128;
    auto graph = liveGraph("comm.zoqfotpik.dialogue", state);
    graph->advance();
    BOOST_TEST(graph->view().speech[0].at("on_start").get<std::string>() == "zoqfot.zoq");
    BOOST_TEST(graph->view().speech[1].at("on_start").get<std::string>() == "zoqfot.pik");
    finishToMenu(*graph);
    for (const auto response :
         {"we_are_vindicator0", "your_race", "where_from", "what_emergency", "how_can_i_help"}) {
        graph->choose(graph->view().entry, response);
        finishToMenu(*graph);
    }
    graph->choose(graph->view().entry, "sure");
    BOOST_TEST(!state.allied);
    BOOST_TEST(state.calendarEvents == 0u);
    graph->finishDialogue(graph->view().entry);
    BOOST_TEST(state.allied);
    BOOST_TEST(state.calendarEvents == 1u);
    BOOST_TEST(state.fields.at("ZOQFOT_HOME_VISITS") == 0);
    BOOST_TEST(graph->ended());
}
BOOST_AUTO_TEST_CASE(zoqfot_rescue_and_war_news_commit_after_their_speaker_batches) {
    ProbeState state;
    state.allied = true;
    state.fields["MET_ZOQFOT"] = 1;
    state.fields["ZOQFOT_DISTRESS"] = 1;
    state.fields["UTWIG_SUPOX_MISSION"] = 1;
    auto graph = liveGraph("comm.zoqfotpik.dialogue", state);
    graph->advance();
    BOOST_TEST(state.escorts == 0);
    finishToMenu(*graph);
    BOOST_TEST(state.escorts == 4);
    BOOST_TEST(state.fields.at("ZOQFOT_DISTRESS") == 0);
    graph->choose(graph->view().entry, "any_war_news");
    BOOST_TEST(state.fields["ZOQFOT_KNOW_MASK"] == 0);
    BOOST_TEST(graph->view().speech[0].at("phrase").get<std::string>() == "UTWIG_DELAY0");
    finishToMenu(*graph);
    BOOST_TEST(state.fields.at("ZOQFOT_KNOW_MASK") == 1);
    BOOST_CHECK_THROW(graph->choose(graph->view().entry, "any_war_news"), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(thraddash_culture_customization_selects_the_next_visits_greeting_and_name) {
    ProbeState state;
    state.fields["THRADDASH_BODY_COUNT"] = 25;
    auto graph = liveGraph("comm.thraddash.dialogue", state);
    graph->advance();
    BOOST_TEST(state.allied);
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "speak_pig_latin");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "the_slave_empire0");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "friendly");
    BOOST_TEST(state.fields.at("THRADD_INTRO") == 2);
    BOOST_TEST(state.fields.at("THRADD_CULTURE") == 3);
    BOOST_TEST(state.fields.at("THRADD_DEMEANOR") == 1);
    auto revisit = liveGraph("comm.thraddash.dialogue", state);
    revisit->advance();
    BOOST_TEST(revisit->view().speech[0][0].get<std::string>() == "HELLO_PIG_LATIN_1");
    BOOST_TEST(revisit->view().speech[2].at("phrase").get<std::string>() == "SLAVE_EMPIRE");
}
BOOST_AUTO_TEST_CASE(thraddash_urquan_mission_is_offered_only_before_the_frenzy) {
    for (const auto frenzy : {0, 1}) {
        ProbeState state;
        state.fields["THRADD_STACK_1"] = 4;
        state.fields["KOHR_AH_FRENZY"] = frenzy;
        auto graph = liveGraph("comm.thraddash.dialogue", state);
        graph->advance();
        finishToMenu(*graph);
        if (frenzy) {
            BOOST_TEST(state.fields.at("THRADD_STACK_1") == 5);
            BOOST_CHECK_THROW(graph->choose(graph->view().entry, "got_idea"),
                              std::invalid_argument);
        } else {
            graph->choose(graph->view().entry, "got_idea");
            BOOST_TEST(state.calendarEvents == 1u);
            BOOST_TEST(state.fields.at("THRADD_STACK_1") == 5);
            BOOST_TEST(state.outcome == "peace");
        }
    }
}

BOOST_AUTO_TEST_CASE(commander_emergency_aid_uses_shared_fuel_crew_and_lander_actions) {
    ProbeState state;
    state.fields["STARBASE_VISITED"] = 1;
    state.shipFuel = 1;
    state.crew = 0;
    state.landers = 0;
    auto graph = liveGraph("comm.commander.dialogue", state);
    graph->advance();
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "i_lost_my_lander");
    BOOST_TEST(state.landers == 1);
    BOOST_TEST(state.crew == 4);
    BOOST_TEST(state.fields.at("LANDERS_LOST") == 1);
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "need_fuel_mercury");
    BOOST_TEST(state.shipFuel == 6);
    BOOST_TEST(state.fields.at("GIVEN_FUEL_BEFORE") == 1);
}
BOOST_AUTO_TEST_CASE(commander_radio_delivery_and_alliance_naming_drive_the_live_graph) {
    ProbeState state;
    state.radioactive = 1;
    auto graph = liveGraph("comm.commander.dialogue", state);
    graph->advance();
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "yes_this_is_supply_ship");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "we_will_transfer_now");
    BOOST_TEST(state.fields.at("RADIOACTIVES_PROVIDED") == 1);
    BOOST_TEST(graph->view().speech[1].at("on_start").get<std::string>() ==
               "commander.restore_power");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "we_are_vindicator0");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "our_mission_was_secret");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "we_are_here_to_help");
    finishToMenu(*graph);
    graph->choose(graph->view().entry, "we_will_take_care_of_base");
    BOOST_TEST(state.fields.at("WILL_DESTROY_BASE") == 1);
    state.fields["PROBE_ILWRATH_ENCOUNTER"] = 1;
    auto victory = liveGraph("comm.commander.dialogue", state);
    victory->advance();
    victory->finishDialogue(victory->view().entry);
    victory->choose(victory->view().entry, "cook_their_butts");
    BOOST_TEST(state.fields.at("STARBASE_AVAILABLE") == 1);
    finishToMenu(*victory);
    victory->choose(victory->view().entry, "name_40");
    BOOST_TEST(state.fields.at("NEW_ALLIANCE_NAME") == 3);
    victory->finishDialogue(victory->view().entry);
    BOOST_TEST(victory->ended());
}
