#include "lander_runtime.hpp"
#include "controls.h"
#include "libs/inplib.h"
#include <optional>
#include <stdexcept>
namespace uqm {
namespace {
class LanderMode final : public GameMode {
public:
    LanderMode(GameSession &session, SOLARSYS_STATE &system, SimulationEngine &source, POINT landing)
        : GameMode(session.dispatcher(), session.nextModeSeed()), session_(session),
          system_(system), source_(source), landing_(landing) {}
    ~LanderMode() override {
        // Remove borrowed lander art/behaviors before visit assets expire, and
        // before the updated persistent scene returns to the orbital parent.
        if (scene_) removeLanderEffects(engine());
    }
    ModeResult run() override {
        runLander(visit_.value());
        if (auto exit = session_.menuExit()) return *exit;
        if (QuitPosted) return SessionExit::shutdown();
        if (detail::encounterLegacy(session_.player()).activity & CHECK_ABORT) return SessionExit::title();
        return ModeCompleted{};
    }
protected:
    void initialize() override {
        scene_.emplace(source_, engine());
        visit_.emplace(session_, system_, engine(), landing_);
    }
private:
    GameSession &session_;
    SOLARSYS_STATE &system_;
    SimulationEngine &source_;
    POINT landing_;
    std::optional<SimulationSceneTransfer> scene_;
    std::optional<LanderVisit> visit_; // Destroyed after effects, before scene handback.
};
}
std::unique_ptr<GameMode> createLanderMode(GameSession &session, SOLARSYS_STATE &system,
        SimulationEngine &surfaceSource, POINT landing) {
    if (!system.worlds || system.worlds->session != &session)
        throw std::invalid_argument("lander requires its owning exploration session");
    return std::make_unique<LanderMode>(session, system, surfaceSource, landing);
}
}
