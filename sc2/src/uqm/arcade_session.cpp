#include "arcade_session.hpp"
#include "legacy_runtime.hpp"
#include "fleet_legacy.hpp"
#include "game_state.hpp"
#include <stdexcept>
namespace uqm {
struct ArcadeSession::Storage {
    GLOBDATA data{};
    Fleet compatibilityFleet{MAX_SHIPS_PER_SIDE};
    SIS_STATE compatibilitySIS{};
    PLAYER_TRAVEL_POSITION travel{};
};
ArcadeSession::ArcadeSession(std::uint32_t seed)
    : Session(RuntimeKind::Arcade, seed), storage_(std::make_unique<Storage>()) {}
ArcadeSession::ArcadeSession(SessionDispatcher &host, std::uint32_t seed) : ArcadeSession(seed) { bindDispatcher(host); loadDefinitions(false); }
detail::LegacyRuntimeView ArcadeSession::legacyRuntime() noexcept {
    return {storage_->data, detail::FleetAccess::queue(storage_->compatibilityFleet),
        storage_->compatibilitySIS, assets(), &storage_->travel};
}
ArcadeSession::~ArcadeSession() = default;
SessionExit ArcadeSession::runArcade() {
    // Global device/frontend services are still single-runtime. Reject nesting
    // before binding any buffer, including another instance of ArcadeSession.
    if (isRunning() || gameSession().isRunning() || (detail::standaloneRuntime() && &detail::standaloneRuntime()->assets != &assets())) {
        try { throw std::logic_error("arcade session requires an unwound title boundary"); }
        catch (...) { return SessionExit::failed(std::current_exception()); }
    }
    try {
        auto view = bindingView();
        detail::LegacyRuntimeBinding legacy{view};
        auto exit = dispatcher().run(detail::createArcadeFleetSelectionMode(*this));
        if (exit.kind == SessionExitKind::LoadGame)
            throw std::logic_error("arcade sessions cannot load a campaign");
        return exit;
    } catch (...) { return SessionExit::failed(std::current_exception()); }
}
}
