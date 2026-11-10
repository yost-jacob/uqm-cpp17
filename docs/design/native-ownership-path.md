# Working down from sessions to game primitives

Use one ownership path at a time. A native wrapper establishes a lifetime; the
next step is to move its orchestration and dependencies inside that lifetime,
then delete the unused compatibility work. Renaming a C file is not sufficient.

| Layer | Responsibility | Dependency contract |
| --- | --- | --- |
| GameSession / ArcadeSession | Own personal campaign state or standalone arcade setup, seeds and dispatcher | A mode borrows its particular session; shared combat does not acquire a campaign singleton |
| ModeDispatcher | Own active and suspended modes and select their compatibility bindings | Child teardown and result return precede parent resume |
| GameMode | Own its SimulationEngine and coordinate a visit | Pass engine/state references into native runtimes and children |
| EncounterRuntime / arcade combat preparation | Assemble participants from persistent vessels or setup choices | Encounter borrows its owning engine; preparation receives the combat child's engine explicitly |
| RuntimeShipRoster | Own fixed-capacity, stable-address participant records and battle outcomes | append returns an optional reference; clear discards resources without publishing gameplay results |
| Ship / projectile / conversation primitives | Perform their individual behavior and own resources | Continue replacing C records/callbacks only after their caller's ownership is explicit |

## Participant construction

Campaign encounter construction now appends native roster slots, retaining
persistent vessel identity, crew, captain and icon/string borrows. SIS is appended
last with its special picker index and energy capacity; its equipment and crew
are still applied at spawn. Enemy construction retains its existing finite and
infinite fleet semantics. The source NPC fragments are still a legacy queue. Navigation now uses the same
native append path. The generic Build entry point and its mixed STARSHIP/fragment
branch are deleted; CreateFleetFragment remains as the specific C recruitment/NPC
buffer boundary, preserving native vessel IDs.

Super Melee previously built fleets inside FillPickMeleeFrame. It now constructs
participants in ArcadeCombatPreparation using the explicitly supplied child
engine. The drawing function draws only labels/icons. Setup side reversal,
network player order, sparse picker-slot indices, costs and captain selection
remain intact. Bad catalogue entries and full pools fail before drawing uses an
invalid participant. Partially prepared resources remain owned by the child.

RuntimeShipRoster::append initializes a new slot and returns no value at capacity;
existing addresses and last-battle outcomes stay unchanged. Captain selection
uses the supplied engine stream and avoids names already occupied by the same
species on that side. Shuffled captain-name cycles are a separate follow-up
(issue #45), not part of this ownership migration.

The campaign combat factory now requires its source roster. It no longer queries
the currently selected process engine. Encounter aftermath reads and clears the
same explicitly borrowed engine that supplied the combat roster. Native arcade
callbacks borrow the setup mode's engine through their scoped frontend record.

## Method for subsequent changes

1. Identify the actual owner and the synchronous borrow lifetime.
2. Give native calls references to that owner/state instead of looking up a global.
3. Move policy/construction out of presentation and C buffer functions.
4. Keep translation of handles, packed records and callbacks at the remaining C
   boundary. Delete each adapter when its last caller moves.
5. Publish gameplay results explicitly. Destructors release resources and restore
   bindings, including incomplete initialization and cancellation.
6. Verify the affected contracts and build both native and legacy paths.

This increment reaches the participant primitive boundary. STARSHIP/ELEMENT
payloads, source/target handles, spawn callbacks and loaded ship assets remain
compatibility machinery. Conversation callbacks, NPC persistence, rendering,
audio and input services retain the boundaries documented in the roadmap; this
change does not claim to encapsulate those services.
