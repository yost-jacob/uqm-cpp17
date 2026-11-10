#ifndef UQM_DISPATCHER_HPP_
#define UQM_DISPATCHER_HPP_
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>
namespace uqm {
struct DispatchCompleted {};
template<class Node, class Exit>
using DispatchResult = std::variant<std::unique_ptr<Node>, Exit, DispatchCompleted>;

// Synchronous owner for both sessions and game modes. Nodes provide their
// activation scope, lifecycle hooks and host identity; Exit supplies validation,
// priority, completed() and failed(). No device or simulation state lives here.
template<class Node, class Exit>
class Dispatcher {
public:
    Dispatcher() = default;
    Dispatcher(const Dispatcher &) = delete;
    Dispatcher &operator=(const Dispatcher &) = delete;
    bool isRunning() const noexcept { return running_; }
    std::optional<Exit> exitRequest() const { return pendingExit_; }
    void requestExit(Exit exit) {
        exit.validate();
        if (!pendingExit_ || exit.priority() > pendingExit_->priority()) pendingExit_ = std::move(exit);
    }
    // Child completion returns to the running parent's call stack. Restore the
    // parent and bindings before propagating exceptions; never destroy a parent
    // while its run() frame is still executing.
    std::optional<Exit> swap(std::unique_ptr<Node> child) {
        if (!running_ || !current_ || !child || child->parent_ || child->host_ != this)
            throw std::invalid_argument("child requires the running dispatcher and a fresh matching node");
        auto suspension = current_->suspend();
        child->parent_.swap(current_);
        current_.swap(child);
        std::exception_ptr failure;
        {
            auto activation = current_->activate();
            try {
                current_->initialize();
                auto result = current_->run();
                if (auto *exit = std::get_if<Exit>(&result)) requestExit(*exit);
                else if (!std::holds_alternative<DispatchCompleted>(result))
                    throw std::logic_error("child must complete or return an exit");
            } catch (...) { failure = std::current_exception(); }
            auto completed = std::move(current_);
            current_ = std::move(completed->parent_);
            activation.discardOnExit();
            completed.reset();
        }
        suspension.reset();
        if (failure) std::rethrow_exception(failure);
        if (!pendingExit_) current_->resume();
        return pendingExit_;
    }
    Exit run(std::unique_ptr<Node> initial) {
        if (running_) {
            try { throw std::logic_error("dispatcher is already running"); }
            catch (...) { return Exit::failed(std::current_exception()); }
        }
        running_ = true;
        pendingExit_.reset();
        current_ = std::move(initial);
        Exit exit = Exit::completed();
        try {
            if (!current_) throw std::logic_error("dispatcher requires an initial node");
            if (current_->host_ != this) throw std::invalid_argument("node belongs to another dispatcher");
            for (;;) {
                auto activation = current_->activate();
                current_->initialize();
                auto result = current_->run();
                if (auto *direct = std::get_if<Exit>(&result)) requestExit(*direct);
                if (pendingExit_) { exit = *pendingExit_; break; }
                if (std::holds_alternative<DispatchCompleted>(result)) break;
                auto &next = std::get<std::unique_ptr<Node>>(result);
                if (!next || next->host_ != this)
                    throw std::invalid_argument("successor requires the same dispatcher");
                activation.discardOnExit();
                current_ = std::move(next);
            }
        } catch (...) { exit = Exit::failed(std::current_exception()); }
        if (current_) {
            auto activation = current_->activate();
            activation.discardOnExit();
            current_.reset();
        }
        pendingExit_.reset();
        running_ = false;
        return exit;
    }
private:
    std::unique_ptr<Node> current_;
    std::optional<Exit> pendingExit_;
    bool running_ = false;
};
}
#endif
