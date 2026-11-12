# Explicit generation inputs (environment-removal steps 1–2)

Generation still uses the existing C algorithms. Its inputs are now ordinary
parameters, not a selected solar-system global or a process-wide RNG.

`UQM_SYSTEM_RUNTIME` owns one `RandomContext` generation workspace and borrows
its immutable catalogue star. Native callers select these through the explicit
system they already own. The catalogue borrow is established before construction
hooks run, so constructor-time generation does not need a global selected star.
The existing C loop/global bindings remain limited to compatibility paths.

The physical-analysis kernel takes a `PlanetAnalysisInput` containing the packed
stellar type, positive orbital radius, immutable planet-type table and a named
policy. `PLANET_ANALYSIS_PREVIEW` omits the orbital atmospheric temperature bonus;
`PLANET_ANALYSIS_ORBIT` includes its legacy adjustment and random draws. This
replaces the old comparison of the output pointer against the current system's
`SysInfo` address. Stack, heap and other correctly initialized outputs now have
the same behavior for the same inputs.

Mineral/life/node generation accepts its random workspace and map bounds
explicitly. Functions still reseed from the appropriate scan seed and leave the
workspace at their resulting position, preserving callers that derive the next
scan seed from it. Bounds must exceed sixteen units on both axes, retaining the
existing eight-unit placement margin. The runtime call sites pass the same
logical map dimensions as before; the algorithms no longer read `ScreenWidth`.

Terrain/topography generation receives its random context explicitly too. The
star background uses a separate local stream. The small high-resolution terrain
refinement recurrence now has a per-surface seed rather than a function-static
seed shared by every visit. This intentionally makes visual detail repeatable
regardless of which world was generated previously. No persistent node identity
or save-format change is introduced.

`ShowDiscoveryReport` borrows its sound, font, effect and text directly. Native
report callers supply those handles from their visit; report-resource ownership
is deliberately unchanged until step 4. The obsolete `GetPlanetInfo()` global
wrapper and disabled duplicate state queries have been deleted.

The new `uqm-planet-generation` Boost.Test target compiles the real analysis and
surface-node kernels plus actual planet-type data. It exercises analysis policy,
output-address independence, independent random streams and reproducible mineral
and biological node identities/bounds without gameplay/display global symbols.
Existing archive, ownership and nested-mode tests continue in the interop target.

Remaining scope: the C graphics/terrain code still uses its active graphics view,
and immutable table pointers remain in some legacy entry code. Mutable lighting /
projection tables, rotation state, physical data storage, flight/NPC state and C
layout projections are the subsequent steps. No new mode-runtime hierarchy is
introduced here.
