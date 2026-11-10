# Synchronous nested game modes

`GameSession::swapGameMode(unique_ptr<GameMode>)` runs a child synchronously.
The parent remains suspended on its ordinary C++ call stack; its local variables
and native runtime owners remain alive. This is a nested call, not a coroutine
or a restart of the parent's `run()` method.

```mermaid
sequenceDiagram
    participant Parent
    participant Session
    participant Child
    Parent->>Session: swapGameMode(child)
    Note over Session,Child: Dispatcher owns child; child owns suspended parent
    Session->>Parent: suspend() returns owned service snapshot
    Session->>Child: run()
    Child-->>Session: ModeCompleted
    Note over Session: Restore parent ownership; destroy child; restore service bindings
    Session->>Parent: resume() unless session is exiting
    Session-->>Parent: optional session exit, or nullopt
    Note over Parent: Continue after the call with the same locals
```

The child must belong to the same state/player, have no existing parent, and be
freshly owned by the call. Only an active mode may invoke this API. An empty
`unique_ptr` represents the absence of a suspended parent; wrapping that owner
in another `optional` is unnecessary.

A normal child finishes with `return ModeCompleted{}`. A child may also return
`SessionExit`; the session records it, restores the parent, and returns the
optional exit to the suspended caller. Callers must propagate a returned exit
outward before further gameplay. The outer session loop destroys modes and
returns the exit only after those frames unwind. Load requests never decode or
replace campaign state inside a suspended handler.

The existing flat `ModeResult` successor path remains available for permanent
replacement. A successor returned by a root mode starts running normally; the
parent of a completed child is restored without calling `run()` again.
Nested children must complete or return an exit, not an unrelated successor.

## Failure unwinding

The shared dispatcher catches child failures long enough to restore parent
ownership, destroy the child under its own bindings, and release the suspension
snapshot. It then rethrows through the parent's normal stack. The root dispatcher
returns a typed failure after cleanup. It never destroys a parent with a live
`run()` frame. Resume/redraw is skipped on failure. Invalid arguments are rejected
before ownership changes. There is no dispatcher abort/death-test policy.

Boost.Test checks nesting, preserved locals, load slot zero, and child exception
unwinding. Session tests also verify destructor/binding restoration order. The
same [dispatcher template serves application sessions](sessions.md).

Orbital system behaviors use this API through SystemEncounterMode. Its native
EncounterRuntime launches ConversationMode as another child for hailed dialogue;
combat follows only after that conversation has been destroyed. Root navigation
encounters and starbase transitions retain their established outer mode path,
and launch the same conversation child. See [conversation ownership](conversation-mode.md)
and the [runtime encapsulation map](runtime-encapsulation.md).

## Service handoff

The parent owns its service state. The dispatcher knows only `suspend()` and
`resume()`: suspension returns a mode-specific RAII snapshot of borrowed service
bindings. After child destruction the dispatcher destroys that snapshot, even
on a session exit. On normal completion it then calls the parent's `resume()`
to re-establish its view/services before continuing the suspended call stack.
No rendering backend types or per-service decisions belong in the dispatcher.

Exploration currently snapshots drawing contexts, graphic scale, and the rotation
callback. Its resume hook redraws the active orbit/scan view. Flight-time story
encounters have no established orbital view and keep their existing transition.
LanderMode owns visit data, callback binding, menu-sound restoration, and transient
effect cleanup. Its takeoff backdrop remains part of the child animation; final
parent redraw belongs to resume. Cargo/crew outcomes are explicit successful
completion work, never destructor side effects. Loads skip outcomes and redraw.

Other services should follow the same mode-owned contract as they migrate:

| Facility | Mode responsibility |
| --- | --- |
| Input/focus | Own routing and callback bindings; restore the parent's focus and prevent stale commands. |
| Audio | Declare ownership of music/effects and restore or restart parent playback deliberately. |
| Clock/update | Declare whether the parent simulation pauses; do not advance it implicitly during a child. |
| Resources/entities | Own transient effects and resources with RAII; release before parent resume. |
| State/results | Commit gameplay results explicitly; propagate session exits before accessing departing state. |
| Callbacks | Bind only while owners live; cancel/unbind before destruction. |

These are lifecycle rules, not new service class hierarchies. The current backend
still has global services, so mode ownership does not yet permit concurrent sessions.
