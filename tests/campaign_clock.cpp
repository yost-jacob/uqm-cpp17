#include "uqm/campaign_clock.hpp"
#include "uqm/game_state.hpp"
#include "uqm/globdata.h"
#include <boost/test/unit_test.hpp>
#include <stdexcept>
#include <type_traits>

// Device/story boundaries: the isolated test binary does not link those engines.
namespace { uqm::GameSession *observedClockOwner = nullptr; }
extern "C" void EventHandler(BYTE) { observedClockOwner = &uqm::gameSession(); }
extern "C" void DrawStatusMessage(const UNICODE *) { observedClockOwner = &uqm::gameSession(); }
namespace {
using namespace uqm;
CLOCK_STATE saved(CampaignDate date, SIZE remaining = 0, SIZE period = 0) {
    return {static_cast<BYTE>(date.day), static_cast<BYTE>(date.month),
            static_cast<COUNT>(date.year), remaining, period};
}
void checkDate(const CampaignClock &clock, CampaignDate date) {
    BOOST_TEST(clock.date().year == date.year);
    BOOST_TEST(clock.date().month == date.month);
    BOOST_TEST(clock.date().day == date.day);
}
}
static_assert(std::is_same_v<decltype(std::declval<const uqm::GameSession &>().clock()), const uqm::CampaignClock &>);

BOOST_AUTO_TEST_CASE(campaign_clock_uses_simulation_ticks_and_preserves_rate_fraction) {
    uqm::CampaignClock clock;
    clock.tick();
    checkDate(clock, {2155, 2, 17});
    clock.setRate(2);
    for (int i = 0; i < 12; ++i) clock.tick();
    BOOST_TEST(clock.snapshot().tick_count == 36);
    clock.setRate(1);
    BOOST_TEST(clock.snapshot().tick_count == 18);
    for (int i = 0; i < 17; ++i) clock.tick();
    checkDate(clock, {2155, 2, 17});
    clock.tick();
    checkDate(clock, {2155, 2, 18});
    clock.setRate(0);
    for (int i = 0; i < 40000; ++i) clock.tick();
    checkDate(clock, {2155, 2, 18});
    BOOST_CHECK_THROW(clock.setRate(65535), std::out_of_range);
}

BOOST_AUTO_TEST_CASE(campaign_calendar_preserves_gregorian_and_relative_offset_rules) {
    uqm::CampaignClock clock;
    clock.restore(saved({2000, 2, 28})); clock.advance(1); checkDate(clock, {2000, 2, 29});
    clock.advance(1); checkDate(clock, {2000, 3, 1});
    clock.restore(saved({1900, 2, 28})); clock.advance(1); checkDate(clock, {1900, 3, 1});
    clock.restore(saved({2155, 12, 31})); clock.advance(1); checkDate(clock, {2156, 1, 1});
    clock.restore(saved({2001, 1, 31}));
    const auto result = clock.resolve({0, 1, 0});
    BOOST_TEST(result.month == 3u); BOOST_TEST(result.day == 3u);
    BOOST_CHECK_THROW(clock.restore(saved({2001, 2, 29})), std::invalid_argument);
    checkDate(clock, {2001, 1, 31});
    clock.restore(saved({65535, 12, 31}));
    BOOST_CHECK_THROW(clock.advance(1), std::out_of_range);
}

BOOST_AUTO_TEST_CASE(campaign_events_are_stable_and_handlers_can_schedule_same_day_events) {
    using E = uqm::CampaignEvent;
    std::vector<unsigned> calls;
    uqm::CampaignClock *owner = nullptr;
    uqm::CampaignClock clock([&](E event) {
        calls.push_back(static_cast<unsigned>(event));
        BOOST_TEST(owner->date().day == 18u); // Reentrant const query under the clock lock.
        if (event == E::ArilouEntrance)
            BOOST_TEST(owner->schedule(owner->date(), E::ShofixtiReturn));
    }, [&] { calls.push_back(100); });
    owner = &clock;
    BOOST_TEST(clock.schedule(uqm::CalendarOffset{0,0,1}, E::ArilouEntrance));
    BOOST_TEST(clock.schedule(uqm::CalendarOffset{0,0,1}, E::ArilouExit));
    clock.advance(1);
    const std::vector<unsigned> expected{100, unsigned(E::ArilouEntrance), unsigned(E::ArilouExit), unsigned(E::ShofixtiReturn)};
    BOOST_TEST(calls == expected, boost::test_tools::per_element());
    BOOST_TEST(clock.events().empty());
}

BOOST_AUTO_TEST_CASE(campaign_event_capacity_and_immediate_dispatch_match_legacy_contract) {
    using E = uqm::CampaignEvent;
    unsigned calls = 0;
    uqm::CampaignClock clock([&](E) { ++calls; });
    BOOST_TEST(clock.schedule(uqm::CalendarOffset{}, E::ArilouEntrance));
    BOOST_TEST(calls == 1u);
    BOOST_TEST(clock.events().empty());
    for (std::size_t i = 0; i < clock.MaxPendingEvents; ++i)
        BOOST_TEST(clock.schedule(uqm::CalendarOffset{0,0,1}, E::ArilouExit));
    BOOST_TEST(!clock.schedule(uqm::CalendarOffset{0,0,2}, E::ShofixtiReturn));
    BOOST_TEST(!clock.schedule(uqm::CampaignDate{2155,2,16}, E::ArilouExit));
    BOOST_CHECK_THROW(clock.schedule(uqm::CampaignDate{2155,13,1}, E::ArilouExit), std::invalid_argument);
    BOOST_CHECK_THROW(clock.schedule(uqm::CalendarOffset{}, static_cast<E>(255)), std::invalid_argument);
    clock.advance(1);
    BOOST_TEST(calls == 1u + clock.MaxPendingEvents);
}

BOOST_AUTO_TEST_CASE(campaign_clock_restore_is_detached_and_does_not_dispatch_events) {
    using E = uqm::CampaignEvent;
    unsigned calls = 0;
    uqm::CampaignClock original;
    original.setRate(5);
    original.tick();
    BOOST_TEST(original.schedule(uqm::CalendarOffset{0,0,1}, E::ArilouEntrance));
    const auto snapshot = original.snapshot();
    auto events = original.events();
    uqm::CampaignClock restored([&](E) { ++calls; });
    restored.restore(snapshot);
    for (const auto &event : events) BOOST_TEST(restored.restoreEvent(event));
    events.clear();
    BOOST_TEST(restored.events().size() == 1u);
    BOOST_TEST(calls == 0u);
    BOOST_TEST(restored.snapshot().tick_count == 119);
    restored.advance(1);
    BOOST_TEST(calls == 1u);
    BOOST_TEST(original.events().size() == 1u);
}

BOOST_AUTO_TEST_CASE(campaign_clock_is_session_owned_and_c_save_projections_are_explicit) {
    using namespace uqm;
    auto state = std::make_shared<GameState>(UserProfile{SinglePlayerId, "Clock"});
    GameSession first{state, SinglePlayerId}, second{state, SinglePlayerId};
    first.clock().advance(3);
    BOOST_TEST(observedClockOwner == &first);
    checkDate(first.clock(), {2155,2,20}); checkDate(second.clock(), {2155,2,17});
    auto &clock = gameSession().clock();
    clock.reset(); clock.advance(4); clock.setRate(5);
    BOOST_TEST(AddEvent(RELATIVE_EVENT, 0, 2, 0, ARILOU_EXIT_EVENT));
    ExportCampaignClock();
    const auto snapshot = GLOBAL(GameClock);
    EVENT event{};
    BOOST_REQUIRE(ReadCampaignEvent(0, &event));
    clock.reset();
    GLOBAL(GameClock) = snapshot;
    BOOST_TEST(ImportCampaignClock());
    BOOST_TEST(RestoreCampaignEvent(&event));
    checkDate(clock, {2155,2,21});
    BOOST_TEST(CampaignEventCount() == 1u);
    GLOBAL(GameClock).month_index = 13;
    BOOST_TEST(!ImportCampaignClock());
    checkDate(clock, {2155,2,21});
    clock.reset();
}

BOOST_AUTO_TEST_CASE(campaign_clock_rejects_reset_from_an_immediate_handler) {
    uqm::CampaignClock *owner = nullptr;
    uqm::CampaignClock clock([&](uqm::CampaignEvent) {
        BOOST_CHECK_THROW(owner->reset(), std::logic_error);
        BOOST_CHECK_THROW(owner->restore(owner->snapshot()), std::logic_error);
    });
    owner = &clock;
    BOOST_TEST(clock.schedule(uqm::CalendarOffset{}, uqm::CampaignEvent::ArilouEntrance));
    clock.reset(); // Callback guards were released after returning.
}

BOOST_AUTO_TEST_CASE(campaign_clock_reserves_the_dispatching_events_capacity_slot) {
    uqm::CampaignClock *owner = nullptr;
    unsigned callbacks = 0;
    bool accepted = true;
    uqm::CampaignClock clock([&](uqm::CampaignEvent) {
        if (++callbacks == 1)
            accepted = owner->schedule(uqm::CalendarOffset{0,0,1}, uqm::CampaignEvent::ArilouExit);
    });
    owner = &clock;
    for (std::size_t i = 0; i < clock.MaxPendingEvents; ++i)
        BOOST_TEST(clock.schedule(uqm::CalendarOffset{0,0,1}, uqm::CampaignEvent::ArilouEntrance));
    clock.advance(1);
    BOOST_TEST(!accepted);
    BOOST_TEST(clock.events().empty());
}
