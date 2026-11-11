# Session game data: economic tables

`GameSession::gameData()` returns the current campaign's `GameData` by reference.
The const overload returns a const reference. `PlayerEntity` owns the storage
through its existing authoritative payload; a session borrowing that player sees
that same data. Independent players have independent copies. There is no new
singleton, second price cache, or shared mutable pricing table.

`game_data.hpp` defines the immutable C++17 defaults and the shared, checked ship
price lookup. `game_data.h` supplies the small C-compatible storage and adapters
while C menus and serializers coexist with C++. Enum values remain table indices.
The module/mineral arrays retain their native sizes, element types and units.

| Table/access | Meaning |
| --- | --- |
| `data.prices.crew` | Current starbase crew hiring rate in RU |
| `data.prices.fuel` | Starbase fuel rate per displayed unit |
| `data.prices.modules[id]` | Module price in `MODULE_COST_SCALE` units |
| `data.prices.minerals[category]` | RU paid per mineral unit |
| `shipPrice(race)` | Immutable RU ship price, also used for fleet valuation and encounter bounty calculation |

C callers borrow these same tables through `GetGameData()`. `GetShipPrice()` is
only the C adapter for the shared ship table. Invalid C++ ship indices throw;
invalid C indices abort rather than allowing an exception to cross the C ABI.
Do not retain adapter pointers across session transitions. Lookup does not change
balances, story flags, resources or RNG. Mutation remains the caller's job.

`defaultGameData()` supplies a detached new-campaign value; `ResetGameData()`
assigns it. New players and new campaigns receive defaults. Loading restores the
saved mutable tables instead of recomputing them from current story flags.
The serializer and both current/legacy readers still write/read the original
crew, fuel, module and mineral bytes in exactly their previous order. The save
chunk size and format are unchanged. No raw struct serialization is introduced.

## Mutation sites to consolidate next

- Shipyard crew accounting raises/lowers hiring rates at its purchase threshold.
- The Shofixti return event lowers the hiring rate.
- Druuge farewell applies the accumulated crew-sale penalty.
- Melnorme technology acquisition sets a module's price/unlock value.
- Chmmr refit writes zero module prices to disable landers/storage bays.

Those callers now use the common tables but retain their existing sequencing and
arithmetic. A follow-up can move each coupled story/rule update into a named
story helper. Do not turn every price read into an interpretation of story flags.

## Deliberate boundaries

This pass packages the existing campaign price/value tables and consolidates the
three copies of ship prices. It is not a universal catalog of every game constant.
Merchant-specific trade recipes/technology descriptions, ship tuning, starmap
content, presentation descriptors and application configuration retain their
existing owners. Base economic defaults remain constexpr definitions here;
loading them from a data file can replace the baseline without changing callers.

Zero module price still doubles as the existing unavailable marker. Splitting
availability from price, general transactions, story helpers and moving merchant
trade recipes into common data are subsequent changes. Rendering still has legacy
state mutations unrelated to these price tables. These are not silently fixed by
renaming the price access path.
