#include "uqm/dialogue/legacy_turn.hpp"
#include <boost/test/unit_test.hpp>
#include <filesystem>
#include <fstream>
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
        if (data.at("_migration").at("status") != "native_handlers")
            continue;
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
    const auto data = read(std::filesystem::path(LIVE_DIALOGUE_ROOT) / "comm.chmmr.dialogue.json");
    dd::LegacyTurnState failure{"chmmr", [&](std::optional<std::size_t>) -> dd::Result {
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
    unsigned coordinates = 0, fades = 0, calendarEvents = 0;
    dd::Json context = {{"won_last_battle", false},
                        {"at_samatra", false},
                        {"at_probe_message", false},
                        {"at_colony", false}};
    dd::Json read(const std::string &scope, const std::string &key) const override {
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
        if (name != "ARILOU_UMGAH_CHECK" || months != 0 || days != 10 || years != 0)
            throw std::invalid_argument("unexpected calendar action");
        ++calendarEvents;
        return {};
    }
    dd::Result prepareReport(const std::string &name, const dd::Json &) override {
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
