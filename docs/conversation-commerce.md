# Graph-owned commerce and starbase reports

All 27 live conversations now own their story branches and response routing in
`sc2/content/base/conversations/comm.*.dialogue.json`. Druuge, Melnorme and the
operational starbase no longer invoke native story turns. Their C files retain
resource/animation/number-speech descriptors. The old turn bridge, response-slot
routing and `handlers.legacy_turn` action are deleted.

The initial Commander and operational starbase remain separate phrase banks.
The combined Hayes/Melnorme design examples are reference experiments, not the
files loaded by the game. Their technology examples reuse the live catalog.

## Operations and ownership

`CampaignDialogueState` borrows the session's player and clock. Its per-visit
`ConversationCommerce` owns immutable market definitions and an optional rescue
offer. `StarbaseReports` owns prepared speech/effects, a token and bulletin replay
state. Neither owns the player or commits gameplay from a destructor.

- Quotes/availability use const state, without RNG, payment, grants or hidden
  acknowledgement. Story conditions and response ordering remain in the graph.
- Druuge purchases reserve actual Fleet entries before charging crew. Failed
  allocation removes reservations and restores RNG; paid purchases are exact.
  Item barter retains the original up-to-available-slots ship gift policy.
- `Fleet::create(race, definition, random)` delegates defaults to validated
  explicit creation. Ordinary captains avoid occupied names. Resources remain
  borrows from the session's loaded race templates; stable vessel IDs identify
  entries and survive reordering. No second vessel store was added.
- Fuel/crew/items/accounts go through Inventory. `fuel` remains whole displayed
  units; `fuel_hundredths` is the explicit fine-unit key. Melnorme fill quotes
  charge the ceiling in displayed units but deliver only actual tank space.
  Druuge's crew-for-fuel offer retains its specified clamping policy.
- Technology order, price, pitch/sold speech and grants live in Melnorme's JSON
  market book. The adapter validates grant kinds, IDs, ranges and prices before
  actions. Source-specific Spathi/Syreen knowledge effects remain a bounded
  native policy; they are not inventory rules.
- Biomass/rainbow sales reject account overflow without taking the consideration.
  Reported rainbow discoveries cannot be sold twice. Druuge slavery accounting
  remains an explicit departure action, including crew-price threshold changes.

## Rescue lifetime

Preparation uses the existing location/equipment seed policy and a local random
context. It stages removable equipment without changing the live ship, protects
required crew/cargo capacity and fuel tanks, and returns the same offer on repeat
preparation. Speech replay never prepares it again. Acceptance checks the original
relevant equipment/crew/fuel/cargo state, applies only the planned equipment/fuel
changes and can succeed once. A stale or redeemed token is rejected.

Charity is a one-time free grant. Penance is a separate strip policy retaining
required crew pods and the original fuel/cargo rules. Offers expire with their
conversation and are not written into saves.

## Report preparation and acknowledgement

Preparing devices, arrival or analysis returns speech and a commit token. The
following dialogue node plays that data; a separate graph action commits it when
playback completes or the player skips it. Leaving for load/shutdown before that
boundary discards the uncommitted plan. A newer plan invalidates an older token;
committing the same current token twice is harmless.

Device availability is a local scan. It never performs the legacy zero-valued
story writes. Egg-case aliases are acknowledged together; the scientist's voice
segment disables talking animation only for that segment. Bulletin replay uses
visit-owned speech and never grants ships or repeats other effects.

The Arilou gift is durably marked when the played report commits, so leaving before
farewell cannot recruit it again. The ready-starbase grant similarly clears its
moonbase-delivery marker at commit. These deliberately make one-time rewards
independent of reaching the farewell menu. Normal post-encounter policy still
handles moonbase/bomb cleanup and Druuge hostility, including attack without
conversation. Cancellation for load/shutdown skips normal completion policy.

## Presentation and compatibility

Melnorme's colors, number speech, alliance-name insertion, depleted-credit routing
and last-selected first-contact stack priority are preserved. The credit-status
binding restores through RAII even on failure. Starbase menu ordering, constructed
ship-name responses, calendar boundaries and mineral-load commentary are retained.

Sales now commit once and refresh gauges; the old incremental cargo tally animation
and its per-unit mutations/waits are removed. Speech/number results still describe
the complete sale. General animation/UI services remain separate cleanup work.

Legacy save storage remains authoritative. These operations add no new save
format or concurrent-session guarantee. Exceptions invalidate a running graph;
this is not a cross-domain rollback framework or completion of the fatal C/service
boundary cleanup in issue #23.

Focused tests exercise actual state adapters and live graphs, account/capacity and
allocation failures, fine fuel, technology/knowledge, discovery resale, rescue
staleness/replay, report timing/idempotence and information-menu traversal.
