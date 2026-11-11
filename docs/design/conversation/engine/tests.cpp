#define BOOST_TEST_MODULE conversation_design
#include "engine.hpp"
#include <boost/test/unit_test.hpp>
#include <fstream>
#include <limits>
namespace cd = uqm::design::conversation;
using cd::Json;
struct TestState : cd::State {
    std::map<std::string, Json> values;
    unsigned mutations = 0, names = 0, purchases = 0;
    cd::Integer onboard = 0, capacity = 100, credits = 100;
    cd::Json read(const std::string &scope, const std::string &key) const override {
        auto it = values.find(scope + "." + key);
        return it == values.end() ? Json() : it->second;
    }
    cd::Result set(const std::string &scope, const std::string &key, const Json &value) override {
        ++mutations;
        values[scope + "." + key] = value;
        return {};
    }
    cd::Integer fuel() const override {
        return onboard;
    }
    cd::Integer fuelCapacity() const override {
        return capacity;
    }
    cd::Integer balance(const std::string &) const override {
        return credits;
    }
    bool marketAvailable(const std::string &, const std::string &) const override {
        return true;
    }
    bool reportAvailable(const std::string &) const override {
        return false;
    }
    bool hasItem(const std::string &) const override {
        return false;
    }
    cd::Integer quantity(const std::string &) const override {
        return 0;
    }
    Json encounter(const std::string &) const override {
        return "starbase";
    }
    std::string nextCaptainName(const std::string &) override {
        ++names;
        return "Generated";
    }
    cd::Integer maximumCrew(const std::string &) const override {
        return 30;
    }
    cd::Result createShip(const std::string &type, const std::string &captain,
                          cd::Integer crew) override {
        ++mutations;
        return {"ok", {{"type", type}, {"captain", captain}, {"crew", crew}}};
    }
    cd::Result prepareReport(const std::string &, const Json &) override {
        return {"ok", {{"speech", Json::array()}}};
    }
    cd::Result purchase(const std::string &, const std::string &, const Json &quantity) override {
        ++purchases;
        auto n = quantity.is_string() ? capacity - onboard : quantity.get<cd::Integer>();
        if (n > capacity - onboard)
            return {"no_room"};
        if (n > credits)
            return {"no_credit", {{"missing", n - credits}}};
        onboard += n;
        credits -= n;
        return {};
    }
};
Json readExample(const std::string &name) {
    std::ifstream f{std::string(DESIGN_ROOT) + "/examples/" + name + ".json"};
    Json j;
    f >> j;
    return j;
}
Json minimal(Json nodes, const std::string &entry = "a") {
    return {{"version", 1}, {"entry", entry}, {"nodes", std::move(nodes)}};
}
BOOST_AUTO_TEST_CASE(binding_defers_side_effects_and_captain_defaults_until_execution) {
    TestState state;
    cd::Memory memory;
    cd::VerbFactory factory{state, memory};
    auto ordinary = factory.action({{"verb", "fleet.give"}, {"args", {"spathi"}}});
    auto fwiffo = factory.action({{"verb", "fleet.give"}, {"args", {"spathi", "Fwiffo", 1}}});
    BOOST_TEST(state.names == 0u);
    BOOST_TEST(state.mutations == 0u);
    auto gift = std::invoke(ordinary);
    BOOST_TEST(state.names == 1u);
    BOOST_TEST(gift.values.at("crew").get<int>() == 30);
    auto special = std::invoke(fwiffo);
    BOOST_TEST(state.names == 1u);
    BOOST_TEST(special.values.at("captain").get<std::string>() == "Fwiffo");
    BOOST_TEST(special.values.at("crew").get<int>() == 1);
}
BOOST_AUTO_TEST_CASE(conditions_read_live_state_and_short_circuit) {
    TestState state;
    cd::Memory memory;
    cd::VerbFactory factory{state, memory};
    auto condition = factory.condition(Json::parse(
        R"({"gt":[{"query":"ship.fuel"},{"subtract":[{"query":"ship.fuel_capacity"},50]}]})"));
    BOOST_TEST(!std::invoke(condition));
    state.onboard = 75;
    BOOST_TEST(std::invoke(condition));
    auto shortCircuit =
        factory.condition(Json::parse(R"({"all":[false,{"get":["result","missing.code"]}]})"));
    BOOST_TEST(!std::invoke(shortCircuit));
    BOOST_TEST(state.mutations == 0u);
}
BOOST_AUTO_TEST_CASE(actions_execute_once_and_stale_media_callbacks_cannot_advance_new_dialogue) {
    TestState state;
    auto json = minimal(Json::parse(R"([
      {"id":"a","kind":"action","action":{"verb":"state.add","args":["story","visits",1]},"save_as":"change","next":"hello"},
      {"id":"hello","kind":"dialogue","variants":[{"speech":["HELLO"]}],"next":"bye"},
      {"id":"bye","kind":"dialogue","variants":[{"speech":["BYE"]}],"next":"end"},
      {"id":"end","kind":"end","outcome":"peace"}])"));
    auto conversation = cd::Conversation::load(json, state);
    BOOST_TEST(state.mutations == 0u);
    conversation->advance();
    auto token = conversation->view().entry;
    conversation->advance();
    conversation->view();
    BOOST_TEST(state.mutations == 1u);
    conversation->finishDialogue(token);
    BOOST_TEST(conversation->view().speech.at(0).get<std::string>() == "BYE");
    BOOST_CHECK_THROW(conversation->finishDialogue(token), std::invalid_argument);
    conversation->finishDialogue(conversation->view().entry);
    BOOST_TEST(conversation->ended());
    BOOST_TEST(state.mutations == 1u);
}
BOOST_AUTO_TEST_CASE(failure_results_branch_without_reexecuting_actions) {
    TestState state;
    state.credits = 0;
    auto json = minimal(Json::parse(R"([
      {"id":"a","kind":"action","action":{"verb":"trade.purchase","args":["melnorme","fuel",10]},"save_as":"purchase","next":"check"},
      {"id":"check","kind":"decision","branches":[{"when":{"eq":[{"get":["result","purchase.code"]},"no_credit"]},"next":"failure"}],"otherwise":"end"},
      {"id":"failure","kind":"dialogue","variants":[{"speech":["NEED_CREDIT"]}],"next":"end"},
      {"id":"end","kind":"end","outcome":"peace"}])"));
    auto c = cd::Conversation::load(json, state);
    c->advance();
    BOOST_TEST(state.purchases == 1u);
    BOOST_TEST(c->view().speech.at(0).get<std::string>() == "NEED_CREDIT");
    c->advance();
    BOOST_TEST(state.purchases == 1u);
}
BOOST_AUTO_TEST_CASE(invalid_graphs_fail_before_actions_and_silent_cycles_are_bounded) {
    TestState state;
    auto json = minimal(Json::parse(
        R"([{"id":"a","kind":"action","action":{"verb":"state.set","args":["story","x",true]},"save_as":"x","next":"missing"}])"));
    BOOST_CHECK_THROW(cd::Conversation::load(json, state), std::invalid_argument);
    BOOST_TEST(state.mutations == 0u);
    auto loop = minimal(Json::parse(
        R"([{"id":"a","kind":"decision","branches":[{"when":true,"next":"a"}],"otherwise":"a"}])"));
    auto c = cd::Conversation::load(loop, state);
    BOOST_CHECK_THROW(c->advance(10), std::runtime_error);
    BOOST_CHECK_THROW(c->advance(), std::logic_error);
}
BOOST_AUTO_TEST_CASE(arithmetic_rejects_overflow_and_invalid_operations) {
    TestState state;
    cd::Memory memory;
    cd::VerbFactory factory{state, memory};
    auto overflow = factory.value({{"add", {std::numeric_limits<cd::Integer>::max(), 1}}});
    BOOST_CHECK_THROW(std::invoke(overflow), std::overflow_error);
    auto divide = factory.value({{"divide", {1, 0}}});
    BOOST_CHECK_THROW(std::invoke(divide), std::overflow_error);
    auto underflow = factory.value({{"subtract", {10, 50}}});
    BOOST_TEST(std::invoke(underflow).get<int>() == -40);
    BOOST_CHECK_THROW(factory.action({{"verb", "DeltaCredit"}, {"args", {-75}}}),
                      std::invalid_argument);
    BOOST_CHECK_THROW(factory.condition({{"verb", "random.draw"}, {"args", {8}}}),
                      std::invalid_argument);
}
BOOST_AUTO_TEST_CASE(examples_load_without_mutating_state_and_offer_fuel_failure_branches) {
    for (const auto &name : {"commander_hayes", "melnorme"}) {
        TestState state;
        auto definition = readExample(name);
        auto c = cd::Conversation::load(definition, state);
        BOOST_TEST(c->nodeCount() > 100u);
        BOOST_TEST(state.mutations == 0u);
        BOOST_TEST(state.purchases == 0u);
    }
    for (const auto &failure : {"no_credit", "no_room", "ok"}) {
        TestState state;
        if (std::string(failure) == "no_credit")
            state.credits = 0;
        if (std::string(failure) == "no_room")
            state.onboard = 95;
        auto definition = readExample("melnorme");
        definition["entry"] = "fuel.ask";
        auto c = cd::Conversation::load(definition, state);
        c->advance();
        const auto entry = c->view().entry;
        c->finishDialogue(entry);
        BOOST_TEST(c->responses().size() == 6u); // All quantities stay selectable.
        c->choose(entry, "buy_10_fuel");
        BOOST_TEST(c->memory().results.at("fuel_purchase").code == failure);
        BOOST_TEST(state.purchases == 1u);
        c->advance();
        BOOST_TEST(state.purchases == 1u);
        BOOST_CHECK_THROW(c->choose(entry, "buy_10_fuel"), std::invalid_argument);
    }
}
BOOST_AUTO_TEST_CASE(hayes_first_contact_marks_the_visit_and_gates_speech_completion) {
    TestState state;
    state.values["story.STARBASE_VISITED"] = false;
    // Select the first-contact entry without requiring a live EncounterMode.
    auto definition = readExample("commander_hayes");
    definition["entry"] = "first.entry";
    auto c = cd::Conversation::load(definition, state);
    c->advance();
    BOOST_TEST(state.values.at("story.STARBASE_VISITED").get<bool>());
    BOOST_TEST(c->view().speech.at(0).get<std::string>() == "commander.ARE_YOU_SUPPLY_SHIP");
    const auto token = c->view().entry;
    BOOST_CHECK_THROW(c->responses(), std::logic_error);
    c->finishDialogue(token);
    BOOST_TEST(c->responses().size() == 3u);
    c->choose(token, "yes_this_is_supply_ship");
    BOOST_TEST(c->view().speech.at(0).get<std::string>() == "commander.ABOUT_TIME");
    c->finishDialogue(c->view().entry);
    c->finishDialogue(c->view().entry); // Empty supply-response menu becomes interactive.
    auto responses = c->responses();
    BOOST_TEST(responses.size() == 2u);
    BOOST_TEST(responses.front().id == "i_lied");
}
BOOST_AUTO_TEST_CASE(failed_action_exceptions_cannot_be_retried_after_partial_mutation) {
    struct Broken : TestState {
        cd::Result set(const std::string &, const std::string &, const Json &) override {
            ++mutations;
            throw std::runtime_error("storage failure");
        }
    } state;
    auto definition = minimal(Json::parse(R"([
      {"id":"a","kind":"action","action":{"verb":"state.set","args":["story","x",true]},"save_as":"x","next":"end"},
      {"id":"end","kind":"end","outcome":"peace"}])"));
    auto c = cd::Conversation::load(definition, state);
    BOOST_CHECK_THROW(c->advance(), std::runtime_error);
    BOOST_TEST(state.mutations == 1u);
    BOOST_CHECK_THROW(c->advance(), std::logic_error);
    BOOST_TEST(state.mutations == 1u);
}
BOOST_AUTO_TEST_CASE(registry_documents_every_factory_verb_and_builds_it_without_invocation) {
    TestState state;
    cd::Memory memory;
    cd::VerbFactory factory{state, memory};
    std::ifstream input{std::string(DESIGN_ROOT) + "/verbs.json"};
    Json registry;
    input >> registry;
    std::vector<std::string> queries, actions;
    const auto argument = [](const std::string &name) -> Json {
        if (name == "scope")
            return "local";
        if (name == "items")
            return Json::array({"LINE"});
        if (name == "repeat_last_or_cycle")
            return "repeat_last";
        if (name == "set_or_clear")
            return "set";
        if (name == "inclusive")
            return false;
        if (name == "months" || name == "days" || name == "years")
            return 0;
        if (name == "mask" || name == "value" || name == "signed_delta" || name == "count" ||
            name == "crew" || name == "signed_quantity" || name == "signed_amount" ||
            name == "quantity_or_fill" || name == "quantity_or_all" ||
            name == "exclusive_upper_bound")
            return 1;
        if (name == "parameters" || name == "configuration")
            return Json{{"literal", Json::object()}};
        return "example";
    };
    for (const auto &spec : registry.at("queries")) {
        auto name = spec.at("verb").get<std::string>();
        queries.push_back(name);
        Json args = Json::array();
        for (std::size_t i = 0; i < spec.at("min_args").get<std::size_t>(); ++i)
            args.push_back(argument(spec.at("arguments").at(i).get<std::string>()));
        BOOST_CHECK_NO_THROW(factory.value({{"query", name}, {"args", args}}));
    }
    for (const auto &spec : registry.at("actions")) {
        auto name = spec.at("verb").get<std::string>();
        actions.push_back(name);
        Json args = Json::array();
        for (std::size_t i = 0; i < spec.at("min_args").get<std::size_t>(); ++i)
            args.push_back(argument(spec.at("arguments").at(i).get<std::string>()));
        BOOST_CHECK_NO_THROW(factory.action({{"verb", name}, {"args", args}}));
    }
    BOOST_TEST(queries == cd::VerbFactory::queryNames(), boost::test_tools::per_element());
    BOOST_TEST(actions == cd::VerbFactory::actionNames(), boost::test_tools::per_element());
    BOOST_TEST(state.mutations == 0u);
    BOOST_TEST(state.names == 0u);
    BOOST_TEST(state.purchases == 0u);
}
BOOST_AUTO_TEST_CASE(ordered_variants_are_selected_once_while_response_conditions_remain_live) {
    TestState state;
    state.values["story.hostile"] = false;
    auto definition = minimal(Json::parse(R"([
      {"id":"a","kind":"dialogue","variants":[{"when":{"get":["story","hostile"]},"speech":["THREAT"]},{"speech":["HELLO"]}],
       "responses":[{"id":"peace","text":"Talk","when":{"not":{"get":["story","hostile"]}},"next":"end"},{"id":"fight","text":"Fight","when":{"get":["story","hostile"]},"next":"end"}]},
      {"id":"end","kind":"end","outcome":"peace"}])"));
    auto c = cd::Conversation::load(definition, state);
    c->advance();
    auto token = c->view().entry;
    state.values["story.hostile"] = true;
    c->advance();
    BOOST_TEST(c->view().speech.at(0).get<std::string>() == "HELLO");
    c->finishDialogue(token);
    BOOST_TEST(c->responses().at(0).id == "fight");
    BOOST_CHECK_THROW(c->choose(token, "peace"), std::invalid_argument);
    c->choose(token, "fight");
    BOOST_TEST(c->ended());
}
BOOST_AUTO_TEST_CASE(repeated_responses_remain_available_and_history_only_changes_styling) {
    TestState state;
    auto definition = minimal(Json::parse(R"([
      {"id":"a","kind":"dialogue","variants":[{"speech":["TOPIC"]}],"responses":[{"id":"again","text":"Again","next":"a"},{"id":"bye","text":"Bye","next":"end"}]},
      {"id":"end","kind":"end","outcome":"peace"}])"));
    auto c = cd::Conversation::load(definition, state);
    c->advance();
    auto token = c->view().entry;
    c->finishDialogue(token);
    BOOST_TEST(!c->responses().at(0).selectedBefore);
    c->choose(token, "again");
    c->finishDialogue(c->view().entry);
    BOOST_TEST(c->responses().size() == 2u);
    BOOST_TEST(c->responses().at(0).selectedBefore);
    BOOST_TEST(!c->responses().at(1).selectedBefore);
    BOOST_TEST(state.mutations == 0u);
}
BOOST_AUTO_TEST_CASE(signed_multiplication_and_offset_sequences_preserve_their_contracts) {
    TestState state;
    cd::Memory memory;
    cd::VerbFactory factory{state, memory};
    for (const auto &pair : std::vector<std::pair<cd::Integer, cd::Integer>>{
             {-2, -3},
             {-2, 3},
             {2, -3},
             {0, std::numeric_limits<cd::Integer>::min()},
             {std::numeric_limits<cd::Integer>::min(), 1}}) {
        auto product = factory.value({{"multiply", {pair.first, pair.second}}});
        BOOST_TEST(std::invoke(product).get<cd::Integer>() == pair.first * pair.second);
    }
    auto overflow = factory.value({{"multiply", {std::numeric_limits<cd::Integer>::min(), -1}}});
    BOOST_CHECK_THROW(std::invoke(overflow), std::overflow_error);
    state.values["story.cursor"] = 5;
    auto sequence = factory.action(
        {{"verb", "sequence.next"},
         {"args",
          {"story", "cursor", Json::array({Json::array({"FIRST"}), Json::array({"LAST"})}),
           "repeat_last", 5}}});
    BOOST_TEST(std::invoke(sequence).values.at("speech").at(0).get<std::string>() == "FIRST");
    BOOST_TEST(state.values.at("story.cursor").get<int>() == 6);
    BOOST_TEST(std::invoke(sequence).values.at("speech").at(0).get<std::string>() == "LAST");
    BOOST_TEST(std::invoke(sequence).values.at("speech").at(0).get<std::string>() == "LAST");
    BOOST_TEST(state.values.at("story.cursor").get<int>() == 6);
}
BOOST_AUTO_TEST_CASE(removal_and_item_overloads_supply_defaults_only_when_invoked) {
    struct Gifts : TestState {
        cd::Integer removed = -1, transferred = 0;
        cd::Integer shipCount(const std::string &) const override {
            return 3;
        }
        cd::Result removeShips(const std::string &, cd::Integer count) override {
            removed = count;
            return {"ok", {{"count", count}}};
        }
        cd::Result transferItem(const std::string &, cd::Integer count) override {
            transferred += count;
            return {};
        }
    } state;
    cd::Memory memory;
    cd::VerbFactory factory{state, memory};
    auto remove = factory.action({{"verb", "fleet.take"}, {"args", {"orz"}}});
    auto give = factory.action({{"verb", "inventory.transfer"}, {"args", {"portal_spawner"}}});
    BOOST_TEST(state.removed == -1);
    BOOST_TEST(state.transferred == 0);
    std::invoke(remove);
    std::invoke(give);
    BOOST_TEST(state.removed == 3);
    BOOST_TEST(state.transferred == 1);
    auto none = factory.action({{"verb", "fleet.take"}, {"args", {"orz", 0}}});
    std::invoke(none);
    BOOST_TEST(state.removed == 0);
}
BOOST_AUTO_TEST_CASE(text_insertions_are_bound_read_only_and_resolved_without_presenter_state) {
    TestState state;
    cd::Memory memory;
    cd::VerbFactory factory{state, memory};
    state.values["story.captain"] = "Zelnick";
    state.values["story.credits"] = 25;
    auto speech = factory.speech(Json::parse(
        R"([{"compose":[{"phrase":"WELCOME"},{"insert":{"get":["story","captain"]}},{"number":{"get":["story","credits"]}}]}])"));
    auto resolved = std::invoke(speech);
    BOOST_TEST(resolved[0]["compose"][1]["insert"].get<std::string>() == "Zelnick");
    BOOST_TEST(resolved[0]["compose"][2]["number"].get<int>() == 25);
    state.values["story.credits"] = 50;
    BOOST_TEST(resolved[0]["compose"][2]["number"].get<int>() == 25);
    BOOST_TEST(std::invoke(speech)[0]["compose"][2]["number"].get<int>() == 50);
    BOOST_TEST(state.mutations == 0u);
}
BOOST_AUTO_TEST_CASE(const_query_binding_observes_updates_from_separately_bound_actions) {
    TestState state;
    cd::Memory memory;
    state.values["story.credits"] = 25;
    memory.local["asked"] = false;
    const cd::State &readState = state;
    const cd::Memory &readMemory = memory;
    cd::QueryFactory queries{readState, readMemory};
    auto credits = queries.value({{"get", {"story", "credits"}}});
    auto asked = queries.condition({{"get", {"local", "asked"}}});
    cd::VerbFactory actions{state, memory};
    auto update = actions.action({{"verb", "state.set"}, {"args", {"story", "credits", 50}}});
    auto mark = actions.action({{"verb", "state.set"}, {"args", {"local", "asked", true}}});
    BOOST_TEST(std::invoke(credits).get<int>() == 25);
    BOOST_TEST(!std::invoke(asked));
    BOOST_TEST(state.mutations == 0u);
    std::invoke(update);
    std::invoke(mark);
    BOOST_TEST(std::invoke(credits).get<int>() == 50);
    BOOST_TEST(std::invoke(asked));
    BOOST_TEST(state.mutations == 1u);
}
BOOST_AUTO_TEST_CASE(specialized_handler_namespace_preserves_query_action_separation) {
    struct Reports : TestState {
        bool reportAvailable(const std::string &report) const override {
            return report == "hayes.devices";
        }
        cd::Result prepareReport(const std::string &report, const Json &parameters) override {
            ++mutations;
            return {"ok", {{"report", report}, {"parameters", parameters}}};
        }
    } state;
    cd::Memory memory;
    cd::QueryFactory queries{static_cast<const cd::State &>(state), memory};
    cd::VerbFactory factory{state, memory};
    auto available =
        queries.condition({{"query", "handlers.report_available"}, {"args", {"hayes.devices"}}});
    auto prepare = factory.action({{"verb", "handlers.prepare_report"},
                                   {"args", {"hayes.devices", {{"literal", {{"detail", true}}}}}}});
    BOOST_TEST(state.mutations == 0u);
    BOOST_TEST(std::invoke(available));
    const auto result = std::invoke(prepare);
    BOOST_TEST(result.values.at("report").get<std::string>() == "hayes.devices");
    BOOST_TEST(result.values.at("parameters").at("detail").get<bool>());
    BOOST_TEST(state.mutations == 1u);
    BOOST_CHECK_THROW(
        queries.value({{"query", "handlers.prepare_report"}, {"args", Json::array()}}),
        std::invalid_argument);
    BOOST_CHECK_THROW(
        factory.action({{"verb", "handlers.report_available"}, {"args", {"hayes.devices"}}}),
        std::invalid_argument);
    BOOST_CHECK_THROW(queries.value({{"query", "handlers.unknown"}, {"args", Json::array()}}),
                      std::invalid_argument);
}
namespace {
Json compactPurchase() {
    auto definition = minimal(Json::parse(R"([
      {"id":"menu","kind":"dialogue","variants":[{"speech":["FUEL"]}],"responses":[
        {"id":"buy","text":"BUY_FIVE","when":{"get":["local","allowed",true]},
         "action":{"verb":"trade.purchase","args":["melnorme","fuel",5]},
         "then":{"ok":"success","no_credit":"failure"}}]},
      {"id":"success","kind":"dialogue","variants":[{"speech":["GOT_FUEL"]}],"next":"end"},
      {"id":"failure","kind":"dialogue","variants":[{"speech":["NEED_CREDIT"]}],"next":"end"},
      {"id":"end","kind":"end","outcome":"peace"}])"));
    definition["entry"] = "menu";
    return definition;
}
} // namespace
BOOST_AUTO_TEST_CASE(compact_responses_expand_without_effects_and_execute_once_after_selection) {
    for (const auto credit : {0, 100}) {
        TestState state;
        state.credits = credit;
        auto c = cd::Conversation::load(compactPurchase(), state);
        BOOST_TEST(c->nodeCount() == 6u); // Four authored nodes, two ordinary generated nodes.
        BOOST_TEST(state.purchases == 0u);
        c->advance();
        auto entry = c->view().entry;
        c->finishDialogue(entry);
        BOOST_TEST(c->responses().size() == 1u);
        BOOST_TEST(c->responses().size() == 1u);
        BOOST_TEST(state.purchases == 0u);
        c->choose(entry, "buy");
        BOOST_TEST(state.purchases == 1u);
        BOOST_TEST(c->view().speech[0].get<std::string>() == (credit ? "GOT_FUEL" : "NEED_CREDIT"));
        c->advance();
        BOOST_TEST(state.purchases == 1u);
        BOOST_CHECK_THROW(c->choose(entry, "buy"), std::invalid_argument);
    }
}
BOOST_AUTO_TEST_CASE(
    compact_responses_validate_destinations_and_reject_ambiguous_fields_before_effects) {
    TestState state;
    auto bad = compactPurchase();
    bad["nodes"][0]["responses"][0]["then"]["ok"] = "missing";
    BOOST_CHECK_THROW(cd::Conversation::load(bad, state), std::invalid_argument);
    bad = compactPurchase();
    bad["nodes"][0]["responses"][0]["next"] = "end";
    BOOST_CHECK_THROW(cd::Conversation::load(bad, state), std::invalid_argument);
    bad = compactPurchase();
    bad["nodes"][0]["responses"][0]["then"] = Json::object();
    BOOST_CHECK_THROW(cd::Conversation::load(bad, state), std::invalid_argument);
    BOOST_TEST(state.purchases == 0u);
}
BOOST_AUTO_TEST_CASE(compact_responses_handle_explicit_fallback_or_poison_on_unhandled_results) {
    struct Unexpected : TestState {
        cd::Result purchase(const std::string &, const std::string &, const Json &) override {
            ++purchases;
            return {"unexpected", Json::object()};
        }
    } state;
    auto definition = compactPurchase();
    auto c = cd::Conversation::load(definition, state);
    c->advance();
    auto entry = c->view().entry;
    c->finishDialogue(entry);
    BOOST_CHECK_THROW(c->choose(entry, "buy"), std::runtime_error);
    BOOST_CHECK_THROW(c->advance(), std::logic_error);
    BOOST_TEST(state.purchases == 1u);
    definition["nodes"][0]["responses"][0]["otherwise"] = "failure";
    definition["nodes"][0]["responses"][0]["save_as"] = "purchase";
    c = cd::Conversation::load(definition, state);
    c->advance();
    entry = c->view().entry;
    c->finishDialogue(entry);
    c->choose(entry, "buy");
    BOOST_TEST(c->view().speech[0].get<std::string>() == "NEED_CREDIT");
    BOOST_TEST(c->memory().results.at("purchase").code == "unexpected");
    BOOST_TEST(state.purchases == 2u);
}
BOOST_AUTO_TEST_CASE(graph_cursors_follow_direct_connections_without_executing_nodes) {
    TestState state;
    auto c = cd::Conversation::load(compactPurchase(), state);
    const auto &graph = c->graph();
    BOOST_TEST(!graph.empty());
    BOOST_TEST(graph.size() == 6u);
    BOOST_CHECK(graph.begin() == graph.start());
    BOOST_CHECK(c->current() == graph.start());
    auto cursor = graph.start();
    const auto &dialogue = std::get<cd::DialogueNode>(cursor->data);
    cursor.next(dialogue.responses.front());
    BOOST_CHECK(std::holds_alternative<cd::ActionNode>((*cursor).data));
    const auto action = cursor++;
    BOOST_CHECK(std::holds_alternative<cd::ActionNode>(action->data));
    BOOST_CHECK(std::holds_alternative<cd::DecisionNode>(cursor->data));
    const auto decision = cursor;
    BOOST_CHECK_THROW(++cursor, std::logic_error); // Branching is explicit.
    BOOST_CHECK(cursor == decision);
    cursor.next(std::get<cd::DecisionNode>(cursor->data).branches.front());
    BOOST_CHECK(std::holds_alternative<cd::DialogueNode>(cursor->data));
    ++cursor; // Follow automatic dialogue continuation.
    BOOST_CHECK(std::holds_alternative<cd::EndNode>(cursor->data));
    ++cursor;
    BOOST_CHECK(cursor == graph.end());
    BOOST_CHECK_THROW(*cursor, std::logic_error);
    BOOST_CHECK_THROW(++cursor, std::logic_error);
    BOOST_CHECK(c->current() == graph.start()); // Cursor copies are independent.
    BOOST_TEST(state.purchases == 0u);          // Moving a cursor never executes an operation.
}
BOOST_AUTO_TEST_CASE(graph_end_is_completion_and_cursors_reject_foreign_connections) {
    cd::ConversationGraph empty;
    BOOST_TEST(empty.empty());
    BOOST_TEST(empty.size() == 0u);
    BOOST_CHECK(empty.begin() == empty.end());
    TestState state;
    auto c = cd::Conversation::load(compactPurchase(), state);
    auto other = cd::Conversation::load(compactPurchase(), state);
    auto cursor = c->graph().start();
    BOOST_CHECK_THROW(cursor.next(other->graph().start()), std::invalid_argument);
    BOOST_CHECK(cursor == c->graph().start());
    BOOST_CHECK_THROW(++cursor, std::logic_error); // Menu requires a response.
    auto definition = minimal(Json::parse(R"([{"id":"a","kind":"end","outcome":"peace"}])"));
    auto terminal = cd::Conversation::load(definition, state);
    BOOST_TEST(!terminal->ended());
    terminal->advance();
    BOOST_TEST(terminal->ended());
    BOOST_CHECK(terminal->current() == terminal->graph().end());
    BOOST_TEST(terminal->outcome() == "peace");
    terminal->advance();
    BOOST_CHECK(terminal->current() == terminal->graph().end());
}
BOOST_AUTO_TEST_CASE(
    graph_cycle_keeps_response_history_on_the_response_and_new_dialogue_generation) {
    TestState state;
    auto definition = minimal(Json::parse(R"([
      {"id":"a","kind":"dialogue","variants":[{"speech":["AGAIN"]}],"responses":[
        {"id":"repeat","text":"REPEAT","next":"a"},
        {"id":"done","text":"DONE","next":"end"}]},
      {"id":"end","kind":"end","outcome":"peace"}])"));
    auto c = cd::Conversation::load(definition, state);
    c->advance();
    const auto entry = c->view().entry;
    c->finishDialogue(entry);
    const auto node = c->current();
    BOOST_TEST(!c->responses()[0].selectedBefore);
    c->choose(entry, "repeat");
    BOOST_CHECK(c->current() == node);
    BOOST_TEST(c->view().entry > entry);
    BOOST_CHECK_THROW(c->finishDialogue(entry), std::invalid_argument);
    c->finishDialogue(c->view().entry);
    BOOST_TEST(c->responses()[0].selectedBefore);
    BOOST_TEST(!c->responses()[1].selectedBefore);
    c->choose(c->view().entry, "done");
    BOOST_CHECK(c->current() == c->graph().end());
}
BOOST_AUTO_TEST_CASE(contiguous_nodes_support_direct_pointer_links_merges_and_large_cycles) {
    TestState state;
    Json nodes = Json::array();
    constexpr std::size_t count = 10000;
    for (std::size_t i = 0; i < count; ++i) {
        const auto id = "node" + std::to_string(i);
        const auto next = "node" + std::to_string((i + 1) % count);
        nodes.push_back({{"id", id},
                         {"kind", "decision"},
                         {"branches", {{{"when", true}, {"next", next}}}},
                         {"otherwise", next}});
    }
    auto definition = minimal(std::move(nodes));
    definition["entry"] = "node0";
    auto c = cd::Conversation::load(definition, state);
    BOOST_TEST(c->graph().size() == count);
    auto cursor = c->graph().start();
    const auto first = cursor;
    bool sharedDestinations = true;
    bool contiguous = true;
    for (std::size_t i = 0; i < count; ++i) {
        contiguous = contiguous && cursor.operator->() == first.operator->() + i;
        const auto &node = std::get<cd::DecisionNode>(cursor->data);
        sharedDestinations = sharedDestinations && node.branches.front().next == node.otherwise;
        cursor.next(node.branches.front());
    }
    BOOST_CHECK(cursor == first);
    BOOST_CHECK(sharedDestinations);
    BOOST_CHECK(contiguous);
    BOOST_TEST(state.mutations == 0u);
    c.reset(); // ASan/LSan cover cleanup despite cyclic non-owning connections.
}
BOOST_AUTO_TEST_CASE(compact_responses_keep_distinct_private_results_in_preallocated_storage) {
    TestState state;
    auto definition = compactPurchase();
    auto second = definition["nodes"][0]["responses"][0];
    second["id"] = "buy_again";
    definition["nodes"][0]["responses"].push_back(second);
    definition["nodes"][1]["next"] = "menu";
    definition["nodes"][2]["next"] = "menu";
    auto c = cd::Conversation::load(definition, state);
    const auto menu = c->graph().start();
    state.credits = 0;
    c->advance();
    c->finishDialogue(c->view().entry);
    c->choose(c->view().entry, "buy");
    c->finishDialogue(c->view().entry); // Return to the same menu after failure.
    BOOST_CHECK(c->current() == menu);
    state.credits = 100;
    c->finishDialogue(c->view().entry);
    c->choose(c->view().entry, "buy_again");
    BOOST_TEST(state.purchases == 2u);
    BOOST_TEST(c->memory().results.size() == 2u);
    bool success = false, failure = false;
    for (const auto &saved : c->memory().results) {
        success = success || saved.second.code == "ok";
        failure = failure || saved.second.code == "no_credit";
    }
    BOOST_CHECK(success && failure);
}
BOOST_AUTO_TEST_CASE(declared_node_count_includes_expansions_and_rejects_stale_or_invalid_values) {
    TestState state;
    auto definition = compactPurchase();
    definition["node_count"] = 6;
    auto c = cd::Conversation::load(definition, state);
    BOOST_TEST(c->graph().size() == 6u);
    for (const auto &bad : {Json(4), Json(7), Json(-1), Json(6.0), Json("6"), Json(true),
                            Json(std::numeric_limits<std::uint64_t>::max())}) {
        definition["node_count"] = bad;
        BOOST_CHECK_THROW(cd::Conversation::load(definition, state), std::invalid_argument);
    }
    BOOST_TEST(state.mutations == 0u);
    BOOST_TEST(state.purchases == 0u);
}
namespace {
// Test-only adapter: prove execution depends on the public graph contract rather
// than private vector fields or a concrete graph/cursor constructor.
struct ObservedGraph {
    using iterator = cd::ConversationGraph::iterator;
    cd::ConversationGraph storage;
    std::size_t preparations = 0, allocations = 0;
    void prepare(std::size_t count) {
        ++preparations;
        storage.prepare(count);
    }
    cd::Node *emplace() {
        ++allocations;
        return storage.emplace();
    }
    void set_start(cd::Node *node) {
        storage.set_start(node);
    }
    cd::Node &at(iterator position) {
        return storage.at(position);
    }
    iterator start() const {
        return storage.start();
    }
    iterator end() const {
        return storage.end();
    }
    std::size_t size() const {
        return storage.size();
    }
};
} // namespace
BOOST_AUTO_TEST_CASE(conversation_accepts_an_alternative_graph_through_the_public_contract) {
    TestState state;
    state.credits = 0;
    using ExperimentalConversation = cd::BasicConversation<ObservedGraph>;
    auto c = ExperimentalConversation::load(compactPurchase(), state);
    BOOST_TEST(c->graph().preparations == 1u);
    BOOST_TEST(c->graph().allocations == 6u);
    BOOST_TEST(state.purchases == 0u);
    c->advance();
    c->finishDialogue(c->view().entry);
    c->choose(c->view().entry, "buy");
    BOOST_TEST(c->view().speech[0].get<std::string>() == "NEED_CREDIT");
    BOOST_TEST(state.purchases == 1u);
    c->finishDialogue(c->view().entry);
    BOOST_TEST(c->ended());
    BOOST_CHECK(c->current() == c->graph().end());
    BOOST_TEST(c->outcome() == "peace");
}
BOOST_AUTO_TEST_CASE(graph_loader_hooks_seal_storage_before_runtime_traversal) {
    cd::ConversationGraph graph;
    graph.prepare(1);
    auto *node = graph.emplace();
    node->data = cd::EndNode{"peace"};
    graph.set_start(node);
    BOOST_CHECK_THROW(graph.prepare(2), std::logic_error);
    BOOST_CHECK_THROW(graph.emplace(), std::logic_error);
    BOOST_CHECK_THROW(graph.set_start(node), std::invalid_argument);
    BOOST_CHECK(&graph.at(graph.start()) == node);
}
