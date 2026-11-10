# Condition and query vocabulary audit

This audits the current 27 conversation scripts to identify the state queries a
new engine needs beyond flag checks. It supplements the [graph catalog](README.md).
Names in the “candidate interface” column are proposed vocabulary, not implemented
APIs or a settled JSON format.

## Queries to expose

A condition can bind directly to a boolean query, or compare the result of a
numeric/enum query. Composite gameplay calculations should stay in their state
interfaces; the JSON definition supplies arguments and comparisons.

| Query family | Existing behavior / evidence | Candidate interface |
| --- | --- | --- |
| Story flags, counters and packed records | `GET_GAME_STATE`, `GET_GAME_STATE_32`; individual device/story prerequisites | Typed story reads and comparisons; named accessors for encoded multi-byte quantities |
| Fleet capacity | [EscortFeasibilityStudy](../../../sc2/src/uqm/build.c#L296) first checks that the race is available, then returns remaining escort slots; used for Fwiffo and several race gifts | `fleet.availableSlots(type)` / `fleet.canAdd(type, count)` |
| Fleet membership and emptiness | [HaveEscortShip](../../../sc2/src/uqm/build.c#L285); Shofixti escort shown to the Yehat, Spathi escort checked at home; Druuge checks an empty escort queue | `fleet.hasShip(type)`, `fleet.empty()` |
| Alliance / extinction / race availability | [CheckAlliance](../../../sc2/src/uqm/build.c#L313) supplies alliance state; Talking Pet checks Umgah availability through a revealing operation | `galaxy.relationship(race)`, a separate read-only race-availability query |
| Fuel quantity and capacity | [Melnorme DoBuy](../../../sc2/src/uqm/comm/melnorm/melnorm.c#L959) sums installed normal/high-efficiency tank capacities and compares purchased quantities with fuel aboard | `sis.fuel()`, `sis.fuelCapacity()`, `sis.freeFuelCapacity()` with explicit units |
| Money and affordability | [GetAvailableCredits / DeltaCredit](../../../sc2/src/uqm/comm/melnorm/melnorm.c#L858) decode Melnorme credits; purchase checks also compare requested price with credit | `player.balance(currency)`, `player.canAfford(currency, price)` |
| Cargo and consumables | Biomass, total mineral mass, individual element amounts, crew, landers and RU are inspected through `GLOBAL_SIS` | Numeric cargo/crew/lander/RU accessors; inventory membership remains separate from quantities |
| Equipment and ship capability | [Hayes AnalyzeCondition](../../../sc2/src/uqm/comm/starbas/starbas.c#L615) counts thrusters, jets, guns, batteries, tracking, storage and defense, then evaluates minimum/maximum loadouts | Equipment counts and derived capabilities; threshold profiles can be data rather than a separate predicate for every speech line |
| Ship / escort value | [ShipWorth](../../../sc2/src/uqm/comm/melnorm/melnorm.c#L490) computes salvage value from equipment; [CalculateEscortsWorth](../../../sc2/src/uqm/comm/starbas/starbas.c#L703) supplies Hayes's fleet-strength comparison | `sis.salvageValue()`, `fleet.value()` with the relevant valuation policy |
| Owned and available technology | [HasTech](../../../sc2/src/uqm/comm/melnorm/melnorm.c#L317) checks technology-dependent storage; [GetNextTechForSale](../../../sc2/src/uqm/comm/melnorm/melnorm.c#L470) scans the sale catalog for the first unowned technology | `player.hasTechnology(id)`, `market.nextTechnology(player)` / `market.hasTechnologyForSale(player)` |
| Information available for sale | [AnyInfoLeftToSell](../../../sc2/src/uqm/comm/melnorm/melnorm.c#L946) compares three purchase-progress counters with their catalog sizes | `market.hasInformation(category, player)`; aggregate “any category” is composition |
| Calendar / elapsed dates | [CheckTiming](../../../sc2/src/uqm/comm/starbas/starbas.c#L1323) tests strictly after a calculated date; [ShipsReady](../../../sc2/src/uqm/comm/pkunk/pkunkc.c#L99) combines Pkunk manner with reaching the next gift date | Date comparison / elapsed-time queries, combined with ordinary story checks; retain strict versus inclusive boundaries |
| Undiscussed devices | [DiscussDevices(FALSE)](../../../sc2/src/uqm/comm/starbas/starbas.c#L1041) scans owned devices and discussion flags | A pure `hasUndiscussedDevices()` query, separate from discussing/marking them |
| Location and rescue eligibility | [DoRescue](../../../sc2/src/uqm/comm/melnorm/melnorm.c#L836) calculates distance to Sol plus a fuel allowance, then prepares a ship-stripping offer | Location/distance reads and a rescue-quote operation; keep proposal generation separate from a repeatable availability predicate |
| Encounter circumstances and outcome | `CurrentActivity`, encounter flags, NPC fleet presence and [getSegue checks](catalog/arilou.md#node-post_arilou_enc) select greetings, outtakes and post-encounter effects | Typed encounter kind, participant availability and outcome reads; these are not all permanent story facts |
| Conversation-local progress | `PHRASE_ENABLED`, local masks, stack arrays and counters determine remaining topics and response order | Conversation-local reads / topic sequence state |

This list includes checks implemented using intermediate variables, not just
functions called directly inside an `if`. Fuel capacity, affordability, salvage
value, fleet strength and capability profiles would be missed by counting only
boolean-returning function names.

## Melnorme fuel: actual checks and outcomes

The current submenu offers **1, 5, 10, 25, and fill-up**. All of those purchase
responses are registered together; individually unaffordable or oversized offers
are not automatically removed from the menu.

`DoBuy`:

1. Computes capacity from the reserve plus installed fuel tanks.
2. Reads the credit balance from its two story-state bytes.
3. For fill-up, computes `(capacity - fuel + FUEL_TANK_SCALE - 1) / FUEL_TANK_SCALE`.
4. Rejects an oversized order using `fuel / FUEL_TANK_SCALE + quantity > capacity / FUEL_TANK_SCALE`.
5. Checks `quantity * (BIO_CREDIT_VALUE / 2)` against credit before adding fuel.
6. Uses `DeltaCredit` to attempt payment and emit insufficient-credit dialogue.
7. Offers the top-level fuel topic only while `fuel < capacity`.

`FUEL_TANK_SCALE` is [100 internal units per displayed fuel unit](../../../sc2/src/uqm/sis.h#L40).
The existing integer rounding is part of the purchase path, not simply a raw
`maxFuel - fuel` comparison in an arbitrary unit system. Query accessors should
make the units explicit and purchase actions should own conversion/payment rules.

A condition such as `canBuyFuel(quantity)` may be useful, but deciding to hide or
gray a purchase response based on it would be a separate presentation/behavior
choice. The existing failure dialogue is an action outcome that must remain
representable.

## Functions that must not become repeatable conditions unchanged

| Existing function | Side effects or mixed responsibility | Required separation |
| --- | --- | --- |
| `DeltaCredit(delta)` | Changes credit and redraws status on success; queues insufficient-credit speech on failure | Pure affordability query; payment action returning success/failure |
| `AddEscortShips(type, count)` | Recruits ships and returns how many were added; Pkunk uses its result to schedule the next gift | Capacity query; recruitment action with a count/result |
| `StartSphereTracking(race)` | Can reveal previously unknown race strength/location while returning availability | Read-only race availability query; reveal-tracking action |
| `StripShip(fuel)` | Clears a story flag, prepares a randomized/seeded rescue proposal, or commits equipment/fuel changes depending on arguments | Rescue eligibility/quote and explicit accept/strip actions; store the prepared proposal |
| `DiscussDevices(FALSE)` | Intended as a check, but the implementation still executes `SET_GAME_STATE(..., FALSE)` for eligible undisclosed devices | A genuinely const/read-only eligibility scan; explicit discussion actions |
| `TFB_Random()` in a condition | Advances RNG state; rebuilding a response menu could reroll a decision | Roll once at the intended entry/selection point and store the result in conversation-local state |
| `AnyButtonPress(...)` | Polls input during sale/presentation loops | Input/presentation service, not a story-condition verb |

Binding these functions makes their invocation convenient; it does not make them
read-only. Availability predicates may be reevaluated whenever a menu is rebuilt.
Action results need an explicit branch mechanism for failure dialogue, partial
recruitment or a generated rescue proposal. That mechanism is still to be designed.

## What does not need a new game-state query

- `PLAYER_SAID` is response routing and becomes a destination edge.
- `LOBYTE`, `LOWORD`, and `MAKE_WORD` are storage-encoding helpers. Expose the decoded value.
- Queue-head checks become `empty()` / participant-presence queries.
- Loop indices, temporary pointer checks and switch selectors often operate the old implementation. Preserve their underlying rule without exposing its machinery as JSON verbs.
- `PHRASE_ENABLED` belongs to conversation-local availability, not a new global story flag for every topic.

## Direct-call inventory

The following inventory scans `if`, conditional expressions, `switch`, and loop
conditions in all 27 active scripts. It records **28 distinct call names across
2,124 syntactic occurrences**, including nested calls, routing macros and UI
checks. These are not 28 proposed query verbs. Conditions computed through locals
and query helper return expressions are covered by the audit above and the full
[per-conversation catalog](README.md).

| Call | Occurrences | Role | Example |
| --- | ---: | --- | --- |
| `AddEscortShips` | 1 | Action with result | [pkunk: PrepareShip](../../../sc2/src/uqm/comm/pkunk/pkunkc.c#L113) |
| `AnyButtonPress` | 2 | Input / presentation | [melnorm: DoSell](../../../sc2/src/uqm/comm/melnorm/melnorm.c#L1223) |
| `AnyInfoLeftToSell` | 2 | Information-availability query | [melnorm: DoBuy](../../../sc2/src/uqm/comm/melnorm/melnorm.c#L1133) |
| `CheckAlliance` | 19 | Alliance-state query | [chmmr: NotReady](../../../sc2/src/uqm/comm/chmmr/chmmrc.c#L266) |
| `CheckTiming` | 5 | Calendar query | [starbas: CheckBulletins](../../../sc2/src/uqm/comm/starbas/starbas.c#L1430) |
| `DeltaCredit` | 2 | Payment action with result | [melnorm: DoBuy](../../../sc2/src/uqm/comm/melnorm/melnorm.c#L1085) |
| `DiscussDevices` | 1 | Mixed query/action helper; split | [starbas: NormalStarbase](../../../sc2/src/uqm/comm/starbas/starbas.c#L1724) |
| `EscortFeasibilityStudy` | 9 | Fleet-capacity query | [druuge: Buy](../../../sc2/src/uqm/comm/druuge/druugec.c#L243) |
| `GET_GAME_STATE` | 554 | Story read | [arilou: ArilouHome](../../../sc2/src/uqm/comm/arilou/arilouc.c#L301) |
| `GET_GAME_STATE_32` | 1 | Packed story read | [starbas: CheckBulletins](../../../sc2/src/uqm/comm/starbas/starbas.c#L1458) |
| `GLOBAL` | 60 | Legacy state projection | [arilou: Intro](../../../sc2/src/uqm/comm/arilou/arilouc.c#L643) |
| `GLOBAL_SIS` | 46 | Player/flagship projection | [chmmr: ExitConversation](../../../sc2/src/uqm/comm/chmmr/chmmrc.c#L193) |
| `GetHeadLink` | 2 | Queue-presence implementation | [druuge: Intro](../../../sc2/src/uqm/comm/druuge/druugec.c#L822) |
| `GetNextTechForSale` | 1 | Market / ownership query | [melnorm: DoBuy](../../../sc2/src/uqm/comm/melnorm/melnorm.c#L1161) |
| `GetPlayerFleetQueue` | 1 | Legacy fleet buffer | [druuge: Intro](../../../sc2/src/uqm/comm/druuge/druugec.c#L822) |
| `HasTech` | 1 | Technology ownership query | [melnorm: GetNextTechForSale](../../../sc2/src/uqm/comm/melnorm/melnorm.c#L476) |
| `HaveEscortShip` | 2 | Fleet-membership query | [spahome: SpathiCouncil](../../../sc2/src/uqm/comm/spahome/spahome.c#L824) |
| `LOBYTE` | 40 | Encoding / activity extraction | [arilou: Intro](../../../sc2/src/uqm/comm/arilou/arilouc.c#L643) |
| `LOWORD` | 1 | Encoding helper | [melnorm: StripShip](../../../sc2/src/uqm/comm/melnorm/melnorm.c#L677) |
| `MAKE_WORD` | 1 | Encoding helper | [starbas: CheckBulletins](../../../sc2/src/uqm/comm/starbas/starbas.c#L1571) |
| `PHRASE_ENABLED` | 329 | Conversation-local availability | [arilou: ArilouHome](../../../sc2/src/uqm/comm/arilou/arilouc.c#L422) |
| `PLAYER_SAID` | 1027 | Response routing | [arilou: ExitConversation](../../../sc2/src/uqm/comm/arilou/arilouc.c#L235) |
| `ShipWorth` | 2 | Salvage-value query | [melnorm: StripShip](../../../sc2/src/uqm/comm/melnorm/melnorm.c#L672) |
| `ShipsReady` | 2 | Story + date query | [pkunk: PkunkFriendlySpace](../../../sc2/src/uqm/comm/pkunk/pkunkc.c#L658) |
| `StartSphereTracking` | 1 | Revealing action with availability result | [talkpet: Intro](../../../sc2/src/uqm/comm/talkpet/talkpet.c#L783) |
| `StripShip` | 1 | Rescue proposal / mutation | [melnorm: DoRescue](../../../sc2/src/uqm/comm/melnorm/melnorm.c#L849) |
| `TFB_Random` | 5 | RNG mutation | [mycon: NormalMycon](../../../sc2/src/uqm/comm/mycon/myconc.c#L377) |
| `getSegue` | 6 | Encounter-outcome query | [arilou: post_arilou_enc](../../../sc2/src/uqm/comm/arilou/arilouc.c#L804) |
