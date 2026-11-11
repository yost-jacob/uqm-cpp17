# Ship tuning and implementation constants

[sc2/content/base/ships.json](../sc2/content/base/ships.json) contains balance data
for all 28 supported ship behaviors. Edit the file and restart the game. It is
loaded through the content/addon filesystem before ship resources are created;
there is no hot reload, compiled balance fallback, launch option or modification
penalty. An addon can override `base/ships.json` through the same content path.

For example, change `ships.arilou.max_crew` from 6 to 1000. A fresh ship starts
with its maximum crew. Current crew is instance/campaign state, so this edit does
not overwrite the surviving crew in an existing save. There is no `crew` property.
The same distinction applies to maximum and current energy.

## Editable fields

Each ship record has the following properties, plus a `projectiles` object:

| Properties | Meaning |
|---|---|
| `max_crew`, `max_energy` | Initial capacity for ordinary fresh ships |
| `max_thrust`, `thrust_increment` | Existing engine movement units |
| `turn_wait`, `thrust_wait` | Existing integer turn/thrust delays; zero is allowed |
| `energy_regeneration`, `energy_wait` | Recharge amount and delay; signed regeneration is supported |
| `weapon_energy_cost`, `weapon_wait` | Primary energy cost and firing delay |
| `special_energy_cost`, `special_wait` | Special ability's base cost and delay |
| `ship_mass` | Existing collision/gravity mass value |
| `melee_cost` | Super Melee fleet cost |

The included projectile profiles expose `speed`, `hit_points`, `damage`, `lifetime` and a `flight` object. For example, the Earthling Cruiser uses
`ships.human.projectiles.missile`; Chenjesu has `missile` and `fragment`, and the
flagship has `blaster`. IDs match the existing source-directory names. Ships with
nonstandard weapons may have an empty `projectiles` object.

Speeds and movement values use the existing world units, not screen pixels. For
example, a former `DISPLAY_TO_WORLD(8)` speed is stored as 32. Lifetimes and waits
use the existing simulation counters. This does not change the angle/facing model,
physics cadence, steering precision or rendering resolution.

Values are explicit, independent settings. Editing an energy capacity does not
implicitly rewrite an energy cost; editing a ship's thrust does not rewrite a
projectile speed. Keep Arilou's thrust increment equal to its maximum thrust to
retain the existing inertialess movement rule.

## Validation and limits

The parser requires schema version 2, exactly the supported ship IDs, the complete
field sets, and the projectile profile names expected by each behavior. It rejects
missing/unknown fields, floats, strings, incorrectly typed booleans and out-of-range integers rather
than truncating or narrowing them. Errors identify `base/ships.json` and the ship
or projectile profile when available. No partial catalog is installed on failure.

- Crew capacity: 1–32767; movement magnitudes: 0–32767.
- Maximum energy: 1–255; costs, waits, mass and fleet cost: 0–255.
- Energy regeneration: -128–127, encoded into the existing signed-byte convention.
- Projectile speed: 0–1023; hit points: 1–255; lifetime: 1–32767; damage: 0–255.
- Flagship base blaster damage: 1–85; hit points at most 85, so its three module
  tiers remain representable by the existing element fields.

These are legacy representation limits, not a balance policy. Extreme settings
still interact with the existing mechanics and their arithmetic. An oversized AI
range hint saturates at its legacy field limit; physical lifetime/speed are not
silently reduced. Save files retain their existing representation and do not embed
this tuning file. Multiplayer participants need matching content/rules.

## What stays in C++

Frame indices, sprite attachment offsets, collision geometry, bit masks, fixed
capacities and animation layout remain typed constants. Complex/nonstandard
weapons retain their implementation-specific constants until their semantics can
be separated cleanly from animation and state encoding. Range hints and other
computed values remain ordinary calculations rather than duplicate JSON numbers.

The flagship derives final capacity and weapon behavior from installed modules;
its JSON record supplies base values. Damaged Shofixti, Mmrnmhrm transformation,
and other special cases still apply their existing behavior. JSON does not make
those mechanics interchangeable or bypass their constraints.

The cleanup removes all 693 object/function/alias macro definitions from the ship
implementation files: 468 existing macro values move to tuning, 212 become
constexpr constants (four unused definitions are deleted), seven become calculated accessors, one becomes a typed
predicate, and the boss generator-count alias becomes explicit field access.
Fleet costs also move from initializer literals into JSON. Stock numeric values
were compared against all 689 captured numeric definitions across the 28 ships;
all match. The shared color conversion is constexpr in C++ and remains inline C.

Ship definitions are initialized lazily after catalog loading. Active definitions
and references remain immutable for the process lifetime. No per-frame JSON parsing
or string lookup is required; ship/profile references are cached on first use.

## Projectile flight (schema 2)

Each profile has these explicit `flight` fields:

| Field | Meaning |
|---|---|
| `steering_period_ticks` | Shared guidance cadence; zero disables guidance |
| `initial_delay_ticks` | Ticks before the first guidance decision |
| `acceleration_per_tick` | Added speed per elapsed lifetime tick, 0–1023 |
| `maximum_speed` | Speed cap, at least launch speed, at most 1023 |
| `directional_sprite` | Shared guidance rotates a facing-based sprite |
| `snap_to_target` | Shared guidance turns directly to the target heading |

The common flight implementation drives Earthling, Spathi, Mmrnmhrm and VUX
guidance and ordinary linear shots. Mycon extends it with aging plasma.
Androsynth uses the cadence with its randomized steering. Specialized weapons
retain their own flight/animation rules: their `flight` fields do not override
those rules. SIS tracking cadence comes from installed modules.

Additional profiles are `mycon.plasma`, `blackurq.gas`, `melnorme.charge`,
`thradd.napalm`, and `zoqfot.tongue` (under each ship's `projectiles`). Melnorme's
charge profile describes the released, uncharged shot; charging scales damage
and hit points through four tiers. Its base damage and hit points must be equal,
positive and at most 31. Mycon plasma also couples positive damage and hit points.
Held/anchored phases can extend or replace the configured finite lifetime.
Art frame counts, launch geometry, fragment patterns and special collision rules
remain implementation details. Schema 1 addon files need the new profiles,
`flight` objects and version before they can load.
