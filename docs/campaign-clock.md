# Campaign clock and scheduled events

`GameSession` owns a `CampaignClock` value. Native callers borrow `session.clock()`;
const sessions expose a const reference. Independent sessions have independent
dates, tick progress, event lists, and synchronization, even if they borrow the
same player. A new campaign resets the clock to 17 February 2155. Loading occurs
only after the previous mode/handler stack has unwound.

The clock owns no device resources. Its constructor receives synchronous event
and date-change callbacks. The production session connects those to the existing
`EventHandler` policy and date display, rebinding legacy access to the owning
session for each callback rather than relying on an unrelated active campaign. Hyperspace navigation borrows its session's
clock explicitly; exploration uses its attached session. Conversation state borrows
that same clock for scheduling and receives read-only calendar snapshots.

## Timing and scheduling contracts

- Simulation ticks drive the clock using the original 24-tick base, including
  the existing Hyperspace/system rate choices. Changing rates preserves the
  remaining fraction of the day. A zero rate pauses calendar progression.
- Dates use the Gregorian leap-year rules. C++17 has no standard calendar-month
  arithmetic, so a small checked calendar calculation preserves the original
  month/year-first relative-offset behavior. January 31 plus one month carries
  through February rather than clamping to its last day.
- `schedule(CampaignDate, CampaignEvent)` schedules an absolute event;
  `schedule(CalendarOffset, CampaignEvent)` schedules a relative event. Past/full
  requests return false; invalid dates/selectors throw.
- A zero relative offset invokes the handler synchronously without allocating an
  event slot. Otherwise a sorted `std::vector` owns pending values. Equal-date
  events retain insertion order, including same-day events added by a callback.
- The former pool capacity (`NUM_EVENTS`) is retained explicitly. A queued event
  being dispatched still occupies its old capacity slot until its handler returns.
- Date display is notified before that day's handlers. Handlers may read the clock
  and schedule more events. Reset/restore during a handler throws. Events are
  removed before dispatch, so exceptions do not cause automatic retries.
- A paused tick no longer decrements an unused signed counter indefinitely; the
  date stays paused without eventual signed overflow.

All public operations synchronize with a clock-owned recursive mutex. Recursion
is needed because legacy handlers and date presentation reenter clock queries.
Native debugging uses `std::lock_guard<CampaignClock>` or detached snapshots,
replacing manual debug lock/unlock calls (including an old early-return lock leak).
Callbacks still run on the gameplay thread; this does not introduce a worker
scheduler or make the rest of campaign state safe for concurrent mutation.

## Save/load and C compatibility

`CLOCK_STATE` is now an eight-byte scalar save projection, and `EVENT` is a value
record. Neither owns a queue or borrowed link. `ExportCampaignClock` projects the
live clock immediately before saving. Both loaders validate/import its scalar
state, then restore event values without invoking handlers during decoding.

The current save still writes day/month/year, signed remaining ticks, and period
in the original field order. Event records remain five bytes each, in stable
calendar order. The compressed legacy loader still consumes the historical unused
pointers and padding. Bad event dates/selectors, excessive pending events and
truncated/misaligned current event records fail loading instead of becoming live
scheduler records. The remaining save parser is otherwise unchanged.

C scripts use small adapters for snapshots, relative-date validation, scheduling,
and advancing days. They select the active campaign and never expose mutable
clock storage. The old `clock.c`, global SDL clock mutex, event-link macros,
`HEVENT` API, `QUEUE_EVENT` pool alternative, and manual clock init/uninit are gone.
The C story-event dispatcher remains policy code to migrate separately.

Tests cover tick/rate behavior, paused ticks, century/leap/year boundaries,
relative offsets, stable/reentrant event delivery, capacity, invalid input,
independent session ownership, and explicit C save projection/restoration.

C record/output pointers must be non-null and valid for the duration of the call.
`ReadCampaignEvent` leaves its output untouched when its index is out of range.
Relative `ValidateEvent` updates only its caller-owned date fields; queries never
change the campaign. Event handlers and device callbacks remain gameplay-thread
operations even though the clock's own data is synchronized.
