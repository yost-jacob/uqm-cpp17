# Live conversation graphs

The game and legacy build share the C++17 engine in `sc2/src/uqm/dialogue`.
`ConversationRuntime` owns its state adapter, graph, resources, response strings,
queued speech and animation state for one visit. ConversationMode uses the normal
child-mode lifecycle; gameplay effects are explicit actions, not destructors.

## Live content

All 27 `sc2/content/base/conversations/comm.*.dialogue.json` definitions are
**graph-owned**, including Druuge, Melnorme and operational starbase. Their 3,451
phrase records preserve existing indexed enums, voice clips and subtitle metadata.
The three final native story/menu bodies and the `handlers.legacy_turn` bridge
are removed. Resource descriptors, bounded specialized operations and presentation
cues remain native. No JSON instruction executes arbitrary C or controls playback
through a whole-turn callback.

Graphs contain dialogue, decision, action and terminal nodes. Conditions bind const
state access. Explicit action results select graph edges; replay/redraw never
reexecutes actions. Cursors follow links resolved during loading. Visit-local topic
flags permit loops and repeated questions without an additional script runtime.
Unknown operations/data fail rather than silently succeeding.

The presenter queues phrase IDs, composed text/name insertions and number speech.
Responses use graph edge identity and owned text, including constructed answers;
there is no native response-slot bridge. End-node labels describe graph outcomes;
actual encounter outcome changes are explicit `encounter.set_outcome` actions.

## Text editing and validation

Edit a phrase's `text`, retaining its `id`, `index` and `original_text_hash`.
Changed text replaces the loaded entry. Unchanged base-language snapshots leave
translated addons and original whitespace intact. Names, clips and timestamps
remain attached to their entries; edited dialogue may require changed recordings
or muted speech. The hash detects edits, not malicious tampering.

The descriptor resource key selects the JSON file. Metadata, graph links, actions
and static presentation cues validate before story execution. The old text/resource
tables remain the source of localization and clip metadata; there is no duplicated
mutable runtime string store.

`python3 tools/conversation_runtime_data.py --check` verifies all snapshots.
Without `--check` it refreshes indexed phrase metadata while preserving authored
graph topology/actions, overwriting local text edits. Missing graphs are errors;
it no longer generates native-handler envelopes. Developer-only source indexing
uses pycparser; the game/build has no Python/parser dependency.

## State and specialized operations

`CampaignDialogueState` borrows PlayerEntity/StoryState, Inventory and CampaignClock.
Named story fields use checked accessors over the existing save layout, alongside
native typed access. Inventory owns Fleet and SIS; scalar/item transactions and
narrative batches stay separate. Unknown fields and invalid ranges throw.
See [package actions](design/conversation/package-actions.md).

Merchant offers, precise fuel, rescue plans and report acknowledgements are
implemented behind bounded operations. See [commerce and reports](conversation-commerce.md)
for ownership, failure, timing and compatibility contracts. All ordinary dialogue
branches/menus remain JSON. The combined Hayes/Melnorme design examples remain
reference experiments rather than the production content files.

Mycon cyclic ramble, probe coordinate speech, Ur-Quan hypnosis and Chmmr refit retain
explicit bounded native policies. The initial Commander and operational starbase
have distinct phrase banks. Their resource/lifecycle configuration is not treated
as another story engine.

## Media and cancellation

Standalone phrases may specify a registered `on_start` cue. Variants may specify
ordered `presentation.before`/`after` cues. Cues change presentation, never story
flags, RNG or graph routing. After cues run once at the playback boundary; replay
can repeat a phrase-start cue. Unknown cues/phases and cues inside spliced
compositions are rejected. Split segments where separate timing is needed.

This covers race speaker changes, palette transitions, Talking Pet strobe, VUX
beast escape and the starbase scientist segment. General animation composition is
still issue #31; device/render/audio/input service extraction is issue #15.

Load/quit requests unwind modes before campaign replacement. Normal completion
policy is explicit and skipped for departing sessions; resources/bindings use RAII.
Fatal C/service adapters remain issue #23. This migration does not make sessions
concurrent, headless or transactionally recoverable after arbitrary exceptions.
