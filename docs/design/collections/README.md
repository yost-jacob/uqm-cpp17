# Game collections

## Ownership and storage

All legacy queue families now use typed C++ collection classes with an STL
storage type parameter. The default is `std::vector`. Existing domain owners
remain responsible for their data; this change does not replace Fleet,
RuntimeShipRoster or SimulationEngine with a second ownership hierarchy.

| Collection | Class / storage | Ownership and reason |
| --- | --- | --- |
| Roaming encounters | `BasicCampaignEncounters<Storage = std::vector<ENCOUNTER>>` (`CampaignEncounters` default alias) | GameSession member; dense records, reserved gameplay limit of 16; explicit selection |
| Player escorts and NPC ship fragments | `FleetShips<Storage = std::vector<Ship>>` | Fleet and legacy NPC roster; fixed-address slots because combat, picker and C callbacks retain vessel addresses |
| Battle participants | `BattleParticipants<Storage = std::vector<STARSHIP>>` | RuntimeShipRoster; participants and loaded behaviors refer to each other during combat and handoff |
| Race fleets | `RaceFleets<Storage = std::vector<FLEET_INFO>>` | Existing session campaign payload; fixed addresses remain required by C race/fleet consumers |
| Interplanetary groups | `SystemGroups<Storage = std::vector<IP_GROUP>>` | Existing session campaign payload; native group entities borrow group records during exploration |
| Simulation elements | `SimulationElements<Storage = std::vector<ELEMENT>>` | SimulationEngine; generational handles, collision phases and delayed removal require stable slots |
| Master ship catalogue | `MasterShips<Storage = std::vector<MASTER_SHIP_INFO>>` | Existing catalogue lifecycle; shares the C collection adapter |

The stable-address specializations share `CollectionStorage<T, Storage>`. Its
payload vector is sized once. Active order and available-slot order are separate
vectors, reserved once. Removing a record from active order moves pointers, not
live objects. Released slots retain the original LIFO reuse behavior. Counts are
constant time. Payload storage must provide vector-like sized construction,
contiguous data and reverse iteration; the ordering vectors remain standard
vectors. `std::pmr::vector` specializations are covered by tests.

The named templates configure storage at the collection layer. Fleet,
RuntimeShipRoster and SimulationEngine retain their established public types;
the C compatibility factory selects the default storage specializations.

## Access

Native encounter callers receive `CampaignEncounters&` from their session.
Iteration, insertion, erasure, selection and clear use the collection's methods.
Insertion returns end on capacity exhaustion. Erasure returns the next iterator
and maintains/clears selection. Selection does not reorder records. Current()
returns end when no encounter is selected.

A suspended navigation mode does not structurally mutate encounters while its
encounter child holds a reference. Child combat/conversation may update crew and
flags. Navigation reconciles/removes entries after return. No mutex is introduced
for this synchronous contract. Encounter runtime IDs survive vector movement;
transient screen associations use those IDs, not record addresses.

For remaining C pools, `detail::fleetShips`, `battleParticipants`, `raceFleets`
and `systemGroups` provide typed, read-only ordering borrows. Their records can
be updated in place. Fleet identity searches, roster scans, group notification
and hyperspace race-fleet traversal now use these vector ranges.

C allocation/insertion/removal still passes through the compatibility facade so
its head/tail/pred/succ projections stay synchronized. Those links are no longer
the ordering authority. Collection borrows end at destruction; active iterators
end on ordering mutation, and object borrows end on release/reset/destruction.
The existing pool limits, insertion order and generation checks remain intact.

## Encounter compatibility

Deleted the encounter QUEUE field, queue kind, intrusive links and queue macros.
C save/load code uses narrow count/append/clear/selection/archive adapters.
Native navigation, orbit scripts and EncounterRuntime use explicit references.
The navigation device menu receives the encounter collection explicitly. The
Talking Pet action retains the current encounter, falling back to the first
record only when none is selected, preserving the former head-record behavior.

Current save formats encode encounter selection by placing that record first.
The archive adapter produces that order without changing the live vector. Both
loaders restore selection from the first decoded record. Runtime IDs are never
serialized. Legacy decoding still consumes old queue/link/pointer placeholders;
serialized encounter fields and lengths remain unchanged. Over-capacity archives
are rejected rather than overflowing storage. The Talking Pet abduction retains
its full-capacity fallback of replacing the first encounter and selecting it.

## Validation

Existing queue tests cover capacities, alignment, order, LIFO reuse, mutation
while traversing, ownership swaps and handle invalidation. New coverage exercises
alternative PMR vector storage, dense encounter capacity, explicit selection after
erasure, stable runtime identity after records move, and archive selection order
without live reordering. Focused live checks exercise navigation contacts and
system NPCs. A real save/load restores a non-first selected encounter and both
fleets' crew, and a live handoff verifies that encounter mode prepares the
selected non-first fleet. Debug/Release builds and tests plus the preserved
legacy build validate the cutover.
