#ifndef UQM_GAME_MODE_HPP_
#define UQM_GAME_MODE_HPP_

#include "simulation_engine.hpp"
#include <exception>
#include <cstdint>
#include <memory>
#include <optional>
#include <variant>

namespace uqm {
class ModeDispatcher;
class GameMode;
// Mode-owned RAII snapshot. Destruction restores borrowed service bindings.
class ModeSuspension { public: virtual ~ModeSuspension() = default; };
struct ModeCompleted {};
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
// A game mode chooses a successor, completion, or a definite session exit.
using MenuResult = std::optional<SessionExit>;
using ModeResult = std::variant<std::unique_ptr<GameMode>, SessionExit, ModeCompleted>;

// Lifecycle and simulation only. Concrete modes borrow their required session.
// run() executes synchronously on the gameplay thread and returns before the
// dispatcher destroys this object. Return a successor, session exit, or ModeCompleted. Nested completion
// automatically returns control to the suspended parent; root completion exits to title.
class GameMode {
public:
    // host must outlive this mode; the dispatcher rejects modes from other hosts.
    // The owning session supplies a seed without consuming another mode's RNG.
    explicit GameMode(ModeDispatcher &host, std::uint32_t seed = RandomDefaultSeed);
    virtual ~GameMode() = default;
    virtual ModeResult run() = 0;
    SimulationEngine &engine() noexcept { return engine_; }
    const SimulationEngine &engine() const noexcept { return engine_; }
    GameMode(const GameMode &) = delete;
    GameMode &operator=(const GameMode &) = delete;
protected:
    // Called once by the dispatcher after full construction, with this mode's
    // engine selected. Derived modes can prepare or take ownership of a scene.
    virtual void initialize() {}
    // Dispatcher calls these around a child. The token restores bindings after
    // child destruction, including exits. resume() restores the continuing parent
    // view and services; it does not restart run() or commit gameplay effects.
    virtual std::unique_ptr<ModeSuspension> suspend() { return {}; }
    virtual void resume() {}
    ModeDispatcher &dispatcher() const noexcept { return host_; }
private:
    friend class ModeDispatcher;
    SimulationEngine engine_;
    std::unique_ptr<GameMode> parentMode_;
    ModeDispatcher &host_;
};
// Reusable synchronous owner. Contains no player, campaign, fleet or device state.
// Each session owns one instance; keep it alive until all of its modes are gone.
class ModeDispatcher {
public:
    ModeDispatcher() = default;
    ~ModeDispatcher() = default;
    ModeDispatcher(const ModeDispatcher &) = delete;
    ModeDispatcher &operator=(const ModeDispatcher &) = delete;
    SessionExit run(std::unique_ptr<GameMode> initialMode);
    // Called from the running parent. Child must belong to this dispatcher and
    // return completion or exit. Destruction precedes parent rebind/resume.
    MenuResult swapGameMode(std::unique_ptr<GameMode> child);
    void requestMenuExit(SessionExit exit);
    MenuResult menuExit() const { return pendingExit_; }
    bool isRunning() const noexcept { return running_; }
private:
    std::unique_ptr<GameMode> currentMode_;
    MenuResult pendingExit_;
    bool running_ = false;
};
} // namespace uqm
#endif
