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
    Session->>Child: run()
    Child-->>Session: resumeParent() transfers parent ownership
    Note over Session: Restore parent; destroy child
    Session-->>Parent: optional session exit, or nullopt
    Note over Parent: Continue after the call with the same locals
```

The child must belong to the same state/player, have no existing parent, and be
freshly owned by the call. Only an active mode may invoke this API. An empty
`unique_ptr` represents the absence of a suspended parent; wrapping that owner
in another `optional` is unnecessary.

A normal child finishes with `return resumeParent()`. A child may also return
`SessionExit`; the session records it, restores the parent, and returns the
optional exit to the suspended caller. Callers must propagate a returned exit
outward before further gameplay. The outer session loop destroys modes and
returns the exit only after those frames unwind. Load requests never decode or
replace campaign state inside a suspended handler.

The existing flat `ModeResult` successor path remains available for permanent
replacement. A successor returned by a root mode starts running normally; the
parent returned by a nested child is restored without calling `run()` again.
Nested children must return their parent or an exit, not an unrelated successor.

## Temporary fatal-error policy

The child `run()` and return/ownership processing are wrapped in `try/catch (...)`.
The catch prints a diagnostic and calls `std::abort()`, with an explicit TODO to
implement graceful shutdown later. It intentionally performs no unwinding or
recovery of the suspended parent. This matches the temporary agreed policy;
there is no failure message box, autosave, or graceful cleanup on this path.
Invalid call arguments are rejected before ownership changes.

Boost.Test checks multiple nesting levels, preserved locals, one run per mode,
parent/child destruction, and propagation of load slot zero. A Linux subprocess
test verifies SIGABRT on a thrown child exception without terminating the test
runner or writing a core dump.

Orbital system behaviors now use this API through SystemEncounterMode, which
runs the unchanged legacy communication backend. See the
[runtime encapsulation map](runtime-encapsulation.md). Dialogue internals and
combat are still nested legacy routines inside that child. Root navigation
encounters and starbase transitions retain their established outer mode path.
