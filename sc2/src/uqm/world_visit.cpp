#include "world_visit.hpp"
#include "world.hpp"
#include "system_runtime.hpp"
#include "system_behavior.hpp"
#include "planet_render_legacy.hpp"
#include "exploration_rendering.hpp"
#include "world_legacy.hpp"
#include "gendef.h"
#include "globdata.h"
#include "setup.h"
#include "libs/inplib.h"
#include "planets/generate/gendefault.h"
#include <stdexcept>
namespace uqm {
std::optional<OrbitHandler> World::selectOrbitHandler(const WorldDefinition &definition) {
    const auto planet = definition.id.planet;
    const auto moon = definition.id.moon;
    const auto body = [&](unsigned p, std::optional<unsigned> m = {}) {
        return planet == p && ((!moon && !m) || (moon && m && *moon == *m));
    };
    bool scripted = false;
    switch (definition.specialSystem) {
        // Sol has distinct analysis/scan seed rules for every body.
        case SOL_DEFINED: scripted = true; break;
        case CHMMR_DEFINED: scripted = body(1) || body(1, 0); break;
        case BURVIXESE_DEFINED: scripted = body(0) || body(0, 0); break;
        case SPATHI_DEFINED: case SYREEN_DEFINED: scripted = body(0) || body(0, 0); break;
        case ANDROSYNTH_DEFINED: scripted = body(1); break;
        case URQUAN_WRECK_DEFINED: scripted = body(6); break;
        case SHIP_VAULT_DEFINED: scripted = body(0, 0); break;
        case SAMATRA_DEFINED: scripted = body(4, 0); break;
        case SLYLANDRO_DEFINED: scripted = body(3); break;
        case TAALO_PROTECTOR_DEFINED: scripted = body(1, 2); break;
        case BOMB_DEFINED: scripted = body(5, 1); break;
        case START_COLONY_DEFINED: case DRUUGE_DEFINED: case ORZ_DEFINED:
        case TALKING_PET_DEFINED: case MYCON_TRAP_DEFINED: case UTWIG_DEFINED:
        case AQUA_HELIX_DEFINED: case THRADD_DEFINED: case SUN_DEVICE_DEFINED:
        case MYCON_DEFINED: case EGG_CASE0_DEFINED: case EGG_CASE1_DEFINED:
        case EGG_CASE2_DEFINED: case MAIDENS_DEFINED: case VUX_BEAST_DEFINED:
        case VUX_DEFINED: case ZOQFOT_DEFINED: case YEHAT_DEFINED:
        case PKUNK_DEFINED: case SUPOX_DEFINED: case RAINBOW_DEFINED:
        case ILWRATH_DEFINED: scripted = body(0); break;
        default: break;
    }
    return scripted ? std::optional<OrbitHandler>{&WorldVisit::runSystemEvent} : std::nullopt;
}
namespace {
// Unlike a resource destructor, this scope only restores the location view.
class VisitOrbitLocation {
public:
    VisitOrbitLocation(SolarSystemLocation &location, const WorldId &id) : location_(location) { location_.enterOrbit(id); }
    ~VisitOrbitLocation() { location_.leaveOrbit(); }
private:
    SolarSystemLocation &location_;
};
class OrbitMode final : public GameMode {
public:
    OrbitMode(GameSession &session, SOLARSYS_STATE &system, SimulationEngine &source)
        : GameMode(session.dispatcher(), session.nextModeSeed()), session_(session), system_(system), source_(source) {}
    ModeResult run() override {
        ExplorationViewScope view{*system_.worlds, ExplorationView::Orbit};
        PlanetOrbitMenu();
        if (auto exit = session_.menuExit()) return *exit;
        if (QuitPosted) return SessionExit::shutdown();
        return ModeCompleted{};
    }
protected:
    void initialize() override { scene_.emplace(source_, engine()); }
    std::unique_ptr<ModeSuspension> suspend() override {
        return suspendExplorationRendering(system_.worlds->view);
    }
    void resume() override {
        if (session_.menuExit() || dispatcher().exitRequest()
                || (GLOBAL(CurrentActivity) & (CHECK_ABORT | CHECK_LOAD | START_ENCOUNTER))) return;
        redrawExploration(system_, system_.worlds->view);
    }
private:
    GameSession &session_;
    SOLARSYS_STATE &system_;
    SimulationEngine &source_;
    std::optional<SimulationSceneTransfer> scene_;
};
}
class WorldVisit::Resources {
public:
    Resources(SOLARSYS_STATE &system, World &world)
        : location_(systemLocation(system), world.id()), rendering_(system, world), graphics_(world.renderResources()) {}
    ~Resources() { if (graphics_.hasSurface()) FinishPlanetOrbit(); }
private:
    VisitOrbitLocation location_;
    detail::PlanetRenderBorrow rendering_;
    PlanetRenderResources &graphics_;
};
WorldVisit::WorldVisit(GameSession &session, SOLARSYS_STATE &system, World &world)
    : session_(session), system_(system), world_(world) {
    if (!system.worlds || system.worlds->session != &session || &orbitalWorld(system) != &world)
        throw std::invalid_argument("orbit visit requires the session's selected world");
    resources_ = std::make_unique<Resources>(system, world);
}
WorldVisit::~WorldVisit() = default;
bool WorldVisit::canContinue() const {
    return !session_.menuExit() && !session_.dispatcher().exitRequest() && !QuitPosted
        && !(GLOBAL(CurrentActivity) & (CHECK_ABORT | CHECK_LOAD | START_ENCOUNTER))
        && session_.player().crew() != static_cast<COUNT>(~0u)
        && session_.player().story().read(StoryCounter::BombProgress) != 2;
}
bool WorldVisit::run() {
    if (!canContinue()) return false;
    loadPlanetProgress(system_);
    if (const auto handler = world_.orbitHandler()) (*handler)(*this);
    else enterOrbit();
    return canContinue();
}
void WorldVisit::enterOrbit() {
    if (!canContinue()) return;
    GenerateDefault_generateOrbital(&system_, &detail::selectedDescriptor(system_));
    presentOrbit();
}
void WorldVisit::runSystemEvent(WorldVisit &visit) {
    if (!visit.canContinue()) return;
    systemBehavior(visit.system_).generateOrbital(visit.system_, detail::selectedDescriptor(visit.system_));
    visit.presentOrbit();
}
void WorldVisit::presentOrbit() {
    LastActivity &= ~(CHECK_LOAD | CHECK_RESTART);
    if (!canContinue() || !world_.renderResources().hasSurface()) return;
    // Surface nodes are prepared in the caller's engine. The orbital child owns
    // them while active and passes that same scene to a lander child as needed.
    if (auto exit = session_.swapGameMode(std::make_unique<OrbitMode>(session_, system_, simulationEngine())))
        GLOBAL(CurrentActivity) |= exit->kind == SessionExitKind::LoadGame ? CHECK_LOAD : CHECK_ABORT;
}
}
