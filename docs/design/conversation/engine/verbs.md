# Consolidated conversation verbs

These are the verbs actually accepted by the [prototype factory](verb_factory.cpp).
Game operations are declared in [state.hpp](state.hpp); implementations and result contracts are documented in [native-contracts.md](native-contracts.md).
The [full legacy mapping](legacy-verbs.json) classifies all active script call sites. Routing, packed encodings, UI polling, drawing and playback are deliberately not new state verbs.

## Read-only queries

| Verb | Arguments | Native accessor |
| --- | --- | --- |
| `fleet.contains` | `race` | `hasShip` |
| `fleet.slots` | `race` | `availableSlots` |
| `fleet.count` | `race_or_all` | `shipCount` |
| `fleet.value` |  | `fleetValue` |
| `galaxy.relationship` | `race` | `relationship` |
| `galaxy.available` | `race` | `raceAvailable` |
| `inventory.has` | `item` | `hasItem` |
| `inventory.quantity` | `resource` | `quantity` |
| `ship.fuel` |  | `fuel` |
| `ship.fuel_capacity` |  | `fuelCapacity` |
| `currency.balance` | `currency` | `balance` |
| `ship.equipment_count` | `equipment` | `equipmentCount` |
| `ship.salvage_value` |  | `salvageValue` |
| `ship.capability` | `profile` | `capability` |
| `technology.owned` | `technology` | `hasTechnology` |
| `market.available` | `merchant`, `category` | `marketAvailable` |
| `calendar.reached` | `saved_date`, `months`, `days`, `inclusive` | `dateReached` |
| `report.available` | `report` | `reportAvailable` |
| `encounter.read` | `field` | `encounter` |
| `location.distance` | `location` | `distanceTo` |
| `ship.free_fuel` |  | `fuelCapacity - fuel` |

Generic `get(scope,key[,default])` covers typed story/local/player quantities. Boolean queries can be predicates directly; compare numeric or enum values explicitly.

## Actions

| Verb | Arguments | Native operation |
| --- | --- | --- |
| `state.set` | `scope`, `key`, `value` | `set / local memory` |
| `state.add` | `scope`, `key`, `signed_delta` | `set / local memory` |
| `state.bits` | `scope`, `key`, `set_or_clear`, `mask` | `set / local memory` |
| `sequence.next` | `scope`, `key`, `items`, `repeat_last_or_cycle`, `optional_base` | `set / local memory` |
| `fleet.give` | `race [, count] OR race, captain, crew` | `createShip / giveShips` |
| `fleet.take` | `race [, count]` | `removeShips` |
| `fleet.set_crew` | `ship_id`, `crew`, `captain` | `setShipCrew` |
| `inventory.transfer` | `item [, signed_quantity]` | `transferItem` |
| `ship.adjust` | `property`, `signed_quantity` | `adjustShip` |
| `ship.configure` | `profile`, `configuration` | `configureShip` |
| `currency.adjust` | `currency`, `signed_amount` | `adjustCurrency` |
| `trade.purchase` | `merchant`, `offer`, `quantity_or_fill` | `purchase` |
| `trade.sell` | `merchant`, `category`, `quantity_or_all` | `sell` |
| `rescue.prepare` | `merchant` | `prepareRescue` |
| `rescue.accept` | `quote_token` | `acceptRescue` |
| `ship.strip` | `policy` | `stripShip` |
| `technology.grant` | `technology` | `grantTechnology` |
| `galaxy.set_relationship` | `race`, `relationship` | `setRelationship` |
| `galaxy.reveal` | `race` | `revealRace` |
| `calendar.schedule` | `event`, `months`, `days`, `years` | `scheduleEvent` |
| `calendar.remember` | `date_key` | `rememberDate` |
| `report.prepare` | `report`, `parameters` | `prepareReport` |
| `encounter.set_outcome` | `outcome` | `setOutcome` |
| `random.draw` | `exclusive_upper_bound` | `randomBelow` |

Every action returns a saved result. Conditions inspect that result rather than invoking the operation again.

## Small expression vocabulary

`get`, `literal`, `query`; arithmetic `add`, `subtract`, `multiply`, `divide`, `mod`, `min`, `max`, `bit_and`; comparisons `eq`, `ne`, `gt`, `ge`, `lt`, `le`; boolean composition `all`, `any`, `not`.

This is a bounded expression tree with no arbitrary source evaluation, assignments, loops, callback names, or RNG in conditions. Sequential work is graph structure. Native eligibility/valuation/date/trade policies stay in their ordinary state methods.

## Consolidation decisions

- Flags/counters, packed-byte credit fields and topic masks become typed reads plus `state.set/add/bits`; the packed encoding is a native serialization concern.
- Single-ship default, explicit Fwiffo and bulk-gift forms are overloads of `fleet.give`.
- Crew/fuel/lander adjustments share `ship.adjust`; equipment, names and larger refits use validated `ship.configure` profiles.
- Technology ownership/grants hide whether the original implementation used ModuleCost, a shield bit, or a story flag.
- Fuel, technology and information sales share `trade.purchase`; their catalog data and result branches differ.
- Mineral/biomass/discovery payments share `trade.sell`; quantities, pricing, once-only discovery claims and speech are domain rules.
- Bulletin/device/advice generation share `report.prepare`, with a bounded set of registered report contracts and separate pure availability queries.
- Greeting variants/counter clamping share `sequence.next`; RNG is always an action.
- Alien-specific handlers become subgraphs. Phrase concatenation, animation/color-map changes, timing, status display and input belong to presentation.
