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
template <class Graph = ConversationGraph> class BasicConversation {
  public:
    static std::unique_ptr<BasicConversation> load(const Json &, State &);
    BasicConversation(const BasicConversation &) = delete;
    BasicConversation &operator=(const BasicConversation &) = delete;
    BasicConversation(BasicConversation &&) = delete;
    BasicConversation &operator=(BasicConversation &&) = delete;
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
    const Graph &graph() const noexcept {
        return graph_;
    }
    typename Graph::iterator current() const noexcept {
        return current_;
    }
    std::size_t nodeCount() const noexcept {
        return graph_.size();
    }

  private:
    explicit BasicConversation(State &state) : state_(state) {}
    State &state_;
    Memory memory_;
    Graph graph_;
    typename Graph::iterator current_;
    std::size_t dialogueGeneration_ = 0;
    std::optional<DialogueView> view_;
    bool finishedSpeech_ = false, failed_ = false;
    std::string outcome_;
    void requireHealthy() const;
};
using Conversation = BasicConversation<>;
extern template class BasicConversation<ConversationGraph>;
} // namespace uqm::design::conversation
#include "engine.tpp"
#endif
