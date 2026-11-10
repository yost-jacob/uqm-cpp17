# Native body selection

SolarSystem owns optional WorldId values for the current inner planet and focused
body. It already owns the generated World tree, so selecting a body resolves that
identity through its existing planet/child containers. No pointer into a temporary
PLANET_DESC array is retained as selection state.

enterInnerSystem selects a planet and changes the visible collection to its moons.
selectWorld changes focus to a generated body in that view. enterOuterSystem
clears both; clearSelection clears focus while retaining the view. Unknown IDs,
moon-as-inner-parent and cross-view selections throw before mutation. Native
selectedWorld/innerPlanet getters return references; absent state throws
bad_optional_access rather than producing another nullable pointer.

Unchanged definition refresh preserves selection and World/resource identity.
Validated planet-layout replacement clears the view/focus. Invalid replacements
leave selection intact. Moon replacement with the same identity is resolved to
the new object on the next lookup; selection cannot dangle into a retired object.

## Flight and compatibility views

SOLARSYS_STATE no longer has pBaseDesc or pOrbitalDesc fields. Native collision,
inner-view validation and drawing iterate the native visible-body collection.
Collision results and saved-orbit restoration return optional reference_wrapper
values to World objects. Entering inner view/orbit uses World references and IDs.
Names, progress lookup and legacy orbit-save markers obtain body identity from
that selection rather than reconstructing it through parent-pointer arithmetic.

world_legacy.hpp exposes descriptor references solely for remaining generation,
geometry and renderer code. The C GetSystemSelectedDescriptor adapter returns a
borrowed view of the selected identity; SelectSystemWorld resolves a C callback's
descriptor argument once and updates the native selection. Resolution compares
addresses inside known arrays, without subtracting unrelated pointers. Foreign
or ungenerated bodies are rejected. The C boundary does not propagate exceptions.
Moons must belong to the currently generated compatibility view.

Selection describes an exploration view/focus, not the player's live orbit or
surface location. PlayerEntity's location variant still owns travel position and
live orbit/surface state. Child encounter, conversation and lander modes retain
the enclosing exploration visit and its native selection.

## Remaining physical payload

The fixed SunDesc/PlanetDesc/MoonDesc arrays, their writable physical fields and
pPrevDesc links remain in the C generation/analysis path. They have not been
copied into another independent geometry database. Seed/kind/shield/layout and
progress continue to use the existing native model/bridge.

Issue #56 tracks moving physical orbit/body properties into native definitions
and separating presentation fields. In particular, SunDesc.radius is a view zoom
scale, while planetary radius describes an orbit. Moving those fields wholesale
would preserve their confusing mixed roles. Issue #52 separately tracks the old
one-based save markers; their file encoding is unchanged by this selection change.
