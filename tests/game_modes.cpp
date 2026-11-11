#include "uqm/game_mode.hpp"
#include "uqm/game_state.hpp"
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <vector>
#include <string>
#include <stdexcept>

namespace {
using namespace uqm;
struct Counts { unsigned live = 0, peak = 0, runs = 0; };
class CountingMode final : public GameMode {
public:
    CountingMode(ModeDispatcher &host, unsigned remaining, Counts &counts)
        : GameMode(host), remaining_(remaining), counts_(counts) {
        ++counts_.live;
        counts_.peak = std::max(counts_.peak, counts_.live);
    }
    ~CountingMode() override { --counts_.live; }
    ModeResult run() override {
        ++counts_.runs;
        if (!remaining_) return SessionExit::title();
        return std::make_unique<CountingMode>(dispatcher(), remaining_ - 1, counts_);
    }
private:
    unsigned remaining_;
    Counts &counts_;
};
class ExitMode final : public GameMode {
public:
    ExitMode(ModeDispatcher &host, SessionExit exit, Counts &counts)
        : GameMode(host), exit_(exit), counts_(counts) { ++counts_.live; }
    ~ExitMode() override { --counts_.live; }
    ModeResult run() override { ++counts_.runs; return exit_; }
private:
    SessionExit exit_;
    Counts &counts_;
};
class BrokenMode final : public GameMode {
public:
    BrokenMode(ModeDispatcher &host, bool throwError, Counts &counts)
        : GameMode(host), throw_(throwError), counts_(counts) { ++counts_.live; }
    ~BrokenMode() override { --counts_.live; }
    ModeResult run() override {
        if (throw_) throw std::runtime_error("mode failure");
        return std::unique_ptr<GameMode>{};
    }
private:
    bool throw_;
    Counts &counts_;
};
std::shared_ptr<GameState> makeState() {
    return std::make_shared<GameState>(UserProfile{SinglePlayerId, "Captain"});
}
}
BOOST_AUTO_TEST_CASE(mode_transitions_are_flat_and_release_the_previous_mode)
{
    auto data = makeState();
    uqm::GameSession session{data, uqm::SinglePlayerId};
    Counts counts;
    const auto exit = session.run(std::make_unique<CountingMode>(session.dispatcher(), 10000, counts));
    BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(uqm::SessionExitKind::ReturnToTitle));
    BOOST_TEST(counts.runs == 10001u);
    BOOST_TEST(counts.peak == 2u);
    BOOST_TEST(counts.live == 0u);
}
BOOST_AUTO_TEST_CASE(mode_load_result_keeps_slot_zero_and_unwinds_before_the_caller_loads)
{
    auto data = makeState();
    uqm::GameSession session{data, uqm::SinglePlayerId};
    session.player().setFuel(123);
    Counts counts;
    const auto exit = session.run(std::make_unique<ExitMode>(session.dispatcher(), uqm::SessionExit::load(0), counts));
    BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(uqm::SessionExitKind::LoadGame));
    BOOST_REQUIRE(exit.loadSlot);
    BOOST_TEST(*exit.loadSlot == 0u);
    BOOST_TEST(counts.live == 0u);
    BOOST_TEST(session.player().ship().fuel == 123u); // Dispatching a request does not decode anything.
}
BOOST_AUTO_TEST_CASE(null_modes_and_cpp_exceptions_fail_the_session_without_leaking_modes)
{
    auto data = makeState();
    uqm::GameSession session{data, uqm::SinglePlayerId};
    Counts counts;
    for (const bool throws : {false, true}) {
        const auto exit = session.run(std::make_unique<BrokenMode>(session.dispatcher(), throws, counts));
        BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(uqm::SessionExitKind::Failed));
        BOOST_REQUIRE(exit.error);
        BOOST_CHECK_THROW(std::rethrow_exception(exit.error), std::exception);
        BOOST_TEST(counts.live == 0u);
    }
    const auto absent = session.run(nullptr);
    BOOST_TEST(static_cast<int>(absent.kind) == static_cast<int>(uqm::SessionExitKind::Failed));
    const auto recovered = session.run(std::make_unique<ExitMode>(session.dispatcher(), uqm::SessionExit::title(), counts));
    BOOST_TEST(static_cast<int>(recovered.kind) == static_cast<int>(uqm::SessionExitKind::ReturnToTitle));
}
BOOST_AUTO_TEST_CASE(menu_requests_propagate_after_the_legacy_mode_returns_and_shutdown_has_priority)
{
    struct MenuMode final : uqm::GameMode {
        MenuMode(uqm::GameSession &session, bool shutdown, Counts &counts)
            : GameMode(session.dispatcher()), session_(session), shutdown_(shutdown), counts_(counts) { ++counts_.live; }
        ~MenuMode() override { --counts_.live; }
        uqm::ModeResult run() override {
            const uqm::MenuResult menu = uqm::SessionExit::load(7);
            if (menu) session_.requestMenuExit(*menu);
            session_.requestMenuExit(uqm::SessionExit::title()); // An outer C abort cannot erase the load.
            if (shutdown_) return uqm::SessionExit::shutdown();
            return std::make_unique<CountingMode>(dispatcher(), 0, counts_);
        }
        uqm::GameSession &session_;
        bool shutdown_;
        Counts &counts_;
    };
    auto data = makeState();
    uqm::GameSession session{data, uqm::SinglePlayerId};
    Counts counts;
    for (bool shutdown : {false, true}) {
        const auto exit = session.run(std::make_unique<MenuMode>(session, shutdown, counts));
        const auto expected = shutdown ? uqm::SessionExitKind::Shutdown : uqm::SessionExitKind::LoadGame;
        BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(expected));
        BOOST_TEST(counts.live == 0u);
        BOOST_TEST(counts.runs == 0u); // Successor was destroyed, never run.
        BOOST_TEST(!session.menuExit().has_value());
    }
}
BOOST_AUTO_TEST_CASE(modes_validate_ownership_and_load_exits_require_a_slot)
{
    Counts counts;
    auto data = makeState();
    uqm::GameSession session{data, uqm::SinglePlayerId};
    uqm::ModeDispatcher foreign;
    const auto rejected = session.run(std::make_unique<CountingMode>(foreign, 0, counts));
    BOOST_TEST(static_cast<int>(rejected.kind) == static_cast<int>(uqm::SessionExitKind::Failed));
    BOOST_TEST(counts.runs == 0u);
    BOOST_TEST(counts.live == 0u);
    const uqm::SessionExit invalid{uqm::SessionExitKind::LoadGame, {}, {}};
    BOOST_CHECK_THROW(session.requestMenuExit(invalid), std::invalid_argument);
    const auto exit = session.run(std::make_unique<ExitMode>(session.dispatcher(), invalid, counts));
    BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(uqm::SessionExitKind::Failed));
    BOOST_TEST(counts.live == 0u);
}

namespace {
class NestedMode final : public uqm::GameMode {
public:
    NestedMode(uqm::GameSession &session,
            Counts &counts, unsigned depth, bool root, bool load)
        : GameMode(session.dispatcher()), session_(session), counts_(counts),
          depth_(depth), root_(root), load_(load) { ++counts_.live; }
    ~NestedMode() override { --counts_.live; }
    uqm::ModeResult run() override {
        ++counts_.runs;
        const unsigned local = depth_ + 17; // Must survive child execution.
        if (depth_) {
            auto exit = session_.swapGameMode(std::make_unique<NestedMode>(
                    session_, counts_, depth_ - 1, false, load_));
            BOOST_TEST(local == depth_ + 17);
            BOOST_TEST(counts_.live == (root_ ? 1u : 2u));
            if (exit) return *exit;
        } else if (load_) return uqm::SessionExit::load(0);
        if (root_) return uqm::SessionExit::title();
        return uqm::ModeCompleted{};
    }
private:
    uqm::GameSession &session_;
    Counts &counts_;
    unsigned depth_;
    bool root_, load_;
};
}
BOOST_AUTO_TEST_CASE(nested_modes_restore_suspended_parents_without_running_them_again)
{
    auto data = makeState();
    uqm::GameSession session{data, uqm::SinglePlayerId};
    Counts counts;
    auto exit = session.run(std::make_unique<NestedMode>(session, counts, 2, true, false));
    BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(uqm::SessionExitKind::ReturnToTitle));
    BOOST_TEST(counts.runs == 3u);
    BOOST_TEST(counts.live == 0u);
}
BOOST_AUTO_TEST_CASE(nested_load_requests_restore_ownership_and_unwind_before_loading)
{
    auto data = makeState();
    uqm::GameSession session{data, uqm::SinglePlayerId};
    Counts counts;
    auto exit = session.run(std::make_unique<NestedMode>(session, counts, 2, true, true));
    BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(uqm::SessionExitKind::LoadGame));
    BOOST_REQUIRE(exit.loadSlot);
    BOOST_TEST(*exit.loadSlot == 0u);
    BOOST_TEST(counts.live == 0u);
}

BOOST_AUTO_TEST_CASE(nested_child_exceptions_restore_parent_and_unwind)
{
    struct Parent final : uqm::GameMode {
        uqm::GameSession &session;
        Counts &counts;
        Parent(uqm::GameSession &owner, Counts &calls)
            : GameMode(owner.dispatcher()), session(owner), counts(calls) {}
        uqm::ModeResult run() override {
            session.swapGameMode(std::make_unique<BrokenMode>(dispatcher(), true, counts));
            BOOST_FAIL("child failure must propagate");
            return uqm::SessionExit::title();
        }
    };
    auto data = makeState();
    uqm::GameSession session{data, uqm::SinglePlayerId};
    Counts counts;
    const auto exit = session.run(std::make_unique<Parent>(session, counts));
    BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(uqm::SessionExitKind::Failed));
    BOOST_REQUIRE(exit.error);
    BOOST_TEST(counts.live == 0u);
    BOOST_CHECK_THROW(std::rethrow_exception(exit.error), std::runtime_error);
}


BOOST_AUTO_TEST_CASE(dispatcher_automates_child_cleanup_renderer_restore_and_parent_redraw_in_order)
{
    for (const bool load : {false, true}) {
        std::vector<std::string> events;
        auto data = makeState();
        uqm::GameSession session{data, uqm::SinglePlayerId};
        struct Token final : uqm::ModeSuspension {
            std::vector<std::string> &events;
            explicit Token(std::vector<std::string> &value) : events(value) {}
            ~Token() override { events.push_back("bindings restored"); }
        };
        struct Child final : uqm::GameMode {
            std::vector<std::string> &events;
            bool load;
            Child(uqm::ModeDispatcher &host, std::vector<std::string> &log, bool requestLoad)
                : GameMode(host), events(log), load(requestLoad) {}
            ~Child() override { events.push_back("child cleanup"); }
            uqm::ModeResult run() override {
                events.push_back("child runs");
                if (load) return uqm::SessionExit::load(0);
                return uqm::ModeCompleted{};
            }
        };
        struct Parent final : uqm::GameMode {
            uqm::GameSession &session;
            std::vector<std::string> &events;
            bool load;
            Parent(uqm::GameSession &owner, std::vector<std::string> &log, bool requestLoad)
                : GameMode(owner.dispatcher()), session(owner), events(log), load(requestLoad) {}
            ~Parent() override { events.push_back("parent cleanup"); }
            std::unique_ptr<uqm::ModeSuspension> suspend() override {
                events.push_back("parent rendering suspended");
                return std::make_unique<Token>(events);
            }
            void resume() override { events.push_back("parent redraw"); }
            uqm::ModeResult run() override {
                auto exit = session.swapGameMode(std::make_unique<Child>(dispatcher(), events, load));
                events.push_back("parent stack resumes");
                if (exit) return *exit;
                return uqm::ModeCompleted{};
            }
        };
        const auto exit = session.run(std::make_unique<Parent>(session, events, load));
        std::vector<std::string> expected{"parent rendering suspended", "child runs", "child cleanup", "bindings restored"};
        if (!load) expected.push_back("parent redraw");
        expected.push_back("parent stack resumes"); expected.push_back("parent cleanup");
        BOOST_TEST(events == expected, boost::test_tools::per_element());
        BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(load ? uqm::SessionExitKind::LoadGame : uqm::SessionExitKind::ReturnToTitle));
    }
}

BOOST_AUTO_TEST_CASE(dispatcher_runs_without_campaign_state_and_rejects_foreign_children_and_successors)
{
    uqm::ModeDispatcher host, foreign;
    Counts counts;
    struct Parent final : uqm::GameMode {
        uqm::ModeDispatcher &foreign;
        Counts &counts;
        Parent(uqm::ModeDispatcher &host, uqm::ModeDispatcher &other, Counts &value)
            : GameMode(host), foreign(other), counts(value) {}
        uqm::ModeResult run() override {
            BOOST_CHECK_THROW(dispatcher().swap(
                std::make_unique<ExitMode>(foreign, uqm::SessionExit::title(), counts)), std::invalid_argument);
            BOOST_TEST(dispatcher().isRunning());
            return std::make_unique<ExitMode>(foreign, uqm::SessionExit::title(), counts);
        }
    };
    const auto rejected = host.run(std::make_unique<Parent>(host, foreign, counts));
    BOOST_TEST(static_cast<int>(rejected.kind) == static_cast<int>(uqm::SessionExitKind::Failed));
    BOOST_TEST(counts.live == 0u);
    BOOST_TEST(counts.runs == 0u);
    const auto recovered = host.run(std::make_unique<CountingMode>(host, 2, counts));
    BOOST_TEST(static_cast<int>(recovered.kind) == static_cast<int>(uqm::SessionExitKind::ReturnToTitle));
    BOOST_TEST(counts.runs == 3u);
    BOOST_TEST(!foreign.isRunning());
}

BOOST_AUTO_TEST_CASE(child_combat_style_handoff_preserves_the_exploration_location_borrow)
{
    auto data = makeState();
    uqm::GameSession session{data, uqm::SinglePlayerId};
    auto &location = session.player().enterSolarSystem();
    location.system = StarId{7};
    location.flight.position = {123, 456};
    location.enterOrbit({uqm::SinglePlayerUniverse, StarId{7}, 2, 0});
    class Child final : public uqm::GameMode {
    public:
        explicit Child(uqm::ModeDispatcher &dispatcher) : GameMode(dispatcher) {}
        uqm::ModeResult run() override { return uqm::ModeCompleted{}; }
    };
    class Parent final : public uqm::GameMode {
    public:
        explicit Parent(uqm::GameSession &session) : GameMode(session.dispatcher()), session_(session) {}
        uqm::ModeResult run() override {
            auto &before = std::get<uqm::SolarSystemLocation>(session_.player().location());
            BOOST_TEST(!session_.swapGameMode(std::make_unique<Child>(dispatcher())).has_value());
            BOOST_TEST(&before == &std::get<uqm::SolarSystemLocation>(session_.player().location()));
            BOOST_TEST(before.flight.position.x == 123);
            BOOST_TEST(before.flight.position.y == 456);
            BOOST_REQUIRE(before.orbit);
            BOOST_TEST(before.orbit->world.planet == 2u);
            BOOST_TEST(before.orbit->world.moon.value() == 0u);
            return uqm::ModeCompleted{};
        }
    private: uqm::GameSession &session_;
    };
    session.run(std::make_unique<Parent>(session));
    BOOST_TEST(&location == &std::get<uqm::SolarSystemLocation>(session.player().location()));
}

BOOST_AUTO_TEST_CASE(dispatcher_reports_a_wrong_location_alternative_as_a_failure)
{
    auto data = makeState();
    uqm::GameSession session{data, uqm::SinglePlayerId};
    class WrongLocation final : public uqm::GameMode {
    public:
        explicit WrongLocation(uqm::GameSession &session) : GameMode(session.dispatcher()), session_(session) {}
        uqm::ModeResult run() override {
            std::get<uqm::SolarSystemLocation>(session_.player().location());
            return uqm::ModeCompleted{};
        }
    private: uqm::GameSession &session_;
    };
    const auto exit = session.run(std::make_unique<WrongLocation>(session));
    BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(uqm::SessionExitKind::Failed));
    BOOST_REQUIRE(exit.error);
    BOOST_CHECK_THROW(std::rethrow_exception(exit.error), std::bad_variant_access);
}
