# Construction-owned system generation

Production exploration now constructs a complete `SolarSystem` layout before
starting the flight loop. Its constructor selects and attaches the system
behavior, seeds the existing generator from the star coordinates, generates the
planet descriptors, and constructs the owned `World` objects.

Each generated planet's `World` constructor creates its moon layout and child
worlds. It keeps the generated moon descriptors with the planet. Entering an
inner-system view copies those descriptors into the legacy `MoonDesc` buffer;
it no longer generates a different set of children at that point. Naming remains
an entry operation because the old naming hooks also update the current player's
planet name and story selection.

The old data-only constructors and descriptor attachment functions remain for
archive/tests/debug callers. They do not claim to run procedural generation.

## Planet surfaces

A world owns `PlanetRenderResources`. Its private `Surface` object generates
terrain and orbital globe resources during construction and releases those
resources on destruction. `LoadPlanet` now requests construction through the
selected world's graphics owner instead of directly invoking the terrain
algorithm. A failed constructor releases its partially generated surface.

Surface construction occurs on entering orbit, after the system's orbital
handler has selected any special surface art, color maps, discoveries and story
behavior. Constructing all those assets when entering a system would require
separating the orbital hooks: some launch conversations or combat instead of
showing a landable planet. Those effects must not occur just because a system
or planet is constructed.

Thus system/planet/moon **layout** construction is eager, while the planet-owned
**surface resource** is constructed when its orbit presentation is requested.
The terrain algorithm, mineral/life scan-seed generation, node generation and
orbit-entry story actions remain their existing implementations. This change
moves generation and ownership boundaries; it does not replace those algorithms
or implement the optional randomized-universe feature.

## Lifetimes and persistence

The active system descriptor is borrowed by generation. Native worlds own their
moon snapshots, while C flight code uses a temporary projection. The current
surface record is likewise a borrowed compatibility view of world-owned assets.
Rotation and shield code can update its handles; the surface owner observes the
final handles when releasing them.

Returning from orbit reconstructs the layout, as the previous path regenerated
it, so story-dependent changes such as shields are reflected. References into
the prior native world tree expire then. Generated art can be released with that
old tree; it is not campaign persistence. Retrieved-node masks stay in shared
world state and are applied again on the next orbit visit.

No destructor awards cargo, publishes retrieval progress or invokes a normal
story-completion action.

## Validation

Boost.Test exercises complete special-system construction without orbit hooks,
owned moon projection without regeneration, retained world progress, and surface
constructor cleanup after success and failure. Device-free tests use the existing
fake engine dependencies. A separate live smoke reaches Luna's orbit menu with
the real Sol layout, generated terrain and globe. This is not a complete story
or visual regression playthrough.

Orbit entry now uses [constructor-selected events and an orbital child mode](orbit-events.md).
