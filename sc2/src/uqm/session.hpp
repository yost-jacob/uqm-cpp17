#ifndef UQM_SESSION_HPP_
#define UQM_SESSION_HPP_
#include "asset_cache.hpp"
#include "game_definitions.hpp"
#include "game_mode.hpp"
#include "legacy_runtime.hpp"
#include "session_activity.h"
#include "libs/math/random.hpp"
namespace uqm {
class UQM_Context;
class Session;
class SessionDispatcher;
enum class ApplicationExitKind { Shutdown, Failed };
struct ApplicationExit {
    ApplicationExitKind kind;
    std::exception_ptr error;
    static ApplicationExit completed() { return {ApplicationExitKind::Shutdown, {}}; }
    static ApplicationExit failed(std::exception_ptr error = {}) { return {ApplicationExitKind::Failed, error}; }
    void validate() const;
    int priority() const;
};
using SessionResult = DispatchResult<Session, ApplicationExit>;
using SessionDispatcherCore = Dispatcher<Session, ApplicationExit>;

class SessionActivation {
public:
    explicit SessionActivation(Session &session) noexcept;
    void discardOnExit() noexcept {} // Binding restoration never reads the retired session.
private:
    detail::LegacyRuntimeView view_;
    detail::LegacyRuntimeBinding legacy_;
    RandomBinding random_;
};
// Owns facilities common to a visit. Campaign/player state and arcade teams
// remain in their derived session. No cache, RNG or mode dispatcher is shared
// between sessions. The application context and host outlive all of them.
class Session {
public:
    virtual ~Session();
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;
    virtual SessionResult run() = 0;
    SessionExit run(std::unique_ptr<GameMode> initial) { return dispatcher_.run(std::move(initial)); }
    // Immutable catalogs; absent for title/intro sessions. Borrows expire with this session.
    const GameDefinitions &definitions() const;
    AssetCache &assets() noexcept { return assets_; }
    std::uint32_t nextModeSeed() noexcept { return random_(); }
    ModeDispatcher &dispatcher() noexcept { return dispatcher_; }
    MenuResult swapGameMode(std::unique_ptr<GameMode> child) { return dispatcher_.swap(std::move(child)); }
    bool isRunning() const noexcept { return dispatcher_.isRunning(); }
    void requestMenuExit(SessionExit exit) { dispatcher_.requestExit(std::move(exit)); }
    MenuResult menuExit() const { return dispatcher_.exitRequest(); }
protected:
    enum class RuntimeKind { Frontend, Campaign, Arcade };
    explicit Session(RuntimeKind kind, std::uint32_t seed);
    Session(SessionDispatcher &host, RuntimeKind kind, std::uint32_t seed);
    void loadDefinitions(bool includeStars);
    void bindDispatcher(SessionDispatcher &host) noexcept;
    SessionDispatcher &sessionDispatcher() const;
    UQM_Context &context() const;
    virtual detail::LegacyRuntimeView legacyRuntime() noexcept;
    detail::LegacyRuntimeView bindingView() noexcept;
    virtual void initialize() {}
    virtual std::unique_ptr<ModeSuspension> suspend() { return {}; }
    virtual void resume() {}
private:
    // Internally synchronized by RAII lock guards. First member: all derived
    // state, modes and other asset holders finish before this cache is destroyed.
    AssetCache assets_;
    std::unique_ptr<GameDefinitions> definitions_;
    std::optional<StarId> selectedStar_;
    RandomStream random_;
    ModeDispatcher dispatcher_;
    UQM_SESSION_ACTIVITY activities_{};
    struct FrontendStorage;
    std::unique_ptr<FrontendStorage> frontend_;
    std::unique_ptr<Session> parent_;
    SessionDispatcherCore *host_ = nullptr; // Unbound only for standalone compatibility/tests.
    friend struct detail::DefinitionsAccess;
    friend class SessionActivation;
    template<class, class> friend class Dispatcher;

    SessionActivation activate() noexcept { return SessionActivation{*this}; }
};
class SessionDispatcher final : public SessionDispatcherCore {
public:
    explicit SessionDispatcher(UQM_Context &context) noexcept : context_(context) {}
    UQM_Context &context() const noexcept { return context_; }
private: UQM_Context &context_;
};
}
#endif
