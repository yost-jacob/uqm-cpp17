#include "uqm/game_mode.hpp"
#include "uqm/game_state.hpp"
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <stdexcept>

namespace {
using namespace uqm;
struct Counts { unsigned live = 0, peak = 0, runs = 0; };
class CountingMode final : public GameMode {
public:
    CountingMode(std::shared_ptr<GameState> state, unsigned remaining, Counts &counts)
        : GameMode(std::move(state), SinglePlayerId), remaining_(remaining), counts_(counts) {
        ++counts_.live;
        counts_.peak = std::max(counts_.peak, counts_.live);
    }
    ~CountingMode() override { --counts_.live; }
    ModeResult run() override {
        ++counts_.runs;
        if (!remaining_) return SessionExit::title();
        return std::make_unique<CountingMode>(stateOwner(), remaining_ - 1, counts_);
    }
private:
    unsigned remaining_;
    Counts &counts_;
};
class ExitMode final : public GameMode {
public:
    ExitMode(std::shared_ptr<GameState> state, SessionExit exit, Counts &counts)
        : GameMode(std::move(state), SinglePlayerId), exit_(exit), counts_(counts) { ++counts_.live; }
    ~ExitMode() override { --counts_.live; }
    ModeResult run() override { ++counts_.runs; return exit_; }
private:
    SessionExit exit_;
    Counts &counts_;
};
class BrokenMode final : public GameMode {
public:
    BrokenMode(std::shared_ptr<GameState> state, bool throwError, Counts &counts)
        : GameMode(std::move(state), SinglePlayerId), throw_(throwError), counts_(counts) { ++counts_.live; }
    ~BrokenMode() override { --counts_.live; }
    ModeResult run() override {
        if (throw_) throw std::runtime_error("mode failure");
        return std::unique_ptr<GameMode>{};
    }
private:
    bool throw_;
    Counts &counts_;
};
std::shared_ptr<GameState> state() {
    return std::make_shared<GameState>(UserProfile{SinglePlayerId, "Captain"});
}
}
BOOST_AUTO_TEST_CASE(mode_transitions_are_flat_and_release_the_previous_mode)
{
    auto data = state();
    uqm::GameSession session{data, uqm::SinglePlayerId};
    Counts counts;
    const auto exit = session.run(std::make_unique<CountingMode>(data, 10000, counts));
    BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(uqm::SessionExitKind::ReturnToTitle));
    BOOST_TEST(counts.runs == 10001u);
    BOOST_TEST(counts.peak == 2u);
    BOOST_TEST(counts.live == 0u);
}
BOOST_AUTO_TEST_CASE(mode_load_result_keeps_slot_zero_and_unwinds_before_the_caller_loads)
{
    auto data = state();
    uqm::GameSession session{data, uqm::SinglePlayerId};
    session.player().setFuel(123);
    Counts counts;
    const auto exit = session.run(std::make_unique<ExitMode>(data, uqm::SessionExit::load(0), counts));
    BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(uqm::SessionExitKind::LoadGame));
    BOOST_REQUIRE(exit.loadSlot);
    BOOST_TEST(*exit.loadSlot == 0u);
    BOOST_TEST(counts.live == 0u);
    BOOST_TEST(session.player().ship().fuel == 123u); // Dispatching a request does not decode anything.
}
BOOST_AUTO_TEST_CASE(null_modes_and_cpp_exceptions_fail_the_session_without_leaking_modes)
{
    auto data = state();
    uqm::GameSession session{data, uqm::SinglePlayerId};
    Counts counts;
    for (const bool throws : {false, true}) {
        const auto exit = session.run(std::make_unique<BrokenMode>(data, throws, counts));
        BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(uqm::SessionExitKind::Failed));
        BOOST_REQUIRE(exit.error);
        BOOST_CHECK_THROW(std::rethrow_exception(exit.error), std::exception);
        BOOST_TEST(counts.live == 0u);
    }
    const auto absent = session.run(nullptr);
    BOOST_TEST(static_cast<int>(absent.kind) == static_cast<int>(uqm::SessionExitKind::Failed));
    const auto recovered = session.run(std::make_unique<ExitMode>(data, uqm::SessionExit::title(), counts));
    BOOST_TEST(static_cast<int>(recovered.kind) == static_cast<int>(uqm::SessionExitKind::ReturnToTitle));
}
BOOST_AUTO_TEST_CASE(menu_requests_propagate_after_the_legacy_mode_returns_and_shutdown_has_priority)
{
    struct MenuMode final : uqm::GameMode {
        MenuMode(std::shared_ptr<uqm::GameState> data, uqm::GameSession &session, bool shutdown, Counts &counts)
            : GameMode(std::move(data), uqm::SinglePlayerId), session_(session), shutdown_(shutdown), counts_(counts) { ++counts_.live; }
        ~MenuMode() override { --counts_.live; }
        uqm::ModeResult run() override {
            const uqm::MenuResult menu = uqm::SessionExit::load(7);
            if (menu) session_.requestMenuExit(*menu);
            session_.requestMenuExit(uqm::SessionExit::title()); // An outer C abort cannot erase the load.
            if (shutdown_) return uqm::SessionExit::shutdown();
            return std::make_unique<CountingMode>(stateOwner(), 0, counts_);
        }
        uqm::GameSession &session_;
        bool shutdown_;
        Counts &counts_;
    };
    auto data = state();
    uqm::GameSession session{data, uqm::SinglePlayerId};
    Counts counts;
    for (bool shutdown : {false, true}) {
        const auto exit = session.run(std::make_unique<MenuMode>(data, session, shutdown, counts));
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
    BOOST_CHECK_THROW((CountingMode{nullptr, 0, counts}), std::invalid_argument);
    auto wrongPlayer = std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::PlayerId{2}, "Other"});
    BOOST_CHECK_THROW((CountingMode{wrongPlayer, 0, counts}), std::out_of_range);
    auto data = state();
    uqm::GameSession session{data, uqm::SinglePlayerId};
    const uqm::SessionExit invalid{uqm::SessionExitKind::LoadGame, {}, {}};
    BOOST_CHECK_THROW(session.requestMenuExit(invalid), std::invalid_argument);
    const auto exit = session.run(std::make_unique<ExitMode>(data, invalid, counts));
    BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(uqm::SessionExitKind::Failed));
    BOOST_TEST(counts.live == 0u);
}
