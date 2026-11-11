#ifndef UQM_CAMPAIGN_CLOCK_HPP_
#define UQM_CAMPAIGN_CLOCK_HPP_
#include "clock.h"
#include "gameev.h"
#include <cstdint>
#include <functional>
#include <mutex>
#include <tuple>
#include <vector>
namespace uqm {
struct CampaignDate {
    unsigned year, month, day;
    bool operator==(CampaignDate other) const noexcept {
        return std::tie(year, month, day) == std::tie(other.year, other.month, other.day);
    }
    bool operator<(CampaignDate other) const noexcept {
        return std::tie(year, month, day) < std::tie(other.year, other.month, other.day);
    }
};
struct CalendarOffset { unsigned years = 0, months = 0, days = 0; };
enum class CampaignEvent : std::uint8_t {
    ArilouEntrance = ARILOU_ENTRANCE_EVENT, ArilouExit = ARILOU_EXIT_EVENT,
    HyperspaceEncounter = HYPERSPACE_ENCOUNTER_EVENT, KohrAhVictorious = KOHR_AH_VICTORIOUS_EVENT,
    AdvancePkunk = ADVANCE_PKUNK_MISSION, AdvanceThraddash = ADVANCE_THRADD_MISSION,
    ZoqfotDistress = ZOQFOT_DISTRESS_EVENT, ZoqfotDeath = ZOQFOT_DEATH_EVENT,
    ShofixtiReturn = SHOFIXTI_RETURN_EVENT, AdvanceUtwigSupox = ADVANCE_UTWIG_SUPOX_MISSION,
    KohrAhGenocide = KOHR_AH_GENOCIDE_EVENT, SpathiShield = SPATHI_SHIELD_EVENT,
    AdvanceIlwrath = ADVANCE_ILWRATH_MISSION, AdvanceMycon = ADVANCE_MYCON_MISSION,
    ArilouUmgahCheck = ARILOU_UMGAH_CHECK, YehatRebel = YEHAT_REBEL_EVENT,
    SlylandroRampUp = SLYLANDRO_RAMP_UP, SlylandroRampDown = SLYLANDRO_RAMP_DOWN
};
struct ScheduledCampaignEvent { CampaignDate date; CampaignEvent event; };
// Calendar time is driven by simulation ticks, not wall time. Callbacks execute
// synchronously on the gameplay thread and may schedule more events or read the
// clock. The recursive mutex supports that reentrancy and scoped debug access.
class CampaignClock {
public:
    using Handler = std::function<void(CampaignEvent)>;
    using DateChanged = std::function<void()>;
    static constexpr unsigned BaseFramerate = 24;
    static constexpr std::size_t MaxPendingEvents = NUM_EVENTS;
    explicit CampaignClock(Handler handler = {}, DateChanged changed = {});
    CampaignClock(const CampaignClock &) = delete;
    CampaignClock &operator=(const CampaignClock &) = delete;
    // New campaign/reset boundary only; resetting during a handler throws.
    void reset();
    CampaignDate date() const;
    CLOCK_STATE snapshot() const;
    // Validate scalars and clear pending events; loading restores events afterward.
    void restore(const CLOCK_STATE &state);
    std::vector<ScheduledCampaignEvent> events() const;
    void clearEvents();
    // Zero pauses. Reject rates that do not fit the existing save tick fields.
    void setRate(unsigned secondsPerDay);
    bool running() const;
    void tick();
    void advance(unsigned days);
    // Relative offsets apply months/years before carrying days, preserving UQM.
    CampaignDate resolve(CalendarOffset offset) const;
    bool accepts(CampaignDate date) const;
    // Equal-date events retain insertion order. Past/full requests return false;
    // malformed dates/selectors throw. A zero offset invokes its handler now.
    bool schedule(CampaignDate date, CampaignEvent event);
    bool schedule(CalendarOffset offset, CampaignEvent event);
    // Save restore queues same-day events without executing them during decoding.
    bool restoreEvent(ScheduledCampaignEvent event);
    // Standard Lockable interface, for std::lock_guard in legacy debug tools.
    void lock() { mutex_.lock(); }
    void unlock() { mutex_.unlock(); }
private:
    mutable std::recursive_mutex mutex_;
    CLOCK_STATE state_{};
    std::vector<ScheduledCampaignEvent> events_;
    Handler handler_;
    DateChanged changed_;
    std::size_t dispatching_ = 0, callbackDepth_ = 0;
    void invoke(CampaignEvent event);
    void nextDay();
    void processEvents();
};
}
#endif
