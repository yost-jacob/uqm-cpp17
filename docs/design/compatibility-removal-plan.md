# Removing the remaining simulation compatibility paths

Work proceeds in dependency order, preserving the current rendering/audio/input
services and existing gameplay rules. Each milestone should compile, run focused
Boost.Test coverage and remain independently reviewable.

| Order | Milestone | Depends on | Result |
|---|---|---|---|
| 1 | Runtime ship roster ownership | Mode-owned engines (#38) | Each mode owns temporary participant pools; combat borrows the encounter's prepared roster through explicit handoff. Persistent Fleet ships remain separate. |
| 2 | Explicit RNG scope and seed delivery | 1; architectural decision below | Define the random state the battle driver will own/use before extracting its global seed manipulation. |
| 3 | Native battle driver and startup/Super Melee modes | 1–2 | Move round progression/results out of battle.c; ordinary modes replace the fallback compatibility engine. Device input/timing remains behind its existing service boundary. |
| 4 | Typed target/source identities | 1 and stable engine/roster lifetimes | Replace raw hTarget/pParent relationships with checked identities and optional borrows. Preserve source-controlled weapons and final retirement semantics. |
| 5 | Native creation interfaces | 4 | Ships create projectiles, beams and effects through typed launch definitions/factories; remove MISSILE_BLOCK/LASER_BLOCK construction from native callers. |
| 6 | Direct native lifecycle dispatch | 3–5 | Engine invokes native entity hooks directly; remove C callback selection and the remaining pointer-identity tests. |
| 7 | Native lander entities/update integration | 4–6 | Surface creatures, hazards, pickups and effects use explicit fields and hooks while preserving the lander's distinct movement/collision rules. |
| 8 | Retire the generic ELEMENT compatibility payload | 4–7 | Replace overloaded fields/flags with the data actually needed by each entity. Ownership/identities/factories are already in place. |
| 9 | Delete obsolete adapters and globals | Last callers migrated in 3–8 | Remove C engine entry points, legacy buffers and macro aliases as they become unused. Story/dialogue state migration remains its separately agreed later work. |

Milestones 4 and 5 can overlap the driver work where their contracts are already
clear; the table gives the default serial implementation order. A large state
record must not be replaced before its callers and identity rules are explicit.

## Milestone 1: runtime rosters

RuntimeShipRoster owns two fixed-capacity STARSHIP pools (14 slots per side),
including any loaded ship behavior/assets attached to those slots. SimulationEngine
owns a roster value. The former global race_q array becomes a C buffer borrow from
the selected mode. Native clients use engine.shipRoster().

EncounterRuntime prepares participants and consumes aftermath. CampaignCombatMode's
initialize() transfers the prepared roster from its parent's engine; destruction
returns crew/status changes before parent resume and aftermath. Pool addresses and
participant identities do not change. The parent keeps its own physical scene and
loaded navigation/system ships while a nested encounter runs.

Scene reset deliberately retains prepared participants. Roster clear, engine
shutdown and destruction release attached resources without calling Ship::finish:
cancellation is not completion. Explicit battle retirement/completion retains its
existing state-commit policy. C reset/load/title callers use ClearRuntimeShipRoster
rather than dropping a queue containing loaded resource owners.

No additional persistent Fleet or Ship representation is introduced. STARSHIP is
still the compatibility participant record; native incarnation/target migration
follows after its ownership is stable.

## First architecture checkpoint: RNG scope and seeding

The current general RNG is a process-global RandomContext. Battle() reseeds it
from the clock; Super Melee netplay negotiates its seed, and checksums inspect its
state. Solar-system generation already uses a separate SysGenRNG context. The
existing recurrence/seed coercion remain intentionally compatible with legacy
planet generation and netplay; this is an ownership decision, not a new algorithm.

Before milestone 2, decide whether child modes consume an independent stream or
advance a parent's/session's shared stream. This affects what the parent resumes
with and how a battle can later be replayed or trained without a display.

Recommended direction for review:

- Each simulation engine has a value-owned general RNG context.
- Mode construction receives an explicit seed; combat receives the negotiated or
  recorded battle seed, rather than reseeding itself from wall-clock time.
- Child modes use independent streams; parent state resumes unchanged.
- Keep deterministic system generation's separate context and existing save
  encoding intact during this step.
- A session seed source supplies seeds for ordinary new modes; specify whether
  and when that source becomes saved/replay state before implementing it.

The alternate policy is a shared session stream advanced by children. It is
closer to the current consumption behavior but makes isolated battle replay and
independent sessions harder. Implementation should pause here for that choice.
