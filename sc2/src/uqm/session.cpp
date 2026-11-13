#include "session.hpp"
#include "battle.h"
#include "fleet_legacy.hpp"
#include "game_state.hpp"
#include <stdexcept>
namespace uqm {
struct Session::FrontendStorage {
    GLOBDATA data{};
    Fleet fleet{MAX_SHIPS_PER_SIDE};
    SIS_STATE sis{};
    PLAYER_TRAVEL_POSITION travel{};
};
Session::Session(RuntimeKind kind, std::uint32_t seed) : random_(seed) {
    if (kind != RuntimeKind::Frontend) definitions_ = std::make_unique<GameDefinitions>();
    if (kind == RuntimeKind::Frontend) {
        frontend_ = std::make_unique<FrontendStorage>();
    }
}
Session::Session(SessionDispatcher &host, RuntimeKind kind, std::uint32_t seed) : Session(kind, seed) {
    bindDispatcher(host);
}
Session::~Session() = default;
const GameDefinitions &Session::definitions() const {
    if (!definitions_) throw std::logic_error("frontend session has no game definitions");
    return *definitions_;
}
void Session::bindDispatcher(SessionDispatcher &host) noexcept { host_ = &host; }
SessionDispatcher &Session::sessionDispatcher() const {
    if (!host_) throw std::logic_error("standalone session has no application dispatcher");
    return static_cast<SessionDispatcher &>(*host_);
}
UQM_Context &Session::context() const { return sessionDispatcher().context(); }
detail::LegacyRuntimeView Session::legacyRuntime() noexcept {
    return {frontend_->data, detail::FleetAccess::queue(frontend_->fleet), frontend_->sis,
        assets(), &frontend_->travel};
}
detail::LegacyRuntimeView Session::bindingView() noexcept {
    auto view = legacyRuntime();
    view.activities = &activities_;
    view.owner = this;
    view.definitions = definitions_.get();
    view.currentStar = view.campaign ? &view.campaign->player().navigationState().currentStar : &selectedStar_;
    return view;
}
SessionActivation::SessionActivation(Session &session) noexcept
    : view_(session.bindingView()), legacy_(view_), random_(session.random_) {}
void ApplicationExit::validate() const { priority(); }
int ApplicationExit::priority() const {
    switch (kind) {
        case ApplicationExitKind::Shutdown: return 0;
        case ApplicationExitKind::Failed: return 1;
    }
    throw std::invalid_argument("unknown application exit kind");
}
}

namespace {
uqm::Session &combatSession() {
    if (auto *view = uqm::detail::boundRuntime(); view && view->owner) return *view->owner;
    return uqm::gameSession(); // Standalone C compatibility entry.
}
}
extern "C" BOOLEAN GetInstantVictory(void) { return combatSession().instantVictory() ? TRUE : FALSE; }
extern "C" void SetInstantVictory(BOOLEAN value) { combatSession().instantVictory() = value != FALSE; }
