#include <boost/test/unit_test.hpp>
#include <SDL_timer.h>
#include <vector>
#include "libs/alarm.h"

namespace {
Uint32 nowMs;
std::vector<int> fired;
void record(void *arg) { fired.push_back(*static_cast<const int *>(arg)); }
struct Scheduler {
    Scheduler() { nowMs = 0; fired.clear(); Alarm_init(); }
    ~Scheduler() { Alarm_uninit(); }
};
struct Reentrant {
    Alarm *victim;
    int added = 3;
    bool nested = false;
};
void dispatchAgain(void *arg) {
    auto &state = *static_cast<Reentrant *>(arg);
    fired.push_back(1);
    Alarm_remove(state.victim);
    Alarm_addRelativeMs(0, record, &state.added);
    state.nested = Alarm_processOne();
}
}

extern "C" Uint32 SDL_GetTicks(void) { return nowMs; }

BOOST_FIXTURE_TEST_CASE(alarm_absolute_deadlines, Scheduler) {
    BOOST_TEST(Alarm_timeBeforeNextMs() == UINT32_MAX);
    BOOST_TEST(!Alarm_processOne());
    int first = 1, second = 2;
    Alarm_addAbsoluteMs(40, record, &second);
    Alarm_addAbsoluteMs(10, record, &first);
    nowMs = 9;
    BOOST_TEST(!Alarm_processOne());
    nowMs = 10;
    BOOST_TEST(Alarm_processOne());
    BOOST_TEST(Alarm_timeBeforeNextMs() == 40u);
    nowMs = 40;
    BOOST_TEST(Alarm_processOne());
    const std::vector<int> expected{1, 2};
    BOOST_TEST(fired == expected, boost::test_tools::per_element());
    BOOST_TEST(Alarm_timeBeforeNextMs() == UINT32_MAX);
}

BOOST_FIXTURE_TEST_CASE(alarm_cancellation_and_equal_deadlines, Scheduler) {
    int first = 1, canceled = 99, last = 3;
    Alarm_addAbsoluteMs(10, record, &first);
    auto middle = Alarm_addAbsoluteMs(10, record, &canceled);
    Alarm_addAbsoluteMs(10, record, &last);
    auto earlier = Alarm_addAbsoluteMs(5, record, &canceled);
    auto later = Alarm_addAbsoluteMs(30, record, &canceled);
    Alarm_remove(middle);
    Alarm_remove(earlier);
    Alarm_remove(later);
    nowMs = 10;
    BOOST_TEST(Alarm_processOne());
    BOOST_TEST(Alarm_processOne());
    BOOST_TEST(!Alarm_processOne());
    const std::vector<int> expected{1, 3};
    BOOST_TEST(fired == expected, boost::test_tools::per_element());
}

BOOST_FIXTURE_TEST_CASE(alarm_relative_deadlines, Scheduler) {
    nowMs = 100;
    int immediate = 1, delayed = 2;
    Alarm_addRelativeMs(25, record, &delayed);
    Alarm_addRelativeMs(0, record, &immediate);
    BOOST_TEST(Alarm_timeBeforeNextMs() == 100u);
    BOOST_TEST(Alarm_processOne());
    BOOST_TEST(Alarm_timeBeforeNextMs() == 125u);
    nowMs = 124;
    BOOST_TEST(!Alarm_processOne());
    nowMs = 125;
    BOOST_TEST(Alarm_processOne());
    const std::vector<int> expected{1, 2};
    BOOST_TEST(fired == expected, boost::test_tools::per_element());
}

BOOST_FIXTURE_TEST_CASE(alarm_reentrant_callbacks, Scheduler) {
    Reentrant state{};
    int canceled = 99, kept = 2;
    Alarm_addAbsoluteMs(10, dispatchAgain, &state);
    state.victim = Alarm_addAbsoluteMs(10, record, &canceled);
    Alarm_addAbsoluteMs(10, record, &kept);
    nowMs = 10;
    BOOST_TEST(Alarm_processOne());
    BOOST_TEST(state.nested);
    BOOST_TEST(Alarm_processOne());
    BOOST_TEST(!Alarm_processOne());
    const std::vector<int> expected{1, 2, 3};
    BOOST_TEST(fired == expected, boost::test_tools::per_element());
}

BOOST_FIXTURE_TEST_CASE(alarm_shutdown_discards_pending_callbacks, Scheduler) {
    int value = 1;
    Alarm_addAbsoluteMs(0, record, &value);
    Alarm_uninit();
    BOOST_TEST(fired.empty());
    Alarm_init();
    BOOST_TEST(Alarm_timeBeforeNextMs() == UINT32_MAX);
    Alarm_addRelativeMs(0, record, &value);
    BOOST_TEST(Alarm_processOne());
    BOOST_TEST(fired.size() == 1u);
}
