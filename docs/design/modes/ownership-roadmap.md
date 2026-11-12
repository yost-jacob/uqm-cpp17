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
| 1 | `HyperSpaceMode`, including Quasispace | Owns `HyperSpaceNavigation`; borrows player, story, clock and mode engine. Graphics arrays remain file-static; entity callbacks resolve a session binding; encounter queues and transition activity flags still use legacy interfaces. **In progress.** |
| 2 | `EncounterMode`, `SystemEncounterMode` | Own `EncounterRuntime`; audit fleet participants, result propagation, encounter selection, starbase/bomb branches and suspended caller borrows. |
| 3 | `OrbitMode` | Owns orbital engine and borrows the enclosing `WorldVisit`; world assets and report services already have owners. Audit remaining menu/input scopes and explicit completion versus cancellation. |
| 4 | `LanderMode` | Owns a child engine; audit surface entities, expedition state, collection/loss publication and landing/return presentation. Keep durable retrieval and inventory state in their existing owners. |
| 5 | `ConversationMode` | Owns `ConversationRuntime`; audit graph/script state, presentation/audio/input bindings and transaction completion. Reuse the existing graph engine, not another dialogue rewrite. |
| 6 | `CombatMode` | Shared campaign/arcade combat mode; audit roster/ship/entity lifetimes, projectile source references, round completion, survivor reconciliation and cancellation. |
| 7 | `FleetSelectionMode` | Owned by the separate `ArcadeSession`; audit frontend/network/selection resources and handoff to the same combat mode. |
| 8 | `LoadMenuMode`, `TitleMenuMode` | Audit UI state and borrowed result storage; saves/loads remain a session responsibility. Ordinary cargo/roster/starmap/options menus are currently functions, not independent modes. |
| 9 | `CutsceneMode` | One concrete mode currently selects startup, attract, credits, introduction or ending. Audit playback ownership/completion before deciding whether separate derived scene types add value. |
| Tooling | `DebugMode` | Debug-only hook runner; explicit inspection context and exception-safe bindings must remain intact. Not a gameplay simulation. |

Outfitting and starbase activities are still legacy entry paths, not existing
independent modes. Their eventual boundaries belong in the UI/state audit; do
not disguise them as completed modes in this inventory.

## Hyperspace ownership sequence

1. **Native scene boundary — implemented.** Pass `HyperSpaceNavigation&` into
   `runSpaceNavigation`; obtain its engine from that owner and pass it into the
   scene. Scene acquisition/release calls that explicit navigation reference.
   Delete the unused global C `LoadHyperspace`/`FreeHyperspace` wrappers. The
   callback binding remains only for legacy callbacks, not native scene setup.
2. **Presentation resources.** Replace file-static `hyperstars`/`hypercmaps` and
   process-wide `FreeHyperData` cleanup with an owned navigation resource lease
   using the session cache where supported. Restore `stars_in_space` before
   releasing its backing resources, including partial initialization and failure.
   Audit FMV and application cleanup callers before deleting those entry points.
3. **Navigation entities.** Move star, portal, encounter and flagship callback
   context into engine-owned native entities, as exploration already does.
   Preserve element traversal/collision phases and handle lifetime contracts.
   Do not let a callback outlive its navigation owner.
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

The initial native-scene regression runs without a `NavigationBinding`, ensuring
orchestration does not reacquire the navigation owner through the active session.
Backend asset loading remains substituted in that orchestration test; it does
not claim to validate actual graphics loading or realm transitions.
