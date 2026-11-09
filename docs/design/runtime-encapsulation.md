# Exploration, navigation and orbital encounter ownership

This describes the live implementation, not the larger battle/conversation
wireframes. Gameplay still runs synchronously on the simulation thread.

```mermaid
flowchart TD
    Session["GameSession dispatcher"] --> Hyper["HyperSpaceMode"]
    Session --> Solar["SolarSystemMode"]
    Hyper --> Nav["HyperSpaceNavigation<br/>borrows PlayerEntity / CampaignState"]
    Nav --> Frame["Battle callback plus explicit context"]
    Frame --> Engine["Existing C element / rendering engine"]
    Engine --> Bind["Checked C navigation entry points<br/>scoped session binding"]
    Bind --> Nav
    Solar --> Explore["SolarSystemExploration<br/>owns engine descriptor + native worlds"]
    Explore --> Rules["SystemBehavior<br/>borrowed campaign, star and session"]
    Explore --> Scan["Native scan caller<br/>direct virtual hooks"]
    Scan --> Lander["LanderVisit<br/>owned launch state / borrowed system and campaign"]
    Rules --> Child["SystemEncounterMode<br/>owns suspended parent through swapGameMode"]
    Child --> Dialog["Unchanged legacy communication / combat"]
    Child -. restore parent before returning .-> Solar
    Rules --> Fleet["OrbitalEncounter scope<br/>temporary fleet cleanup / group restoration"]
```

## Exploration

`SolarSystemMode` owns `SolarSystemExploration` as a member. Its opaque runtime
contains the actual `SOLARSYS_STATE` descriptor, native SolarSystem owner,
borrowed CampaignState and GameSession, and shared WorldState. Descriptor
addresses stay stable through nested encounters. The C input backend still
borrows its existing descriptor shape; no owning C++ members enter save records.

Exploration, scanning and debug traversal now compile as C++. They call
`SystemBehavior` directly. The `genFuncs` field, all thirteen generator bridge
functions, the bridge table and `generate.h` are deleted. Active-system binding
is restored when exploration returns or unwinds, and completed initialization
has scoped `UninitSolarSys` cleanup. Partial failures *inside* legacy initialization
are not made transactional by this scope.

Native system behaviors receive their immutable catalogue record before hooks.
Their story methods no longer read `CurStarDescPtr`; private helpers such as the
Melnorme selector use the attached record too. Androsynth discovery reads use the
injected campaign. Catalogue records outlive every explored system and cannot
be rebound midway through a visit.

## HyperSpace and QuasiSpace

`HyperSpaceMode` owns one `HyperSpaceNavigation`, borrowing its player and
campaign. Fuel-distance accumulation belongs to this visit. The portal location
belongs to transient PlayerEntity navigation memory, because it must survive an
encounter and subsequent navigation-mode recreation. It retains the old
nonserialized lifetime semantics rather than changing the legacy save format.

Navigation reads story fields through CampaignState and accesses flagship fuel,
crew and logical coordinates through PlayerEntity methods. Its frame callback
now receives an explicit context pointer in `BATTLE_STATE`. Combat callers with
no callback pass null context. This runtime field is not serialized.

C entry points that cannot yet carry userdata use a scoped, checked session
binding. It rejects another player's navigation object and restores the previous
binding on normal return or exception. It owns nothing. There is no new
process-global owning navigation singleton. Native helpers call each other on
their existing object rather than repeatedly looking up the active session.

`SaveSisHyperState` remains a standalone compatibility operation: it is also
called from an encounter save screen *after* the navigation mode has ended.
Requiring an active navigation owner there would break existing saves. Sprite
and colormap caches keep their existing process/resource lifecycle.

## Orbital encounters

The explored behavior is attached to its production session. Its `communicate`
method creates a `SystemEncounterMode` and calls `swapGameMode`. The child runs
the existing `InitCommunication`, then returns its suspended parent or a pending
session exit. It does not rerun the parent. The communication result borrows a
variable on the suspended caller's stack, whose lifetime spans the whole child.

For load/title/shutdown requests, the helper translates the result to the legacy
exit flags so remaining exploration frames unwind. No new campaign is decoded
while an old handler or lander is suspended. Native production modes also receive
their session explicitly rather than looking up the active process session in
each mode body. Legacy debug/test contexts without an attached session retain
their explicit synchronous backend entry.

Sixteen temporary orbital encounter setups now use `OrbitalEncounter`. It saves
system groups before clearing them, owns responsibility for the temporary fleet,
and restores at the original explicit story boundary or on early scope exit.
Restoration is idempotent. Load/abort retains the original handoff to campaign
teardown; it does not restore groups into a departing campaign.

Sol's starbase handoff is intentionally different: it leaves exploration and
hands state to the outer encounter mode, rather than restoring a temporary
fleet within the orbital call. Its persistent group-save operation remains
explicit. NPC lifecycle/archive operations also remain distinct from temporary
encounter scopes.

## Lander

`runLander(system, location)` owns a private `LanderVisit`. The formerly public
PLANETSIDE_DESC is removed from the C header. Crew, movement/weapon delays,
damage, explosion and cargo data live in that visit rather than separate process
statics. Surface callbacks use its borrowed system/campaign references; no
`pSolarSysState` or story macros remain in the lander implementation.

One scoped nonowning visit pointer remains for legacy C element callbacks with
no userdata. It is restored before the visit owner disappears. Graphics/music
caches retain their old resource lifetime. The existing `InitLander` C UI entry
obtains the active campaign once because it can run outside a launched visit.
Creature, mineral, hazard and projectile algorithms remain procedural.

## Remaining boundaries and verification

This does not make gameplay concurrent or headless. Rendering/input handles,
physics queues, selected-star compatibility storage, portions of the exploration
engine and cargo/UI mutation still use the legacy backend. The conversation
implementation and combat mechanics are unchanged. The nested-mode catch still
uses the agreed diagnostic + abort policy with a graceful-shutdown TODO.

Boost.Test covers scoped navigation binding, player isolation, portal-memory
lifetime, packed story encodings, explicit star binding, nested system-encounter
resumption/load requests, and temporary-fleet restoration/abort handoff. Fake UI
and encounter services exercise ownership contracts without simulating dialogue
or terrain rendering. The full game links all real implementations. Build,
sanitizer and focused GUI results are recorded in the PR; they are not exhaustive
planet or story playthroughs.

Sources: [exploration owner](../../sc2/src/uqm/system_runtime.hpp),
[engine](../../sc2/src/uqm/planets/solarsys.cpp),
[navigation](../../sc2/src/uqm/hyper_navigation.hpp),
[scoped navigation binding](../../sc2/src/uqm/navigation_context.cpp),
[child encounter](../../sc2/src/uqm/system_encounter.cpp),
[fleet scope](../../sc2/src/uqm/orbital_encounter.hpp),
[lander](../../sc2/src/uqm/planets/lander.cpp).

## Validation for this pass

Debug and Release CMake/Ninja builds and CTest pass. A fresh legacy dependency
regeneration and link pass, and the executable reports its version. The 142
Boost.Test cases / 13,445 assertions pass under ASan/UBSan. Navigation, exploration,
scan, lander, mode and encounter/context sources pass strict warning checks.

A focused GUI run started a new campaign in Sol, flew into HyperSpace, entered a
normal fleet encounter, saved slot zero from that encounter screen, and loaded
it back to the same date/location/fuel/crew. Orbital child encounters and lander
ownership are covered by dependency-isolated regression checks and compilation,
not exhaustive interactive story or landing playthroughs.
