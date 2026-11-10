# Native contracts and conversion boundaries

The [prototype engine](README.md) is executable. These are the contracts its
state/presentation adapters must implement before a gameplay migration. No method
below forwards a string to an arbitrary legacy callback, executes C from JSON,
or silently reports success when missing.

## Read-only state access

`State` borrows the session's player/story/galaxy and the current encounter.
Domain identifiers in the JSON should be resolved to the real enums/IDs by the
native binding layer. The prototype's facade uses strings to keep its dependency
on the live game out of the design build.

- `read(scope,key)` returns a bool, signed integer, string or explicitly requested
  data. Missing values are null so an explicit `get` fallback can apply. Unknown
  registered domain keys must be rejected by the real adapter, not invented.
- Fleet slots retain the race-availability check in EscortFeasibilityStudy;
  membership/count/value do not recruit, remove ships or generate captain names.
- Fuel getters use displayed units. If exact fractional quantities matter, the
  native adapter needs a consistent finer-unit representation; do not mix the
  legacy integer hundredths with these public quantities.
- Currency identifies the account (`melnorme_credit`). RU, merchant credit, quantities and
  salvage-value units are different domains despite using integer storage.
- `dateReached(savedDate,months,days,inclusive)` applies a calendar offset and
  explicit strict/inclusive comparison. Pkunk gift dates and Hayes CheckTiming
  have different boundaries. This is not a naive integer day approximation.
- `marketAvailable` scans owned technology, remaining information, or categories;
  it never charges, grants items or advances purchase cursors.
- `reportAvailable` is a pure scan. The legacy DiscussDevices(FALSE) implementation
  still writes zero flags and must not be used unchanged.
- `encounter(field)` reads typed circumstances, participants or the last outcome.
  It does not launch another mode. Race availability is separate from revealing
  that race on the starmap.

## Action results

All actions return `Result { code, values }`; values is always an object.
Ordinary `state.set/add/bits`, relationship changes, technology grants, date
remembering and encounter outcome assignment return `ok` after validating the
request. Invalid identifiers/ranges are programming/data errors. State actions
must not draw, play speech or wait for player input.

| Operation | Codes and payload / behavior |
| --- | --- |
| `fleet.give` | `ok`, `partial`, `no_room`; payload includes `count` and stable vessel IDs. One argument generates default captain/crew on invocation. Three arguments request explicit captain/crew. Bulk gifts use the two-argument count overload. |
| `fleet.take`, `fleet.set_crew` | Omitted removal count removes all of that race; zero is a valid no-op. Validate stable vessel/race identifiers and crew rules; report removed/changed count. No iterator or borrowed ship pointer enters JSON. |
| `inventory.transfer`, `ship.adjust` | Item quantity defaults to +1 when omitted. Signed quantities, game capacity/availability checks and explicit failure results; no unsigned wrap or silently fabricated cargo. |
| `ship.configure` | Named validated refit/equipment/name profile, with explicit parameters. Bomb upgrades and shipyard values belong to ship rules. |
| `currency.adjust` | `ok` or `no_credit`, with `missing` when appropriate. Never enqueue NEED_MORE_CREDIT speech; the graph does that. |
| `trade.purchase` | Commit validation, payment, ownership and any offer-specific story effects together. `ok`, `no_credit`, `no_room`, `unavailable`; successful tech/info payload includes prepared `speech`; insufficient credit includes `missing`. No audio playback. |
| `trade.sell` | Commit the selected cargo/discovery sale once. Payload includes amount/payment and prepared `speech`. Mineral tally animation is presentation of the result, not repeated payment per redraw. |
| `rescue.prepare` | Generate a quote once with the legacy deterministic location/equipment seed policy. `offer` includes opaque `token` and prepared `speech`. `free` commits the free rescue and returns its speech. No partial strip is committed for an unaccepted paid offer. |
| `rescue.accept` | Redeem a quote token once; reject stale/reused tokens. `ok` commits equipment changes and fuel; failures need explicit graph destinations. The sample routes unexpected failures to `design_error`. |
| `ship.strip` | A validated penalty policy, distinct from accepting a rescue quote. `ok` or `not_worth_stripping`; protect required crew pods and fuel rules according to the source policy. |
| `calendar.schedule` | Relative event ID/month/day/year, with calendar validation. No arbitrary function pointer is serialized. |
| `report.prepare` | Run a registered report operation once, applying its specified bookkeeping and returning utterance data. This is an action even when the report mostly describes state. |
| `galaxy.reveal` | Reveal the requested race/sphere, returning availability if needed. Query eligibility separately through `galaxy.available`. |
| `random.draw` | Consume session RNG once; return `value` in `[0,upperBound)`. Decision nodes inspect that saved value. |
| `sequence.next` | Choose and advance a named local/story cursor; return `speech` and `index`. Repeat-last/cycle policy and optional numeric base are explicit data; Melnorme preserves its cursor offset of five. |

A callable is not a transaction. The native operation must coordinate its
multiple changes and define partial-success behavior. A thrown exception makes
this conversation instance unusable; it does not roll back underlying state.
Destructors never commit rewards or run normal-completion story actions.

## Report contracts used by Hayes and Melnorme

The adapter implementing these reports is per conversation visit. It may own
prepared report/quote bookkeeping while borrowing authoritative session state.
Prepared speech consists of phrase IDs and resolved typed insertions/numbers.
It is data for the following dialogue node, not an instruction to run a handler.

| Report | Required implementation / source |
| --- | --- |
| `hayes.arrival` | NormalStarbase's initial ready/return/random greeting selection; initial 2,500 RU and starbase date where appropriate; CheckBulletins(FALSE); visit flag and current bulletin mask. Read speech configuration for voiced/name fragments. |
| `hayes.repeat_bulletins` | Replay only the current visit's selected bulletins; no repeated gifts, events or permanent rewards. |
| `hayes.departure` | Merge current bulletin mask into permanent bulletin history; choose farewell once using RNG, or bomb-ready farewell. |
| `hayes.devices` | Scan owned/undiscussed devices, return the matching explanation sequence and mark the corresponding discussion flags. The VUX scientist segment includes talking-animation cues. |
| `hayes.flagship_analysis` | Count equipment, crew, fuel, storage, allied races and escort strength, then return AnalyzeCondition's explanation/advice. Capability thresholds should be named data rather than hidden magic numbers in a JSON expression. |
| `melnorme.technology_pitch` | Return the first unowned technology's sale line from the checked-in market catalog. No purchase or grant. Selection/order remains consistent with the subsequent `next_technology` purchase. |

The first five reports still need implementations. They are explicit seams in a
theoretical engine, not a claim that the existing C logic has been converted.
Their complete original condition/action catalog remains in
[Hayes starbase](../catalog/starbas.md). Melnorme commerce/rescue implementations
likewise remain future native work; [its catalog](../catalog/melnorm.md) is the
behavior reference.

Hayes's Arilou bulletin gift is especially important: replay must not recruit
three more ships. A report's eligibility check cannot itself give those ships.
Prepare the bulletin result/action once, record it, then present it.

## Market data and story effects

[Melnorme JSON](examples/melnorme.json) includes:

- Fuel quantities 1/5/10/25/fill at one merchant credit per displayed unit, with
  explicit legacy hundredths and fill/capacity rounding policy.
- All 13 technology offers at 150 credits, their pitch/sold phrases, and the
  underlying module/state/lander-shield grant description.
- Information categories of 8 current events, 16 alien reports and 9 histories,
  at 75 credits each; special Spathi-password and Syreen-shattered-world effects
  are explicit offer data.
- Biomass at two credits per unit and newly reported rainbow worlds at 500
  credits each; discovery sales must not pay twice.
- A rescue quote allowance of two displayed fuel units beyond distance to Sol,
  plus seeded quote and once-only acceptance policies.

The engine does not silently interpret the market book through its graph
traversal. The native market adapter consumes this data while implementing the
trade/query methods. Offer effects are part of the purchase commit; they are not
free-floating availability predicates.

## Presentation contract

A presenter keeps its shared assets and playback state across nodes. Profiles
refer to the extracted sprite-layer data, palette indices, fonts and optional
music. Per-segment cues can change a speaker, palette, talking animation or
one-shot layer at an explicit speech boundary. Presentation does not mutate
story/fleet/cargo. User input and animation timing do not become condition verbs.

`Presentation::show` is idempotent for a view entry; `resume` restores its retained
playback/view. `stop` cancels callbacks while owners still live. ConversationMode
would own that RAII service binding, use its session's asset cache, and return
an end outcome through the existing encounter/mode dispatcher.

This prototype contains no SDL renderer, audio mixer, font shaper, localized
number-speech formatter or save/load integration. The media file is design data
extracted from existing defaults; it is not a tested replacement compositor.

## Draft differences that must be resolved before migration

- Some first-contact topic ordering is normalized; the old scripts prioritize
  the last selected stack dynamically.
- Some reply effects currently run after playback, while the legacy script
  queues speech and changes state before that speech plays. Choose action timing
  deliberately before implementing abort/load behavior.
- Fuel getters in the prototype contract use whole displayed units. Legacy fuel
  may contain fractions; preserve fine units inside actual purchase/quote rules.
- Native state/domain IDs, report eligibility, atomic trade, rescue quoting and
  complete per-segment presentation still need real adapters. The test state
  supplies outcomes only for the paths tested; it is not game logic.

The existing game and save format are unchanged. These files are for design
review and executable orchestration experiments, not a ready-to-enable content port.
