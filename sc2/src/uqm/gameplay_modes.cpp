#include "game_state.hpp"
#include "system_runtime.hpp"
#include "hyper_navigation.hpp"
#include "game_menu.hpp"
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

// Legacy clock/resources remain C-owned internally. This scope ends only after
// every mode and nested menu has returned; no load runs with these frames alive.
class CampaignRuntime {
public:
    void initialize() {
        input_ = true;
        if (!SetPlayerInputAll()) throw std::runtime_error("could not initialize player input");
        if (!InitGameStructures()) throw std::runtime_error("could not initialize campaign structures");
        structures_ = true;
        if (!InitGameClock()) throw std::runtime_error("could not initialize campaign clock");
        clock_ = true;
        AddInitialGameEvents();
    }
    ~CampaignRuntime() {
        StopSound();
        if (clock_) UninitGameClock();
        if (structures_) UninitGameStructures();
        if (input_) ClearPlayerInputAll();
    }
    CampaignRuntime() = default;
    CampaignRuntime(const CampaignRuntime &) = delete;
    CampaignRuntime &operator=(const CampaignRuntime &) = delete;
private:
    bool input_ = false, structures_ = false, clock_ = false;
};
ModeResult finishMode(GameSession &session) {
    SetFlashRect(nullptr);
    LastActivity = GLOBAL(CurrentActivity);
    if (QuitPosted) return SessionExit::shutdown();
    if (auto exit = session.menuExit()) return *exit;
    if (GLOBAL(CurrentActivity) & CHECK_ABORT) return SessionExit::title();
    if (!(GLOBAL(CurrentActivity) & CHECK_LOAD)
            && (LOBYTE(GLOBAL(CurrentActivity)) == WON_LAST_BATTLE
                || session.player().crew() == static_cast<COUNT>(~0u))) {
        if (session.player().campaign().read(StoryFlag::KohrAhKilledAll)) InitCommunication(BLACKURQ_CONVERSATION);
        else if (GLOBAL(CurrentActivity) & CHECK_RESTART) GLOBAL(CurrentActivity) &= ~CHECK_RESTART;
        return SessionExit::title();
    }
    return selectMode(session);
}
class HyperSpaceMode final : public GameMode {
public:
    HyperSpaceMode(GameSession &session)
        : GameMode(session.stateOwner(), session.playerId()), session_(session), navigation_(session.player()) {}
    ModeResult run() override {
        GLOBAL(CurrentActivity) = MAKE_WORD(IN_HYPERSPACE, 0);
        DrawAutoPilotMessage(TRUE);
        SetGameClockRate(HYPERSPACE_CLOCK_RATE);
        navigation_.run(session_);
        return finishMode(session_);
    }
private:
    GameSession &session_;
    HyperSpaceNavigation navigation_;
};
class SolarSystemMode final : public GameMode {
public:
    SolarSystemMode(GameSession &session)
        : GameMode(session.stateOwner(), session.playerId()), session_(session), exploration_(session) {}
    ModeResult run() override {
        GLOBAL(CurrentActivity) = MAKE_WORD(IN_INTERPLANETARY, 0);
        DrawAutoPilotMessage(TRUE);
        SetGameClockRate(INTERPLANETARY_CLOCK_RATE);
        exploration_.run();
        return finishMode(session_);
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
        : GameMode(session.stateOwner(), session.playerId()), session_(session), kind_(kind) {}
    ModeResult run() override {
        if (kind_ == EncounterKind::InstallBomb) InstallBombAtEarth();
        else {
            GLOBAL(CurrentActivity) |= START_ENCOUNTER;
            if (kind_ == EncounterKind::Starbase) VisitStarBase();
            else RaceCommunication();
        }
        if (!(GLOBAL(CurrentActivity) & (CHECK_ABORT | CHECK_LOAD))) {
            GLOBAL(CurrentActivity) &= ~START_ENCOUNTER;
            if (LOBYTE(GLOBAL(CurrentActivity)) == IN_INTERPLANETARY)
                GLOBAL(CurrentActivity) |= START_INTERPLANETARY;
        }
        return finishMode(session_);
    }
private:
    GameSession &session_;
    EncounterKind kind_;
};
class LoadMenuMode final : public GameMode {
public:
    explicit LoadMenuMode(GameSession &session)
        : GameMode(session.stateOwner(), session.playerId()), session_(session) {}
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
        : GameMode(session.stateOwner(), session.playerId()), session_(session) {}
    ModeResult run() override {
        auto hook = debugHook;
        debugHook = nullptr;
        if (hook) hook();
        return selectMode(session_);
    }
private:
    GameSession &session_;
};
#endif
std::unique_ptr<GameMode> selectMode(GameSession &session) {
#ifdef DEBUG
    if (debugHook) return std::make_unique<DebugMode>(session);
#endif
    SetStatusMessageMode(SMM_DEFAULT);
    if (!((GLOBAL(CurrentActivity) | NextActivity) & CHECK_LOAD))
        ZeroVelocityComponents(&GLOBAL(velocity));
    else if (GLOBAL(CurrentActivity) & CHECK_LOAD)
        GLOBAL(CurrentActivity) = NextActivity;

    if ((GLOBAL(CurrentActivity) & START_ENCOUNTER) || session.player().campaign().read(StoryCounter::BombProgress) == 2) {
        const auto kind = session.player().campaign().read(StoryCounter::BombProgress) == 2 && !session.player().campaign().read(StoryFlag::StarbaseAccessGranted)
                ? EncounterKind::InstallBomb
                : session.player().campaign().read(StoryCounter::EncounterContext) == static_cast<BYTE>(~0u)
                    || session.player().campaign().read(StoryCounter::BombProgress) == 2
                ? EncounterKind::Starbase : EncounterKind::Communication;
        return std::make_unique<EncounterMode>(session, kind);
    }
    if (GLOBAL(CurrentActivity) & START_INTERPLANETARY)
        return std::make_unique<SolarSystemMode>(session);
    return std::make_unique<HyperSpaceMode>(session);
}
void logFailure(const SessionExit &exit) {
    try {
        if (exit.error) std::rethrow_exception(exit.error);
        log_add(log_Error, "Game session failed; returning to the title menu.");
    } catch (const std::exception &error) {
        log_add(log_Error, "Game session failed: %s", error.what());
    } catch (...) {
        log_add(log_Error, "Game session failed with an unknown exception.");
    }
}
}
SessionExit GameSession::runCampaign(std::optional<unsigned> slot) {
    try {
        if (this != &gameSession()) throw std::logic_error("legacy campaign requires the active process session");
        CampaignRuntime runtime;
        runtime.initialize();
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
extern "C" UQM_SESSION_EXIT RunGameSession(int slot) {
    try {
        const auto exit = uqm::gameSession().runCampaign(slot < 0 ? std::nullopt : std::optional<unsigned>{static_cast<unsigned>(slot)});
        switch (exit.kind) {
            case uqm::SessionExitKind::LoadGame: return {UQM_SESSION_LOAD, exit.loadSlot.value()};
            case uqm::SessionExitKind::Shutdown: return {UQM_SESSION_SHUTDOWN, 0};
            case uqm::SessionExitKind::ReturnToTitle: return {UQM_SESSION_TITLE, 0};
            case uqm::SessionExitKind::Failed:
                uqm::logFailure(exit);
                return {UQM_SESSION_FAILED, 0};
        }
    } catch (...) {
        log_add(log_Error, "Game session adapter failed.");
    }
    return {UQM_SESSION_FAILED, 0};
}
