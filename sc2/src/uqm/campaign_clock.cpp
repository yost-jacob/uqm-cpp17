#include "campaign_clock.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
namespace uqm {
namespace {
constexpr unsigned daysInMonth(unsigned month, unsigned year) {
    constexpr unsigned days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)
        ? 29 : days[month - 1];
}
void validate(CampaignDate date) {
    if (date.year == 0 || date.year > std::numeric_limits<COUNT>::max()
        || date.month < 1 || date.month > 12 || date.day < 1
        || date.day > daysInMonth(date.month, date.year))
        throw std::invalid_argument("invalid campaign date");
}
void validate(CampaignEvent event) {
    if (static_cast<unsigned>(event) >= NUM_EVENTS)
        throw std::invalid_argument("unknown campaign event");
}
}
CampaignClock::CampaignClock(Handler handler, DateChanged changed)
    : handler_(std::move(handler)), changed_(std::move(changed)) { reset(); }
void CampaignClock::reset() {
    std::lock_guard lock(mutex_);
    if (callbackDepth_) throw std::logic_error("cannot reset the campaign inside an event handler");
    state_ = {17, 2, START_YEAR, 0, 0};
    events_.clear();
    events_.reserve(MaxPendingEvents);
}
CampaignDate CampaignClock::date() const {
    std::lock_guard lock(mutex_);
    return {state_.year_index, state_.month_index, state_.day_index};
}
CLOCK_STATE CampaignClock::snapshot() const { std::lock_guard lock(mutex_); return state_; }
void CampaignClock::restore(const CLOCK_STATE &state) {
    std::lock_guard lock(mutex_);
    validate({state.year_index, state.month_index, state.day_index});
    if (state.day_in_ticks > 0 && state.tick_count > state.day_in_ticks)
        throw std::invalid_argument("campaign tick count exceeds its period");
    if (state.day_in_ticks < 0) throw std::invalid_argument("negative campaign clock period");
    if (callbackDepth_) throw std::logic_error("cannot load the campaign inside an event handler");
    state_ = state;
    events_.clear();
}
std::vector<ScheduledCampaignEvent> CampaignClock::events() const {
    std::lock_guard lock(mutex_); return events_;
}
void CampaignClock::clearEvents() { std::lock_guard lock(mutex_); events_.clear(); }
bool CampaignClock::running() const { std::lock_guard lock(mutex_); return state_.day_in_ticks != 0; }
void CampaignClock::setRate(unsigned secondsPerDay) {
    std::lock_guard lock(mutex_);
    if (secondsPerDay > unsigned(std::numeric_limits<SIZE>::max()) / BaseFramerate)
        throw std::out_of_range("campaign clock period exceeds save field");
    const auto period = static_cast<SIZE>(secondsPerDay * BaseFramerate);
    SIZE remaining;
    if (!state_.day_in_ticks) remaining = period;
    else if (state_.tick_count <= 0) remaining = 0;
    else remaining = std::max<SIZE>(1, std::int64_t(state_.tick_count) * period / state_.day_in_ticks);
    state_.day_in_ticks = period;
    state_.tick_count = remaining;
}
CampaignDate CampaignClock::resolve(CalendarOffset offset) const {
    std::lock_guard lock(mutex_);
    // Wide arithmetic rejects overflow before narrowing into the save fields.
    const std::uint64_t months = std::uint64_t(state_.month_index) - 1 + offset.months;
    std::uint64_t year = std::uint64_t(state_.year_index) + offset.years + months / 12;
    unsigned month = months % 12 + 1;
    std::uint64_t day = std::uint64_t(state_.day_index) + offset.days;
    while (year <= std::numeric_limits<COUNT>::max() && day > daysInMonth(month, year)) {
        day -= daysInMonth(month, year);
        if (++month > 12) { month = 1; ++year; }
    }
    if (year > std::numeric_limits<COUNT>::max()) throw std::out_of_range("campaign year overflow");
    const CampaignDate result{unsigned(year), month, unsigned(day)};
    validate(result);
    return result;
}
bool CampaignClock::accepts(CampaignDate when) const {
    std::lock_guard lock(mutex_); validate(when); return !(when < date());
}
bool CampaignClock::schedule(CampaignDate when, CampaignEvent event) {
    std::lock_guard lock(mutex_);
    validate(when); validate(event);
    if (when < date() || events_.size() + dispatching_ >= MaxPendingEvents) return false;
    const auto at = std::upper_bound(events_.begin(), events_.end(), when,
        [](CampaignDate value, const auto &entry) { return value < entry.date; });
    events_.insert(at, {when, event});
    return true;
}
bool CampaignClock::schedule(CalendarOffset offset, CampaignEvent event) {
    std::lock_guard lock(mutex_); validate(event);
    if (!offset.years && !offset.months && !offset.days) {
        invoke(event);
        return true;
    }
    return schedule(resolve(offset), event);
}
bool CampaignClock::restoreEvent(ScheduledCampaignEvent event) { return schedule(event.date, event.event); }
void CampaignClock::invoke(CampaignEvent event) {
    ++callbackDepth_;
    try { if (handler_) handler_(event); }
    catch (...) { --callbackDepth_; throw; }
    --callbackDepth_;
}
void CampaignClock::nextDay() {
    const auto tomorrow = resolve(CalendarOffset{0, 0, 1});
    state_.year_index = tomorrow.year;
    state_.month_index = tomorrow.month;
    state_.day_index = tomorrow.day;
    if (changed_) changed_();
}
void CampaignClock::processEvents() {
    while (!events_.empty() && events_.front().date == date()) {
        const auto event = events_.front().event;
        events_.erase(events_.begin());
        // Preserve the old pool's occupied dispatch slot until its callback ends.
        ++dispatching_;
        try { invoke(event); }
        catch (...) { --dispatching_; throw; }
        --dispatching_;
    }
}
void CampaignClock::tick() {
    std::lock_guard lock(mutex_);
    if (!state_.day_in_ticks) return; // A paused clock cannot underflow its counter.
    if (state_.tick_count > 1) { --state_.tick_count; return; }
    state_.tick_count = state_.day_in_ticks;
    nextDay();
    processEvents();
}
void CampaignClock::advance(unsigned days) {
    std::lock_guard lock(mutex_);
    for (unsigned i = 0; i < days; ++i) { nextDay(); processEvents(); }
    state_.tick_count = state_.day_in_ticks;
}
}
