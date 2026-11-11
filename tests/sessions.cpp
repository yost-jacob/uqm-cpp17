#include <boost/test/unit_test.hpp>
#include "uqm/session.hpp"
#include "uqm/game_state.hpp"
#include "uqm/sis_state.hpp"
#include "uqm/arcade_session.hpp"
#include "uqm/uqm_context.hpp"

// Production entry methods live with the device/frontend integration. This
// test target exercises ownership without initializing SDL or presenting UI.
namespace uqm {
SessionResult GameSession::run() { return DispatchCompleted{}; }
SessionResult ArcadeSession::run() { return DispatchCompleted{}; }
UQM_Context::~UQM_Context() = default;
}
BOOST_AUTO_TEST_CASE(session_dispatcher_releases_each_successor_before_running_the_next)
{
    uqm::UQM_Context context;
    uqm::SessionDispatcher dispatcher{context};
    unsigned live = 0, visits = 0;
    struct Visit final : uqm::Session {
        uqm::SessionDispatcher &host;
        unsigned &live, &visits;
        Visit(uqm::SessionDispatcher &host, unsigned &live, unsigned &visits)
            : Session(host, RuntimeKind::Frontend, 7), host(host), live(live), visits(visits) { ++live; }
        ~Visit() override { --live; }
        uqm::SessionResult run() override {
            BOOST_TEST(live == 1u);
            BOOST_TEST(GetLegacyGlobData()->Game_state.CurrentActivity == 0u);
            *GetLastSessionActivity() = 42;
            GetLegacyGlobData()->Game_state.CurrentActivity = 1;
            if (++visits < 100) return std::make_unique<Visit>(host, live, visits);
            return uqm::ApplicationExit::completed();
        }
    };
    const auto exit = dispatcher.run(std::make_unique<Visit>(dispatcher, live, visits));
    BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(uqm::ApplicationExitKind::Shutdown));
    BOOST_TEST(visits == 100u);
    BOOST_TEST(live == 0u);
}

BOOST_AUTO_TEST_CASE(nested_sessions_restore_bindings_before_parent_stack_unwinds)
{
    uqm::UQM_Context context;
    uqm::SessionDispatcher dispatcher{context};
    auto *startup = GetLegacyGlobData();
    bool parentCleaned = false, childCleaned = false;
    struct Child final : uqm::Session {
        bool &cleaned;
        Child(uqm::SessionDispatcher &host, bool &cleaned)
            : Session(host, RuntimeKind::Frontend, 2), cleaned(cleaned) {}
        ~Child() override { BOOST_TEST(*GetLastSessionActivity() == 13u); cleaned = true; }
        uqm::SessionResult run() override {
            *GetLastSessionActivity() = 13;
            throw std::runtime_error("nested session failure");
        }
    };
    struct Parent final : uqm::Session {
        uqm::SessionDispatcher &host;
        bool &cleaned, &childCleaned;
        Parent(uqm::SessionDispatcher &host, bool &cleaned, bool &childCleaned)
            : Session(host, RuntimeKind::Frontend, 1), host(host), cleaned(cleaned), childCleaned(childCleaned) {}
        ~Parent() override { BOOST_TEST(*GetLastSessionActivity() == 7u); }
        uqm::SessionResult run() override {
            *GetLastSessionActivity() = 7;
            struct Cleanup {
                bool &cleaned, &childCleaned;
                ~Cleanup() { BOOST_TEST(childCleaned); BOOST_TEST(*GetLastSessionActivity() == 7u); cleaned = true; }
            } cleanup{cleaned, childCleaned};
            host.swap(std::make_unique<Child>(host, childCleaned));
            return uqm::DispatchCompleted{};
        }
    };
    const auto exit = dispatcher.run(std::make_unique<Parent>(dispatcher, parentCleaned, childCleaned));
    BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(uqm::ApplicationExitKind::Failed));
    BOOST_REQUIRE(exit.error);
    BOOST_CHECK_THROW(std::rethrow_exception(exit.error), std::runtime_error);
    BOOST_TEST(parentCleaned);
    BOOST_TEST(GetLegacyGlobData() == startup);
}

BOOST_AUTO_TEST_CASE(application_campaign_binding_selects_the_owned_player_and_restores_startup)
{
    uqm::UQM_Context context;
    uqm::SessionDispatcher dispatcher{context};
    auto &startup = uqm::gameSession();
    auto *startupSIS = GetPlayerSISState();
    struct Campaign final : uqm::GameSession {
        bool &visited;
        Campaign(uqm::SessionDispatcher &host, bool &visited)
            : GameSession(host, Entry::NewGame, 19), visited(visited) {}
        uqm::SessionResult run() override {
            BOOST_TEST(&uqm::gameSession() == this);
            BOOST_TEST(&uqm::detail::sessionAssets() == &assets());
            BOOST_TEST(!uqm::detail::standaloneRuntime());
            player().sis().setCrew(17);
            BOOST_TEST(GetPlayerSISState()->CrewEnlisted == 17u);
            *GetNextSessionActivity() = 8;
            visited = true;
            return uqm::ApplicationExit::completed();
        }
    };
    bool visited = false;
    const auto exit = dispatcher.run(std::make_unique<Campaign>(dispatcher, visited));
    BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(uqm::ApplicationExitKind::Shutdown));
    BOOST_TEST(visited);
    BOOST_TEST(&uqm::gameSession() == &startup);
    BOOST_TEST(GetPlayerSISState() == startupSIS);
}

BOOST_AUTO_TEST_CASE(context_owns_stable_configuration_across_session_transitions_and_shutdown)
{
    auto *startup = GetGameConfiguration();
    const auto original = *startup;
    struct Restore { uqm::GameConfiguration value; ~Restore() { *GetGameConfiguration() = value; } } restore{original};
    startup->musicScale = 0.35f; // Command-line/config-file startup precedes context construction.
    {
        uqm::UQM_Context context;
        BOOST_TEST(&context.configuration() == startup);
        BOOST_TEST(context.configuration().musicScale == 0.35f);
        BOOST_CHECK_THROW(uqm::UQM_Context{}, std::logic_error);
        uqm::SessionDispatcher dispatcher{context};
        struct Visit final : uqm::Session {
            uqm::SessionDispatcher &host;
            bool first;
            Visit(uqm::SessionDispatcher &host, bool first)
                : Session(host, RuntimeKind::Frontend, 3), host(host), first(first) {}
            uqm::SessionResult run() override {
                if (first) {
                    context().configuration().subtitles = TRUE;
                    context().configuration().musicDisabled = TRUE;
                    context().configuration().inputTemplates[0].name[0] = 'X';
                    return std::make_unique<Visit>(host, false);
                }
                BOOST_TEST(GetGameConfiguration()->subtitles == TRUE);
                BOOST_TEST(context().configuration().musicDisabled == TRUE);
                BOOST_TEST(GetGameConfiguration()->inputTemplates[0].name[0] == 'X');
                return uqm::ApplicationExit::completed();
            }
        };
        const auto exit = dispatcher.run(std::make_unique<Visit>(dispatcher, true));
        BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(uqm::ApplicationExitKind::Shutdown));
    }
    BOOST_TEST(GetGameConfiguration() == startup);
    BOOST_TEST(startup->musicScale == 0.35f); // Backend shutdown still sees the same record.
    BOOST_TEST(startup->subtitles == TRUE);
}
