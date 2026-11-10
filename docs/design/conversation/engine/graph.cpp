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
void ConversationGraph::prepare(size_type count) {
    if (sealed_ || !nodes_.empty())
        throw std::logic_error("conversation graph storage is already prepared");
    nodes_.resize(count);
}
void ConversationGraph::set_start(Node *entry) {
    if (sealed_ || used_ != nodes_.size() || !entry || entry->owner_ != this)
        throw std::invalid_argument("invalid or unfinished conversation graph entry");
    entry_ = iterator(this, entry);
    sealed_ = true;
}
Node &ConversationGraph::at(iterator position) {
    if (position.graph_ != this || !position.node_)
        throw std::invalid_argument("invalid conversation graph position");
    return *position.node_;
}
Node *ConversationGraph::emplace() {
    if (sealed_)
        throw std::logic_error("conversation graph allocation is sealed");
    // The loader preallocates the complete vector, including compact expansions.
    // Filling a slot cannot reallocate or invalidate existing node connections.
    auto &node = nodes_.at(used_);
    node.owner_ = this;
    ++used_;
    return &node;
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
