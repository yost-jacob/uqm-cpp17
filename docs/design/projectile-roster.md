# Native projectile roster

Ship missile builders now publish pool-owned C++ `Projectile` objects. The engine
calls `update`, `postprocess` and `collide` through one compatibility boundary in
projectile_legacy.cpp. Each callback borrows the owning slot only for that call.
Default collision retains the existing weapon damage/explosion code.

`ConfiguredProjectile` replaces duplicate steering/acceleration callbacks with
value-owned JSON flight settings and a per-shot timer. Native derived classes
retain distinctive behavior:

| Weapon | Native responsibility |
|---|---|
| Androsynth bubble | Randomized tracking and independent animation/steering counters |
| Chenjesu crystal | Held fire, release into fragments, crystal collision |
| Kohr-Ah saw | Held, braking, tracking and splinter phases |
| Kohr-Ah gas / Thraddash napalm | Expanding or fading hazards |
| Melnorme charge | Charging/flying phases, charge animation and impact |
| Melnorme confusion | Dart animation and debuff creation |
| Mycon plasma | Aging damage, sprite growth and impact |
| VUX limpet | Shared guidance plus animation and attachment |
| Ilwrath / Pkunk / Umgah / Zoq-Fot-Pik | Weapon-specific animation, movement or source binding |
| Sa-Matra | Comet/sentinel flight and collision |

The ship still supplies launch position, heading and initial velocity through
its existing MISSILE_BLOCK builder. `prepareMissile` converts this into detached
native definition/launch values; `publishMissile<Flight>` constructs the behavior
and transfers its unique ownership to the element pool. This keeps C layout and
asset ownership unchanged while removing projectile function-pointer selection
from ship builders. The publisher now initializes the engine record directly
from those typed values; initialize_missile and its duplicate C construction path
are deleted. Beams use BeamLaunch and a pool-owned Beam behavior; LASER_BLOCK
and initialize_laser are also deleted. See [the current ownership checkpoint](native-entity-checkpoint.md).

ELEMENT remains authoritative for mutable position, damage, lifetime and collision
state. Specialized classes still borrow those fields and existing ship input;
this is an incremental migration, not a replacement physics engine. Fighters,
marines, DOGIs, satellites, asteroids and effects subsequently gained the common
[CombatEntity lifecycle](combat-entities.md). Transformations and engine resource
services retain their existing implementations.

Raw ELEMENT copies never copy native ownership. Kohr-Ah's queue relocation
explicitly clones its saw phase into the destination owner. Chenjesu AI and
Sa-Matra collision identification use native type checks because callback
addresses are shared by all native projectiles. Melnorme pool exhaustion stops
charge replacement without dereferencing a null handle.

See [ship tuning](../ship-tuning.md) for schema 2 settings and constraints.
Boost.Test covers shared flight cadence, independent shot timers, acceleration
caps, directional sprites, virtual hooks, allocation failure and pool cleanup.
Specialized weapon logic preserves the existing procedural rules inside small
classes; it does not add a parallel battle model or a scripting framework.
