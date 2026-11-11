#include "campaign_clock.hpp"
#include "game_state.hpp"
#include "globdata.h"
#include "libs/log.h"
#include <cstdlib>
#include <stdexcept>
namespace {
uqm::CampaignClock &activeClock() {
    if (auto *runtime = uqm::detail::boundRuntime(); runtime && !runtime->campaign)
        throw std::logic_error("active session has no campaign clock");
    return uqm::gameSession().clock();
}
}
extern "C" {
CLOCK_STATE GetCampaignClockState(void) try { return activeClock().snapshot(); } catch (...) { std::abort(); }
void MoveGameClockDays(COUNT days) try { activeClock().advance(days); } catch (...) { std::abort(); }
BOOLEAN ValidateEvent(EVENT_TYPE type, COUNT *month, COUNT *day, COUNT *year) {
    try {
        const auto when = type == RELATIVE_EVENT ? activeClock().resolve({*year, *month, *day})
            : uqm::CampaignDate{*year, *month, *day};
        if (type == RELATIVE_EVENT) { *year = when.year; *month = when.month; *day = when.day; }
        return activeClock().accepts(when) ? TRUE : FALSE;
    } catch (...) { return FALSE; }
}
BOOLEAN AddEvent(EVENT_TYPE type, COUNT month, COUNT day, COUNT year, BYTE selector) try {
    const auto event = static_cast<uqm::CampaignEvent>(selector);
    const bool accepted = type == RELATIVE_EVENT ? activeClock().schedule(uqm::CalendarOffset{year, month, day}, event)
        : activeClock().schedule(uqm::CampaignDate{year, month, day}, event);
    return accepted ? TRUE : FALSE;
} catch (...) { std::abort(); }
size_t CampaignEventCount(void) try { return activeClock().events().size(); } catch (...) { std::abort(); }
BOOLEAN ReadCampaignEvent(size_t index, EVENT *out) try {
    const auto events = activeClock().events();
    if (index >= events.size()) return FALSE;
    const auto &entry = events[index];
    *out = {static_cast<BYTE>(entry.date.day), static_cast<BYTE>(entry.date.month),
            static_cast<COUNT>(entry.date.year), static_cast<BYTE>(entry.event)};
    return TRUE;
} catch (...) { std::abort(); }
void ClearCampaignEvents(void) try { activeClock().clearEvents(); } catch (...) { std::abort(); }
BOOLEAN RestoreCampaignEvent(const EVENT *event) {
    try {
        return activeClock().restoreEvent({{event->year_index, event->month_index, event->day_index},
                                    static_cast<uqm::CampaignEvent>(event->func_index)}) ? TRUE : FALSE;
    } catch (const std::exception &error) {
        log_add(log_Error, "Invalid saved campaign event: %s", error.what()); return FALSE;
    }
}
void ExportCampaignClock(void) try { GLOBAL(GameClock) = activeClock().snapshot(); } catch (...) { std::abort(); }
BOOLEAN ImportCampaignClock(void) {
    try { activeClock().restore(GLOBAL(GameClock)); return TRUE; }
    catch (const std::exception &error) {
        log_add(log_Error, "Invalid saved campaign clock: %s", error.what()); return FALSE;
    }
}
}
