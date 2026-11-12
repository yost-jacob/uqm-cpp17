#include "uqm/conversation.hpp"
#include "uqm/game_state.hpp"
#include "uqm/globdata.h"
#include "uqm/controls.h"
#include "uqm/setup.h"
#include "libs/threadlib.h"
#include "libs/inplib.h"
#include "libs/sndlib.h"
#include <boost/test/unit_test.hpp>
#include <functional>
#include <stdexcept>
#include <vector>

namespace {
unsigned fontsFreed = 0, contextsFreed = 0;
MENU_SOUND_FLAGS arrows{}, menuSelect{};
std::function<void()> scriptPost, scriptUninit, uiDriver;
void initScript() {}
void postScript() { if (scriptPost) scriptPost(); }
COUNT uninitScript() { if (scriptUninit) scriptUninit(); return 0; }
LOCDATA configuration() {
    LOCDATA data{};
    data.init_encounter_func = initScript;
    data.post_encounter_func = postScript;
    data.uninit_encounter_func = uninitScript;
    data.AlienTextWidth = 123;
    return data;
}
struct Fixture {
    ACTIVITY saved = GLOBAL(CurrentActivity);
    int savedQuit = QuitPosted;
    Fixture() { GLOBAL(CurrentActivity) = 0; QuitPosted = 0; }
    ~Fixture() { GLOBAL(CurrentActivity) = saved; QuitPosted = savedQuit; scriptPost = scriptUninit = uiDriver = {}; }
};
}
unsigned destroyedTestFonts() noexcept { return fontsFreed; }

// Only drawing/input/audio are simulated. The real mode factory, dispatcher,
// script bindings, script completion policy and resource owner are linked.
void uqm::ConversationRuntime::run() {
    binding_.emplace(resources_.script);
    if (uiDriver) uiDriver();
    finishWithoutDialogue();
}
std::unique_ptr<uqm::ModeSuspension> uqm::ConversationRuntime::suspend() { return {}; }
void uqm::ConversationRuntime::resume() {}
void uqm::ConversationRuntime::ClearSubtitles() {}
extern "C" {
volatile int QuitPosted = 0;
BOOLEAN DestroyFont(FONT font) { ++fontsFreed; delete reinterpret_cast<char *>(font); return TRUE; }
BOOLEAN DestroyContext(CONTEXT context) { ++contextsFreed; delete reinterpret_cast<char *>(context); return TRUE; }
void GetMenuSounds(MENU_SOUND_FLAGS *first, MENU_SOUND_FLAGS *second) { *first = arrows; *second = menuSelect; }
void SetMenuSounds(MENU_SOUND_FLAGS first, MENU_SOUND_FLAGS second) { arrows = first; menuSelect = second; }
StatMsgMode SetStatusMessageMode(StatMsgMode mode) { static auto current = SMM_DATE; const auto old = current; current = mode; return old; }
Segue getSegue(void) { return Segue_peace; }
void StopSound() {}
void StopMusic() {}
void StopTrack() {}
DWORD FadeMusic(BYTE, SIZE) { return 0; }
void SleepThreadUntil(TimeCount) {}
void FlushColorXForms() {}
}

BOOST_AUTO_TEST_CASE(conversation_c_scripts_borrow_only_bound_owner_data_and_phrase_storage)
{
    uqm::ConversationScriptState outer{}, inner{};
    outer.data.AlienTextWidth = 11; inner.data.AlienTextWidth = 22;
    BOOST_CHECK_THROW(GetConversationData(), std::logic_error);
    BOOST_CHECK_THROW(GetConversationPhraseBuffer(), std::logic_error);
    {
        uqm::ConversationBinding first{outer};
        BOOST_TEST(GetConversationData() == &outer.data);
        GetConversationPhraseBuffer()[0] = 'A';
        try {
            uqm::ConversationBinding second{inner};
            BOOST_TEST(GetConversationData() == &inner.data);
            GetConversationPhraseBuffer()[0] = 'B';
            throw std::runtime_error("unwind");
        } catch (const std::runtime_error &) {}
        BOOST_TEST(GetConversationData()->AlienTextWidth == 11u);
        BOOST_TEST(GetConversationPhraseBuffer()[0] == 'A');
    }
    BOOST_TEST(inner.phrases[0] == 'B');
    BOOST_CHECK_THROW(GetConversationData(), std::logic_error);
}
BOOST_AUTO_TEST_CASE(conversation_intro_request_is_consumed_once_at_child_entry)
{
    SetCommIntroMode(CIM_FADE_IN_SCREEN, 77);
    const auto request = uqm::takeConversationIntro();
    BOOST_TEST(static_cast<int>(request.mode) == static_cast<int>(CIM_FADE_IN_SCREEN));
    BOOST_TEST(request.duration == 77u);
    BOOST_TEST(static_cast<int>(uqm::takeConversationIntro().mode) == static_cast<int>(CIM_DEFAULT));
}
BOOST_AUTO_TEST_CASE(conversation_resources_release_partial_initialization_during_unwind)
{
    const auto fonts = fontsFreed, contexts = contextsFreed;
    try {
        uqm::ConversationResources owned{configuration()};
        owned.script.data.AlienFont = reinterpret_cast<FONT>(new char);
        owned.textCache = reinterpret_cast<CONTEXT>(new char);
        throw std::runtime_error("remaining asset failed");
    } catch (const std::runtime_error &) {}
    BOOST_TEST(fontsFreed == fonts + 1);
    BOOST_TEST(contextsFreed == contexts + 1);
}
BOOST_FIXTURE_TEST_CASE(conversation_script_cleanup_runs_once_but_post_actions_skip_load_and_abort, Fixture)
{
    for (const unsigned flag : {0u, unsigned(CHECK_LOAD), unsigned(CHECK_ABORT)}) {
        uqm::GameSession session{std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Session"}), uqm::SinglePlayerId};
        unsigned post = 0, uninit = 0;
        scriptPost = [&] { ++post; BOOST_TEST(GetConversationData()->AlienTextWidth == 123u); };
        scriptUninit = [&] { ++uninit; };
        GLOBAL(CurrentActivity) = static_cast<ACTIVITY>(flag);
        {
            uqm::ConversationRuntime conversation{session, configuration()};
            conversation.finishWithoutDialogue();
            BOOST_CHECK_THROW(conversation.finishWithoutDialogue(), std::logic_error);
        }
        BOOST_TEST(post == (flag ? 0u : 1u));
        BOOST_TEST(uninit == 1u);
        BOOST_CHECK_THROW(GetConversationData(), std::logic_error);
    }
}
BOOST_FIXTURE_TEST_CASE(conversation_child_propagates_exit_and_unbinds_before_its_parent_continues, Fixture)
{
    for (const unsigned action : {0u, 1u, 2u}) {
        const bool load = action == 1;
        const bool shutdown = action == 2;
        QuitPosted = 0;
        auto state = std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Player"});
        uqm::GameSession session{state, uqm::SinglePlayerId};
        struct Parent final : uqm::GameMode {
            uqm::GameSession &session;
            explicit Parent(uqm::GameSession &owner) : GameMode(owner.dispatcher(), owner.nextModeSeed()), session(owner) {}
            uqm::ModeResult run() override {
                auto exit = session.swapGameMode(uqm::createConversationMode(session,
                    std::make_unique<uqm::ConversationRuntime>(session, configuration())));
                BOOST_CHECK_THROW(GetConversationData(), std::logic_error);
                if (exit) return *exit;
                return uqm::ModeCompleted{};
            }
        };
        unsigned post = 0, uninit = 0;
        uiDriver = [&] { if (load) session.requestLoad(0); else if (shutdown) QuitPosted = 1; };
        scriptPost = [&] { ++post; };
        scriptUninit = [&] { ++uninit; BOOST_TEST(GetConversationData()->AlienTextWidth == 123u); };
        const auto exit = session.run(std::make_unique<Parent>(session));
        BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(load ? uqm::SessionExitKind::LoadGame : shutdown ? uqm::SessionExitKind::Shutdown : uqm::SessionExitKind::ReturnToTitle));
        BOOST_TEST(post == (action ? 0u : 1u));
        BOOST_TEST(uninit == 1u);
        if (load) { BOOST_REQUIRE(exit.loadSlot); BOOST_TEST(*exit.loadSlot == 0u); }
    }
}

BOOST_FIXTURE_TEST_CASE(conversation_backend_failure_releases_the_script_borrow_before_failed_exit, Fixture)
{
    auto state = std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Player"});
    uqm::GameSession session{state, uqm::SinglePlayerId};
    unsigned completed = 0;
    scriptPost = scriptUninit = [&] { ++completed; };
    uiDriver = [] {
        BOOST_TEST(GetConversationData()->AlienTextWidth == 123u);
        throw std::runtime_error("dialogue backend failed");
    };
    const auto exit = session.run(uqm::createConversationMode(session,
        std::make_unique<uqm::ConversationRuntime>(session, configuration())));
    BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(uqm::SessionExitKind::Failed));
    BOOST_TEST(completed == 0u);
    BOOST_CHECK_THROW(GetConversationData(), std::logic_error);
}

BOOST_FIXTURE_TEST_CASE(graph_owned_conversation_needs_no_empty_native_lifecycle_callbacks, Fixture)
{
    uqm::GameSession session{std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Graph"}), uqm::SinglePlayerId};
    LOCDATA data{};
    data.AlienTextWidth = 77;
    {
        uqm::ConversationRuntime conversation{session, data};
        conversation.finishWithoutDialogue();
        BOOST_TEST(GetConversationData()->AlienTextWidth == 77u);
    }
    BOOST_CHECK_THROW(GetConversationData(), std::logic_error);
}
