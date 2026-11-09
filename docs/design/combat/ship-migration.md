# Production ship-class migration

All 28 ship implementations now compile as C++17 and create concrete native
classes. Native [ship execution](../../../sc2/src/uqm/ship.cpp) and
[cyborg prediction](../../../sc2/src/uqm/cyborg.cpp) call the virtual interface
directly. The common
[Ship interface](../../../sc2/src/uqm/ship_behavior.hpp) owns each mutable
RACE_DESC and dispatches before-motion, after-motion, weapon and completion hooks.
Ordinary procedural helpers stay in their ship's source file, preserving the
existing algorithms instead of rewriting all weapons at once.

## Ownership and compatibility

[createShip](../../../sc2/src/uqm/ship_factory.hpp) now switches directly on
SPECIES_ID and returns unique_ptr<Ship>. Each concrete builder returns native
ownership; there are no C init factories, code-resource names, numeric SHIP IDs,
resource-handler registrations or capture/release handles in this path.

load_ship is a C buffer implemented in C++. It holds the unique owner while
loading existing image/audio/text resources, and releases it to the C gameplay
layer only on success. RACE_DESC::ship_owner is the remaining opaque link needed
by C behavior dispatch. free_ship reconstructs the native owner at that boundary.
An asset-load rollback guard releases partially loaded resources without publishing
campaign state. Existing selective icon/asset transfers remain supported.

The former dummy.c, coderes.h, corecode.h, dummy.h, 28 icode.h files and 27
init-only headers are deleted. Orz retains its two cross-ship C callback declarations.
Generated resource records are removed from uqm.rmp and resources.csv together;
legacy package recipes drop the corresponding obsolete code entries.

Bundled content/addons do not remap ship code. Third-party SHIP record remapping
no longer selects behavior; old records may be ignored with unknown-type warnings.
Graphics, sound, text and JSON tuning overrides keep their existing paths.

free_ship explicitly calls finish before releasing assets. finish is idempotent
and blocks further gameplay hooks. Destructors release owned optional values
without publishing campaign results, including abandoned resource-load paths. There are no owning C++ objects embedded in packed
save records, and no save encoding or serialized campaign layout changes.

The five ship-specific private record allocators (Androsynth, Umgah, Mmrnmhrm,
Pkunk and flagship) are replaced with typed optional members. RACE_DESC::data is
only a borrowed C view into those values. Initial setup uses local values and
copies them into the new owner before returning; it cannot retain stack pointers.
Static definition records are const, while mutable per-instance records are owned.

Native hooks use references and std::array for the six weapon slots. Ship execution
and cyborg prediction use that array directly; initializeWeapon clears unused slots.
The three former C dispatch wrappers and ship_behavior_c.h are deleted. Exported
engine callbacks retain C linkage through ship.h, intel.h and element.h, so C
input/render/network/element code still compiles and calls the same functions.

## Ship-specific cases

- Pkunk, Chmmr and VUX use an explicit preprocessing-enabled flag rather than
  rewriting a function pointer. Pkunk can disable/restore preprocessing across
  its current reincarnation sequence.
- Melnorme has a typed per-instance weapon mode for normal fire, primary-weapon
  AI probing and confusion AI probing. Its AI restores normal mode afterward,
  as the old implementation restored its function pointer.
- The flagship chooses hyperspace/battle hooks from its captured initialization
  mode. Module/drive/jet configuration, crew/captain conversion and current legacy
  campaign completion remain in the existing helpers.
- Sa-Matra and the Ur-Quan drone explicitly return no ordinary primary weapons;
  their special hooks/behavior retain the existing implementation.

## Coverage

| Concrete class | Native hooks |
|---|---|
| [AndrosynthShip](../../../sc2/src/uqm/ships/androsyn/androsyn.cpp) | before motion, after motion, weapons, completion |
| [ArilouShip](../../../sc2/src/uqm/ships/arilou/arilou.cpp) | before motion, weapons |
| [KohrAhShip](../../../sc2/src/uqm/ships/blackurq/blackurq.cpp) | before motion, after motion, weapons |
| [ChenjesuShip](../../../sc2/src/uqm/ships/chenjesu/chenjesu.cpp) | before motion, after motion, weapons |
| [ChmmrShip](../../../sc2/src/uqm/ships/chmmr/chmmr.cpp) | before motion, after motion, weapons |
| [DruugeShip](../../../sc2/src/uqm/ships/druuge/druuge.cpp) | before motion, after motion, weapons |
| [EarthlingShip](../../../sc2/src/uqm/ships/human/human.cpp) | after motion, weapons |
| [IlwrathShip](../../../sc2/src/uqm/ships/ilwrath/ilwrath.cpp) | before motion, weapons |
| [SaMatraShip](../../../sc2/src/uqm/ships/lastbat/lastbat.cpp) | before motion, after motion, weapons |
| [MelnormeShip](../../../sc2/src/uqm/ships/melnorme/melnorme.cpp) | after motion, weapons |
| [MmrnmhrmShip](../../../sc2/src/uqm/ships/mmrnmhrm/mmrnmhrm.cpp) | before motion, after motion, weapons, completion |
| [MyconShip](../../../sc2/src/uqm/ships/mycon/mycon.cpp) | after motion, weapons |
| [OrzShip](../../../sc2/src/uqm/ships/orz/orz.cpp) | before motion, weapons |
| [PkunkShip](../../../sc2/src/uqm/ships/pkunk/pkunk.cpp) | before motion, after motion, weapons, completion |
| [UrQuanProbeShip](../../../sc2/src/uqm/ships/probe/probe.cpp) | weapons |
| [ShofixtiShip](../../../sc2/src/uqm/ships/shofixti/shofixti.cpp) | after motion, weapons |
| [Flagship](../../../sc2/src/uqm/ships/sis_ship/sis_ship.cpp) | before motion, after motion, weapons, completion |
| [SlylandroShip](../../../sc2/src/uqm/ships/slylandr/slylandr.cpp) | before motion, after motion, weapons |
| [SpathiShip](../../../sc2/src/uqm/ships/spathi/spathi.cpp) | after motion, weapons |
| [SupoxShip](../../../sc2/src/uqm/ships/supox/supox.cpp) | before motion, weapons |
| [SyreenShip](../../../sc2/src/uqm/ships/syreen/syreen.cpp) | after motion, weapons |
| [ThraddashShip](../../../sc2/src/uqm/ships/thradd/thradd.cpp) | before motion, weapons |
| [UmgahShip](../../../sc2/src/uqm/ships/umgah/umgah.cpp) | before motion, after motion, weapons, completion |
| [UrQuanShip](../../../sc2/src/uqm/ships/urquan/urquan.cpp) | after motion, weapons |
| [UtwigShip](../../../sc2/src/uqm/ships/utwig/utwig.cpp) | before motion, weapons |
| [VuxShip](../../../sc2/src/uqm/ships/vux/vux.cpp) | before motion, after motion, weapons |
| [YehatShip](../../../sc2/src/uqm/ships/yehat/yehat.cpp) | before motion, after motion, weapons |
| [ZoqFotPikShip](../../../sc2/src/uqm/ships/zoqfot/zoqfot.cpp) | after motion, weapons |

## Deliberate remaining boundaries

This is the live behavior/ownership migration, not a complete native combat engine.
The C element pipeline still owns projectile callbacks, collision ordering and
retirement cleanup. Its ditty, reincarnation, withdrawal, netplay readiness and
fleet-selection rules are preserved. The broader Battle mockup remains a target.
Shared source lifetime and ProjectileBehavior are still exercised in the separate
prototype rather than substituted into gameplay prematurely.

AI remains the existing ship-specific cyborg backend, independently registered in
each instance. Keyboard/network/alternative-AI controller replacement is separate
work. Base ship and standard projectile balance values now load from
[ships.json](../../ship-tuning.md); structural geometry and complex weapon
mechanics remain typed C++ constants.
Legacy asset handles, C globals and Pkunk's existing sound-selection history still
prevent multiple independent battles in one process. These classes alone do not
make combat thread-safe or headless.

The [lifecycle wireframe](lifecycle.md) records the agreed retirement/victory rules.
BattleRules::persistentProjectiles is a compile-time false flag in the prototype,
with if constexpr guarding retirement cleanup. Normal gameplay retains its
existing cleanup; there is no launch option or alternate runtime rule yet.

## Verification

Boost.Test exercises direct native dispatch, stable descriptor identity,
independent mutable descriptors, complete species-to-builder mapping, cleared
weapon slots, disabled/restored hooks,
completion idempotence, partial-asset rollback, selective icon transfer, and
destruction without campaign publication. The projectile prototype also
checks default source-retirement removal without detonating the crystal.
Full Debug/Release CMake/Ninja, legacy build and sanitizer results are recorded
in the PR. GUI smoke testing is a focused resource/dispatch check, not an exhaustive
balance or special-ability playthrough of all 28 implementations.
