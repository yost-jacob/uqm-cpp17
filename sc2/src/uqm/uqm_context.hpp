#ifndef UQM_CONTEXT_HPP_
#define UQM_CONTEXT_HPP_
#include "libs/compiler.h"
#include "game_configuration.hpp"
namespace uqm {
// Gameplay-thread lifetime of existing backend services. SDL's event/graphics
// pump still lives on main(); this facade does not change thread affinity.
class UQM_Context {
public:
    UQM_Context() = default;
    ~UQM_Context();
    UQM_Context(const UQM_Context &) = delete;
    UQM_Context &operator=(const UQM_Context &) = delete;
    GameConfiguration &configuration() noexcept { return configuration_.get(); }
    const GameConfiguration &configuration() const noexcept { return configuration_.get(); }
    // Called once before dispatch; failures propagate to Game::run.
    void initialize();
    // Complete startup during the splash; deadline is an absolute engine tick.
    void prepareKernel(DWORD deadline);
private:
    ConfigurationOwner configuration_; // Outlives every backend cleanup in this context.
    bool kernel_ = false;
    bool master_ = false;
    bool game_ = false;
};
class Game {
public:
    // context outlives the dispatcher and every session it constructs.
    int run(UQM_Context &context);
};
}
#endif
