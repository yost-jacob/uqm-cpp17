# Game-mode ownership roadmap

## Goal

Bring each mode to the exploration ownership checkpoint before introducing the
common runtime/simulation interfaces. A mode owns transient simulation and visit
resources; the session owns persistent state and shared assets. Native operations
receive explicit references. Remaining backend callbacks have narrow, scoped
borrows. Teardown releases resources without applying completion rewards.

This is an ownership migration, not a requirement to remove every C algorithm or
implement a universal physics engine. Share mechanisms where their semantics
match; preserve mode-specific rules and gameplay capacities.

## Actual modes and proposed order

The order below follows navigation into its child activities, then shared combat
into its arcade host, then the front end. A row may contain several concrete mode
classes sharing one ownership concern. This is not a claim that all rows are
independent or need the same simulation type.

| Order | Existing mode(s) | Current owner / remaining boundary |
| --- | --- | --- |
| Checkpoint | `SolarSystemMode` (ExploreSpace) | `SolarSystemExploration`, native `SolarSystem`/`World` tree, `SystemFlightState`; eight-step ownership migration complete. Remaining drawing/collision projections and C service interfaces are documented separately. |
| 1 | `HyperSpaceMode` | Owns `SpaceNavigation`; borrows player, story, clock and mode engine. Run-scoped presentation retains cached graphics and owns its colour map; native entity callbacks borrow navigation directly; encounter screen associations are visit-owned and generation-checked; campaign queues and transition flags still use legacy interfaces. **Ownership checkpoint complete.** |
| 2 | `QuasiSpaceMode` | Separate mode owning `SpaceNavigation` configured for Quasispace. Shares mechanisms with Hyperspace; portal destinations and fuel/encounter rules differ. **Ownership checkpoint complete.** |
| 3 | `EncounterMode`, `SystemEncounterMode` | Own `EncounterRuntime` with explicit campaign references and session-owned group archive; scoped arena resources. Menus/tally services remain deferred. **Ownership checkpoint complete at the agreed boundary.** |
| 4 | `OrbitMode` | Owns `OrbitRuntime`, scan cursor and orbital engine; borrows the enclosing `WorldVisit`. Visit-owned contexts/music and planet-owned generated assets have scoped handoffs. Menus remain deferred. **Ownership checkpoint complete at the agreed boundary.** |
| 5 | `LanderMode` | Owns a child engine; audit surface entities, expedition state, collection/loss publication and landing/return presentation. Keep durable retrieval and inventory state in their existing owners. |
| 6 | `ConversationMode` | Owns `ConversationRuntime`; audit graph/script state, presentation/audio/input bindings and transaction completion. Reuse the existing graph engine, not another dialogue rewrite. |
| 7 | `CombatMode` | Shared campaign/arcade combat mode; audit roster/ship/entity lifetimes, projectile source references, round completion, survivor reconciliation and cancellation. |
| 8 | `FleetSelectionMode` | Owned by the separate `ArcadeSession`; audit frontend/network/selection resources and handoff to the same combat mode. |
| 9 | `LoadMenuMode`, `TitleMenuMode` | Audit UI state and borrowed result storage; saves/loads remain a session responsibility. Ordinary cargo/roster/starmap/options menus are currently functions, not independent modes. |
| 10 | `CutsceneMode` | One concrete mode currently selects startup, attract, credits, introduction or ending. Audit playback ownership/completion before deciding whether separate derived scene types add value. |
| Tooling | `DebugMode` | Debug-only hook runner; explicit inspection context and exception-safe bindings must remain intact. Not a gameplay simulation. |

Outfitting and starbase activities are still legacy entry paths, not existing
independent modes. Their eventual boundaries belong in the UI/state audit; do
not disguise them as completed modes in this inventory.

## Hyperspace and Quasispace separation

The dispatcher chooses a concrete mode from the saved story realm marker. Each
mode constructs `SpaceNavigation` with a fixed `NavigationRealm`; fuel use,
encounter eligibility, graphics and location access use that realm. Portal
collision phase still belongs to the legacy story counter. A crossing stages
the destination and realm marker, ends the navigation scene, and lets the
dispatcher select the successor. Only the incoming mode converts the player
location variant, after outgoing callbacks and resource scopes have unwound.

## Shared navigation ownership sequence

1. **Native scene boundary — implemented.** Pass `SpaceNavigation&` into
   `runSpaceNavigation`; obtain its engine from that owner and pass it into the
   scene. Scene acquisition/release calls that explicit navigation reference.
   Delete the unused global C `LoadHyperspace`/`FreeHyperspace` wrappers. The
   callback binding remains only for legacy callbacks, not native scene setup.
2. **Presentation resources — implemented.** Each navigation run owns a
   `NavigationPresentation` lease: ambient/selected-realm graphics use the session
   asset cache, and the selected colour map has scoped ownership. Elements borrow
   a stable frame address until the scene resets its engine. Scene teardown
   restores `stars_in_space` before `UninitSpace`; the lease also restores it on
   exception unwind before releasing its assets. Deleted static graphics/colour
   arrays, `FreeHyperData`, and movie/application cleanup callers. Missing assets
   throw and release partial acquisitions. Other-realm artwork is not loaded.
3. **Navigation entities — implemented.** Engine slots own native star,
   portal, encounter and flagship behaviors holding `SpaceNavigation&`. The
   flagship is attached before the first navigation clock/simulation pass; its
   flying/arriving/departing phases preserve shared ship updates and suppress
   postprocessing during transitions. Star/portal/encounter collision and
   expiration hooks retain their original simulation phases. Encounter
   `death_func` presence still represents active transition animation; no pointer
   comparison with a removed navigation trampoline remains.

   Deleted `NavigationBinding`, `GameSession::navigation`, its stored pointer and
   navigation-specific C trampolines. Ship arrival and flagship menus use their
   entity's explicit navigation reference. Save-facing preparation also resolves
   its supplied flagship record. The remaining C camera/clock functions lack an
   entity parameter; they resolve the selected engine's flagship owner, never an
   active-session getter. Shared ambient animation remains navigation-independent.
   Navigation captures a single `GameSession&` at construction and derives its
   player, story, clock and encounter borrows from that owner; unrelated borrow
   combinations cannot be supplied. `run()` uses that captured owner.
   Scene reset destroys all behaviors before navigation/resources can expire.
4. **Encounter data inputs — implemented.**
   Deleted the screen-element handle from persistent `ENCOUNTER`. Each navigation
   visit indexes campaign records through generation-checked `EntityHandle`s;
   free/reset/reuse cannot resolve a stale projection. Clear the index after
   selecting the collided campaign record, and erase an entry before freeing its
   campaign slot. Dropping a projection does not itself mutate fleet/crew data.
   Modern save fields and record size on disk remain unchanged. Legacy decoding
   still consumes its historical pointer placeholder without storing it.

   Campaign encounter access now uses the explicit session-owned dense vector and
   selection interface described in [Game collections](../collections/README.md).
   Navigation reads race-fleet strength, location and ship defaults through its
   owning session's checked `raceFleets()` view. It no longer resolves fleet
   queues or races through GLOBAL/Lock/GetStarShipFromIndex. Generation reads
   current fleet records, not detached snapshots. Spawn percentages, encounter
   size ranges and pursuit speeds are typed constexpr `NavigationRules` exposed
   by Session. Deleted their list macros and packed-nibble lookup at callers;
   interplanetary group generation shares the same size table. Numerical values,
   random-call order and pursuit limits are unchanged.

   Catalogue searches use the owning session's immutable StarMap with the mode's
   fixed realm. Star selection updates the owner's NavigationState by StarId.
   Radar and star entities read native properties; navigation no longer calls
   FindStar/GetCurrentStar/SetCurrentStar or resolves C descriptor/index buffers.
   Step 5 below narrows transition flags, autopilot and ship-stamp compatibility;
   archive-format redesign remains separate.
5. **Transitions and cleanup — implemented.** `SpaceNavigation::run()` returns
   an optional `NavigationTransition` value after its scene/resource scopes have
   unwound. It identifies solar-system entry, an encounter or either navigation
   realm, carrying a StarId when needed. The outgoing mode chooses a concrete
   successor from that result. Scripted bomb/debug priorities and shared status /
   velocity preparation remain intact. One mode-boundary function projects the
   result into START_* flags for remaining C consumers; those flags no longer
   determine ordinary navigation's destination.

   Navigation menu batch/context/colour/flash bindings now restore through RAII,
   including exception unwind. Legacy device-menu results are adapted at the
   input boundary. Load/abort and
   requested session exits discard pending transitions; no successor launches
   against a campaign being replaced. Portal-side normalization and remaining
   contacts cannot overwrite a staged destination. World position and realm
   markers remain player-owned, and only the incoming mode converts location.
6. **Final audit and checkpoint — complete.** `hyper.cpp` has no GLOBAL,
   active-session, fleet-index or C catalogue getter paths. Remaining legacy
   activity/autopilot/drawing-stamp accesses are a narrow three-reference borrow
   of the supplied player's session payload (`NavigationLegacyView`), not a
   replacement global environment or duplicate data authority. Temporary portal
   visuals use position/frame values, not fake STAR_DESC records. Native fleet
   departure saves facing from its supplied flagship record; only C save/device
   entry points need the no-argument compatibility wrapper.

   The ownership map and exact C boundaries below are the stopping point before
   the shared runtime/simulation design. No runtime framework was introduced.

## Verification

Use existing Boost.Test suites and focused lifetime/cancellation cases for changed
boundaries. Build with CMake/Ninja and the preserved legacy build at cutovers.
Use a short live navigation/realm/encounter check when changing actual gameplay
callbacks; do not require a campaign playthrough for each ownership increment.

The native-scene regression runs without a session navigation binding. Native
entity-hook regression coverage verifies explicit owner routing, flagship phase
switches and cancellation without gameplay expiration hooks.
A live debugger-driven realm round trip also advances native arrival, sustained
normal flight and departure hooks before scene teardown. Backend asset loading
remains substituted in the orchestration test; it does
not claim to validate actual graphics loading or realm transitions.

## Runtime composition target

The eventual mode runtime is parameterized by simulation, UI and compositor,
with the shared AV compositor as the default final template argument. Navigation
modes share NavigationUI; combat and conversations have their own UIs, and title
and starbase activities have separate UI concerns. UI owns pane layout, relative
sizes at a resolution, menu scripts, selection and cursor placement. The shared
compositor presents scene/UI layers and coordinates animation/audio playback.
Simulation and UI vary independently. This ownership cutover does not introduce
that framework or a second compositor implementation.

## Completed navigation ownership map

| Responsibility | Owner / borrow |
| --- | --- |
| Travel position/facing, realm markers, portal coordinates and selected StarId | PlayerEntity / NavigationState / StoryState |
| Encounter population and explicit selection | GameSession::CampaignEncounters; runtime identity survives dense-vector moves |
| Live race-fleet records and ship defaults | Session campaign payload through checked typed raceFleets() borrow |
| Immutable catalogue and navigation baseline rules | Session definitions / constexpr NavigationRules |
| Realm, pending transition and navigation counters | Mode-owned SpaceNavigation |
| Flagship, stars, portals, vortices and ambient effects | Mode-owned SimulationEngine; entity borrows end at reset before navigation dies |
| Ambient/realm graphics, colour map and star-field lease | Run-owned NavigationPresentation, retaining session-cached graphics |
| C activity/autopilot/stamp fields | Explicit NavigationLegacyView references to the owning player's payload |

### Remaining compatibility boundaries

| Boundary | Reason / contract |
| --- | --- |
| C camera MoveSIS and clock encounter hook | Signatures have no entity argument; resolve the selected engine's native flagship owner, never an active session. |
| SaveSisHyperState | C save/device callers lack a native navigation argument; facing is written through the live flagship's owner. No-op when an encounter screen has no flagship. |
| Legacy frame loop | IN_BATTLE is still its active-loop/stop protocol. The departure buffer callback ends that loop; pending typed data identifies the destination. |
| Legacy device/options/cargo/roster/starmap menus | Synchronous input/service boundaries. Device encounter flags are adapted into a typed result; load/abort exits take priority. |
| Shared ship physics, collision/drawing and audio/input services | Continue through the selected engine/service bindings. Scene reset precedes asset release; future runtime/UI/compositor work replaces these services separately. |
| Mode-entry legacy projection | One function in gameplay_modes.cpp publishes START_* fields for remaining C code; native navigation mode selection consumes its result directly. |

Regression coverage verifies owner-specific fleet/catalogue/C-field borrows while
another session is active, realm searches, deferred location conversion and
portal normalization after a staged transition. Debug/Release tests and the
legacy build pass. Targeted live debugger checks cover both realm directions,
system entry with a catalogue target, an explicitly selected fleet handoff and
save/load with a pending realm transition cancelled, and menu return to flight.
These are small focused
checks, not a complete manual campaign playthrough.

EncounterMode/SystemEncounterMode now have explicit campaign ownership as
described below; their menus remain deferred. The eventual runtime remains
parameterized by simulation, UI and a default shared compositor.

## Encounter ownership audit

EncounterMode owns EncounterRuntime and its SimulationEngine. SystemEncounterMode
owns the same orchestration as a synchronous child; its result reference borrows
its suspended caller's stack. The session, player story and immutable catalogue
entry outlive these modes. Deep-space encounters legitimately have no star.

| Responsibility | Current owner / remaining boundary |
| --- | --- |
| Selected roaming encounter | GameSession::CampaignEncounters; borrowed through synchronous children, with no structural mutation until they return |
| Battle participants | Mode SimulationEngine::shipRoster; stable source ship identities point back to their fleet |
| Planet artwork used by encounter and combat | EncounterRuntime::EncounterArena; explicit release before aftermath, automatic release on cancellation/unwind |
| Dialogue assets and playback | Child ConversationRuntime; constructed after hail/attack selection |
| Source player fleet | Explicit session player fleet; detached story-only encounter construction has been removed |
| Source NPC fleet, activity, flags and ship stamp | EncounterLegacyView references the supplied session player payload; C services see the same fields through their bindings |
| Encounter choice menu | Legacy synchronous input, graphics and music calls; deferred until the UI/Menu framework |
| Orbital NPC group suspension | OrbitalEncounter borrows GameSession::groupArchive and owner-specific collections/activity. Normal exits restore once; abort/load leaves the handoff untouched |
| Rewards and survivor reconciliation | Explicit finish/run steps, mixed with tally presentation; these must remain completion effects, never destructor effects |

NPC/activity and orbital persistence now meet the agreed ownership boundary.
GroupArchive owns last-encounter bookkeeping and borrows its session's world
bytes, clock, selected star, race definitions and collections. Native loads and
saves do not select a different active session. Legacy Get/Put/InitGroupInfo
remain thin adapters for callers that have not migrated; group generation and
its C BuildGroup entry are separate remaining exploration compatibility paths.
The binary header/fragment/group codecs and their save layout are unchanged.
That includes the existing one-byte crew encoding and its sentinel narrowing;
this ownership cutover does not redesign that format.
Archive file cursors close through RAII, including exceptional returns.

Ship cloning shares one explicit-template implementation with the C recruitment
adapter. Captain selection still uses the selected gameplay RNG service; it no
longer looks up race defaults through the active campaign. Orbit scripts pass
session-owned templates and queues. EncounterRuntime requires a session; a
system behavior without an attached campaign throws instead of silently using
another campaign. The nullable star remains an immutable catalogue borrow for
deep-space encounters.

The choice-screen input callback, FleetIsInfinite C combat service, presentation
labels/gauges and the tally still use legacy service bindings. Menu ownership
and UI redesign are explicitly deferred by agreement; no replacement menu or
runtime framework is introduced here. Reward/survivor effects remain explicit
completion steps, never destructor effects.

Arena regression coverage substitutes graphics loading and checks normal
completion, repeated reset, replacement, child failure and partial-load failure.
Debug and Release CMake builds/tests and the legacy build pass. A focused live
check hands off the explicitly selected non-first roaming fleet; a second loads
real planet artwork, cancels at the choice screen and verifies arena release
before aftermath. It does not exercise a complete battle or salvage tally.


Owner-routing regression coverage now uses the real GroupArchive and actual
world bytes with another session selected. It covers orbital restoration and
load/abort handoff, surviving/destroyed fleets, scripted groups, captain/crew
round trips, campaign-specific expiry dates and cursor release on exceptions.


For the NPC/activity and group archive cutover, Debug and Release CMake/Ninja
builds and all five CTest suites pass, along with the legacy build. Focused live
checks cover explicitly selected roaming fleet handoff, planet artwork release
on cancellation, and native flagship/NPC flight in Sol. These do not constitute
a complete campaign playthrough or a menu/tally redesign.


## Completed orbital ownership map

| Responsibility | Owner / borrow |
| --- | --- |
| Generated terrain, globe, lighting/projection buffers, body-specific sprites | Native World::PlanetRenderResources; a visit borrows its graphics record. Assets survive the visit and regenerate on a later visit |
| Orbital scene and lander handoff | OrbitMode's SimulationEngine; RAII transfers the prepared exploration scene into orbit and into synchronous lander children, then returns it |
| Scan cursor position, flashing deadlines and saved background image | OrbitMode::OrbitRuntime; saved image uses SystemFrame and also resets after each scan |
| Campaign activity, NPC groups/ships, SIS fuel/crew/lander availability and next/last activity | Explicit session/player references passed through OrbitRuntime and WorldVisit |
| Planet and lazily created scan drawing contexts | WorldVisit::OrbitPresentation, with unique ownership and backend context restoration on failed construction |
| Selected orbital music | Visit-retained MusicAsset from the session cache; only the selected theme is loaded, playback stops before lease release |
| Orbital focus, generated-data view and presentation borrow | WorldVisit resources; teardown detaches views and leaves no runtime pointer in the parent visit |
| Permanent retrieval masks and inventory changes | Existing world/story/inventory owners; publication remains explicit, never a presentation destructor effect |

OrbitRuntime is a concrete ownership boundary, not the future generic runtime
framework. It is bound only while its mode runs. Scan and lander callbacks
resolve it from the explicitly supplied WorldVisit, rather than a process-wide
selected campaign or engine. Lander handoff uses that runtime's engine, and
Fwiffo preparation uses its owner's group archive, race template and NPC queue.
WorldVisit uses the enclosing exploration engine explicitly for orbit entry.

### Remaining orbital compatibility boundaries

- PlanetContext is a borrowed C drawing projection for the existing sphere and
  lander helpers. OrbitPresentation restores the previous projection before
  destroying its contexts. It is no longer a context owner.
- The legacy input-frame callback cannot carry userdata. Its scoped graphics
  borrow and callback registration restore their previous values on exit.
- C reports borrow the visit's scan context or own an independent temporary
  context outside orbit. There is no globally retained spare ScanContext.
- Shared drawing, audio/input services, fonts/strings, scan node rendering and
  the existing menu scripts remain service boundaries. No UI/Menu framework
  or second AV compositor is introduced.
- Lander-specific simulation, resources and inventory publication are the next
  mode's audit, not claimed as completed by this orbital cutover.

Deleted the exploration-owned scan cursor, globally owned drawing contexts,
selected LanderMusic/OrbitMusic array, SetPlanetMusic and unused orbit-theme
loader. PlanetContext remains only as the C borrow described above. Regression
coverage uses real presentation/context owners with a fake device backend; it
checks child handback, cancellation/failure cleanup, unrelated active-session
routing, retry after context creation failure, independent report contexts and
selected-theme cache lifetime.

Debug and Release CMake/Ninja builds and all five CTest suites pass for this
cutover, as does the preserved legacy build. A focused live Sol/Luna check
reaches the native orbital runtime with real terrain/globe resources and
advances sphere rotation. A second saves in orbit, requests a load, verifies
that the previous presentation has retired and PlanetContext is detached
before reconstruction, then reaches the replacement orbit with loaded terrain.
No full campaign playthrough or UI/Menu redesign is claimed.
