# Remaining global-state boundaries

This is an ownership inventory of the main gameplay/UI boundaries, not a count
of every external declaration or immutable constant in the repository. Many
legacy global-looking accesses already select session-owned storage.

| Data | Current ownership/access | Remaining cleanup |
| --- | --- | --- |
| Story flags/counters | Player-owned `StoryState`; legacy macro access remains | Pass explicit references through remaining callers |
| Calendar and scheduled story events | **Moved:** session-owned `CampaignClock`, vector scheduler and clock-owned synchronization | C scheduling adapters and legacy story-event policy remain |
| Race knowledge, alliances, fleet strength/influence | Mixed in `avail_race_q` | Separate personal knowledge from galaxy fleet state |
| NPC encounter ships | Legacy `npc_built_ship_q` | Encounter-owned fleet |
| Solar-system ship groups | Legacy `ip_group_q` | Solar-system simulation ownership |
| Hyperspace encounters | Legacy `encounter_q` | Navigation simulation ownership |
| Activity/control flags | Part dispatcher state, part legacy payload | Finish typed transitions and mode/session state |
| Navigation fields | Live player location plus legacy save projections | Keep projections inside serialization |
| Graphics contexts, fonts, frames, display primitives | Global device/presentation state | Renderer/device services |
| Input state, pause/exit flags, control selection | Global device/control state | Input services and explicit session controls |
| Menu widget state | File-static widgets, selection pointers, flags | Menu-instance ownership |
| Master ship resources | Legacy global `master_q`; resource lifetime now scoped to gameplay session `run()` through RAII | Move the container behind a resource/service owner |
| Starmap catalog and current-star selection | **Moved:** campaign-owned `GameDefinitions`; current star is player-owned optional `StarId` | C adapters remain; no process-global catalog/current-star pointer |
| Ship tuning catalog | **Moved:** campaign/arcade-owned `GameDefinitions`, loaded during construction | C/legacy lookup adapters remain; no process-static catalog or cached tuning references |

Application settings already have a `GameConfiguration` owner in `UQM_Context`.
Mutable economic tables already have `GameSession::gameData()` / player ownership.
`PlayerEntity::inventory()` now owns its fleet and provides exact resource/device
transactions over the existing save storage. Narrative updates remain in StoryState;
legacy callers and packed inventory bits have not all been removed. All 27
conversation trees now use graph-owned story routing; bounded merchant and
starbase report operations borrow these owners explicitly.
Merchant recipes and other compiled game/presentation definitions have not all
been consolidated into catalogs. See [game data](game-data.md) and
[session definitions](session-definitions.md).

See [campaign clock](campaign-clock.md) for timing/save contracts and
[positioned NPC fleets](design/npc-fleets.md) for the next queue ownership boundary.
