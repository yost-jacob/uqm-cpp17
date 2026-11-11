# Game mode runtime requirements audit

This is a source-based requirements audit, not an implementation or a decision
about SDL versions or replacement libraries. It records the capabilities needed
to replace the current execution paths with understandable runtime classes.
Existing behavior and proposed contracts are distinguished below. Source links
are relative to this document. Future multiplayer and learned AI are not baseline
requirements; existing optional Super Melee netplay is.

## Ownership and execution boundary

The proposed ownership is:

```text
UQM_Context: device services and mutable application configuration
  Session: session assets, definitions, player/galaxy state, dispatcher
    GameMode: lifecycle, child-mode handoff, mode loop
      ModeRuntime: coordinates the following members
        Simulation: mode objects, rules, decisions, progression
        Menu/UI: focus, selection, editing, modal interaction, HUD models
        Presentation: animation players and layered visual composition
        AudioMix: this runtime's playback instances and mixing policy
```

Sessions and context outlive their modes. Runtimes borrow the relevant state and
services explicitly. Simulations own their transient objects; persistent player
and galaxy data stays in the session. Immutable clips, images, and sounds can be
shared through the session asset cache. Playback cursors, active effects, focus,
and graph execution state belong to the runtime using them.

The current `UQM_Context` is a startup/configuration facade, not an implemented
set of these service interfaces. Current classes named Runtime also do not yet
share this proposed structure. See [context](../../sc2/src/uqm/uqm_context.hpp),
[modes](../../sc2/src/uqm/game_mode.hpp), and
[application sessions](../../sc2/src/uqm/application_sessions.cpp).

## Required capabilities by runtime

These are behavioral groupings, not a demand to introduce a class for every row.
Flight/orbit/scan can remain states of one exploration runtime.

| Runtime / view | Simulation and state | UI | Visual composition | Audio |
| --- | --- | --- | --- | --- |
| Conversation | Graph traversal, local visit state, const queries, explicit story/inventory actions, presentation completion | Conditional responses, scrolling, selection, transcript, replay/seek/skip | Alien background, independent ambient layers, talking transitions, subtitles, response feedback, waveform and progress display | Queued speech, inserted number/name segments, music, speech-relative music level, seeking and replay |
| Combat | Two participant sides, controllers, entities, projectiles, collision/gravity, damage, retirement/reincarnation, round completion | Ship selection, crew/energy, pause/confirmation, result display | Moving sprites/effects, camera/zoom/wrapping, planet, HUD, transitions | Positional effects with priority, combat music, victory ditty and its completion |
| HyperSpace / QuasiSpace | Travel, fuel, autopilot, NPC encounter movement, campaign time, portals/system transitions | Navigation options, starmap, cargo/devices, status | Scrolling stars/grid, moving fleets/flagship, transition effects, HUD | Navigation music and travel/transition effects |
| Solar system flight | Generated system and bodies, flagship flight, NPC groups, world/story behavior | Navigation, body identification, devices and status | Back-to-front bodies, star field, inner/outer views, zoom and flight effects | System music and effects |
| Orbit / scan | World properties, retrieval state, scan progression and landing eligibility | Scan category, progressive reveal, landing cursor, reports | Rotating planet, shield/tint layers, map, scan sweep, markers/cursor, zoom | Music, scan/menu cues, landing transition cues |
| Lander | Surface entities, hazards, creatures, projectiles, collection, crew/cargo limits, departure/loss | Crew/cargo and pickups, controls, departure | Scrolling terrain, independent objects and effects, collection text, launch/return | Weapons, hazards, pickup and lander effects |
| Encounter | Fleet setup, hail/attack selection, child conversation/combat, aftermath | Contact choice and fleet/result information | Encounter panels, ship/fleet portraits, transitions | Music/cues; child runtimes own their playback |
| Arcade fleet setup | Two independent fleet configurations, control selection, match launch; optional netplay setup | Ship grid, fleet slots, text entry, load/save teams and selection | Previews, highlights, costs, fleet composition | Menu music/effects and ship previews where used |
| Outfitting / shipyard child modes | Equipment rules, inventory transactions, fleet changes | Slot/grid selection, purchase/removal, capacity and price feedback | Flagship/module layout, fleet panels, gauges | Menu and operation feedback |
| Other gameplay UI / title / setup | Explicit inventory/fleet/configuration operations; save/load requests | Lists, grids, sliders, toggles, text/key editing, confirmation, save summaries | Panels, text, gauges, highlights, ship/module placement and map views | Menu cues, music policy, live volume preview |
| Cutscene / credits | Script progression, skip/timeout, completion | Skip/exit, credits interaction | Video or image sequences, scrolling text, fades | Synchronized media, music, completion |

Evidence: [conversation](../../sc2/src/uqm/conversation.hpp),
[navigation](../../sc2/src/uqm/hyper_navigation.hpp),
[system runtime](../../sc2/src/uqm/system_runtime.hpp),
[lander](../../sc2/src/uqm/planets/lander.cpp),
[encounters](../../sc2/src/uqm/encounter_runtime.hpp),
[fleet setup](../../sc2/src/uqm/supermelee/melee.cpp),
[outfitting](../../sc2/src/uqm/outfit.c),
[shipyard](../../sc2/src/uqm/shipyard.c),
[cutscenes](../../sc2/src/uqm/fmv.c), and
[credits](../../sc2/src/uqm/credits.c).

## Audio mix requirements

### What exists

The existing sound layer provides music, speech, effects, channel priority,
volume controls, looping, fades, pause/resume, music seeking, and positional
updates. Effects are collected during gameplay and flushed from `RedrawQueue`.
Active positional sounds retain associations to legacy elements; object removal
detaches those associations. Speech is a richer track structure with subtitle
segments, queued/spliced recordings, callbacks, position, and forward/reverse
navigation. The oscilloscope reads actual stream sample data.

Sources: [sound API](../../sc2/src/libs/sndlib.h),
[effect scheduling](../../sc2/src/uqm/sounds.c),
[speech tracks](../../sc2/src/libs/sound/trackplayer.h),
[streaming](../../sc2/src/libs/sound/stream.c),
[waveform display](../../sc2/src/uqm/oscill.c).

### Required runtime contract

- Own playback instances separately from shared sound assets. Support multiple
  concurrent effects plus music and speech, with explicit categories and volume
  policy. Existing channel limits are implementation details; priority and
  interruption behavior are observable requirements.
- Start, stop, pause, resume, loop, fade, and query playback. Seeking is necessary
  for speech replay; expose it for supported playback rather than assuming every
  clip or stream is seekable.
- Provide stable playback identities and playback positions. Speech position
  drives subtitle/talking presentation; wall-clock estimates are insufficient
  after pauses, seeks, or playback stalls.
- Support speech sequences containing several recordings and subtitle cues;
  inserted recordings must preserve their relationship to text and replay.
- Report completion/cancellation to the runtime on its execution thread. Device
  callbacks must not directly traverse a graph, mutate story state, or destroy a
  game mode. Ignore completions belonging to replaced playback instances.
- Update positional effects from stable entity identity or copied positions.
  When the source disappears, detach or stop according to effect policy without
  retaining an entity allocation merely to finish its sound.
- Support category/group pause and cancellation so a child mode cannot stop an
  unrelated parent's playback. Suspend policy is explicit: preserve, pause,
  lower volume, or stop, depending on the mode.
- Expose a bounded waveform/sample view for the existing communication scope.
  This is distinct from inventing phoneme-based lip synchronization: the current
  presentation requires speech-related talking animation, not a new phoneme model.
- Permit timing without an audio device. Combat's victory interval needs an
  explicit headless completion policy rather than waiting for unavailable audio.

`RuntimeAudioMix` is the proposed runtime owner/coordinator; actual sample mixing,
decoding, and output remain service implementation responsibilities. We should
not build a second low-level mixer as part of runtime encapsulation.

## Animation player and compositor requirements

### What exists

Alien animations support circular, random and reversing sequences, neutral
frames, randomized frame/restart delays, one-shot and disabled sequences,
incompatible ambient layers, and talk entry/exit transitions. Some sequences
animate color maps rather than sprite indices. Their current implementation
combines advancement, conflict arbitration, timing, and drawing.

Other views add moving sprites, scrolling, zoom, text, fades, procedurally
generated planet images, shield overlays and tinting. A compositor consisting
only of a ring of static sprite frames cannot cover these views.

Sources: [animation semantics](../../sc2/src/uqm/commanim.h),
[animation execution](../../sc2/src/uqm/conversation_animation.cpp),
[planet presentation](../../sc2/src/uqm/planets/pl_stuff.c),
[surface generation](../../sc2/src/uqm/planets/surface.c), and
[scene projection](../../sc2/src/uqm/process.cpp).

### Required runtime contract

- Separate immutable animation definitions from mutable players. Definitions
  describe frames, timing, looping and neutral state; players hold cursor,
  elapsed time, enabled/paused status and any presentation random state.
- Advance independently from drawing. Repaint and resume must not advance a
  sequence, consume randomness, replay sound, or execute a story action.
- Support ordinary elapsed-time/tick advancement and externally positioned
  playback for speech-linked presentation. Seeking/replay must reposition the
  associated presentation coherently.
- Compose ordered layers with local position, origin/pivot, scale, optional
  rotation, clipping, visibility, opacity and color effects. Include text and
  dynamic/generated images as visual elements. Child composites allow a whole
  alien, ship presentation or panel to move together.
- Treat frame animation, object movement and planet surface generation as
  different producers of visual state. A moving sprite does not need to become
  a timeline animation to be drawable.
- Preserve ambient conflicts, talk transitions and neutral-frame return through
  an animation-group controller. These are presentation coordination rules,
  not responsibilities of every generic sprite or the renderer.
- Provide completion/cue observations with a defined skip policy. Presentation
  cues may repeat during replay; gameplay actions remain simulation/graph actions
  with explicit once-only execution.
- Allow simulation, UI and presentation to pause separately. Ambient animation
  may continue while the world is paused; talking may stop without freezing an
  alien's entire background.
- Retain the assets referenced by active layers and release them on teardown.
  Clear/cancel queued work before those assets disappear.

Proposed composition: `AnimationClip` + `AnimationPlayer`, `VisualComposite`, and
an optional `AnimationGroup` for coordination. A small `VisualElement` interface
can cover sprites, text, generated imagery and child composites if that makes
ownership simpler. Simulation entities do not need to inherit that interface.

## UI and menu requirements

### What exists

There is no single menu engine today. `MENU_STATE`/`DoInput` loops, gameplay
screens, setup widgets and conversation response lists each own interaction and
drawing differently. Setup already has screen, choice, button, label, slider,
text-entry and control-binding widgets. Game menus return optional session exits
through an adapter; many surrounding loops still interpret activity flags.

Sources: [menu navigation](../../sc2/src/uqm/menu.c),
[existing widgets](../../sc2/src/libs/graphics/widgets.h),
[setup](../../sc2/src/uqm/setupmenu.c),
[game menu boundary](../../sc2/src/uqm/game_menu.cpp),
[conversation controls](../../sc2/src/uqm/conversation.cpp).

### Required runtime contract

- Own selection, focus, scroll position, editing buffers, modal screens and
  selection feedback. Support directional navigation, repeat, confirm, cancel,
  text input and binding capture using semantic input events.
- Offer lists, grids, sliders, choices, buttons, labels, text editing and HUD
  views. Outfitting, shipyard and the starmap need specialized screen controllers;
  a dialogue-style response list does not replace their spatial interaction.
- Read explicit state/definitions to produce labels, values and enabled options.
  Submit typed operations for transactions, fleet changes and configuration.
  The UI must not create a second authoritative inventory or implement prices.
- Keep query/visibility evaluation free of mutations. Validate and perform an
  operation when activated; refresh from authoritative state after its result.
- Return a local result, child-mode request, or session/application request.
  Closing a menu normally must remain distinct from load, shutdown and failure.
- Route input to the active modal UI before gameplay. Opening, holding and
  closing a menu must have defined held-key/repeat behavior to prevent input
  leaking into the resumed parent.
- Produce visual models/layers for the shared compositor and audio requests for
  the runtime mix. A menu controller must not own another device pump or sleep
  loop. Passive HUDs need updating without taking input focus.
- Accept hardcoded or data-loaded definitions through a common model where
  useful. Reuse conversation presentation/navigation concepts without making
  all menus JSON conversation scripts.
- Support transaction tally presentation from before/after snapshots. The
  atomic state change occurs once; fast-forward, replay or cancel cannot repeat
  it. The lost mineral/biomass tally is tracked in issue #79.

Proposed composition: `MenuController` owns the current screen/modal state;
small controls and screen models describe presentation. Derive specialized
screen controllers only where interaction differs. A hierarchy for every menu
label or story response would add maintenance rather than remove it.

## Simulation requirements

### What exists

`SimulationEngine` already owns an element pool, runtime ship roster, random
stream and tick count. `step()` runs pre/post-processing, but that processing
still prepares graphics primitives and contains presentation dependencies.
`RedrawQueue` calls `step()`, updates sound positions, draws and flushes sound.
`SimulationLoop` also handles controls, optional netplay, transitions and pacing.
Thus the current `step()` signature alone does not establish a device-independent
simulation boundary.

Conversation graph execution, world generation, NPC group logic and surface
hazards are also simulations in the architectural sense. They should not be
forced into the combat element pool merely to share a runtime interface.

Sources: [engine owner](../../sc2/src/uqm/simulation_engine.hpp),
[processing](../../sc2/src/uqm/process.cpp),
[frame loop](../../sc2/src/uqm/simulation_loop.cpp),
[combat orchestration](../../sc2/src/uqm/battle_driver.cpp),
[retirement and ditty](../../sc2/src/uqm/tactrans.c),
[graph execution](../../sc2/src/uqm/dialogue/engine.hpp).

### Required runtime contract

- Own mode-local objects, stable identities, spawn/removal rules, controller
  state and random streams. Borrow player/galaxy state explicitly; keep campaign
  progress publication separate from resource destruction.
- Accept semantic commands and advance by explicit simulation ticks. Do not
  poll devices, sleep, draw, or wait for audio inside the simulation step.
- Preserve behavior order, collision semantics, finite capacities that affect
  gameplay, safe spawn/removal during traversal, targeting, damage and lifetime
  rules. Generalizing storage must not silently make bounded encounters unbounded.
- Keep combat retirement, true destruction and reincarnation distinct. Continue
  relevant entities during the victory interval and re-evaluate survivors before
  replacement selection. Audio completion enters as an observation; headless
  runs provide the agreed substitute interval.
- Allow human, cyborg and optional network controllers to produce the same
  command representation. Existing netplay has input delay/checksum ordering
  requirements; future online campaign design is a separate scope.
- Publish visual state and explicit presentation requests without exposing
  mutable entity storage to the renderer or audio thread. A borrowed const view
  valid until the next step is sufficient for synchronous consumption; copying
  the whole world every frame is not a requirement.
- Keep presentation randomness separate from simulation randomness so different
  redraw rates or optional ambient effects do not change gameplay outcomes.
- Support modes without physical entities: graph progression, economic actions
  and cutscene sequencing may advance on commands or completion observations.
  An idle conversation must not repeatedly evaluate an action node each frame.
- Preserve parent-local state through child modes. Current lander code transfers
  a prepared surface scene to its child and returns it; a replacement needs an
  equally explicit owner/borrow contract rather than duplicating live worlds.
- Produce completion/transition requests for the runtime/mode to handle. The
  simulation must not replace its own owning mode or load a campaign mid-step.

## Proposed class hierarchy and interactions

Use one runtime interface at the game-mode boundary, with concrete runtimes for
the behaviorally different modes. The common responsibilities are command
handling, advancement, presentation access, lifecycle suspension/resumption and
completion. Exact method signatures should follow the first implementation.

```text
ModeRuntime (interface)
  ConversationRuntime
    ConversationSimulation + ConversationAnimationGroup
  CombatRuntime
    CombatSimulation + combat HUD/menu
  NavigationRuntime
    NavigationSimulation + navigation HUD/menu
  ExplorationRuntime
    SolarSystem simulation + flight/orbit/scan views
  LanderRuntime
    Surface simulation + lander HUD
  OutfittingRuntime / ShipyardRuntime
  FleetSelectionRuntime / MenuRuntime / CutsceneRuntime

Each concrete runtime owns or composes:
  UI controller, visual composite/animation players, runtime audio mix
```

This is a proposed destination, not the current inheritance tree. Existing
`GameMode` eagerly owns a combat-style `SimulationEngine` even for conversations
and menus. Move that engine under the concrete simulation/runtime when adopting
this structure; do not retain two authoritative engines. Share useful mechanics
as members/helpers instead of requiring every simulation to expose collisions.

Typical update: route input to UI/simulation; advance eligible simulation ticks;
apply its presentation requests; observe playback position/completion; advance
eligible animation/UI time; submit the composite and audio changes. Completion
observations can become next-update inputs, avoiding reentrant graph execution.

There are three distinct clocks: simulation ticks, presentation elapsed time,
and media playback position. Runtime policy decides which run while paused or
suspended. There should be one owner for each progression; drawing never owns
time. The actual backend/device execution thread is a service concern, not a
requirement to put every part of a runtime on a new thread.

## Cross-subsystem contracts to settle before implementation

| Contract | Recommended requirement |
| --- | --- |
| Parent/child lifecycle | Suspend parent; run child; cancel/release child-local work; restore bindings; resume parent view without replaying effects. |
| Completion | Distinguish completed, skipped and cancelled. Cancellation for load/shutdown must not execute normal-completion story actions. |
| Presentation identity | Identify active speech/sequences; discard stale completion after replacement or cancellation. |
| Script ownership | Conversation simulation owns graph/script execution; presentation owns playback. JSON remains the content source for conversation behavior/cues. |
| Gameplay side effects | Explicit state operations; destruction releases resources and bindings, not rewards or story advancement. |
| Lifetime | Active players/layers retain needed assets. Borrowed state/views have documented validity; no audio callback retains a raw gameplay pointer. |
| Exceptions | Reject malformed definitions before execution where possible; unwind ownership and report at the established outer boundary. No exception through C/device callbacks. |
| Headless operation | Advance simulations without drawing/audio devices; inject explicit completion/timing policies for presentation-dependent transitions. |

## Recommended replacement boundaries

1. Define the small playback, visual-layer, input and completion value types from
   the requirements above. Keep device API and library selection out of them.
2. Use conversation as the first complete runtime slice: graph execution already
   exists, while animation, speech, subtitles and menu interaction expose the
   necessary coordination requirements in one bounded subsystem.
3. Replace its presentation/UI loop as one coherent unit, retaining existing
   content formats where practical. Remove the replaced globals, flags and
   callbacks at cutover rather than maintaining a second permanent control path.
4. Apply the proven composition to navigation/exploration and combat. Their
   simulations need their own deeper cleanup; matching a common runtime interface
   does not by itself remove primitive queues or callback dependencies.

This audit inspected subsystem owners and representative execution paths. It is
not an exhaustive per-call-site inventory or visual playthrough. Existing
content animation descriptors, media variants and optional netplay builds still
need targeted verification when their corresponding replacement is implemented.
