# Session-owned game definitions

Production campaign and arcade constructors load their definitions after the
application has initialized the content filesystem. Title-menu and intro sessions
load neither catalog. `Session::definitions()` returns a const reference; calling
it on a frontend session throws rather than borrowing somebody else's catalog.

| Session | Ship tuning | Starmap |
| --- | --- | --- |
| Campaign | Own immutable catalog | Own immutable map and C projection |
| Arcade | Own immutable catalog | Not loaded |
| Title / intro | Not loaded | Not loaded |

Native code uses `session.definitions().ships()` and `.stars()`. Both provide
checked, read-only interfaces. The starmap uses stable `StarId` values; tuning
uses checked ship/profile names. References expire when that session is destroyed.
No shared or process-lifetime catalog remains. Catalog data is not asset-cache
content and is not serialized into saves.

`GameDefinitions` constructs and validates a complete candidate before the session
publishes it. Errors identify the content path. File handles close through RAII,
including partial loads. A failed successor constructor leaves its caller's
catalog untouched. Edits are read again for the next gameplay session.

Standalone/test sessions can start with an empty definitions owner and explicitly
load fixtures. `LoadStarMap` / `FreeStarMap` remain narrow C compatibility entry
points for standalone starmap integration; production startup no longer calls
them. The unused C ship-tuning loader and its header were deleted. Replacing a live catalog is rejected. Clearing a standalone test catalog
requires that no borrowers remain.

## Current star and legacy adapters

The old global `CurStarDescPtr` is gone. Campaign selection is an optional `StarId`
in `PlayerEntity::navigationState()`. `GetCurrentStar()` / `SetCurrentStar()`
translate between that identity and the active session's C descriptor buffer.
Frontend/arcade compatibility selection is isolated in each session and starts
empty. C descriptors, names, and profile borrows must never cross session
boundaries. The saved descriptor projection and its restoration path are unchanged.

A scoped runtime binding supplies the active definitions and selection slot to
remaining C code. This is a compatibility lookup, not an owner. Native code with
a session reference should use it directly. The existing standalone `gameSession()`
fallback remains for C compatibility/tests; it does not supply production catalogs
to other sessions. Accessing an absent/invalid catalog at a C boundary fails
without allowing a C++ exception to escape that boundary.

## Ship resources and cached definitions

The splash screen no longer constructs the master ship list. Each gameplay
session's `run()` acquires that legacy resource list through an RAII scope, after
its definitions are bound. It releases it before returning to title and before
the definitions owner is destroyed. The `master_q` container remains legacy global
infrastructure, with a session-scoped resource lifetime; moving the container
itself is a separate cleanup.

All 28 ship implementations now resolve tuning/profile references from the active
session, without function-static cached references. Derived `RACE_DESC` values
are rebuilt for each created ship rather than frozen on first use. This prevents
stale references and values after returning to title or editing tuning between
sessions. Only ordinary catalog lookups happen during simulation; JSON parsing
happens during session construction.

Validation covers definition isolation, immutable native access, missing/malformed
files, arcade operation without a starmap, frontend operation without either file,
active C lookup across successive sessions with different tuning, and player-owned
star selection. Live campaign and arcade checks complement the unit tests; they
do not exhaust every ship or campaign branch.
