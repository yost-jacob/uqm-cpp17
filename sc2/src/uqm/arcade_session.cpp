#include "arcade_session.hpp"
#include "legacy_runtime.hpp"
#include "fleet_legacy.hpp"
#include "game_state.hpp"
#include "game_session_c.h"
#include "libs/log.h"
#include <stdexcept>
namespace uqm {
struct ArcadeSession::Storage {
    GLOBDATA data{};
    Fleet compatibilityFleet{MAX_SHIPS_PER_SIDE};
    SIS_STATE compatibilitySIS{};
};
ArcadeSession::ArcadeSession(std::uint32_t seed)
    : storage_(std::make_unique<Storage>()), modeSeeds_(seed) {}
ArcadeSession::~ArcadeSession() = default;
SessionExit ArcadeSession::run() {
    // Global device/frontend services are still single-runtime. Reject nesting
    // before binding any buffer, including another instance of ArcadeSession.
    if (dispatcher_.isRunning() || gameSession().isRunning() || detail::standaloneRuntime()) {
        try { throw std::logic_error("arcade session requires an unwound title boundary"); }
        catch (...) { return SessionExit::failed(std::current_exception()); }
    }
    try {
        storage_->data.Game_state.glob_flags = GLOBAL(glob_flags);
        detail::LegacyRuntimeView view{storage_->data,
            detail::FleetAccess::queue(storage_->compatibilityFleet), storage_->compatibilitySIS, assets_};
        detail::LegacyRuntimeBinding legacy{view};
        auto exit = dispatcher_.run(detail::createArcadeFleetSelectionMode(*this));
        if (exit.kind == SessionExitKind::LoadGame)
            throw std::logic_error("arcade sessions cannot load a campaign");
        return exit;
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
