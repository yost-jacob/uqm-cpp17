# Next design checkpoint: native entity relationships and state

Sessions, modes and visit runtimes now provide explicit simulation owners.
Navigation borrows its engine/RNG, exploration carries an engine reference, and
lander creation validates its explicit surface scene source. Missile publication
uses typed definitions/launch values directly. All beam callers use BeamLaunch;
Beam participates in the existing CombatEntity lifecycle and retains the original
one-tick collision rules. Custom ship beam collision/postprocess hooks remain.

The next substantial removal is not another function rename. CombatEntity hooks
still borrow ELEMENT, whose shared fields encode several different concepts.
Replacing those fields requires agreeing on the native entity model first.

| Existing representation | Actual uses that must be separated |
| --- | --- |
| ELEMENT::hTarget | Homing target; effect attachment; satellite/point-defense anchor; Pkunk's sentinel to suppress a ship transition |
| ELEMENT::pParent | Combat STARSHIP association; interplanetary group association; optional presence used by damage/retirement logic |
| life_span and related unions | Tick lifetime in combat, persistent surface scan-node identity and lander hazard counters |
| preprocess/postprocess/death callbacks | Native lifecycle adapters mixed with specialized presentation, control and phase-transition callbacks |
| STARSHIP versus persistent Fleet::Ship | Temporary combat incarnation versus persistent vessel identity/crew/results |

Concrete examples include the Pkunk reincarnation sentinel in pkunk.cpp,
Chmmr point-defense's satellite anchor in chmmr.cpp, and the Melnorme confusion
effect following an attached ship in melnorme.cpp. Treating every hTarget as a
homing target would change behavior.

EntityHandle already supplies generation-checked, non-owning physical identity;
reuse it rather than adding another registry. Before migrating the fields, decide
which relationships are targets, source controls and attachments, and what each
does when the referenced incarnation retires. A source's persistent fleet vessel
and its current physical incarnation are different identities. Preserve existing
reincarnation, post-retirement projectile and victory timing rules.

The associated hook question is how native behaviors receive their engine and
state during a tick. An explicit simulation reference or small tick context would
let them call native operations without selected-engine globals. It need not
include or anticipate the eventual renderer/input/audio uqm_context. Distinct
lander state should retain its own movement/collision and scan semantics.

No new relationship hierarchy, event bus, generic ECS, or dispatcher templates
are introduced by this increment. Conversation/menu redesign and the outer
session/title dispatcher remain separate design work. This document records the
boundary reached and the decisions required before deleting the next payloads.

Tracked in [issue #48](https://github.com/yost-jacob/uqm-cpp17/issues/48). Independent
ownership/bookkeeping cleanup continues without replacing these relationship
fields or requiring the new model first.
