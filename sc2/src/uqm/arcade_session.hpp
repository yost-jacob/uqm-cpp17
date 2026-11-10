#ifndef UQM_ARCADE_SESSION_HPP_
#define UQM_ARCADE_SESSION_HPP_
#include "game_mode.hpp"
#include <memory>
#include <utility>
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
    // Borrow for mode construction/nesting; enter this runtime through run().
    ModeDispatcher &dispatcher() noexcept { return dispatcher_; }
    std::uint32_t nextModeSeed() noexcept { return static_cast<std::uint32_t>(modeSeeds_()); }
    MenuResult swapGameMode(std::unique_ptr<GameMode> child) { return dispatcher_.swapGameMode(std::move(child)); }
    MenuResult menuExit() const { return dispatcher_.menuExit(); }
private:
    struct Storage;
    std::unique_ptr<Storage> storage_;
    std::minstd_rand0 modeSeeds_;
    ModeDispatcher dispatcher_;
};
}
#endif
