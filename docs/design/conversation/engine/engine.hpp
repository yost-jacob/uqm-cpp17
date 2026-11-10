#ifndef UQM_DESIGN_CONVERSATION_ENGINE_HPP
#define UQM_DESIGN_CONVERSATION_ENGINE_HPP
#include "verb_factory.hpp"
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <variant>
namespace uqm::design::conversation {
using NodeId = std::size_t;
struct Edge {
    std::string id;
    Value text;
    Condition when;
    NodeId next;
};
struct DialogueVariant {
    Condition when;
    Value speech;
    Json presentation;
};
struct DialogueNode {
    std::vector<DialogueVariant> variants; // First matching variant, once per entry.
    std::vector<Edge> responses;
    std::optional<NodeId> next; // Automatic continuation after playback, without a menu.
};
struct DecisionBranch {
    Condition when;
    NodeId next;
};
struct DecisionNode {
    std::vector<DecisionBranch> branches;
    std::optional<NodeId> otherwise; // Missing fallback rejects an unhandled action result.
};
struct ActionNode {
    Action action;
    std::string saveAs;
    NodeId next;
};
struct EndNode {
    std::string outcome;
};
using Node = std::variant<DialogueNode, DecisionNode, ActionNode, EndNode>;
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
        return ended_;
    }
    const std::string &outcome() const noexcept {
        return outcome_;
    }
    const Memory &memory() const noexcept {
        return memory_;
    }
    std::size_t nodeCount() const noexcept {
        return nodes_.size();
    }

  private:
    explicit Conversation(State &state) : state_(state) {}
    State &state_;
    Memory memory_;
    std::vector<Node> nodes_;
    NodeId current_ = 0;
    std::size_t entries_ = 0;
    std::optional<DialogueView> view_;
    bool finishedSpeech_ = false, ended_ = false, failed_ = false;
    std::string outcome_;
    std::set<std::pair<NodeId, std::string>>
        selected_; // Visit-local styling, never automatic pruning.
    void requireHealthy() const;
};
} // namespace uqm::design::conversation
#endif
