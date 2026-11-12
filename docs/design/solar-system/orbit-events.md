# Constructor-selected orbital events

Each `World` stores `std::optional<OrbitHandler>`, where `OrbitHandler` is a plain
`void (*)(WorldVisit&)` function pointer. Its canonical constructor initializes
this member from `selectOrbitHandler(definition_)`. The selector uses the
immutable special-system ID and the planet/moon identity. It never reads story
state or runs an event during construction.

- `nullopt`: the visit prepares ordinary orbital data and enters an orbital child
  mode when a surface is available.
- Stored pointer: invoke the procedural system-event script instead. It can
  communicate, fight, prepare an orbit, hand off to another activity, or return
  to flight without showing an orbital menu.

There is no hash registry, string lookup, `std::function` allocation or switch on
orbit entry. The switch runs once per World construction. A reconstructed system
creates new worlds and selects their handlers again; no process-wide mutable
handler cache is introduced.

## Existing scripts and selection

The initial event function reuses `SystemBehavior::generateOrbital`, retaining
its existing authored sequences and current-story checks. This virtual call is
an adapter to those scripts, not a new lookup of the world's event identity.
Their future extraction into individual free functions is independent of this
change. Ordinary worlds bypass that virtual orbital hook.

Selection is restricted to the bodies affected by each existing script: for
example, Chmmr planet 1 and its starbase, the Taalo moon at planet 1/moon 2, and
the bomb moon at planet 5/moon 1. NPC-only special systems have no orbital event.
Sol is intentionally different: every body uses custom analysis/scan-seed rules,
so all Sol worlds receive its event function. Handler selection does not depend
on temporary conditions such as whether the Chmmr have been freed.

The special-system ID is copied from the star descriptor into runtime world
definitions, including moons and legacy descriptor-only attachment paths. It is
not a new saved flag and does not change the save format.

## Visit and mode ownership

```text
SolarSystemMode / exploration
  WorldVisit: selected world, location and graphics service scopes
    optional event script
      SystemEncounterMode -> ConversationMode / CombatMode as needed
    OrbitMode: prepared orbital menu and surface simulation scene
      LanderMode: temporarily takes the same surface scene
    return to exploration, or propagate a session/activity exit
```

`WorldVisit` borrows the session, system and selected world for one synchronous
visit. It owns the location and graphics binding scopes. Destruction finishes
active orbital services and detaches the borrowed graphics record; the world
continues owning its generated assets. It does not commit story rewards or
retrieval progress in a destructor.

`OrbitMode` borrows that visit's system and transfers the prepared surface scene
from the caller's simulation into its own engine. The scene is returned during
child teardown, including exceptions and session exits. The lander now takes
its source from the active orbital engine rather than the exploration engine.
Orbital suspend/resume uses the existing drawing-binding token and restores the
current orbit/scan view after a lander child without restarting generation.

Load/shutdown/failure must stop the remaining sequence. The existing legacy
activity flags are still translated at the boundary, while the dispatcher
propagates typed session exits. The visit does not enter the orbital menu after
an event requests an exit. Flight reconstruction happens only after a completed
visit permits it.

## Validation and remaining boundaries

Boost.Test covers applicable/ordinary selection, special-system propagation to
constructed planets/moons, current Chmmr story changes after pointer selection,
a nested encounter requesting load, ordinary orbital child execution, nested
surface-scene transfer, normal resume, load cancellation and exception cleanup.
The device-free tests use fake menus and existing fake engine dependencies.

The orbital menu/scan drawing, physical analysis, terrain generation and authored
system scripts still contain legacy code. This change establishes the event and
mode handoff; it does not claim to replace those subsystems. NPC groups continue
using their ordinary encounter path and acquire no new optional scripting field.
