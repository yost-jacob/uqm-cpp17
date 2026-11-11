# Positioned NPC fleets: next ownership boundary

A discrete NPC group naturally composes a persistent `Fleet` with a stable group
identity, location, and mission state. Use composition rather than a fleet class
hierarchy. Position must distinguish galaxy coordinates from local solar-system
coordinates; a bare unqualified `POINT` would retain the current ambiguity.

The current records are not all the same kind of fleet:

| Current storage | What it represents |
| --- | --- |
| `npc_built_ship_q` | The ship participants prepared for the current encounter; it has no independent position |
| `ip_group_q` / `IP_GROUP` | Moving system groups with position, group ID, task, destination/orbit, counters and reform/ignore flags; composition is also represented in group-state storage |
| `encounter_q` / `ENCOUNTER` | Hyperspace contact position, race and brief ship roster, transition/reappearance state, and a temporary rendering/simulation handle |
| `avail_race_q` / `FLEET_INFO` | Race-wide strategic strength/influence, movement/destination, alliance status and the player's last-known information; it is not an enumerated group of ships |

For actual positioned groups, the likely native owner is `NpcFleet`: a `Fleet`,
`GroupId`, explicit location and mission/destination values. The simulation owns
movement and transient collision/render entities. Campaign/galaxy storage owns
persistent group data; an encounter borrows or explicitly materializes its ship
participants. The exact persistence/materialization interface is still to design.

Do not fold player knowledge into the actual fleet position, or interpret a
race-wide sphere of influence as a container of individual vessels. Preserve
stable group IDs, iteration order, encounter limits, and the current persistence
of defeated/reforming groups. Infinite scripted reinforcements should become an
explicit policy rather than continuing to overload a ship's crew count.

This is an inventory and proposed direction, not an implemented NPC migration.
The campaign-clock migration does not change these queues or fleet behavior.
