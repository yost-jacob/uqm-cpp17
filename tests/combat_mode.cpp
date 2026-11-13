#include "uqm/combat_mode.hpp"
#include "uqm/arcade_session.hpp"
#include "uqm/runtime_ship_roster_legacy.hpp"
#include "uqm/game_state.hpp"
#include "uqm/encounter_legacy.hpp"
#include "uqm/sis_legacy.hpp"
#include "uqm/globdata.h"
#include "uqm/battle.h"
#include "uqm/simulation_loop.hpp"
#include "uqm/battlecontrols.h"
#include "uqm/intel.h"
#include "uqm/setup.h"
#include "uqm/sounds.h"
#include <boost/test/unit_test.hpp>
#include <functional>
#include <vector>

extern "C" UWORD nth_frame;
extern bool allowTestCombatSound;

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
        allowTestCombatSound = true;
        GLOBAL(CurrentActivity) = IN_INTERPLANETARY;
        GLOBAL(glob_flags) = CYBORG_ENABLED;
        PlayerInput[0] = &originalInput;
        PlayerControl[0] = HUMAN_CONTROL | GOOD_RATING;
        nth_frame = 37;
        GameSounds = reinterpret_cast<SOUND>(&originalInput);
        inputsCleared = 0;
    }
    void configure(uqm::GameSession &session) {
        session.activity() = GLOBAL(CurrentActivity);
        uqm::detail::encounterLegacy(session.player()).flags = GLOBAL(glob_flags);
    }
    ~CombatFixture() {
        allowTestCombatSound = false;
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
}
void uqm::SimulationLoop::run() {
    if (beforeSimulation_) beforeSimulation_();
    if (combatDriver && (GLOBAL(glob_flags) & CYBORG_ENABLED)) {
        BOOST_TEST(PlayerInput[0] == &temporaryInput);
        BOOST_TEST(PlayerControl[0] == (CYBORG_CONTROL | AWESOME_RATING));
    } else if (combatDriver) {
        BOOST_TEST(PlayerInput[0] == &originalInput);
    }
    if (combatDriver) combatDriver();
}

BOOST_FIXTURE_TEST_CASE(campaign_combat_child_restores_bindings_before_parent_resume_and_aftermath, CombatFixture)
{
    for (const unsigned action : {0u, 1u, 2u}) {
        uqm::GameSession session{std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Pilot"}), uqm::SinglePlayerId};
        configure(session);
        std::vector<int> events;
        STARSHIP *participant = nullptr;
        uqm::RuntimeShipRoster *encounterRoster = nullptr;
        struct Encounter final : uqm::GameMode {
            uqm::GameSession &session;
            std::vector<int> &events;
            STARSHIP *&participant;
            uqm::RuntimeShipRoster *&encounterRoster;
            Encounter(uqm::GameSession &s, std::vector<int> &e, STARSHIP *&p, uqm::RuntimeShipRoster *&roster)
                : GameMode(s.dispatcher(), s.nextModeSeed()), session(s), events(e), participant(p), encounterRoster(roster) {}
            uqm::ModeResult run() override {
                events.push_back(1);
                encounterRoster = &engine().shipRoster();
                auto &queue = uqm::detail::RuntimeRosterAccess::queues(*encounterRoster)[0];
                auto handle = AllocLink(&queue);
                participant = static_cast<STARSHIP *>(handle); *participant = {};
                participant->crew_level = 13; PutQueue(&queue, handle);
                const auto before = TFB_GetRandomSeed();
                const auto exit = session.swapGameMode(uqm::createCampaignCombatMode(session, engine().shipRoster(), 876));
                BOOST_TEST(TFB_GetRandomSeed() == before);
                BOOST_TEST(PlayerInput[0] == &originalInput);
                BOOST_TEST(PlayerControl[0] == (HUMAN_CONTROL | GOOD_RATING));
                BOOST_TEST(GameSounds == reinterpret_cast<SOUND>(&originalInput));
                BOOST_TEST(nth_frame == 37u);
                BOOST_TEST(GLOBAL(CurrentActivity) == IN_INTERPLANETARY);
                BOOST_TEST(encounterRoster->size(uqm::RosterSide::First) == 1u);
                BOOST_TEST(participant->crew_level == 9u);
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
            BOOST_TEST(TFB_GetRandomSeed() == 876u);
            std::minstd_rand0 battleRandom(876);
            BOOST_TEST(TFB_Random() == battleRandom());
            BOOST_REQUIRE(participant);
            BOOST_TEST(encounterRoster->size(uqm::RosterSide::First) == 0u);
            auto &queue = uqm::detail::RuntimeRosterAccess::queues(uqm::simulationEngine().shipRoster())[0];
            BOOST_TEST(GetHeadLink(&queue) == participant);
            participant->crew_level = 9;
            if (action == 1) session.requestLoad(2);
            if (action == 2) session.requestMenuExit(uqm::SessionExit::shutdown());
        };
        const auto exit = session.run(std::make_unique<Encounter>(session, events, participant, encounterRoster));
        const std::vector<int> expected = action ? std::vector<int>{1, 2, 4} : std::vector<int>{1, 2, 3, 4};
        BOOST_CHECK_EQUAL_COLLECTIONS(events.begin(), events.end(), expected.begin(), expected.end());
        BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(action == 1 ? uqm::SessionExitKind::LoadGame : action == 2 ? uqm::SessionExitKind::Shutdown : uqm::SessionExitKind::ReturnToTitle));
        if (action == 1) { BOOST_REQUIRE(exit.loadSlot); BOOST_TEST(*exit.loadSlot == 2u); }
    }
    BOOST_TEST(inputsCleared == 3u);
}

BOOST_FIXTURE_TEST_CASE(campaign_combat_backend_unwind_restores_temporary_input_and_activity, CombatFixture)
{
    uqm::GameSession session{std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Pilot"}), uqm::SinglePlayerId};
        configure(session);
    uqm::RuntimeShipRoster source;
    combatDriver = [] { throw std::runtime_error("combat backend failure"); };
    const auto exit = session.run(uqm::createCampaignCombatMode(session, source));
    BOOST_REQUIRE(exit.error);
    BOOST_CHECK_THROW(std::rethrow_exception(exit.error), std::runtime_error);
    BOOST_TEST(PlayerInput[0] == &originalInput);
    BOOST_TEST(PlayerControl[0] == (HUMAN_CONTROL | GOOD_RATING));
    BOOST_TEST(nth_frame == 37u);
    BOOST_TEST(GLOBAL(CurrentActivity) == IN_INTERPLANETARY);
    BOOST_TEST(inputsCleared == 1u);
}

BOOST_FIXTURE_TEST_CASE(campaign_human_combat_keeps_input_and_preserves_legacy_defeat_policy, CombatFixture)
{
    GLOBAL(glob_flags) = 0;
    uqm::GameSession session{std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Pilot"}), uqm::SinglePlayerId};
        configure(session);
    uqm::RuntimeShipRoster source;
    combatDriver = [] { GLOBAL(CurrentActivity) |= CHECK_ABORT; };
    session.run(uqm::createCampaignCombatMode(session, source));
    BOOST_TEST(session.player().crew() == static_cast<COUNT>(~0u));
    BOOST_TEST(PlayerInput[0] == &originalInput);
    BOOST_TEST(nth_frame == 37u);
    BOOST_TEST(GLOBAL(CurrentActivity) == IN_INTERPLANETARY);
    BOOST_TEST(inputsCleared == 0u);
}

BOOST_FIXTURE_TEST_CASE(arcade_dispatcher_runs_shared_combat_and_returns_prepared_participants, CombatFixture)
{
    GLOBAL(glob_flags) = 0;
    for (const bool ready : {true, false}) {
        uqm::ArcadeSession arcade(51);
        std::vector<int> events;
        class Preparation final : public uqm::CombatPreparation {
        public:
            Preparation(bool ready, STARSHIP &ship, std::vector<int> &events)
                : ready_(ready), ship_(ship), events_(events) {}
            ~Preparation() override { events_.push_back(4); }
            bool prepare(uqm::SimulationEngine &) override { events_.push_back(1); return ready_; }
            void complete() override { events_.push_back(3); ship_.crew_level = 7; }
        private:
            bool ready_; STARSHIP &ship_; std::vector<int> &events_;
        };
        struct Selection final : uqm::GameMode {
            uqm::ArcadeSession &session;
            std::vector<int> &events;
            bool ready;
            Selection(uqm::ArcadeSession &owner, std::vector<int> &log, bool play)
                : GameMode(owner.dispatcher(), 23), session(owner), events(log), ready(play) {}
            uqm::ModeResult run() override {
                auto &queue = uqm::detail::RuntimeRosterAccess::queues(engine().shipRoster())[0];
                auto handle = AllocLink(&queue);
                auto *ship = static_cast<STARSHIP *>(handle); *ship = {}; ship->crew_level = 13;
                PutQueue(&queue, handle);
                auto *sourceEngine = &engine();
                combatDriver = [&] {
                    events.push_back(2);
                    BOOST_TEST(&uqm::simulationEngine() != sourceEngine);
                    auto &participants = uqm::detail::RuntimeRosterAccess::queues(uqm::simulationEngine().shipRoster())[0];
                    BOOST_TEST(GetHeadLink(&participants) == ship);
                    BOOST_TEST(TFB_GetRandomSeed() == 79u);
                    TFB_Random();
                };
                const auto before = TFB_GetRandomSeed();
                auto exit = session.swapGameMode(uqm::createCombatMode(session,
                    engine().shipRoster(), 79, std::make_unique<Preparation>(ready, *ship, events)));
                combatDriver = {};
                BOOST_TEST(!exit);
                BOOST_TEST(TFB_GetRandomSeed() == before);
                BOOST_TEST(GetHeadLink(&queue) == ship);
                BOOST_TEST(ship->crew_level == (ready ? 7u : 13u));
                events.push_back(5);
                return uqm::ModeCompleted{};
            }
        };
        const auto exit = arcade.dispatcher().run(std::make_unique<Selection>(arcade, events, ready));
        BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(uqm::SessionExitKind::ReturnToTitle));
        const std::vector<int> expected = ready ? std::vector<int>{1, 2, 3, 4, 5} : std::vector<int>{1, 4, 5};
        BOOST_TEST(events == expected, boost::test_tools::per_element());
    }
}

BOOST_FIXTURE_TEST_CASE(combat_load_skips_preparation_completion_and_restores_unrelated_campaign, CombatFixture)
{
    uqm::GameSession owner{std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Owner"}), uqm::SinglePlayerId};
    uqm::GameSession other{std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Other"}), uqm::SinglePlayerId};
    owner.activity() = IN_INTERPLANETARY;
    other.activity() = IN_HYPERSPACE;
    uqm::SessionActivation unrelated{other};
    auto *otherData = GetLegacyGlobData();
    uqm::RuntimeShipRoster source;
    unsigned completed = 0, released = 0;
    struct Preparation final : uqm::CombatPreparation {
        uqm::GameSession &owner;
        unsigned &completed, &released;
        Preparation(uqm::GameSession &owner, unsigned &completed, unsigned &released)
            : owner(owner), completed(completed), released(released) {}
        bool prepare(uqm::SimulationEngine &) override {
            BOOST_TEST(&uqm::gameSession() == &owner);
            return true;
        }
        void complete() override { ++completed; }
        ~Preparation() override { BOOST_TEST(&uqm::gameSession() == &owner); ++released; }
    };
    combatDriver = [&] {
        BOOST_TEST(&uqm::gameSession() == &owner);
        owner.requestLoad(3);
    };
    const auto exit = owner.run(uqm::createCombatMode(owner, source, 12,
        std::make_unique<Preparation>(owner, completed, released)));
    BOOST_CHECK(exit.kind == uqm::SessionExitKind::LoadGame);
    BOOST_TEST(completed == 0u);
    BOOST_TEST(released == 1u);
    BOOST_TEST(GetLegacyGlobData() == otherData);
    BOOST_TEST(other.activity() == IN_HYPERSPACE);
}

BOOST_FIXTURE_TEST_CASE(combat_sound_load_failure_restores_the_parent_without_running_frames, CombatFixture)
{
    uqm::GameSession session{std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Pilot"}), uqm::SinglePlayerId};
    configure(session);
    allowTestCombatSound = false;
    uqm::RuntimeShipRoster source;
    bool ran = false;
    combatDriver = [&] { ran = true; };
    const auto exit = session.run(uqm::createCampaignCombatMode(session, source));
    BOOST_CHECK(exit.kind == uqm::SessionExitKind::Failed);
    BOOST_TEST(!ran);
    BOOST_TEST(PlayerInput[0] == &originalInput);
    BOOST_TEST(PlayerControl[0] == (HUMAN_CONTROL | GOOD_RATING));
    BOOST_TEST(GameSounds == reinterpret_cast<SOUND>(&originalInput));
    BOOST_TEST(session.activity() == IN_INTERPLANETARY);
    BOOST_TEST(inputsCleared == 1u);
}
