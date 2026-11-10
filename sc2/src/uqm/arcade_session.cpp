#include "arcade_session.hpp"
#include "legacy_runtime.hpp"
#include "fleet_legacy.hpp"
#include "game_state.hpp"
#include "game_session_c.h"
#include "libs/inplib.h"
#include "libs/log.h"
#include <stdexcept>
namespace uqm {
struct ArcadeSession::Storage {
    GLOBDATA data{};
    Fleet compatibilityFleet{MAX_SHIPS_PER_SIDE};
    SIS_STATE compatibilitySIS{};
};
ArcadeSession::ArcadeSession(std::uint32_t seed)
    : storage_(std::make_unique<Storage>()), engine_(MAX_DISPLAY_ELEMENTS, seed) {}
ArcadeSession::~ArcadeSession() = default;
SessionExit ArcadeSession::run() {
    // Global device/frontend services are still single-runtime. Reject nesting
    // before binding any buffer, including another instance of ArcadeSession.
    if (running_ || gameSession().isRunning() || detail::standaloneRuntime()) {
        try { throw std::logic_error("arcade session requires an unwound title boundary"); }
        catch (...) { return SessionExit::failed(std::current_exception()); }
    }
    running_ = true;
    struct RunningScope { bool &value; ~RunningScope() { value = false; } } running{running_};
    try {
        if (!engine_.initialized()) engine_.initialize();
        storage_->data.Game_state.glob_flags = GLOBAL(glob_flags);
        detail::LegacyRuntimeView view{storage_->data,
            detail::FleetAccess::queue(storage_->compatibilityFleet), storage_->compatibilitySIS};
        detail::LegacyRuntimeBinding legacy{view};
        SimulationBinding simulation{engine_};
        struct SceneCleanup {
            SimulationEngine &engine;
            ~SceneCleanup() { engine.shutdown(); }
        } scene{engine_}; // Destroy participants while their state is still selected.
        detail::runArcadeFrontend();
        // CHECK_ABORT also means leaving the melee menu; only an application
        // quit request closes the process. Ordinary arcade exit returns to title.
        return QuitPosted ? SessionExit::shutdown() : SessionExit::title();
    } catch (...) { return SessionExit::failed(std::current_exception()); }
}
}
extern "C" UQM_SESSION_EXIT RunArcadeSession(void) {
    try {
        uqm::ArcadeSession session;
        const auto exit = session.run();
        if (exit.kind == uqm::SessionExitKind::Shutdown) return {UQM_SESSION_SHUTDOWN, 0};
        if (exit.kind == uqm::SessionExitKind::ReturnToTitle) return {UQM_SESSION_TITLE, 0};
        if (exit.error) std::rethrow_exception(exit.error);
    } catch (const std::exception &error) {
        log_add(log_Error, "Arcade session failed: %s", error.what());
    } catch (...) { log_add(log_Error, "Arcade session failed."); }
    return {UQM_SESSION_FAILED, 0};
}
