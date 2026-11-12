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
| 1 | `HyperSpaceMode` | Owns `SpaceNavigation`; borrows player, story, clock and mode engine. Run-scoped presentation retains cached graphics and owns its colour map; native entity callbacks borrow navigation directly; encounter queues and transition activity flags still use legacy interfaces. **In progress.** |
| 2 | `QuasiSpaceMode` | Separate mode owning `SpaceNavigation` configured for Quasispace. Shares mechanisms and resource cleanup work with Hyperspace; portal destinations and fuel/encounter rules differ. |
| 3 | `EncounterMode`, `SystemEncounterMode` | Own `EncounterRuntime`; audit fleet participants, result propagation, encounter selection, starbase/bomb branches and suspended caller borrows. |
| 4 | `OrbitMode` | Owns orbital engine and borrows the enclosing `WorldVisit`; world assets and report services already have owners. Audit remaining menu/input scopes and explicit completion versus cancellation. |
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
   Native `run(session)` validates its player/clock borrows before acquisition.
   Scene reset destroys all behaviors before navigation/resources can expire.
4. **Encounter state.** Separate persistent campaign encounters from transient
   on-screen representations. Pass fleet/catalogue/state/RNG references into
   spawning and movement. Preserve pursuit limits, save identities and campaign
   clock behavior; leave archive-format redesign to save/load work.
5. **Transitions and cleanup.** Give realm changes, system entry and encounters
   explicit results; confine activity-flag translation to remaining C boundaries.
   Persistent travel/portal information remains in `PlayerEntity`. Audit menus,
   load cancellation, child-mode suspension/resumption and save callbacks.
6. **Delete superseded paths and checkpoint.** Inventory remaining globals and C
   callback adapters, remove unused wrappers, and record the final ownership map.
   Only then extract common movement/collision/tick mechanisms into the new
   runtime design, using real shared requirements from exploration and hyperspace.

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
