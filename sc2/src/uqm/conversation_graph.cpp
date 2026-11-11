#include "conversation.hpp"
#include "conversation_state.hpp"
#include "conversation_text.hpp"
#include "dialogue/text.hpp"
#include "game_state.hpp"
#include "gameev.h"
#include "libs/mathlib.h"
#include "libs/reslib.h"
#include "options.h"
#include <limits>
#include <sstream>
#include <stdexcept>
namespace uqm {
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
        if (definition.contains("mycon_ramble")) {
            const auto &policy = definition.at("mycon_ramble");
            MyconRamblePolicy ramble{policy.at("phrases").get<std::vector<std::string>>(),
                                     policy.at("reveal_at").get<std::size_t>(),
                                     policy.at("reveal_minimum").get<unsigned>()};
            validateMyconRamblePolicy(ramble);
            for (const auto &phrase : ramble.phrases)
                graphPhrase(phrase);
            myconRamble_ = std::move(ramble);
        }
        applyConversationText(definition, resource, data_.ConversationPhrases);
        dialogue::Json player = dialogue::Json::object();
        if (!nativeGraph_ || definition.contains("alliance_name_base")) {
            const auto ship = session_.player().ship();
            player = {{"captain_name", ship.commanderName}, {"ship_name", ship.shipName}};
            if (definition.contains("alliance_name_base")) {
                const auto base =
                    graphPhrase(definition.at("alliance_name_base").get<std::string>()) - 1;
                const auto variant = story_.readNamed("NEW_ALLIANCE_NAME");
                const auto index = base + variant;
                const auto count = GetStringTableCount(data_.ConversationPhrases);
                if (index >= count || (variant == 3 && index + 1 >= count))
                    throw std::out_of_range("invalid alliance name phrase range");
                std::string name =
                    GetStringAddress(SetAbsStringTableIndex(data_.ConversationPhrases, index));
                if (variant == 3) {
                    name += ship.commanderName;
                    name += GetStringAddress(
                        SetAbsStringTableIndex(data_.ConversationPhrases, index + 1));
                }
                player["alliance_name"] = std::move(name);
            }
        }
        graphState_ = std::make_unique<CampaignDialogueState>(
            session_.player(), definition.at("id").get<std::string>(),
            [this](std::optional<std::size_t> selection) { return runNativeTurn(selection); },
            [this](const std::string &report,
                   const dialogue::Json &parameters) -> dialogue::Result {
                const std::string resource = data_.ConversationPhrasesRes;
                if (report == "mycon.ramble" && resource == "comm.mycon.dialogue" && myconRamble_) {
                    if (parameters.size() != 1 || !parameters.contains("response"))
                        throw std::invalid_argument("invalid Mycon ramble arguments");
                    const auto response = parameters.at("response").get<std::string>();
                    graphPhrase(response);
                    return prepareMyconRamble(
                        story_, (story_.readNamed("GLOBAL_FLAGS_AND_DATA") & 128) != 0, response,
                        *myconRamble_, myconMadeChoice_);
                }
                if (!parameters.empty())
                    throw std::invalid_argument("unexpected report parameters");
                if (report == "probe.coordinates" && resource == "comm.probe.dialogue")
                    QueueProbeCoordinates();
                else if (report == "urquan.hypno_intro" && resource == "comm.urquan.dialogue")
                    setIntro(CIM_FADE_IN_SCREEN, ONE_SECOND * 5);
                else
                    throw std::invalid_argument("unknown specialized report handler: " + report);
                return {"ok", {{"speech", dialogue::Json::array()}}};
            },
            dialogue::Json{
                {"player", std::move(player)},
                {"won_last_battle", LOBYTE(GLOBAL(CurrentActivity)) == WON_LAST_BATTLE},
                {"in_last_battle", LOBYTE(GLOBAL(CurrentActivity)) == IN_LAST_BATTLE},
                {"calendar_campaign_year", GLOBAL(GameClock.year_index) - START_YEAR},
                {"calendar_month", GLOBAL(GameClock.month_index)},
                {"calendar_day", GLOBAL(GameClock.day_index)},
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
    if (!graph_->view().presentation.empty())
        throw std::invalid_argument(
            "graph presentation cues require a registered live media handler");
    const auto &speech = graph_->view().speech;
    if (!speech.is_array())
        throw std::invalid_argument("live speech must be a phrase array");
    const auto enqueue = [this](auto &&self, const dialogue::Json &part, bool splice) -> void {
        if (part.is_array()) {
            for (const auto &child : part)
                self(self, child, splice);
            return;
        }
        if (part.is_string() || (part.is_object() && part.size() == 1 && part.contains("phrase"))) {
            const auto id =
                graphPhrase((part.is_string() ? part : part.at("phrase")).get<std::string>());
            if (splice)
                NPCPhrase_splice(id);
            else
                NPCPhrase(id);
        } else if (part.is_object() && part.size() == 1 && part.contains("compose")) {
            if (!part.at("compose").is_array())
                throw std::invalid_argument("speech composition requires an array");
            bool first = true;
            for (const auto &child : part.at("compose")) {
                self(self, child, splice || !first);
                first = false;
            }
        } else if (part.is_object() && part.size() == 1 && part.contains("insert")) {
            graphSpeechInsertions_.push_back(part.at("insert").get<std::string>());
            SpliceTrack(nullptr, graphSpeechInsertions_.back().data(), nullptr, nullptr);
        } else if (part.is_object() && part.size() == 1 && part.contains("number")) {
            const auto &value = part.at("number");
            if (!value.is_number_integer() ||
                (value.is_number_unsigned() &&
                 value.get<std::uint64_t>() >
                     static_cast<std::uint64_t>(std::numeric_limits<dialogue::Integer>::max())))
                throw std::out_of_range("invalid native number speech");
            const auto number = value.get<dialogue::Integer>();
            if (number < std::numeric_limits<int>::min() ||
                number > std::numeric_limits<int>::max())
                throw std::out_of_range("native number speech out of range");
            NPCNumber(static_cast<int>(number), nullptr);
        } else
            throw std::invalid_argument("unsupported live speech segment");
    };
    enqueue(enqueue, speech, false);
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
            graphResponseText_[i] =
                dialogue::formatText(responses[i].text, [this](const std::string &name) {
                    const auto *text = GetStringAddress(
                        SetAbsStringTableIndex(data_.ConversationPhrases, graphPhrase(name) - 1));
                    return text ? std::string(text) : std::string();
                });
            auto &target = input_.response_list[i];
            target.response_ref = 0; // Identity is the graph edge, including constructed answers.
            target.response_func = nullptr; // Graph connections replace native callback dispatch.
            target.response_text.pStr = graphResponseText_[i].data();
            target.response_text.CharCount = static_cast<COUNT>(~0u);
        }
    }
    return false;
}
} // namespace uqm
