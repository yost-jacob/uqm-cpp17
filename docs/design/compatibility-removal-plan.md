# Removing the remaining simulation compatibility paths

Work proceeds in dependency order, preserving the current rendering/audio/input
services and existing gameplay rules. Each milestone should compile, run focused
Boost.Test coverage and remain independently reviewable.

| Order | Milestone | Depends on | Result |
|---|---|---|---|
| 1 | Runtime ship roster ownership | Mode-owned engines (#38) | Each mode owns temporary participant pools; combat borrows the encounter's prepared roster through explicit handoff. Persistent Fleet ships remain separate. |
| 2 | Mode-owned standard RNG streams (implemented) | 1 | std::minstd_rand0 per engine; session seed source and explicit combat seed; child execution leaves parent randomness unchanged. |
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

## Resolved RNG ownership and seeding

Each engine now owns a general gameplay RandomStream backed by std::minstd_rand0.
The standard engine supplies the recurrence; a cached state supports the C
seed/restore/checksum interface. SimulationBinding selects it for existing C
callers and restores the previous stream when a child returns. Scene reset does
not rewind a stream, and scene/roster handoffs do not transfer it.

GameSession has a separate std::minstd_rand0 seed source. Production mode
constructors request their seed through GameMode(session), rather than consuming
the parent's gameplay stream. The session accepts an explicit seed for repeatable
mode construction; its default samples std::random_device once. Like the previous
clock seeds, this source is transient and is not added to the legacy save format.
This is not a promise of whole-campaign replay across save/load.

Campaign combat also accepts an explicit battle seed. Battle no longer reseeds
ordinary modes from the clock. Super Melee retains its existing seed negotiation;
C startup/Super Melee callers use a standard-engine fallback until they acquire
native modes. Legacy journal seed delivery is retained for journal builds.

SysGenRNG retains RandomContext's legacy arithmetic and its separate ownership.
General gameplay random sequences now follow the standard engine, including its
seed normalization. Netplay protocol 0.5 rejects older peers using protocol 0.4,
whose general random sequence would diverge. Both peers must run matching builds;
this step does not introduce a portable replay/save format for RNG state.

The architectural choice was resolved in favor of the standard library and
independent streams. The next implementation milestone is the battle driver,
followed by startup/Super Melee mode ownership.

## Next ownership checkpoint: non-campaign sessions

The current GameSession requires a GameState and one PlayerId, and every GameMode
validates that player binding. The C title loop invokes campaign sessions, while
StartGame also invokes Melee outside them. Melee begins with InitGlobData and owns
two arcade fleet/control configurations. It cannot simply become a child of a
live campaign mode: its legacy reset would mutate the suspended player's payload.

Before removing the startup/Super Melee fallback owner, choose whether standalone
modes have their own non-campaign session or GameSession becomes a general host
with optional campaign state. The recommended boundary is a standalone arcade
session above its two battle sides, leaving player-bound campaign sessions intact.
A reusable dispatcher can serve both, but its mode/state validation contract needs
a wireframe before changing ownership. Battle's shared navigation/combat frame
runner can then be separated from combat-only round and fleet progression.
