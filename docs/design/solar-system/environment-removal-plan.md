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

## 1. Make callers explicit at the outer boundary

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

## 2. Extract explicit generation inputs and RNG ownership

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

## 3. Separate planet data from the current `SysInfo` work area

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

## 4. Finish the orbital visit and report ownership split

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

## 5. Move generation scratch and globe animation into their owners

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

## 6. Isolate flight and NPC/group operations

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

## 7. Make layout descriptors borrowed projections

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

## 8. Delete the remaining global environment and stop

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
- [Terrain / globe work state](../../../sc2/src/uqm/planets/plangen.c)
- [Rotation cursors](../../../sc2/src/uqm/planets/pl_stuff.c)
- [NPC callbacks](../../../sc2/src/uqm/ipdisp.c)
- [Group loading](../../../sc2/src/uqm/grpinfo.c)
