#ifndef UQM_GAME_MODE_HPP_
#define UQM_GAME_MODE_HPP_

#include "world_types.hpp"
#include <exception>
#include <cstdint>
#include <memory>
#include <optional>
#include <variant>

namespace uqm {
class GameState;
class GameMode;
enum class SessionExitKind { LoadGame, ReturnToTitle, Shutdown, Failed };
struct SessionExit {
    SessionExitKind kind;
    std::optional<unsigned> loadSlot;
    std::exception_ptr error;
    static SessionExit load(unsigned slot) { return {SessionExitKind::LoadGame, slot, {}}; }
    static SessionExit title() { return {SessionExitKind::ReturnToTitle, {}, {}}; }
    static SessionExit shutdown() { return {SessionExitKind::Shutdown, {}, {}}; }
    static SessionExit failed(std::exception_ptr error = {}) { return {SessionExitKind::Failed, {}, error}; }
};
// A menu's nullopt means no session action: close normally and resume its caller.
// A completed game mode always chooses a successor or a definite session exit.
using MenuResult = std::optional<SessionExit>;
using ModeResult = std::variant<std::unique_ptr<GameMode>, SessionExit>;

// Named callable with shared state ownership and a checked player identity.
// run() executes synchronously on the gameplay thread and returns before the
// dispatcher destroys this object. Return a non-null successor or a session exit.
class GameMode {
public:
    GameMode(std::shared_ptr<GameState> state, PlayerId player);
    virtual ~GameMode() = default;
    virtual ModeResult run() = 0;
    GameMode(const GameMode &) = delete;
    GameMode &operator=(const GameMode &) = delete;
protected:
    // Finish a nested run by returning ownership of its suspended parent.
    // Valid only for a child installed by GameSession::swapGameMode.
    ModeResult resumeParent();
    const std::shared_ptr<GameState> &stateOwner() const noexcept;
    PlayerId playerId() const noexcept;
private:
    friend class GameSession;
    std::unique_ptr<GameMode> parentMode_;
    std::shared_ptr<GameState> state_;
    PlayerId player_;
};
} // namespace uqm
#endif
