#include "graph.hpp"
namespace uqm::design::conversation {
ConversationGraph::ConversationGraph() : entry_(this, nullptr) {}
ConversationGraph::~ConversationGraph() = default;
ConversationGraph::size_type ConversationGraph::size() const noexcept {
    return nodes_.size();
}
bool ConversationGraph::empty() const noexcept {
    return nodes_.empty();
}
ConversationGraph::iterator ConversationGraph::position(size_type index) {
    return iterator(this, &nodes_.at(index));
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
    // In particular, never let a branch borrow nodes belonging to another visit.
    if (!graph_ || !node_ || graph_ != destination.graph_)
        throw std::invalid_argument("invalid or foreign conversation graph connection");
    node_ = destination.node_;
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
        return next(*dialogue->next);
    }
    if (const auto *action = std::get_if<ActionNode>(&data))
        return next(action->next);
    if (std::holds_alternative<EndNode>(data))
        return next(graph_->end());
    throw std::logic_error("decision requires a branch selection");
}
} // namespace uqm::design::conversation
