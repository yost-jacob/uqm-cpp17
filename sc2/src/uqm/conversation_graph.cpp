#include "conversation.hpp"
#include "conversation_text.hpp"
#include "game_state.hpp"
#include "gameev.h"
#include "libs/reslib.h"
#include "options.h"
#include <limits>
#include <sstream>
#include <stdexcept>
namespace uqm {
namespace {
class CampaignDialogueState final : public dialogue::LegacyTurnState {
  public:
    using Report = std::function<dialogue::Result(const std::string &)>;
    CampaignDialogueState(StoryState &story, std::string script, Callback turn, Report report,
                          dialogue::Json context)
        : LegacyTurnState(std::move(script), std::move(turn)), story_(story),
          report_(std::move(report)), context_(std::move(context)) {}
    dialogue::Json read(const std::string &scope, const std::string &key) const override {
        if (scope == "encounter")
            return context_.at(key);
        if (scope != "story")
            throw std::invalid_argument("unsupported live state scope: " + scope);
        const StoryState &story = story_;
        return static_cast<dialogue::Integer>(story.readNamed(key));
    }
    dialogue::Result set(const std::string &scope, const std::string &key,
                         const dialogue::Json &value) override {
        if (scope != "story")
            throw std::invalid_argument("unsupported live state scope: " + scope);
        if (value.is_boolean())
            story_.setNamed(key, value.get<bool>() ? 1u : 0u);
        else {
            if (!value.is_number_integer() || value < 0 ||
                value > std::numeric_limits<unsigned>::max())
                throw std::out_of_range("invalid story field value: " + key);
            story_.setNamed(key, value.get<unsigned>());
        }
        return {};
    }
    dialogue::Result setOutcome(const std::string &outcome) override {
        if (outcome == "peace")
            setSegue(Segue_peace);
        else if (outcome == "hostile")
            setSegue(Segue_hostile);
        else if (outcome == "victory")
            setSegue(Segue_victory);
        else if (outcome == "defeat")
            setSegue(Segue_defeat);
        else
            throw std::invalid_argument("unknown encounter outcome: " + outcome);
        return {};
    }
    dialogue::Result scheduleEvent(const std::string &event, dialogue::Integer months,
                                   dialogue::Integer days, dialogue::Integer years) override {
        if (event != "ARILOU_UMGAH_CHECK")
            throw std::invalid_argument("unregistered calendar event: " + event);
        for (auto value : {months, days, years})
            if (value < 0 || value > std::numeric_limits<COUNT>::max())
                throw std::out_of_range("calendar offset out of range");
        AddEvent(RELATIVE_EVENT, months, days, years, ARILOU_UMGAH_CHECK);
        return {};
    }
    dialogue::Result prepareReport(const std::string &report,
                                   const dialogue::Json &parameters) override {
        if (!parameters.is_object() || !parameters.empty())
            throw std::invalid_argument("unexpected specialized report parameters");
        return report_(report);
    }

  private:
    StoryState &story_;
    Report report_;
    const dialogue::Json context_;
};
} // namespace
void ConversationRuntime::loadGraph() {
    const std::string resource = data_.ConversationPhrasesRes;
    // Resource keys come from the native descriptor, not from the JSON file.
    if (resource.empty() ||
        resource.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._") != std::string::npos)
        throw std::invalid_argument("invalid native conversation resource key");
    const std::string path = "base/conversations/" + resource + ".json";
    try {
        using Stream = std::unique_ptr<uio_Stream, decltype(&res_CloseResFile)>;
        const Stream file(res_OpenResFile(contentDir, path.c_str(), "rb"), res_CloseResFile);
        if (!file)
            throw std::runtime_error("cannot open conversation definition");
        std::string bytes(LengthResFile(file.get()), '\0');
        if (ReadResFile(bytes.data(), 1, bytes.size(), file.get()) != bytes.size())
            throw std::runtime_error("incomplete conversation definition");
        const auto definition = dialogue::Json::parse(bytes);
        dialogue::validateLiveDefinition(definition, resource,
                                         GetStringTableCount(data_.ConversationPhrases));
        const auto status = definition.at("_migration").at("status");
        if (status != "native_handlers" && status != "graph_with_native_media")
            throw std::invalid_argument("unknown conversation migration status");
        nativeGraph_ = status == "native_handlers";
        for (const auto &phrase : definition.at("phrases"))
            graphPhrases_.emplace(phrase.at("id").get<std::string>(),
                                  phrase.at("index").get<COUNT>() + 1);
        graphState_ = std::make_unique<CampaignDialogueState>(
            story_, definition.at("id").get<std::string>(),
            [this](std::optional<std::size_t> selection) { return runNativeTurn(selection); },
            [this](const std::string &report) -> dialogue::Result {
                const std::string resource = data_.ConversationPhrasesRes;
                if (report == "probe.coordinates" && resource == "comm.probe.dialogue")
                    QueueProbeCoordinates();
                else if (report == "urquan.hypno_intro" && resource == "comm.urquan.dialogue")
                    setIntro(CIM_FADE_IN_SCREEN, ONE_SECOND * 5);
                else
                    throw std::invalid_argument("unknown specialized report handler: " + report);
                return {"ok", {{"speech", dialogue::Json::array()}}};
            },
            dialogue::Json{
                {"won_last_battle", LOBYTE(GLOBAL(CurrentActivity)) == WON_LAST_BATTLE},
                {"at_samatra",
                 LOBYTE(GLOBAL(CurrentActivity)) == IN_INTERPLANETARY &&
                     GLOBAL(BattleGroupRef) != 0 &&
                     GLOBAL(BattleGroupRef) == story_.read(StoryCounter::SamatraGrpoffs0)},
                {"at_probe_message",
                 LOBYTE(GLOBAL(CurrentActivity)) == IN_INTERPLANETARY &&
                     GLOBAL(BattleGroupRef) != 0 &&
                     GLOBAL(BattleGroupRef) == story_.read(StoryCounter::UrquanProbeGrpoffs0)},
                {"at_colony",
                 LOBYTE(GLOBAL(CurrentActivity)) == IN_INTERPLANETARY &&
                     GLOBAL(BattleGroupRef) != 0 &&
                     GLOBAL(BattleGroupRef) == story_.read(StoryCounter::ColonyGrpoffs0)}});
        graph_ = dialogue::Conversation::load(definition, *graphState_);
        applyConversationText(definition, resource, data_.ConversationPhrases);
    } catch (const std::exception &error) {
        throw std::runtime_error(path + ": " + error.what());
    }
}
dialogue::Result ConversationRuntime::runNativeTurn(std::optional<std::size_t> selection) {
    if (selection) {
        if (*selection >= input_.num_responses)
            throw std::out_of_range("native response is no longer offered");
        const auto selected = input_.response_list[*selection];
        if (!selected.response_func)
            throw std::logic_error("native response has no handler");
        input_.num_responses = 0;
        selected.response_func(selected.response_ref);
    } else {
        data_.init_encounter_func();
    }
    dialogue::Json values = {{"has_responses", input_.num_responses != 0}};
    for (std::size_t slot = 0; slot < MaxResponses; ++slot) {
        const bool available = slot < input_.num_responses;
        values["slot_" + std::to_string(slot)] = available;
        if (available) {
            const auto *text = input_.response_list[slot].response_text.pStr;
            if (!text)
                throw std::logic_error("native response has no text");
            values["text_" + std::to_string(slot)] = text;
        }
    }
    return {"ok", std::move(values)};
}
COUNT ConversationRuntime::graphPhrase(const std::string &name) const {
    const auto found = graphPhrases_.find(name);
    if (found == graphPhrases_.end())
        throw std::invalid_argument("unknown dialogue phrase: " + name);
    return found->second;
}
void ConversationRuntime::queueGraphSpeech() {
    if (graph_->ended())
        return;
    const auto &speech = graph_->view().speech;
    if (!speech.is_array())
        throw std::invalid_argument("live speech must be a phrase array");
    const auto enqueue = [this](auto &&self, const dialogue::Json &part) -> void {
        if (part.is_array())
            for (const auto &child : part)
                self(self, child);
        else
            NPCPhrase(graphPhrase(part.get<std::string>()));
    };
    enqueue(enqueue, speech);
    // Empty graph menu nodes must not restart the retained track for replay.
    TalkingFinished = speech.empty() ? TRUE : FALSE;
}
bool ConversationRuntime::finishGraphSpeech() {
    if (!graph_ || graph_->ended())
        return false;
    const auto entry = graph_->view().entry;
    graph_->finishDialogue(entry);
    if (!nativeGraph_ && !graph_->ended() && graph_->view().entry != entry) {
        queueGraphSpeech();
        return true;
    }
    if (graph_->ended()) {
        input_.num_responses = 0;
        return false;
    }
    const auto responses = graph_->responses();
    if (responses.size() > MaxResponses ||
        (nativeGraph_ && responses.size() != input_.num_responses))
        throw std::logic_error("conversation graph and native response menu disagree");
    graphResponseIds_.clear();
    if (!nativeGraph_) {
        input_.num_responses = responses.size();
        input_.cur_response = 0;
        input_.top_response = static_cast<BYTE>(~0u);
        graphResponseText_.resize(responses.size());
    }
    for (std::size_t i = 0; i < responses.size(); ++i) {
        graphResponseIds_.push_back(responses[i].id);
        if (!nativeGraph_) {
            const auto phrase = graphPhrase(responses[i].text.get<std::string>());
            graphResponseText_[i] =
                GetStringAddress(SetAbsStringTableIndex(data_.ConversationPhrases, phrase - 1));
            auto &target = input_.response_list[i];
            target.response_ref = phrase;
            target.response_func = nullptr; // Graph connections replace native callback dispatch.
            target.response_text.pStr = graphResponseText_[i].data();
            target.response_text.CharCount = static_cast<COUNT>(~0u);
        }
    }
    return false;
}
} // namespace uqm
