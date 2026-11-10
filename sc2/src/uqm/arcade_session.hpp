#ifndef UQM_ARCADE_SESSION_HPP_
#define UQM_ARCADE_SESSION_HPP_
#include "game_mode.hpp"
#include <memory>
namespace uqm {
// Standalone Super Melee visit. Owns its simulation/RNG and isolated temporary
// legacy buffers, with no campaign player or galaxy. The frontend owns its two
// setup sides. Enter only from the title boundary, after any campaign unwinds.
class ArcadeSession {
public:
    explicit ArcadeSession(std::uint32_t seed = std::random_device{}());
    ~ArcadeSession();
    ArcadeSession(const ArcadeSession &) = delete;
    ArcadeSession &operator=(const ArcadeSession &) = delete;
    SessionExit run();
private:
    struct Storage;
    std::unique_ptr<Storage> storage_;
    SimulationEngine engine_;
    bool running_ = false;
};
}
#endif
