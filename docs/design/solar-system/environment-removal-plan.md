# Plan: retire the solar-system global environment

## Goal and stopping point

Remove implicit access to the current solar-system environment and separate its
remaining responsibilities before introducing the broader mode-runtime hierarchy.
Keep the current dispatcher, event scripts and C loops working throughout.

The intended result is an explicitly owned `SolarSystem`/`World` model, a small
flight state, an explicitly scoped orbital visit, and planet-owned generation /
presentation data. Remaining C code receives borrowed views of those objects.
There must not be a replacement global `SolarSystemContext` containing everything
formerly reachable through `pSolarSysState`.

This planning change does not modify gameplay code. Each numbered step is a
reviewable migration boundary;
large steps can use several commits, but should finish their cutover and remove
the superseded access path before moving on.

## Current evidence

At planning time, `pSolarSysState` appears on **105 source lines in 15 files**
under `sc2/src`, including its declaration/definition, debug code, and three
lines inside disabled code in `globdata.c`. These are lexical line counts, not
105 independent operations or active runtime call sites.

| File | Lines mentioning the pointer | Responsibility |
| --- | ---: | --- |
| `planets/plangen.c` | 33 | Terrain, colors, globe generation and work buffers |
| `uqmdebug.cpp` | 19 | Temporary debug enumeration bindings |
| `planets/solarsys.cpp` | 14 | Entry binding, flight/selection and C callbacks |
| `planets/pl_stuff.c` | 10 | Globe rotation, tint and orbital drawing |
| `planets/planets.c` | 6 | Orbital services and menu execution |
| `ipdisp.c` | 5 | NPC movement and system geometry |
| `planets/report.c` | 4 | Discovery report text/fonts |
| `globdata.c` | 3 | Disabled compatibility queries |
| `planets/calc.c` | 3 | Physical analysis and implicit analysis policy |
| `planet_render.cpp` | 2 | Surface construction boundary |
| `planets/scan.cpp` | 2 | Remaining C scan entry adapters |
| `grpinfo.c`, `world_c.cpp`, `planets/devices.cpp` | 1 each | Groups, progress and device entry adapters |
| `planets/planets.h` | 1 | Global declaration |

Eliminating only these pointer mentions would be insufficient. Explicit
`SOLARSYS_STATE&` arguments still expose a large mixed environment, and other
file-scope variables remain: notably `SysGenRNG`, globe rotation cursors in
`pl_stuff.c`, and the lighting/projection tables in `plangen.c`.

Current ownership already helps: exploration owns `UQM_SYSTEM_RUNTIME`; it owns
`SolarSystem` and the legacy descriptor storage. Worlds own moon snapshots and
`PlanetRenderResources`. `WorldVisit` owns orbital service/location scopes.
Player location and persistent world retrieval state already have session-owned
authorities. Those responsibilities should not move back into a new struct.

## Steps 1–2 completion checkpoint

- Reports now receive explicit text/font/effect borrows. Native report callers
  no longer rediscover the current solar-system environment.
- Deleted the unused production `GetPlanetInfo()` wrapper and disabled duplicate
  state queries. Archive tests use an explicit, test-only C-to-native bridge.
- Native surface ownership no longer checks the process-global selected system.
  The remaining legacy C surface-loader entry still resolves that binding.
- Catalogue context is an explicit visit borrow, available during construction;
  native star lookup no longer falls back through the active-session C getter.
- Deleted `SysGenRNG`. Each exploration/debug visit owns its generation workspace;
  layout, analysis, nodes and terrain receive explicit random contexts. Star-field
  randomness and high-resolution surface refinement have separate local streams.
- Analysis takes stellar type, orbital distance, immutable planet-type data and
  a named preview/orbit policy. Output address no longer controls temperature
  adjustments or random consumption. Node generation takes explicit map bounds
  rather than reading display width.
- Real analysis/node kernels and real planet-type tables now have an isolated
  Boost.Test target, without a selected world, star or display environment.

The lexical `pSolarSysState` inventory is now **93 lines across 11 files**, down
from 105/15. Remaining references belong to terrain/globe drawing (33), debug
binding (19), legacy flight/input and naming callbacks (14), globe animation (10),
orbital C loops (6), NPC callbacks (5), scan C adapters (2), and one each in group
loading, the C surface-loader adapter, the C device-menu adapter and its header
declaration. These counts do not imply that the remaining mixed environment is
already separated into proper data owners.

No physical planet data, scan progress, report-resource ownership or layout
storage moved as part of steps 1–2. Steps 3–7 are recorded below; step 8 remains pending. In
particular, mutable globe work tables and rotation state stay for step 5.

Compatibility: seed recipes and the legacy generation recurrence remain intact;
map bounds at game call sites retain their previous values. The formerly static
surface-refinement seed is now reset per generated surface, making visual detail
independent of visit history. That refinement does not change mineral/biological
node identities. Save format is unchanged; randomized universes are not added.

## Field destinations

These are responsibility names, not a demand for a class per field.

| Existing data | Intended owner / interface |
| --- | --- |
| Sun, planet and moon definitions, orbital geometry, stable body identities | `SolarSystem` and its `World` children |
| `WaitIntersect`, turn/thrust counters, flight speed limits | Exploration-owned `FlightState` |
| `InputFunc` | Small C input-loop envelope, created by the caller |
| `InIpFlight` | Explicit flight/visit transition state; eliminate redundant flags where derivable |
| `InOrbit` | Separate pending orbit-entry intent from the player's active orbit location; do not mechanically replace with one bool |
| Stellar size/intensity/energy | Immutable per-system analysis inputs |
| Physical planet properties and base scan seeds | Planet-owned generated data |
| Story-dependent physical overrides | Explicit visit preparation / documented world update, not a second independent property store |
| `ScanRetrieveMask` | Short-lived projection of persistent `WorldProgress`; accepted collection publishes through the existing state interface |
| Discovery text and lander report fonts | RAII resources for the active world visit/report |
| Graphics pointer and generated terrain/globe assets | Borrowed view of the world's graphics owner |
| Rotation cursors, projection/lighting scratch, tint state | Planet presentation/generation owner; working state separate from immutable tables |
| `worlds` back-pointer | Remove from native operations; retain only in a temporary C projection if necessary |

## 1. Make callers explicit at the outer boundary — complete

**Depends on:** existing `WorldVisit`, orbital child and session ownership.

- Inventory active uses and distinguish native callers, C callbacks and debug
  binding. Delete the disabled `globdata.c` queries instead of modernizing them.
- Give native device/progress/report entry points their actual required borrow:
  session, selected world, progress, report data or view. Stop resolving the
  active environment inside native functions.
- Keep any unavoidable global resolution inside named C adapters only. A native
  call must not travel through such an adapter and then rediscover its owner.
- Preserve the existing distinction between the active orbital simulation and
  exploration's engine when handing a surface scene to a lander.

**Done when:** remaining global-pointer references are confined to documented
legacy subsystems / C adapters; new native code has no implicit lookup. Update
this inventory once at the cutover, not after every function edit.

## 2. Extract explicit generation inputs and RNG ownership — complete

**Depends on:** step 1 for entry paths; enables steps 3 and 5.

- Replace `calc.c`'s reads of the current star/system with explicit stellar inputs,
  body parameters and parent orbital distance. Read immutable planet-type data
  through the session definitions or an explicit const table reference.
- Replace the address test `SysInfoPtr == &pSolarSysState->SysInfo` with a named
  analysis policy. It currently changes atmospheric temperature adjustment and
  random consumption; moving the output object must not change that behavior.
- Give generation an explicitly owned random context / passed reference instead
  of `SysGenRNG`. Keep generation random streams separate from simulation and
  presentation randomness.
- Preserve the existing seed derivation, reseeding points and consumption order
  needed for persistent mineral/life/node identities. Randomized universes (#80)
  remain a separate feature.

**Done when:** analysis and node generation can run for an explicit body without
an active global system; the ordinary analysis routine is data-only. Small
numerical differences are acceptable, but saved retrieval masks must continue
identifying the same generated nodes.

## 3. Separate planet data from the current `SysInfo` work area — complete

**Depends on:** step 2.

- Split physical properties and base scan seeds from retrieval masks, discovery
  handles and fonts. `PLANET_INFO` must stop mixing all four concerns.
- Construct/store generated body data with its native world owner. Use explicit
  typed preparation inputs for special worlds and story-dependent overrides.
- Distinguish immutable generated values from visit overrides. Commit a change
  only to its intended owner; do not maintain two independently mutable copies
  of the same live property.
- Let scripts and scan-node queries access the appropriate world/prepared visit
  directly. Populate a C record only for remaining C algorithms, with a clear
  one-way input/output contract.

**Done when:** selecting another world does not overwrite the only copy of the
previous world's analysis; physical data and seeds no longer live authoritatively
in `SOLARSYS_STATE::SysInfo`. Persistent retrieval progress still belongs to
`WorldState`, and player cargo still belongs to player inventory.

### Step 3 ownership checkpoint

Each `World` now owns a zero-initialized `PlanetGeneratedData` record containing
physical analysis and scan seeds. Orbit preparation and special event overrides
write that same record. Native scripts use `WorldVisit::data()`; remaining C
analysis, scanning, terrain and lander routines borrow it through
`PLANET_INFO::data`. Scoped borrows restore the previous pointer on normal return
and exception unwind. No copy-in/copy-out current-planet authority remains.

Analysis still runs at the existing preparation point: retained values do not
freeze story-dependent overrides or skip preparation. Preview analysis uses its
own short-lived record. System reconstruction transfers owned records for matching
world definitions, including seed, body type and orbital distance; changed
inputs retain a fresh empty record. This is runtime retention, not new save data.

At this checkpoint, retrieval masks and discovery report resources remained in
the compatibility visit record; step 4 below removes report resources. Stellar
analysis metadata remains in the system view. The
`pSolarSysState` lexical count remains 93 lines across 11 files: this step changes
ownership rather than deleting drawing/flight callback bindings. Tests exercise
nested borrow restoration, world independence, and reconstruction/invalidation,
as well as the real analysis and node kernels.

## 4. Finish the orbital visit and report ownership split — complete

**Depends on:** steps 1 and 3.

- Move discovery strings/fonts and their explicit load/reset operations into a
  visit/report RAII owner. Pass report content and resources to report code.
- Make ordinary preparation, special surface preparation, scan and report calls
  take their required world/visit/graphics references. Remove accesses through
  `pSolarSysState` and `system.worlds` in these paths.
- Keep event helper definitions in `uqm::orbit_events`, with state actions and
  child-mode sequencing visible. Replace their `legacySystem()` / descriptor
  access in bounded groups once the data/report operations exist.
- Replace the pending `InOrbit` flag with explicit orbit-entry intent while using
  the existing player location to represent the active visit. Homeworld contact
  can be an entry attempt without reaching a landable orbital view.

**Done when:** a visit can suspend for conversation/combat/lander and resume using
its own references. Teardown releases resources without giving rewards or
publishing ordinary-completion story actions. Load/shutdown cancels the rest of
the sequence, and the report code needs no global system.

### Step 4 ownership checkpoint

`WorldVisit` owns a `PlanetReport`: its text table, font and font effect are
private RAII resources. Scripts load/select/clear reports through that owner;
lander callbacks borrow the explicitly attached visit. Report display takes
only borrowed handles and menu sounds. Replacing or consuming text releases
it immediately; the font/effect pair lasts until visit teardown. Report-only
visits, inspection and exception unwinding release assets without requiring a
planet surface or publishing story/inventory changes.

The surface loader is now native C++ with explicit system/world references.
Deleted `LoadPlanet`, `ConstructPlanetSurface`, `GeneratePlanetSide`, the
load/free-font functions, and `FinishPlanetOrbit`. A visit-owned
`OrbitPresentation` scope establishes the display contexts and releases them,
rotation services and music before detaching graphics. It also handles partial
surface/node generation failure. The C drawing backend remains in use; mutable
rotation and terrain work are step 5, not a new renderer implementation here.
Both CMake and the legacy build compile `planets.cpp`.

Removed the overloaded `InOrbit` Boolean. Flight queues an optional `WorldId`
as orbit-entry intent and consumes it before running the visit. Device/save/UI
queries use the player's typed orbital location. Scripted contact can finish
without ever creating an orbit display. Mycon egg-case availability now checks
retrieval state rather than the lifetime of a loaded report table.

The lexical global-pointer inventory is now **84 lines across 9 files**:
terrain/globe generation (33), debug binding (19), flight/input/naming (14), globe
animation (9), NPC callbacks (5), and one each in group loading, the scan adapter,
the device-menu adapter and the declaration. No report resource fields remain in
`PLANET_INFO`; it holds only the generated-data borrow and retrieval-mask staging.
The temporary active-visit borrow belongs to the explicit exploration runtime,
is restored on unwind, and is not a new process-global binding. Remaining
`legacySystem()` use in event scripts is descriptor/generation/gameplay work
for later component migrations.

Tests cover report-only normal/load/exception exits, resource replacement and
cycling, nested mode scene handoff, restored visit/data bindings and partial
presentation failure. Teardown does not commit gameplay actions. The save format
and successful report sequencing are unchanged.

## 5. Move generation scratch and globe animation into their owners — complete

**Depends on:** steps 2–4; does not require a new renderer API.

- Pass prepared body data and owned graphics buffers into terrain/globe routines.
  Remove their reliance on the active selected descriptor and current system.
- Move mutable `light_diff` / `map_rotate` work tables, rotation frame/point,
  direction and shield animation state into the owning generation/presentation
  object. Keep truly immutable tables const/shared rather than copying them
  into every planet.
- Retain existing drawing functions as backend calls for now. This step owns
  their state; it does not introduce the future animation compositor or choose
  SDL libraries.
- Keep redraw distinct from animation advancement and allow cancellation before
  releasing any retained buffer/asset.

**Done when:** generating or advancing one world's presentation cannot overwrite
another world's mutable work state; `plangen.c`, `pl_stuff.c` and their native
owners no longer resolve the current solar-system global.

### Step 5 ownership checkpoint

`PlanetRenderResources` now lazily owns a `PlanetSphereData` workspace for its
generated surface. Lighting and tilt-projection tables use `std::array`;
elevation, RGBA and shader scratch buffers use `std::vector`. The C render record
borrows those stable buffers and the workspace only until reset. Rotation frame,
map position, direction, shield state and the next-frame deadline belong to the
same workspace. Regeneration starts with fresh animation state. Reset and failed
construction release the workspace and clear its borrowed pointers.

Converted `plangen.c` and `pl_stuff.c` to C++17. Their entry points now receive
explicit graphics, analyzed body data, descriptor, stellar-relative position and
random-workspace references as needed. Neither file accesses `pSolarSysState` or
selects a descriptor indirectly. Surface generation uses the analyzed body's
immutable planet-type reference rather than looking up a global table again.
Temporary interpolation/scaling buffers use vectors; temporary graphics contexts
and supplied/rescaled definition artwork have scoped ownership. Deleted the
shared lighting/projection arrays, static rotation timer/cursors and disabled
profiling accumulators. Immutable algorithm tables remain shared and const.

Native orbit, scan and lander callers pass their graphics explicitly. Deleted
the now-unused `DrawStarBackGround`, `ScanSystem` and `DevicesMenu` C adapters
after switching their last callers to explicit native operations. Redraw
functions draw the prepared frames without advancing the animation; the existing
rotation/zoom routines remain responsible for advancement. The orbit input-frame
callback still resolves the active graphics at its single boundary in
`planets.cpp`, because the legacy callback API has no userdata parameter. Its
binding is now restored by RAII, including starmap cancellation and exception
unwind. No replacement process-global workspace or renderer framework was added.

The lexical global-pointer inventory is **40 lines across 6 files**: debug
binding (19), flight/input/naming (13), NPC callbacks (5), and one each in group
loading, the orbital input callback and the declaration. The terrain/globe implementations have zero references.

Tests exercise two live graphics owners, independent buffers/projection values,
reset/regeneration and partial-construction cleanup without binding a selected
global system. The existing physical/node generation tests remain in place.
Per-body projection tables increase retained memory compared with one shared
workspace; allocation remains lazy and reset/system teardown releases it. The
existing renderer, animation cadence and generation recipes remain in use.

## 6. Isolate flight and NPC/group operations — complete

**Depends on:** step 1; complete data migration uses step 3.

- Move flight counters, collision suppression and derived flagship speed/turn
  limits into the exploration-owned flight state. Persistent position remains
  in `PlayerEntity`; this object does not acquire another authoritative position.
- Give NPC/group routines explicit system geometry and group/encounter state.
  Replace `ipdisp.c` and `grpinfo.c` global reads with the small data they need.
- Reuse ordinary encounter dispatch. Do not add optional NPC scripting merely
  to make it look like planet scripting.
- Where old callbacks cannot carry user data, isolate their context resolution at
  that C boundary. Use existing native behavior/entity ownership where possible;
  do not introduce another process-wide current-runtime binding for convenience.

**Done when:** native flight/NPC operations do not reach into the orbital visit,
report resources or selected planet analysis; callback adapters are the only
remaining exceptions and their borrow lifetimes are explicit.

### Step 6 ownership checkpoint

The exploration runtime owns `SystemFlightState`: turn/thrust countdowns,
loadout-derived speed/turn limits, collision suppression, flight activation,
frame timing and pending orbit intent. Those fields no longer live in
`SOLARSYS_STATE`. Persistent player position is unchanged. One `configure()`
operation serves normal entry and debug loadout changes without resetting active
countdowns. Thrust/control and collision-frame helpers receive the flight state
and legacy velocity/stamp/autopilot borrows they actually need.

Converted `ipdisp.c` to C++17. Native flagship and NPC group entities use the
existing engine-owned entity hooks. Each captures geometry, the native system
owner, player location/story, group queue and encounter output references. The
NPC's original death-hook movement stage, two-phase collision handshake,
reinforcement notifications, traversal order and encounter dispatch remain.
No callback resolves `pSolarSysState`, player location or story through globals.
The remaining C notification entry resolves its queue once; native callbacks
notify their explicitly supplied queue. The C element projection still carries
a non-owning group marker for the existing collision code; native group logic
uses a typed group reference rather than treating a group as a ship.

Converted `grpinfo.c` to C++17. Initial group placement takes `SystemGeometry`
and the destination queue. Sol's probe helper now also receives that geometry.
Saved-list/single-fleet loading remains a separate C operation and cannot
implicitly initialize placements. Serialization functions retain C linkage and
their byte layout. Deleted `DoMissions`, `BuildGroups`, the global planet-location
adapter and disabled duplicate destination-selection code. The existing group
file routines, last-encounter metadata, fleet-building session adapters and
legacy encounter output variables remain part of persistence/campaign cleanup;
this step does not redesign save storage or move those globals into flight state.

`SystemGeometry` is a read-only borrow of descriptor backing for now, with an
indexed planet-position operation. Step 7 replaces that backing authority;
there is no second copied planet layout. All entity hooks must finish before
that exploration/player context is destroyed, and pool teardown invokes no
state-publishing callbacks.

The lexical global-pointer inventory is **30 lines across 4 files**: debug
binding (16), flight/input/naming boundaries (12), the orbital input callback
(1), and the declaration (1). NPC movement and group-placement implementations
have zero references. Tests cover loadout changes preserving live countdowns,
geometry bounds, native group expiration/collision hooks and captured player
state remaining independent of the currently activated session. The live Sol
check advances both flagship and NPC entities through the actual engine loop.

## 7. Make layout descriptors borrowed projections — complete

**Depends on:** steps 3 and 6; presentation adapters consume step 5's interfaces.

- Move authoritative sun/body geometry out of `SunDesc` / `PlanetDesc` / the
  active `MoonDesc` arrays and into the native system/world tree.
- Build the fixed-capacity C descriptor views from those owners for remaining
  C drawing/collision code. Use stable world identities to represent parents
  and selections; do not retain pointers into a reused moon-view buffer.
- Keep generation-time mutation in a short construction buffer where old
  algorithms require it, then adopt the result into native ownership. Do not
  keep generator buffers and native objects as competing live authorities.
- Preserve gameplay capacities, traversal/drawing order, save identifiers and
  special-body semantics. Make story-driven regeneration/invalidation explicit
  rather than hiding it in rendering or recreating cached assets accidentally.

**Done when:** `SOLARSYS_STATE` arrays are removable projections, and changing
views cannot invalidate authoritative world identity or erase another body's
owned data.

### Step 7 ownership checkpoint

`WorldDefinition::orbit` owns each planet/moon's generated radius and logical
position; seed, type, shielding and declared children are also native values.
`SolarSystem` owns the generated stellar seed/type and derives the planet count
from its native children. Parent relationships use WorldId, never pointers into
a reused descriptor array. Deleted `WorldGeneration` and its separate vector of
moon-descriptor snapshots: moon projections are built directly from native
children. Geometry participates in reconstruction matching, so changed inputs
invalidate retained analysis conservatively.

System construction uses a private temporary `SOLARSYS_STATE` generation buffer.
Existing generation algorithms fill it, then the native tree adopts the values.
Construction does not publish that scratch backing into the active flight view.
The production boundary publishes the completed candidate's descriptor projection
after adoption. Debug import boundaries explicitly adopt generated planet/moon
values; their C arrays are construction inputs, not live parallel authorities.

`SolarSystem::project()` restores physical fields in the C view while retaining
view-owned frame/color state. Typed native shield changes appear on the next
projection. `World::projectMoons()` requires the correct parent view and records
its parent identity. Accessing a moon through another parent's active projection
throws; selecting/rebuilding a view never changes the native moon or its ID.
Raw descriptor and parent-pointer borrows end when that view is reprojected.

The old SunDesc radius/location were camera zoom and an outer-view anchor, not
stellar properties. These now belong to `SystemFlightState`; native flight and
coordinate helpers use that owner. The sun view projects them for the remaining
NPC/drawing consumers. C descriptor arrays remain in the compatibility envelope
as fixed-capacity drawing/collision views. They retain gameplay capacities,
ordering and visual state, and may be removed when their backend consumers are
migrated; they are not a second editable native layout.

The lexical global-pointer inventory remains **30 lines across 4 files**. This
step changes layout authority, not the remaining debug/input/UI bindings. Tests
cover construction without publishing scratch buffers, independent native and
C-view lifetimes, restoring a modified view, retaining render state, typed shield
updates, moon-parent validation and stable moon identities across view changes.
The existing retrieval-state, generation and reconstruction tests remain in use.
A live lunar-orbit return rebuilds the system view and checks its moon seed and
parent link; a separate live flight check advances flagship and NPC entities.
Save identifiers and record formats are unchanged. The ownership map is ready
for step 8's remaining-boundary audit; no new mode-runtime hierarchy was added.

## 8. Delete the remaining global environment and stop — next

**Depends on:** the completed component cutovers above.

- Remove `pSolarSysState` declaration, definition and production binding when no
  C callback needs it. If one hard C boundary remains, keep a narrowly scoped,
  non-owning adapter there and record its exact caller/reason; no native API may
  depend on it.
- Reduce the old state record to the input envelope / borrowed projections still
  required by C functions, or delete it entirely if those callers are gone.
- Remove stale fields, context back-pointers, wrappers, old getters and header
  dependencies as their final callers disappear. Do not preserve an unused
  fallback environment for startup/debug convenience.
- Use explicit inspection inputs for the debug universe dump; preserve its
  no-mode/no-location-change contract.

**Ready for the new runtime architecture when:** world data, flight state,
orbital visit/report resources and generated presentation state have clear
owners; gameplay work takes explicit references; and the global environment is
absent or confined to a genuinely unavoidable C callback adapter. New runtimes
can then compose these components without another ownership migration.

Full menu modernization, animation/audio compositor replacement, removal of all
C graphics calls, save-format redesign and online simulation are not prerequisites
for this stopping point. Those remain their own subsystem work.

## Validation budget

For each component cutover, use focused Boost.Test cases for its real contracts:
analysis policy independent of output address, stable generation/node identities,
state publication, independent planet work buffers, nested scene handoff and
cancellation. Avoid tests that merely restate getters or class layout.

At a reviewable cutover, run Debug/Release CMake+Ninja builds and existing tests,
and verify the legacy build. Use a small live orbit/scan/lander smoke only when
execution/presentation boundaries change. Do not require repeated full-game
playthroughs or bit-perfect numerical comparisons. Update the ownership map and
remaining-reference counts at the end of each completed step.

## Primary source anchors

- [Mixed state record](../../../sc2/src/uqm/planets/planets.h)
- [Exploration owner](../../../sc2/src/uqm/system_runtime.hpp)
- [Native world ownership](../../../sc2/src/uqm/world.hpp)
- [Visit and child handoff](../../../sc2/src/uqm/world_visit.cpp)
- [Inline event helpers](../../../sc2/src/uqm/orbit_events.hpp)
- [Physical analysis / implicit policy](../../../sc2/src/uqm/planets/calc.c)
- [Mixed planet-info record](../../../sc2/src/uqm/planets/plandata.h)
- [Terrain / globe work state](../../../sc2/src/uqm/planets/plangen.cpp)
- [Rotation cursors](../../../sc2/src/uqm/planets/pl_stuff.cpp)
- [NPC callbacks](../../../sc2/src/uqm/ipdisp.cpp)
- [Group loading](../../../sc2/src/uqm/grpinfo.cpp)
