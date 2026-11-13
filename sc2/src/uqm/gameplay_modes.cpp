#include "game_state.hpp"
#include "encounter_runtime.hpp"
#include "starmap.h"
#include "system_runtime.hpp"
#include "hyper_navigation.hpp"
#include "game_menu.hpp"
#include "mode_rendering.hpp"
#include "game_session_c.h"
#include "globdata.h"
#include "battle.h"
#include "comm.h"
#include "controls.h"
#include "clock.h"
#include "gameev.h"
#include "hyper.h"
#include "menustat.h"
#include "nameref.h"
#include "save.h"
#include "setup.h"
#include "starbase.h"
#include "status.h"
#include "uqmdebug.h"
#include "planets/planets.h"
#include "libs/inplib.h"
#include "libs/log.h"
#include "libs/sndlib.h"
#include <limits>
#include <stdexcept>
#include <utility>

namespace uqm {
namespace {
std::unique_ptr<GameMode> selectMode(GameSession &session);
std::unique_ptr<GameMode> selectNavigationMode(GameSession &, const NavigationTransition &);
// One projection into the legacy mode/menu protocol. Native navigation never
// chooses a successor by writing START_* bits or moving a fleet to the front.
void projectNavigationTransition(GameSession &session, const NavigationTransition &transition) {
    const auto legacy = detail::navigationLegacy(session.player());
    if (transition.destination == NavigationDestination::SolarSystem)
        session.player().navigationState().currentStar = transition.star.value();
    else if (transition.star) session.player().navigationState().currentStar = transition.star;
    legacy.activity &= ~(START_INTERPLANETARY | START_ENCOUNTER);
    if (transition.destination == NavigationDestination::SolarSystem)
        legacy.activity |= START_INTERPLANETARY;
    else if (transition.destination == NavigationDestination::Encounter)
        legacy.activity |= START_ENCOUNTER;
}

// Legacy input/resources remain C-owned internally. This scope ends only after
// every mode and nested menu has returned; no load runs with these frames alive.
class CampaignRuntime {
public:
    void initialize(GameSession &session) {
        session.clock().reset();
        input_ = true;
        if (!SetPlayerInputAll()) throw std::runtime_error("could not initialize player input");
        if (!InitGameStructures()) throw std::runtime_error("could not initialize campaign structures");
        structures_ = true;
        AddInitialGameEvents();
    }
    ~CampaignRuntime() {
        StopSound();
        if (structures_) UninitGameStructures();
        if (input_) ClearPlayerInputAll();
    }
    CampaignRuntime() = default;
    CampaignRuntime(const CampaignRuntime &) = delete;
    CampaignRuntime &operator=(const CampaignRuntime &) = delete;
private:
    bool input_ = false, structures_ = false;
};
ModeResult finishMode(GameSession &session, SimulationEngine &engine,
        std::optional<NavigationTransition> transition = std::nullopt) {
    if (transition) projectNavigationTransition(session, *transition);
    SetFlashRect(nullptr);
    LastActivity = GLOBAL(CurrentActivity);
    if (QuitPosted) return SessionExit::shutdown();
    if (auto exit = session.menuExit()) return *exit;
    if (GLOBAL(CurrentActivity) & CHECK_ABORT) return SessionExit::title();
    if (!(GLOBAL(CurrentActivity) & CHECK_LOAD)
            && (LOBYTE(GLOBAL(CurrentActivity)) == WON_LAST_BATTLE
                || session.player().crew() == static_cast<COUNT>(~0u))) {
        if (session.player().story().read(StoryFlag::KohrAhKilledAll)) {
            EncounterRuntime encounter{session, GetCurrentStar(), engine};
            encounter.communicate(BLACKURQ_CONVERSATION);
        }
        else if (GLOBAL(CurrentActivity) & CHECK_RESTART) GLOBAL(CurrentActivity) &= ~CHECK_RESTART;
        return SessionExit::title();
    }
    if (transition && !(GLOBAL(CurrentActivity) & CHECK_LOAD)) {
#ifdef DEBUG
        if (HasDebugHook()) return selectMode(session);
#endif
        return selectNavigationMode(session, *transition);
    }
    return selectMode(session);
}
class HyperSpaceMode final : public GameMode {
public:
    HyperSpaceMode(GameSession &session)
        : GameMode(session.dispatcher(), session.nextModeSeed()), session_(session), navigation_(session, engine(), NavigationRealm::HyperSpace) {}
    ModeResult run() override {
        session_.player().enterNavigation(false);
        GLOBAL(CurrentActivity) = MAKE_WORD(IN_HYPERSPACE, 0);
        DrawAutoPilotMessage(TRUE);
        session_.clock().setRate(HYPERSPACE_CLOCK_RATE);
        const auto transition = navigation_.run();
        return finishMode(session_, engine(), transition);
    }
private:
    GameSession &session_;
    SpaceNavigation navigation_;
};
// Shares navigation mechanisms with hyperspace; the realm is fixed for this mode.
class QuasiSpaceMode final : public GameMode {
public:
    explicit QuasiSpaceMode(GameSession &session)
        : GameMode(session.dispatcher(), session.nextModeSeed()), session_(session),
          navigation_(session, engine(), NavigationRealm::QuasiSpace) {}
    ModeResult run() override {
        session_.player().enterNavigation(true);
        GLOBAL(CurrentActivity) = MAKE_WORD(IN_HYPERSPACE, 0);
        DrawAutoPilotMessage(TRUE);
        session_.clock().setRate(HYPERSPACE_CLOCK_RATE);
        const auto transition = navigation_.run();
        return finishMode(session_, engine(), transition);
    }
private:
    GameSession &session_;
    SpaceNavigation navigation_;
};
class SolarSystemMode final : public GameMode {
public:
    SolarSystemMode(GameSession &session)
        : GameMode(session.dispatcher(), session.nextModeSeed()), session_(session), exploration_(session, engine()) {}
    ModeResult run() override {
        GLOBAL(CurrentActivity) = MAKE_WORD(IN_INTERPLANETARY, 0);
        DrawAutoPilotMessage(TRUE);
        session_.clock().setRate(INTERPLANETARY_CLOCK_RATE);
        exploration_.run();
        return finishMode(session_, engine());
    }
protected:
    std::unique_ptr<ModeSuspension> suspend() override { return exploration_.suspendRendering(); }
    void resume() override { exploration_.resumeRendering(); }
private:
    GameSession &session_;
    SolarSystemExploration exploration_;
};
enum class EncounterKind { Communication, Starbase, InstallBomb };
class EncounterMode final : public GameMode {
public:
    EncounterMode(GameSession &session, EncounterKind kind)
        : GameMode(session.dispatcher(), session.nextModeSeed()), session_(session), kind_(kind), encounter_(session, GetCurrentStar(), engine()) {}
    ModeResult run() override {
        if (kind_ == EncounterKind::InstallBomb) InstallBombAtEarth();
        else {
            GLOBAL(CurrentActivity) |= START_ENCOUNTER;
            if (kind_ == EncounterKind::Starbase) VisitStarBase();
            else encounter_.run();
        }
        if (!(GLOBAL(CurrentActivity) & (CHECK_ABORT | CHECK_LOAD))) {
            GLOBAL(CurrentActivity) &= ~START_ENCOUNTER;
            if (LOBYTE(GLOBAL(CurrentActivity)) == IN_INTERPLANETARY)
                GLOBAL(CurrentActivity) |= START_INTERPLANETARY;
        }
        return finishMode(session_, engine());
    }
private:
    GameSession &session_;
    EncounterKind kind_;
    EncounterRuntime encounter_;
};
class LoadMenuMode final : public GameMode {
public:
    explicit LoadMenuMode(GameSession &session)
        : GameMode(session.dispatcher(), session.nextModeSeed()), session_(session) {}
    ModeResult run() override {
        if (auto exit = runGameOptionsMenu(session_)) return *exit;
        return SessionExit::title();
    }
private:
    GameSession &session_;
};
#ifdef DEBUG
class DebugMode final : public GameMode {
public:
    explicit DebugMode(GameSession &session)
        : GameMode(session.dispatcher(), session.nextModeSeed()), session_(session) {}
    ModeResult run() override {
        SessionActivation legacy{session_};
        RandomBinding random{engine().random()};
        auto drawing = suspendDrawingBindings({ScreenContext, SpaceContext, StatusContext, OffScreenContext});
        auto hook = TakeDebugHook();
        if (hook) hook();
        if (auto exit = session_.menuExit()) return *exit;
        if (QuitPosted) return SessionExit::shutdown();
        return selectMode(session_);
    }
private:
    GameSession &session_;
};
#endif
std::unique_ptr<GameMode> createEncounterMode(GameSession &session) {
        const auto kind = session.player().story().read(StoryCounter::BombProgress) == 2 && !session.player().story().read(StoryFlag::StarbaseAccessGranted)
                ? EncounterKind::InstallBomb
                : session.player().story().read(StoryCounter::EncounterContext) == static_cast<BYTE>(~0u)
                    || session.player().story().read(StoryCounter::BombProgress) == 2
                ? EncounterKind::Starbase : EncounterKind::Communication;
        return std::make_unique<EncounterMode>(session, kind);
}
void prepareModeSelection() {
    SetStatusMessageMode(SMM_DEFAULT);
    if (!((GLOBAL(CurrentActivity) | NextActivity) & CHECK_LOAD))
        ZeroVelocityComponents(&GLOBAL(velocity));
    else if (GLOBAL(CurrentActivity) & CHECK_LOAD)
        GLOBAL(CurrentActivity) = NextActivity;
}
std::unique_ptr<GameMode> selectNavigationMode(GameSession &session, const NavigationTransition &transition) {
    prepareModeSelection();
    // Preserve the existing scripted bomb-installation priority.
    if (session.player().story().read(StoryCounter::BombProgress) == 2)
        return createEncounterMode(session);
    switch (transition.destination) {
        case NavigationDestination::SolarSystem: return std::make_unique<SolarSystemMode>(session);
        case NavigationDestination::Encounter: return createEncounterMode(session);
        case NavigationDestination::HyperSpace: return std::make_unique<HyperSpaceMode>(session);
        case NavigationDestination::QuasiSpace: return std::make_unique<QuasiSpaceMode>(session);
    }
    throw std::logic_error("unknown navigation destination");
}
std::unique_ptr<GameMode> selectMode(GameSession &session) {
#ifdef DEBUG
    if (HasDebugHook()) return std::make_unique<DebugMode>(session);
#endif
    prepareModeSelection();

    if ((GLOBAL(CurrentActivity) & START_ENCOUNTER) || session.player().story().read(StoryCounter::BombProgress) == 2) {
        return createEncounterMode(session);
    }
    if (GLOBAL(CurrentActivity) & START_INTERPLANETARY)
        return std::make_unique<SolarSystemMode>(session);
    if (session.player().story().read(StoryCounter::ArilouSpaceSide) > 1)
        return std::make_unique<QuasiSpaceMode>(session);
    return std::make_unique<HyperSpaceMode>(session);
}

}
SessionExit GameSession::runCampaign(std::optional<unsigned> slot) {
    try {
        if (this != &gameSession()) throw std::logic_error("legacy campaign requires the active process session");
        CampaignRuntime runtime;
        runtime.initialize(*this);
        if (slot) {
            if (*slot > std::numeric_limits<COUNT>::max()) throw std::out_of_range("save slot exceeds legacy range");
            // The former mode/menu stack and runtime were destroyed by the
            // previous runCampaign call. Decoder mutation is safe at this boundary.
            if (!LoadGame(static_cast<COUNT>(*slot), nullptr))
                throw std::runtime_error("could not load the selected saved game");
            LastActivity = CHECK_LOAD;
            GLOBAL(CurrentActivity) |= CHECK_LOAD;
        }
        auto initial = !slot && LastActivity == CHECK_LOAD
                ? std::unique_ptr<GameMode>(std::make_unique<LoadMenuMode>(*this))
                : selectMode(*this);
        auto exit = run(std::move(initial));
        if (exit.kind == SessionExitKind::Failed) {
            LastActivity = 0;
            NextActivity = 0;
            GLOBAL(CurrentActivity) = CHECK_ABORT;
        }
        return exit;
    } catch (...) {
        // Never resume a partially initialized/decoded legacy campaign.
        LastActivity = 0;
        NextActivity = 0;
        GLOBAL(CurrentActivity) = CHECK_ABORT;
        return SessionExit::failed(std::current_exception());
    }
}
} // namespace uqm

extern "C" BOOLEAN RequestGameSessionLoad(COUNT slot) {
    try {
        SUMMARY_DESC summary;
        if (!LoadGame(slot, &summary)) return FALSE; // Read-only preflight; no live-state mutation.
        uqm::gameSession().requestLoad(slot);
        return TRUE;
    } catch (...) { return FALSE; }
}
