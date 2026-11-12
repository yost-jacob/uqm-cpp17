#include "world_visit.hpp"
#include "orbit_events.hpp"
#include "world.hpp"
#include "system_runtime.hpp"
#include "system_behavior.hpp"
#include "planet_render_legacy.hpp"
#include "exploration_rendering.hpp"
#include "world_legacy.hpp"
#include "gendef.h"
#include "globdata.h"
#include "setup.h"
#include "sounds.h"
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
    using namespace orbit_events;
    switch (definition.specialSystem) {
        case SOL_DEFINED: return &sol;
        case CHMMR_DEFINED: if (body(1) || body(1, 0)) return &chmmr; break;
        case BURVIXESE_DEFINED: if (body(0) || body(0, 0)) return &burvixese; break;
        case SPATHI_DEFINED: if (body(0) || body(0, 0)) return &spathi; break;
        case SYREEN_DEFINED: if (body(0) || body(0, 0)) return &syreen; break;
        case ANDROSYNTH_DEFINED: if (body(1)) return &androsynth; break;
        case URQUAN_WRECK_DEFINED: if (body(6)) return &urquanWreck; break;
        case SHIP_VAULT_DEFINED: if (body(0, 0)) return &shipVault; break;
        case SAMATRA_DEFINED: if (body(4, 0)) return &saMatra; break;
        case SLYLANDRO_DEFINED: if (body(3)) return &slylandro; break;
        case TAALO_PROTECTOR_DEFINED: if (body(1, 2)) return &orz; break;
        case BOMB_DEFINED: if (body(5, 1)) return &utwig; break;
        case START_COLONY_DEFINED: if (body(0)) return &colony; break;
        case DRUUGE_DEFINED: if (body(0)) return &druuge; break;
        case ORZ_DEFINED: if (body(0)) return &orz; break;
        case TALKING_PET_DEFINED: if (body(0)) return &talkingPet; break;
        case MYCON_TRAP_DEFINED: if (body(0)) return &myconTrap; break;
        case UTWIG_DEFINED: if (body(0)) return &utwig; break;
        case AQUA_HELIX_DEFINED: case THRADD_DEFINED: if (body(0)) return &thraddash; break;
        case SUN_DEVICE_DEFINED: case MYCON_DEFINED: case EGG_CASE0_DEFINED:
        case EGG_CASE1_DEFINED: case EGG_CASE2_DEFINED: if (body(0)) return &mycon; break;
        case MAIDENS_DEFINED: case VUX_BEAST_DEFINED: case VUX_DEFINED: if (body(0)) return &vux; break;
        case ZOQFOT_DEFINED: if (body(0)) return &zoqFotPik; break;
        case YEHAT_DEFINED: if (body(0)) return &yehat; break;
        case PKUNK_DEFINED: if (body(0)) return &pkunk; break;
        case SUPOX_DEFINED: if (body(0)) return &supox; break;
        case RAINBOW_DEFINED: if (body(0)) return &rainbowWorld; break;
        case ILWRATH_DEFINED: if (body(0)) return &ilwrath; break;
        default: break;
    }
    return std::nullopt;
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
    : WorldVisit(session, system, world, false) {}
WorldVisit::WorldVisit(GameSession &session, SOLARSYS_STATE &system, World &world, bool inspection)
    : session_(session), system_(system), world_(world), inspection_(inspection) {
    if (!system.worlds || (!inspection && system.worlds->session != &session) || &orbitalWorld(system) != &world)
        throw std::invalid_argument("orbit visit requires the session's selected world");
    if (!inspection) resources_ = std::make_unique<Resources>(system, world);
    else if (!system.graphics) throw std::logic_error("world inspection requires a graphics borrow");
}
WorldVisit::~WorldVisit() = default;
bool WorldVisit::canContinue() const {
    if (inspection_) return true;
    return !session_.menuExit() && !session_.dispatcher().exitRequest() && !QuitPosted
        && !(GLOBAL(CurrentActivity) & (CHECK_ABORT | CHECK_LOAD | START_ENCOUNTER))
        && session_.player().crew() != static_cast<COUNT>(~0u)
        && session_.player().story().read(StoryCounter::BombProgress) != 2;
}
bool WorldVisit::run() {
    if (!canContinue()) return false;
    if (!inspection_) loadPlanetProgress(system_);
    if (const auto handler = world_.orbitHandler()) (*handler)(*this);
    else enterOrbit();
    return canContinue();
}
void WorldVisit::inspectPreparedWorld(GameSession &session, SOLARSYS_STATE &system) {
    WorldVisit visit{session, system, orbitalWorld(system), true};
    visit.run();
}
StoryState &WorldVisit::story() const noexcept { return session_.player().story(); }
const STAR_DESC &WorldVisit::star() const { return systemStar(system_); }
PLANET_DESC &WorldVisit::legacyDescriptor() { return detail::selectedDescriptor(system_); }
COUNT WorldVisit::converse(CONVERSATION conversation) {
    if (!canInteract()) return 0;
    return converseInSystem(session_, &star(), conversation);
}
void WorldVisit::prepareOrbit() {
    if (!canContinue()) return;
    PrepareDefaultOrbit(&system_, &legacyDescriptor());
    loadSurface(nullptr);
}
void WorldVisit::loadSurface(FRAME definition) {
    if (canInteract()) LoadPlanet(definition);
    else if (definition) DestroyDrawable(ReleaseDrawable(definition));
}
bool WorldVisit::canInteract() const { return !inspection_ && canContinue(); }
void WorldVisit::reportDiscovery() { if (canInteract()) ShowDiscoveryReport(MenuSounds, system_.SysInfo.PlanetInfo.LanderFont,
    system_.SysInfo.PlanetInfo.LanderFontEff, system_.SysInfo.PlanetInfo.DiscoveryString); }
void WorldVisit::restoreBorder() { if (canInteract()) RepairSISBorder(); }
void WorldVisit::enterOrbit() {
    prepareOrbit();
    enterPreparedOrbit();
}
void WorldVisit::enterPreparedOrbit() {
    if (inspection_) return;
    LastActivity &= ~(CHECK_LOAD | CHECK_RESTART);
    if (!canContinue() || !world_.renderResources().hasSurface()) return;
    // Surface nodes are prepared in the caller's engine. The orbital child owns
    // them while active and passes that same scene to a lander child as needed.
    if (auto exit = session_.swapGameMode(std::make_unique<OrbitMode>(session_, system_, simulationEngine())))
        GLOBAL(CurrentActivity) |= exit->kind == SessionExitKind::LoadGame ? CHECK_LOAD : CHECK_ABORT;
}
}
