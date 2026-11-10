#include <algorithm>
#include <unordered_set>
namespace uqm::design::conversation {
namespace engine_detail {
inline std::string nodeString(const Json &value) {
    if (!value.is_string() || value.get_ref<const std::string &>().empty())
        throw std::invalid_argument("expected nonempty string");
    return value.get<std::string>();
}
inline void nodeFields(const Json &value, std::initializer_list<const char *> allowed) {
    if (!value.is_object())
        throw std::invalid_argument("node must be an object");
    for (const auto &item : value.items()) {
        if (!item.key().empty() && item.key()[0] == '_')
            continue; // Source/coverage annotations.
        bool known = false;
        for (const auto *key : allowed)
            if (item.key() == key)
                known = true;
        if (!known)
            throw std::invalid_argument("unexpected node field: " + item.key());
    }
}
} // namespace engine_detail
template <class Graph>
std::unique_ptr<BasicConversation<Graph>> BasicConversation<Graph>::load(const Json &definition,
                                                                         State &state) {
    if (definition.at("version") != 1)
        throw std::invalid_argument("unsupported conversation version");
    const auto &source = definition.at("nodes");
    if (!source.is_array() || source.empty())
        throw std::invalid_argument("conversation needs nodes");
    auto result = std::unique_ptr<BasicConversation>(new BasicConversation(state));
    const auto locals = definition.value("locals", Json::object());
    if (!locals.is_object())
        throw std::invalid_argument("locals must be an object");
    for (const auto &item : locals.items())
        result->memory_.local.emplace(item.key(), item.value());
    std::size_t count = source.size();
    for (const auto &item : source)
        for (const auto &edge : item.value("responses", Json::array()))
            if (edge.contains("action"))
                count += 2;
    if (definition.contains("node_count")) {
        const auto &declared = definition.at("node_count");
        if (!declared.is_number_integer() || declared != count)
            throw std::invalid_argument(
                "node_count must equal authored nodes plus compact expansions");
        count = declared.get<std::size_t>();
    }
    result->graph_.prepare(count); // One allocation; no resizing after wiring pointers.
    std::size_t generatedResults = 0;
    std::unordered_map<std::string, Node *> ids;
    for (const auto &item : source)
        if (!ids.emplace(engine_detail::nodeString(item.at("id")), result->graph_.emplace()).second)
            throw std::invalid_argument("duplicate node ID");
    const auto resolve = [&ids](const Json &value) {
        const auto id = engine_detail::nodeString(value);
        const auto found = ids.find(id);
        if (found == ids.end())
            throw std::invalid_argument("missing destination: " + id);
        return found->second;
    };
    VerbFactory factory{state, result->memory_};
    for (const auto &item : source) {
        const auto kind = engine_detail::nodeString(item.at("kind"));
        if (kind == "dialogue") {
            engine_detail::nodeFields(item, {"id", "kind", "variants", "responses", "next"});
            DialogueNode node;
            const auto &variants = item.at("variants");
            if (!variants.is_array() || variants.empty())
                throw std::invalid_argument("dialogue needs variants");
            const auto &last = variants.back();
            if (last.contains("when") && last.at("when") != true)
                throw std::invalid_argument("dialogue needs an unconditional fallback variant");
            for (const auto &variant : variants) {
                engine_detail::nodeFields(variant, {"when", "speech", "presentation"});
                auto presentation = variant.value("presentation", Json::object());
                if (!presentation.is_object())
                    throw std::invalid_argument("presentation must be an object");
                node.variants.push_back({factory.condition(variant.value("when", Json(true))),
                                         factory.speech(variant.at("speech")),
                                         std::move(presentation)});
            }
            const auto responses = item.value("responses", Json::array());
            if (!responses.is_array())
                throw std::invalid_argument("responses must be an array");
            std::unordered_set<std::string> responseIds;
            for (const auto &edge : responses) {
                engine_detail::nodeFields(
                    edge, {"id", "text", "when", "next", "action", "then", "otherwise", "save_as"});
                auto id = engine_detail::nodeString(edge.at("id"));
                if (!responseIds.emplace(id).second)
                    throw std::invalid_argument("duplicate response ID");
                Node *next;
                if (edge.contains("action")) {
                    if (edge.contains("next"))
                        throw std::invalid_argument("response action uses then, not next");
                    const auto &destinations = edge.at("then");
                    if (!destinations.is_object() || destinations.empty())
                        throw std::invalid_argument("response action needs result destinations");
                    next = result->graph_.emplace();
                    auto *decisionNode = result->graph_.emplace();
                    const auto save = edge.contains("save_as")
                                          ? engine_detail::nodeString(edge.at("save_as"))
                                          : "__response_" + std::to_string(++generatedResults);
                    if (save.find('.') != std::string::npos ||
                        (edge.contains("save_as") && save.rfind("__response_", 0) == 0))
                        throw std::invalid_argument("invalid or reserved response result name");
                    DecisionNode decision{{}, nullptr};
                    if (edge.contains("otherwise"))
                        decision.otherwise = resolve(edge.at("otherwise"));
                    for (const auto &destination : destinations.items()) {
                        if (destination.key().empty())
                            throw std::invalid_argument("empty action result code");
                        const Json condition = {
                            {"eq", {{{"get", {"result", save + ".code"}}}, destination.key()}}};
                        decision.branches.push_back(
                            {factory.condition(condition), resolve(destination.value())});
                    }
                    next->data = ActionNode{factory.action(edge.at("action")), save, decisionNode};
                    decisionNode->data = std::move(decision);
                } else {
                    if (edge.contains("then") || edge.contains("otherwise") ||
                        edge.contains("save_as"))
                        throw std::invalid_argument("response result routing requires an action");
                    next = resolve(edge.at("next"));
                }
                node.responses.push_back({std::move(id), factory.speech(edge.at("text")),
                                          factory.condition(edge.value("when", Json(true))), next});
            }
            if (item.contains("next"))
                node.next = resolve(item.at("next"));
            if (node.responses.empty() && !node.next)
                throw std::invalid_argument("dialogue needs responses or a continuation");
            if (!node.responses.empty() && node.next)
                throw std::invalid_argument(
                    "dialogue cannot have both responses and a continuation");
            resolve(item.at("id"))->data = Node::Data(std::move(node));
        } else if (kind == "decision") {
            engine_detail::nodeFields(item, {"id", "kind", "branches", "otherwise"});
            DecisionNode node{{}, nullptr};
            if (item.contains("otherwise"))
                node.otherwise = resolve(item.at("otherwise"));
            const auto &branches = item.at("branches");
            if (!branches.is_array() || branches.empty())
                throw std::invalid_argument("decision needs branches");
            for (const auto &branch : branches) {
                engine_detail::nodeFields(branch, {"when", "next"});
                node.branches.push_back(
                    {factory.condition(branch.at("when")), resolve(branch.at("next"))});
            }
            resolve(item.at("id"))->data = Node::Data(std::move(node));
        } else if (kind == "action") {
            engine_detail::nodeFields(item, {"id", "kind", "action", "save_as", "next"});
            if (engine_detail::nodeString(item.at("save_as")).find('.') != std::string::npos ||
                engine_detail::nodeString(item.at("save_as")).rfind("__response_", 0) == 0)
                throw std::invalid_argument("result names cannot contain dots");
            resolve(item.at("id"))->data =
                ActionNode{factory.action(item.at("action")),
                           engine_detail::nodeString(item.at("save_as")), resolve(item.at("next"))};
        } else if (kind == "end") {
            engine_detail::nodeFields(item, {"id", "kind", "outcome"});
            resolve(item.at("id"))->data =
                Node::Data(EndNode{engine_detail::nodeString(item.at("outcome"))});
        } else
            throw std::invalid_argument("unknown node kind: " + kind);
    }
    result->graph_.set_start(resolve(definition.at("entry")));
    result->current_ = result->graph_.start();
    return result;
}
template <class Graph> void BasicConversation<Graph>::requireHealthy() const {
    if (failed_)
        throw std::logic_error("conversation failed; recreate it rather than retrying an action");
}
template <class Graph> void BasicConversation<Graph>::advance(std::size_t silentBudget) {
    requireHealthy();
    if (ended() || view_)
        return;
    try {
        while (silentBudget--) {
            const auto &node = current_->data;
            if (const auto *dialogue = std::get_if<DialogueNode>(&node)) {
                for (const auto &variant : dialogue->variants)
                    if (std::invoke(variant.when)) {
                        auto speech = std::invoke(variant.speech);
                        if (!speech.is_array() && !speech.is_string())
                            throw std::invalid_argument(
                                "speech must resolve to a phrase or an utterance list");
                        view_ = DialogueView{std::move(speech), variant.presentation,
                                             ++dialogueGeneration_};
                        finishedSpeech_ = false;
                        return;
                    }
                throw std::logic_error("no matching dialogue variant");
            }
            if (const auto *decision = std::get_if<DecisionNode>(&node)) {
                auto next = decision->otherwise;
                for (const auto &branch : decision->branches)
                    if (std::invoke(branch.when)) {
                        next = branch.next;
                        break;
                    }
                if (!next)
                    throw std::runtime_error("unhandled conversation decision or action result");
                current_.next(next);
            } else if (const auto *action = std::get_if<ActionNode>(&node)) {
                // Persist the outcome before moving on. Any exception poisons
                // this instance so a partially committed operation cannot replay.
                auto result = std::invoke(action->action);
                if (result.code.empty() || !result.values.is_object())
                    throw std::invalid_argument("invalid action result");
                memory_.results[action->saveAs] = std::move(result);
                current_.next();
            } else {
                outcome_ = std::get<EndNode>(node).outcome;
                current_.next();
                return;
            }
        }
        throw std::runtime_error("silent traversal budget exhausted: possible graph cycle");
    } catch (...) {
        failed_ = true;
        throw;
    }
}
template <class Graph> const DialogueView &BasicConversation<Graph>::view() const {
    requireHealthy();
    if (!view_)
        throw std::logic_error("conversation is not waiting on dialogue");
    return *view_;
}
template <class Graph> void BasicConversation<Graph>::finishDialogue(std::size_t entry) {
    if (view().entry != entry)
        throw std::invalid_argument("stale playback completion");
    if (finishedSpeech_)
        return;
    finishedSpeech_ = true;
    const auto &dialogue = std::get<DialogueNode>(current_->data);
    if (dialogue.next) {
        current_.next();
        view_.reset();
        advance();
    }
}
template <class Graph> std::vector<ResponseView> BasicConversation<Graph>::responses() const {
    view();
    if (!finishedSpeech_)
        throw std::logic_error("finish or skip speech before offering responses");
    std::vector<ResponseView> result;
    const auto &dialogue = std::get<DialogueNode>(current_->data);
    for (const auto &edge : dialogue.responses)
        if (std::invoke(edge.when))
            result.push_back({edge.id, std::invoke(edge.text), edge.selectedBefore});
    if (result.empty())
        throw std::logic_error("dialogue has no eligible responses");
    return result;
}
template <class Graph>
void BasicConversation<Graph>::choose(std::size_t entry, const std::string &id) {
    if (view().entry != entry)
        throw std::invalid_argument("stale response selection");
    if (!finishedSpeech_)
        throw std::logic_error("response selected before speech completed");
    auto &dialogue = std::get<DialogueNode>(graph_.at(current_).data);
    const auto edge = std::find_if(dialogue.responses.begin(), dialogue.responses.end(),
                                   [&id](const Edge &edge) { return edge.id == id; });
    if (edge == dialogue.responses.end() || !std::invoke(edge->when))
        throw std::invalid_argument("response is unavailable");
    edge->selectedBefore = true;
    current_.next(*edge);
    view_.reset();
    advance();
}
} // namespace uqm::design::conversation
