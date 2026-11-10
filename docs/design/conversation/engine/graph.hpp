#ifndef UQM_DESIGN_CONVERSATION_GRAPH_HPP
#define UQM_DESIGN_CONVERSATION_GRAPH_HPP
#include "verb_factory.hpp"
#include <cstddef>
#include <memory>
#include <utility>
#include <variant>
#include <vector>
namespace uqm::design::conversation {
struct Node;
struct Edge;
struct DecisionBranch;
class Conversation;
// Owns individually allocated nodes. Outgoing graph links are non-owning pointers.
// Neither graph nor nodes move after loading. Cursors borrow this graph and must
// not outlive it. Topology is immutable during traversal; response history is not.
class ConversationGraph {
  public:
    using value_type = Node;
    using size_type = std::size_t;
    class iterator {
      public:
        using value_type = Node;
        using reference = const Node &;
        using pointer = const Node *;
        iterator() = default;
        const Node &operator*() const;
        const Node *operator->() const;
        iterator &next(); // Unconditional connection, or end after a terminal node.
        iterator &next(iterator destination); // Follow an already resolved branch.
        iterator &next(Node *destination);    // nullptr is the end position.
        iterator &next(const Edge &response);
        iterator &next(const DecisionBranch &branch);
        iterator &operator++() {
            return next();
        }
        iterator operator++(int) {
            auto previous = *this;
            next();
            return previous;
        }
        friend bool operator==(iterator a, iterator b) noexcept {
            return a.graph_ == b.graph_ && a.node_ == b.node_;
        }
        friend bool operator!=(iterator a, iterator b) noexcept {
            return !(a == b);
        }

      private:
        friend class ConversationGraph;
        friend class Conversation;
        iterator(const ConversationGraph *graph, Node *node) : graph_(graph), node_(node) {}
        const ConversationGraph *graph_ = nullptr;
        Node *node_ = nullptr;
    };
    using const_iterator = iterator; // Cursors expose read-only nodes.
    ConversationGraph();
    ~ConversationGraph();
    ConversationGraph(const ConversationGraph &) = delete;
    ConversationGraph &operator=(const ConversationGraph &) = delete;
    ConversationGraph(ConversationGraph &&) = delete;
    ConversationGraph &operator=(ConversationGraph &&) = delete;
    iterator begin() const noexcept {
        return entry_;
    } // Entry, rather than storage order.
    iterator start() const noexcept {
        return begin();
    }
    iterator end() const noexcept {
        return iterator(this, nullptr);
    }
    const_iterator cbegin() const noexcept {
        return begin();
    }
    const_iterator cend() const noexcept {
        return end();
    }
    size_type size() const noexcept;
    bool empty() const noexcept;

  private:
    friend class Conversation;
    std::unique_ptr<Node> first_; // Ownership only; independent of graph connections.
    size_type size_ = 0;
    iterator entry_;
    Node *emplace(); // Loader only. A new node never moves existing nodes.
};
struct Edge {
    std::string id;
    Value text;
    Condition when;
    Node *next = nullptr;
    bool selectedBefore = false; // Visit-local styling, never automatic pruning.
};
struct DialogueVariant {
    Condition when;
    Value speech;
    Json presentation;
};
struct DialogueNode {
    std::vector<DialogueVariant> variants;
    std::vector<Edge> responses;
    Node *next = nullptr;
};
struct DecisionBranch {
    Condition when;
    Node *next = nullptr;
};
struct DecisionNode {
    std::vector<DecisionBranch> branches;
    Node *otherwise = nullptr;
};
struct ActionNode {
    Action action;
    std::string saveAs;
    Node *next = nullptr;
};
struct EndNode {
    std::string outcome;
};
struct Node {
    using Data = std::variant<DialogueNode, DecisionNode, ActionNode, EndNode>;
    Node() = default;
    explicit Node(Data value) : data(std::move(value)) {}
    Data data;

  private:
    friend class ConversationGraph;
    friend class ConversationGraph::iterator;
    const ConversationGraph *owner_ = nullptr;
    std::unique_ptr<Node> ownedNext_; // Never used for graph traversal.
};
} // namespace uqm::design::conversation
#endif
