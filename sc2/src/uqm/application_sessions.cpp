#include "uqm_context.hpp"
#include "session.hpp"
#include "mode_rendering.hpp"
#include "game_state.hpp"
#include "arcade_session.hpp"
#include "restart.h"
#include "fmv.h"
#include "credits.h"
#include "setup.h"
#include "save.h"
#include "master.h"
#include "controls.h"
#include "intel.h"
#include "libs/inplib.h"
#include "planets/planets.h"
#include "libs/tasklib.h"
#include "libs/log.h"
#include <cstdlib>
#include <stdexcept>

// Existing audio initialization runs on the gameplay worker until backend
// ownership/threading is moved into the application context.
extern "C" { sint32 initAudio(sint32, sint32); }
extern "C" { extern const BYTE element_array[]; extern const PlanetFrame planet_array[]; }
namespace uqm {
namespace {
// SplashScreen's C callback cannot carry a context. Bind it only for the
// synchronous splash call; it never escapes onto a worker thread.
thread_local UQM_Context *splashContext = nullptr;
void initializeSplash(DWORD deadline) { splashContext->prepareKernel(deadline); }
class SplashBinding {
public:
    explicit SplashBinding(UQM_Context &context) : previous_(splashContext) { splashContext = &context; }
    ~SplashBinding() { splashContext = previous_; }
private: UQM_Context *previous_;
};
// Construct only after the session activation binds its catalog. Resources
// are released before the session's definitions and before returning to title.
class MasterShipResources {
public:
    MasterShipResources() {
        try { LoadMasterShipList(TaskSwitch); }
        catch (...) { FreeMasterShipList(); throw; }
    }
    ~MasterShipResources() { FreeMasterShipList(); }
    MasterShipResources(const MasterShipResources &) = delete;
    MasterShipResources &operator=(const MasterShipResources &) = delete;
};
class CutsceneMode final : public GameMode {
public:
    enum class Scene { Startup, Attract, Credits, Introduction, Ending };
    CutsceneMode(Session &session, UQM_Context &context, Scene scene)
        : GameMode(session.dispatcher(), session.nextModeSeed()), session_(session), context_(context), scene_(scene) {}
    ModeResult run() override {
        SessionActivation legacy{session_};
        RandomBinding random{engine().random()};
        auto drawing = suspendDrawingBindings({ScreenContext, OffScreenContext, SpaceContext, StatusContext});
        if (auto exit = session_.menuExit()) return *exit;
        if (QuitPosted) return SessionExit::shutdown();
        switch (scene_) {
            case Scene::Startup: { SplashBinding binding{context_}; SplashScreen(initializeSplash); break; }
            case Scene::Attract: SplashScreen(nullptr); break;
            case Scene::Credits: ::Credits(FALSE); break;
            case Scene::Introduction: ::Introduction(); break;
            case Scene::Ending: ShowCampaignEnding(); break;
        }
        if (auto exit = session_.menuExit()) return *exit;
        return QuitPosted ? ModeResult{SessionExit::shutdown()} : ModeResult{ModeCompleted{}};
    }
private:
    Session &session_;
    UQM_Context &context_;
    Scene scene_;
};
class TitleMenuMode final : public GameMode {
public:
    // choice is session-stack storage; the synchronous dispatcher destroys this
    // mode before run() returns to that stack. Failed runs publish no selection.
    TitleMenuMode(Session &session, std::optional<UQM_TITLE_CHOICE> &choice)
        : GameMode(session.dispatcher(), session.nextModeSeed()), session_(session), choice_(choice) {}
    ModeResult run() override {
        SessionActivation legacy{session_};
        RandomBinding random{engine().random()};
        const auto selected = RunTitleMenu();
        if (auto exit = session_.menuExit()) return *exit;
        if (QuitPosted) return SessionExit::shutdown();
        choice_ = selected;
        return ModeCompleted{};
    }
private:
    Session &session_;
    std::optional<UQM_TITLE_CHOICE> &choice_;
};
class TitleMenuSession final : public Session {
public:
    explicit TitleMenuSession(SessionDispatcher &host) : Session(host, RuntimeKind::Frontend, std::random_device{}()) {}
    SessionResult run() override;
};
class IntroSession final : public Session {
public:
    IntroSession(SessionDispatcher &host, bool attract = false)
        : Session(host, RuntimeKind::Frontend, std::random_device{}()), attract_(attract) {}
    SessionResult run() override {
        auto exit = Session::run(std::make_unique<CutsceneMode>(*this, context(),
            attract_ ? CutsceneMode::Scene::Attract : CutsceneMode::Scene::Startup));
        if (exit.kind == SessionExitKind::Failed) return ApplicationExit::failed(exit.error);
        if (QuitPosted) return ApplicationExit::completed();
        if (attract_) {
            exit = Session::run(std::make_unique<CutsceneMode>(*this, context(), CutsceneMode::Scene::Credits));
            if (exit.kind == SessionExitKind::Failed) return ApplicationExit::failed(exit.error);
        }
        return std::make_unique<TitleMenuSession>(sessionDispatcher());
    }
private: bool attract_;
};
SessionResult TitleMenuSession::run() {
    std::optional<UQM_TITLE_CHOICE> choice;
    const auto exit = Session::run(std::make_unique<TitleMenuMode>(*this, choice));
    if (exit.kind == SessionExitKind::Failed) return ApplicationExit::failed(exit.error);
    if (QuitPosted) return ApplicationExit::completed();
    if (exit.kind == SessionExitKind::Shutdown) return ApplicationExit::completed();
    if (!choice) throw std::logic_error("title menu completed without a selection");
    switch (*choice) {
        case UQM_TITLE_NEW_GAME: return std::make_unique<GameSession>(sessionDispatcher(), GameSession::Entry::NewGame);
        case UQM_TITLE_LOAD_GAME: return std::make_unique<GameSession>(sessionDispatcher(), GameSession::Entry::LoadMenu);
        case UQM_TITLE_ARCADE: return std::make_unique<ArcadeSession>(sessionDispatcher());
        case UQM_TITLE_ATTRACT: return std::make_unique<IntroSession>(sessionDispatcher(), true);
        case UQM_TITLE_QUIT: return ApplicationExit::completed();
    }
    throw std::logic_error("unknown title choice");
}
}
SessionResult GameSession::run() {
    MasterShipResources ships;
    LastActivity = entry_ == Entry::NewGame ? CHECK_LOAD | CHECK_RESTART : CHECK_LOAD;
    GLOBAL(CurrentActivity) = IN_INTERPLANETARY;
    if (entry_ == Entry::NewGame) {
        const auto exit = Session::run(std::make_unique<CutsceneMode>(*this, context(), CutsceneMode::Scene::Introduction));
        if (exit.kind == SessionExitKind::Failed) return ApplicationExit::failed(exit.error);
        if (QuitPosted) return ApplicationExit::completed();
        if (GLOBAL(CurrentActivity) & CHECK_ABORT) return std::make_unique<TitleMenuSession>(sessionDispatcher());
    }
    Elements = element_array;
    PlanData = planet_array;
    PlayerControl[0] = HUMAN_CONTROL | STANDARD_RATING;
    PlayerControl[1] = COMPUTER_CONTROL | AWESOME_RATING;
    auto exit = runCampaign();
    while (exit.kind == SessionExitKind::LoadGame) exit = runCampaign(exit.loadSlot);
    if (exit.kind == SessionExitKind::Failed) return ApplicationExit::failed(exit.error);
    if (exit.kind == SessionExitKind::Shutdown || QuitPosted) return ApplicationExit::completed();
    exit = Session::run(std::make_unique<CutsceneMode>(*this, context(), CutsceneMode::Scene::Ending));
    if (exit.kind == SessionExitKind::Failed) return ApplicationExit::failed(exit.error);
    return std::make_unique<TitleMenuSession>(sessionDispatcher());
}
SessionResult ArcadeSession::run() {
    MasterShipResources ships;
    const auto exit = runArcade();
    if (exit.kind == SessionExitKind::Failed) return ApplicationExit::failed(exit.error);
    if (exit.kind == SessionExitKind::Shutdown || QuitPosted) return ApplicationExit::completed();
    return std::make_unique<TitleMenuSession>(sessionDispatcher());
}
void UQM_Context::initialize() {
    initAudio(configuration().audioDriver, configuration().audioFlags);
    if (!LoadKernel(0, 0)) throw std::runtime_error("could not load basic content; check the content packages directory");
    kernel_ = true;
}
void UQM_Context::prepareKernel(DWORD deadline) {
    TaskSwitch();
    InitGameKernel();
    game_ = true;
    while (GetTimeCounter() <= deadline && !(GLOBAL(CurrentActivity) & CHECK_ABORT)) {
        UpdateInputState();
        TaskSwitch();
    }
}
UQM_Context::~UQM_Context() {
    if (game_) UninitGameKernel();
    if (kernel_) FreeKernel();
}
int Game::run(UQM_Context &context) {
    try {
        context.initialize();
        SessionDispatcher dispatcher{context};
        const auto exit = dispatcher.run(std::make_unique<IntroSession>(dispatcher));
        if (exit.kind == ApplicationExitKind::Failed) {
            if (exit.error) std::rethrow_exception(exit.error);
            throw std::runtime_error("application session failed");
        }
        return EXIT_SUCCESS;
    } catch (const std::exception &error) {
        log_add(log_Error, "Game stopped: %s", error.what());
    } catch (...) { log_add(log_Error, "Game stopped with an unknown exception."); }
    log_showBox(true, true);
    return EXIT_FAILURE;
}
}
