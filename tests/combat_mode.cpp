#include "uqm/combat_mode.hpp"
#include "uqm/game_state.hpp"
#include "uqm/globdata.h"
#include "uqm/battle.h"
#include "uqm/battlecontrols.h"
#include "uqm/intel.h"
#include "uqm/setup.h"
#include "uqm/sounds.h"
#include <boost/test/unit_test.hpp>
#include <functional>
#include <vector>

extern "C" UWORD nth_frame;

namespace {
std::function<void()> combatDriver;
InputContext originalInput{}, temporaryInput{};
unsigned inputsCleared = 0;
struct CombatFixture {
    ACTIVITY activity = GLOBAL(CurrentActivity);
    BYTE flags = GLOBAL(glob_flags);
    COUNT crew = GLOBAL_SIS(CrewEnlisted);
    BYTE control = PlayerControl[0];
    InputContext *input = PlayerInput[0];
    UWORD frame = nth_frame;
    SOUND sounds = GameSounds;
    CombatFixture() {
        GLOBAL(CurrentActivity) = IN_INTERPLANETARY;
        GLOBAL(glob_flags) = CYBORG_ENABLED;
        PlayerInput[0] = &originalInput;
        PlayerControl[0] = HUMAN_CONTROL | GOOD_RATING;
        nth_frame = 37;
        GameSounds = reinterpret_cast<SOUND>(&originalInput);
        inputsCleared = 0;
    }
    ~CombatFixture() {
        GLOBAL(CurrentActivity) = activity;
        GLOBAL(glob_flags) = flags;
        GLOBAL_SIS(CrewEnlisted) = crew;
        PlayerControl[0] = control;
        PlayerInput[0] = input;
        nth_frame = frame;
        GameSounds = sounds;
        combatDriver = {};
    }
};
}
extern "C" {
UWORD nth_frame = 0;
BYTE PlayerControl[NUM_SIDES]{};
InputContext *PlayerInput[NUM_SIDES]{};
SOUND GameSounds = nullptr;
bool SetPlayerInput(COUNT player) { PlayerInput[player] = &temporaryInput; return true; }
void ClearPlayerInput(COUNT player) { ++inputsCleared; PlayerInput[player] = nullptr; }
void WaitForSoundEnd(COUNT) {}
BOOLEAN Battle(BattleFrameCallback *callback, void *context) {
    BOOST_TEST(callback == nullptr);
    BOOST_TEST(context == nullptr);
    if (GLOBAL(glob_flags) & CYBORG_ENABLED) {
        BOOST_TEST(PlayerInput[0] == &temporaryInput);
        BOOST_TEST(PlayerControl[0] == (CYBORG_CONTROL | AWESOME_RATING));
    } else {
        BOOST_TEST(PlayerInput[0] == &originalInput);
    }
    if (combatDriver) combatDriver();
    return TRUE;
}
}

BOOST_FIXTURE_TEST_CASE(campaign_combat_child_restores_bindings_before_parent_resume_and_aftermath, CombatFixture)
{
    for (const unsigned action : {0u, 1u, 2u}) {
        uqm::GameSession session{std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Pilot"}), uqm::SinglePlayerId};
        std::vector<int> events;
        struct Encounter final : uqm::GameMode {
            uqm::GameSession &session;
            std::vector<int> &events;
            Encounter(uqm::GameSession &s, std::vector<int> &e)
                : GameMode(s.stateOwner(), s.playerId()), session(s), events(e) {}
            uqm::ModeResult run() override {
                events.push_back(1);
                const auto exit = session.swapGameMode(uqm::createCampaignCombatMode(session));
                BOOST_TEST(PlayerInput[0] == &originalInput);
                BOOST_TEST(PlayerControl[0] == (HUMAN_CONTROL | GOOD_RATING));
                BOOST_TEST(GameSounds == reinterpret_cast<SOUND>(&originalInput));
                BOOST_TEST(nth_frame == 37u);
                BOOST_TEST(GLOBAL(CurrentActivity) == IN_INTERPLANETARY);
                events.push_back(4); // Encounter aftermath runs after child teardown.
                if (exit) return *exit;
                return uqm::ModeCompleted{};
            }
            std::unique_ptr<uqm::ModeSuspension> suspend() override { events.push_back(2); return {}; }
            void resume() override {
                BOOST_TEST(PlayerInput[0] == &originalInput);
                events.push_back(3);
            }
        };
        session.player().story().set(uqm::StoryFlag::BattleSegue, false);
        combatDriver = [&] {
            BOOST_TEST(session.player().story().read(uqm::StoryFlag::BattleSegue));
            if (action == 1) session.requestLoad(2);
            if (action == 2) session.requestMenuExit(uqm::SessionExit::shutdown());
        };
        const auto exit = session.run(std::make_unique<Encounter>(session, events));
        const std::vector<int> expected = action ? std::vector<int>{1, 2, 4} : std::vector<int>{1, 2, 3, 4};
        BOOST_CHECK_EQUAL_COLLECTIONS(events.begin(), events.end(), expected.begin(), expected.end());
        BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(action == 1 ? uqm::SessionExitKind::LoadGame : action == 2 ? uqm::SessionExitKind::Shutdown : uqm::SessionExitKind::ReturnToTitle));
        if (action == 1) { BOOST_REQUIRE(exit.loadSlot); BOOST_TEST(*exit.loadSlot == 2u); }
    }
    BOOST_TEST(inputsCleared == 3u);
}

BOOST_FIXTURE_TEST_CASE(campaign_combat_backend_unwind_restores_temporary_input_and_activity, CombatFixture)
{
    uqm::GameState state{{uqm::SinglePlayerId, "Pilot"}};
    combatDriver = [] { throw std::runtime_error("combat backend failure"); };
    BOOST_CHECK_THROW(uqm::runCampaignCombat(state.player(uqm::SinglePlayerId).story()), std::runtime_error);
    BOOST_TEST(PlayerInput[0] == &originalInput);
    BOOST_TEST(PlayerControl[0] == (HUMAN_CONTROL | GOOD_RATING));
    BOOST_TEST(nth_frame == 37u);
    BOOST_TEST(GLOBAL(CurrentActivity) == IN_INTERPLANETARY);
    BOOST_TEST(inputsCleared == 1u);
}

BOOST_FIXTURE_TEST_CASE(campaign_human_combat_keeps_input_and_preserves_legacy_defeat_policy, CombatFixture)
{
    GLOBAL(glob_flags) = 0;
    uqm::GameState state{{uqm::SinglePlayerId, "Pilot"}};
    combatDriver = [] { GLOBAL(CurrentActivity) |= CHECK_ABORT; };
    uqm::runCampaignCombat(state.player(uqm::SinglePlayerId).story());
    BOOST_TEST(GLOBAL_SIS(CrewEnlisted) == static_cast<COUNT>(~0u));
    BOOST_TEST(PlayerInput[0] == &originalInput);
    BOOST_TEST(nth_frame == 37u);
    BOOST_TEST(GLOBAL(CurrentActivity) == IN_INTERPLANETARY);
    BOOST_TEST(inputsCleared == 0u);
}
