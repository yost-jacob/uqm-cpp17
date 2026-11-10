#ifndef UQM_DESIGN_CONVERSATION_ENGINE_HPP
#define UQM_DESIGN_CONVERSATION_ENGINE_HPP
#include "graph.hpp"
#include <memory>
#include <optional>
#include <unordered_map>
namespace uqm::design::conversation {
struct ResponseView {
    std::string id;
    Json text;
    bool selectedBefore = false;
};
struct DialogueView {
    Json speech;
    Json presentation;
    std::size_t entry;
};
// Executable design prototype, intentionally independent of the game build.
// The caller owns media playback/assets and calls finishDialogue() when voice
// completes or is skipped. Reading a view or resuming never executes a node.
class Conversation {
  public:
    static std::unique_ptr<Conversation> load(const Json &, State &);
    Conversation(const Conversation &) = delete;
    Conversation &operator=(const Conversation &) = delete;
    Conversation(Conversation &&) = delete;
    Conversation &operator=(Conversation &&) = delete;
    void advance(std::size_t silentBudget = 10000);
    const DialogueView &view() const;
    void finishDialogue(std::size_t entry);
    std::vector<ResponseView> responses() const;
    void choose(std::size_t entry, const std::string &id);
    bool ended() const noexcept {
        return current_ == graph_.end();
    }
    const std::string &outcome() const noexcept {
        return outcome_;
    }
    const Memory &memory() const noexcept {
        return memory_;
    }
    const ConversationGraph &graph() const noexcept {
        return graph_;
    }
    ConversationGraph::iterator current() const noexcept {
        return current_;
    }
    std::size_t nodeCount() const noexcept {
        return graph_.size();
    }

  private:
    explicit Conversation(State &state) : state_(state) {}
    State &state_;
    Memory memory_;
    ConversationGraph graph_;
    ConversationGraph::iterator current_;
    std::size_t dialogueGeneration_ = 0;
    std::optional<DialogueView> view_;
    bool finishedSpeech_ = false, failed_ = false;
    std::string outcome_;
    void requireHealthy() const;
};
} // namespace uqm::design::conversation
#endif
