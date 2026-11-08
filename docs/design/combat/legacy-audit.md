# Legacy combat migration audit

This audit identifies concrete constraints for the Battle/Ship/Projectile design.
The [live ship migration](ship-migration.md) has now replaced the descriptor's
four behavior callbacks and private-data allocators. The element/timing findings
below still constrain the remaining Battle/projectile work. No weapon, save-format,
collision-timing or victory rule is intentionally changed.

## Existing ownership is implicit

Before migration, [races.h](../../../sc2/src/uqm/races.h) combined characteristics,
mutable crew/energy, resources, private data, AI and behavior pointers in RACE_DESC.
Its four ship behavior callbacks now live in native classes; the cyborg callback
and mixed legacy resource/data views remain as compatibility boundaries. [loadship.cpp](../../../sc2/src/uqm/loadship.cpp) loads graphics,
strings and audio assets around a direct native factory and invokes explicit
per-ship completion. The old code-resource layer is removed. These need separate
native definition, instance, behavior/controller and asset ownership boundaries.

[ship.c](../../../sc2/src/uqm/ship.c) provides common energy, cooldown, movement
and firing logic around ship-specific preprocess/postprocess hooks. An interface
with only firePrimary/useSpecial would miss ongoing transformations and changes
to movement/control state. Ability hooks need explicit simulation phases.

## Weapon dependencies

| Existing implementation | Dependency and behavior | Migration consequence |
|---|---|---|
| [Chenjesu crystal](../../../sc2/src/uqm/ships/chenjesu/chenjesu.cpp) | crystal_preprocess reads source WEAPON every tick. Holding compensates lifetime decrement; release sets lifetime to one and installs crystal_postprocess, which spawns eight fragments. chenjesu_preprocess suppresses repeated shots while held. | CrystalBehavior can retain its source and read published controls. Preserve one active controlled crystal and release timing. |
| Chenjesu collision | crystal_collision calls weapon_collision and installs sparkle animation on the blast. It does not itself call the fragment-spawning release callback. | Separate removal/contact from explicit release detonation. A native collision adapter must preserve the actual collision/postprocess interaction. |
| Chenjesu DOGI | doggy_preprocess updates the launching ship's active-DOGI counter; death/collision also use its sound resources. | Use source-ID/weak access and separate asset lifetime; DOGIs must not become extra persistent strong owners of the crystal source. |
| [Earthling missile](../../../sc2/src/uqm/ships/human/human.cpp) | nuke_preprocess tracks a target and accelerates toward a capped speed according to age. | No firing-control dependency in missile guidance. Copy its launch configuration; resolve targets by ID. Constant-speed prototype steering is not a complete port. |
| [Kohr-Ah disc](../../../sc2/src/uqm/ships/blackurq/blackurq.cpp) | buzzsaw_preprocess reads source WEAPON. Releasing halves lifetime and switches to deceleration, then proximity tracking; spin_preprocess also reads source crew and updates a saw limit counter. | The earlier description as entirely fire-and-forget was incomplete. Keep release/owner interaction explicit; do not give several discs the crystal's sole-owner test. Audit their separate lifetime rule. |
| [Ur-Quan fighter](../../../sc2/src/uqm/ships/urquan/urquan.cpp) | Fighters steer, collide, and interact with their carrier, rather than behaving as ordinary one-hit missiles. | Battle owns the entities, but return-to-carrier and crew effects need their own behavior contract. |
| [Orz marine](../../../sc2/src/uqm/ships/orz/orz.cpp) | Marine/intruder logic references source and target ships, boarding state, counters and crew. | Do not force every persistent effect into a simple missile model. Preserve boarding and retirement effects explicitly. |
| [Androsynth comet](../../../sc2/src/uqm/ships/androsyn/androsyn.cpp) | Ship hooks transform motion, collisions, controls and energy behavior. | The ship remains a multi-tick stateful object; an isolated special-fire function is insufficient. |

These are representative stress cases, not an exhaustive audit of every ship.

## Frame ordering

[process.c](../../../sc2/src/uqm/process.c) performs element setup/preprocessing,
velocity integration, finite-lifetime decrement and collision processing, then
postprocessing. PostProcessQueue can preprocess newly added elements and run
collisions in that same frame. Delaying every spawn to the next tick would change
weapon behavior. The prototype defers child stepping only to demonstrate safe
container ownership; the gameplay adapter must preserve appropriate activation
phases instead of copying that simplification.

On crystal release, crystal_preprocess requests fragments through postprocessing,
so common motion occurs before fragment positions are read. The prototype currently
detonates at its pre-motion position. That is an explicitly unbound design example,
not verified numerical/gameplay compatibility.

## Death, retirement, and victory

[tactrans.c](../../../sc2/src/uqm/tactrans.c) separates initial ship death, explosion,
cleanup_dead_ship, victory music/readiness, and new_ship/fleet selection. Lingering
effects can affect the opponent before cleanup. cleanup_dead_ship then disconnects
owned elements and removes most of them, excluding the designated crew objects.
The legacy contract does not wait indefinitely for every projectile to expire.

[Pkunk reincarnation](../../../sc2/src/uqm/ships/pkunk/pkunk.cpp) intercepts death
callbacks, retains state through an explosion and reconstructs the ship. Zero crew
is not sufficient to declare final retirement. Warping out has another outcome.
Multiplayer readiness also coordinates the transition frame with the peer.

The native Battle must therefore distinguish:

- no longer accepting combat input;
- explosion/reincarnation/withdrawal still resolving;
- definitive retirement, when Battle releases its ship owner;
- lingering simulation effects still capable of changing the outcome;
- presentation/fleet-selection readiness.

The crystal's count==1 contract identifies owner release, not any particular
crew value. Selecting that release boundary must account for these existing rules.
No additional persistent strong owners should be introduced to represent death
animation; use detached presentation data or defer Battle's release appropriately.
The [lifecycle wireframe](lifecycle.md) now records these attachment states and
the proposed fleet-selection result before replacing legacy death/selection logic. This audit deliberately does not implement a guessed
winner test or automatic source removal at zero crew.

## Campaign writeback and practical sequence

[Flagship initialization](../../../sc2/src/uqm/ships/sis_ship/sis_ship.cpp) builds
configuration from module, drive and jet slots. It distinguishes hyperspace
behavior from battle behavior and adjusts crew for the captain. uninit_sis writes
crew back to campaign globals. Native constructors should receive a resolved
loadout/current crew; explicit battle results should replace destructor writeback.

[Fleet selection](../../../sc2/src/uqm/pickship.c) also distinguishes Super Melee,
player escorts/flagship, NPC succession and infinite fleets. Infinite fleets can
reuse the same STARSHIP address. Native IDs must identify the new deployment,
not that reused address; the previous source must not silently become its successor.
Retreat fuel costs and defeat sentinels are session result application work, not
ProjectileBehavior responsibilities.

A useful next implementation sequence is:

1. Settle ship lifecycle/retirement and reinforce/fleet transition results.
2. Add a legacy Battle adapter preserving element phases and cleanup.
3. Separate shared immutable asset resources from instance mutable state.
4. Route commands through native controller adapters without changing existing AI.
5. Migrate one ordinary ship/projectile, then the controlled crystal path.
6. Migrate death/reincarnation and selection only with their audited state machine.

JSON property loading and model training remain separate work. There is no need
to choose a neural-network library to establish these ownership boundaries.
