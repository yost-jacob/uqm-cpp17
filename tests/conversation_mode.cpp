#include "uqm/conversation.hpp"
#include "uqm/game_state.hpp"
#include "uqm/globdata.h"
#include "uqm/controls.h"
#include "uqm/setup.h"
#include "libs/threadlib.h"
#include "libs/inplib.h"
#include "libs/graphics/context.h"
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
BOOLEAN DestroyContext(CONTEXT context) { ++contextsFreed; delete context; return TRUE; }
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
    auto &session = uqm::gameSession();
    const auto request = uqm::takeConversationIntro(session);
    BOOST_TEST(static_cast<int>(request.mode) == static_cast<int>(CIM_FADE_IN_SCREEN));
    BOOST_TEST(request.duration == 77u);
    BOOST_TEST(static_cast<int>(uqm::takeConversationIntro(session).mode) == static_cast<int>(CIM_DEFAULT));
}
BOOST_AUTO_TEST_CASE(conversation_resources_release_partial_initialization_during_unwind)
{
    const auto fonts = fontsFreed, contexts = contextsFreed;
    try {
        uqm::ConversationResources owned{configuration()};
        owned.script.data.AlienFont = reinterpret_cast<FONT>(new char);
        owned.textCache = CreateContext("test.conversation");
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
        uqm::detail::encounterLegacy(session.player()).activity = static_cast<ACTIVITY>(flag);
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

BOOST_FIXTURE_TEST_CASE(conversation_entry_state_and_intro_belong_to_the_supplied_session, Fixture)
{
    auto makeState = [] { return std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Pilot"}); };
    uqm::GameSession first{makeState(), uqm::SinglePlayerId}, other{makeState(), uqm::SinglePlayerId};
    auto campaign = uqm::detail::encounterLegacy(first.player());
    campaign.activity = IN_INTERPLANETARY;
    campaign.flags = CYBORG_ENABLED;
    campaign.battleGroupRef = 42;
    first.player().story().set(uqm::StoryCounter::SamatraGrpoffs0, 42);
    first.conversationPresentation().intro = {CIM_FADE_IN_SCREEN, 91};
    first.conversationPresentation().window.corner = {7, 8};
    other.conversationPresentation().intro = {CIM_CROSSFADE_SCREEN, 37};
    uqm::detail::encounterLegacy(other.player()).activity = WON_LAST_BATTLE;
    {
        uqm::SessionActivation unrelated{other};
        uqm::ConversationRuntime conversation{first, configuration()};
        const auto random = conversation.random().state();
        const auto snapshot = conversation.encounterContext(true);
        BOOST_TEST(snapshot.at("at_samatra").get<bool>());
        BOOST_TEST(snapshot.at("cyborg_enabled").get<bool>());
        BOOST_TEST(!snapshot.at("won_last_battle").get<bool>());
        BOOST_TEST(snapshot.at("using_speech").get<bool>());
        BOOST_TEST(conversation.random().state() == random);
        BOOST_CHECK(uqm::takeConversationIntro(first).mode == CIM_DEFAULT);
        BOOST_TEST(GetConversationWindow() == &other.conversationPresentation().window);
        BOOST_CHECK(uqm::takeConversationIntro(other).mode == CIM_CROSSFADE_SCREEN);
        BOOST_CHECK(uqm::takeConversationIntro(other).mode == CIM_DEFAULT);
    }
}

BOOST_FIXTURE_TEST_CASE(discarding_a_conversation_consumes_only_its_owners_intro_and_never_runs_hooks, Fixture)
{
    uqm::GameSession session{std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Pilot"}), uqm::SinglePlayerId};
    unsigned called = 0;
    scriptPost = scriptUninit = [&] { ++called; };
    session.conversationPresentation().intro = {CIM_FADE_IN_SCREEN, 10};
    { uqm::ConversationRuntime abandoned{session, configuration()}; }
    BOOST_TEST(called == 0u);
    BOOST_CHECK(uqm::takeConversationIntro(session).mode == CIM_DEFAULT);
    BOOST_CHECK_THROW(GetConversationData(), std::logic_error);
}

BOOST_FIXTURE_TEST_CASE(conversation_construction_does_not_switch_status_panel_or_run_script_effects, Fixture)
{
    uqm::GameSession session{std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Pilot"}), uqm::SinglePlayerId};
    const auto previous = SetStatusMessageMode(SMM_DATE);
    auto data = configuration();
    data.ConversationPhrasesRes = "comm.melnorme.dialogue";
    {
        uqm::ConversationRuntime conversation{session, data};
        BOOST_CHECK(SetStatusMessageMode(SMM_DATE) == SMM_DATE);
    }
    SetStatusMessageMode(previous);
}

BOOST_FIXTURE_TEST_CASE(conversation_lifecycle_hooks_select_the_owner_and_restore_an_unrelated_campaign, Fixture)
{
    auto state = [] { return std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Pilot"}); };
    uqm::GameSession owner{state(), uqm::SinglePlayerId}, other{state(), uqm::SinglePlayerId};
    uqm::detail::encounterLegacy(other.player()).activity = CHECK_LOAD;
    unsigned post = 0, uninit = 0;
    scriptPost = [&] {
        ++post;
        BOOST_TEST(&uqm::gameSession() == &owner);
        uqm::gameSession().player().story().set(uqm::StoryFlag::BattleSegue);
    };
    scriptUninit = [&] { ++uninit; BOOST_TEST(&uqm::gameSession() == &owner); };
    {
        uqm::SessionActivation unrelated{other};
        uqm::ConversationRuntime conversation{owner, configuration()};
        conversation.finishWithoutDialogue();
        BOOST_TEST(post == 1u); BOOST_TEST(uninit == 1u);
        BOOST_TEST(owner.player().story().read(uqm::StoryFlag::BattleSegue));
        BOOST_TEST(!other.player().story().read(uqm::StoryFlag::BattleSegue));
        BOOST_TEST(&uqm::gameSession() == &other);
    }
}
