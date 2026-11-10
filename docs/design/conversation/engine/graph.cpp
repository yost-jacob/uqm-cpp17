#include "graph.hpp"
namespace uqm::design::conversation {
ConversationGraph::ConversationGraph() : entry_(this, nullptr) {}
ConversationGraph::~ConversationGraph() {
    // Iterative RAII cleanup: cycles/merges in outgoing links do not own nodes,
    // and a long ownership chain must not recurse through unique_ptr destructors.
    while (first_) {
        auto remaining = std::move(first_->ownedNext_);
        first_ = std::move(remaining);
    }
}
ConversationGraph::size_type ConversationGraph::size() const noexcept {
    return size_;
}
bool ConversationGraph::empty() const noexcept {
    return !first_;
}
Node *ConversationGraph::emplace() {
    auto node = std::make_unique<Node>();
    node->owner_ = this;
    node->ownedNext_ = std::move(first_);
    first_ = std::move(node);
    ++size_;
    return first_.get();
}
const Node &ConversationGraph::iterator::operator*() const {
    if (!graph_ || !node_)
        throw std::logic_error("cannot dereference conversation graph end");
    return *node_;
}
const Node *ConversationGraph::iterator::operator->() const {
    return &**this;
}
ConversationGraph::iterator &ConversationGraph::iterator::next(iterator destination) {
    if (graph_ != destination.graph_)
        throw std::invalid_argument("foreign conversation graph cursor");
    return next(destination.node_);
}
ConversationGraph::iterator &ConversationGraph::iterator::next(Node *destination) {
    if (!graph_ || !node_ || (destination && destination->owner_ != graph_))
        throw std::invalid_argument("invalid or foreign conversation graph connection");
    node_ = destination;
    return *this;
}
ConversationGraph::iterator &ConversationGraph::iterator::next(const Edge &response) {
    return next(response.next);
}
ConversationGraph::iterator &ConversationGraph::iterator::next(const DecisionBranch &branch) {
    return next(branch.next);
}
ConversationGraph::iterator &ConversationGraph::iterator::next() {
    const auto &data = (**this).data;
    if (const auto *dialogue = std::get_if<DialogueNode>(&data)) {
        if (!dialogue->next)
            throw std::logic_error("dialogue requires a response selection");
        return next(dialogue->next);
    }
    if (const auto *action = std::get_if<ActionNode>(&data))
        return next(action->next);
    if (std::holds_alternative<EndNode>(data))
        return next(graph_->end());
    throw std::logic_error("decision requires a branch selection");
}
} // namespace uqm::design::conversation
