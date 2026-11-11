# Existing conversation graph catalog

This catalogs the **current game scripts**, not the proposed replacement engine.
Each conversation page contains its actual lifecycle entry points, a handler
graph, response edges and their conditions, dialogue text, operations and state
updates, and the exact decision bodies with links to source lines.

For the proposed replacement, see the [conversation model](model.md) and the
[standalone C++17 engine prototype, verb vocabulary, Hayes and Melnorme JSON](engine/README.md).
For current ownership and mode integration, see [conversation child mode](../conversation-mode.md).

## How to read the graphs

The current scripts do not have explicit dialogue-node objects. A handler is a
**parameterized node**: its behavior depends on the incoming response ID, game
state, and local calculations. `PLAYER_SAID(R, response)` branches identify its
incoming-response variants. Lifecycle initialization selects the entry handler;
post/uninit callbacks can also change state after dialogue ends.

A solid graph arrow is a `Response` or `DoResponsePhrase` registration: choosing
that response invokes the destination callback with its phrase ID. Dashed arrows
without labels are immediate local C calls, with arguments and conditions in the
node tables. Labeled dashed arrows identify lifecycle bindings, passed callback
references, or candidates for a computed function call. A callback reference is
not itself an immediate execution.
Neither kind of arrow implies a separate game mode. Multiple response sites
between two handlers are grouped only in the diagram; the edge tables retain
every registration site.

Conditions in the tables are **enclosing source branches and loops**, not a
normalized predicate over campaign state. Conditions that use local variables,
computed phrase arrays, early returns, and switch fallthrough must be read with
the accompanying updates and exact decision body. “No enclosing branch condition”
does not mean a response is reachable in every execution. Calls made by helper
functions also inherit the calling path's conditions. The condition catalog
assigns linked IDs to the original expressions. Computed response destinations
list their assignment conditions and candidate callbacks. The catalog retains
those dependencies in the source-backed representation.

`PHRASE_ENABLED` / `DISABLE_PHRASE` are recorded where used. Responses are not
assumed to be one-shot: cycles, repeated topics, explicit disabling, and counter-
selected alternatives retain the behavior written in the script. Seen-response
gray styling is a proposed feature, not behavior inferred by this catalog.

The operation tables include calls, assignments, local initialization, increments,
and control flow. A call is not automatically classified as a state mutation:
queries, animation changes, fleet/cargo operations, events, and helper calls are
all visible with their original arguments. Inspecting a helper's own node exposes
its transitive effects. `NPCPhrase_cb` retains queued presentation callbacks in
its recorded arguments. The original LOCDATA presentation descriptor remains
linked in the script; this catalog does not flatten its animation state machine.

Phrase catalogs contain full **base-content** text and voice metadata. Installed
addons may override resources; dynamic names, numbers, and constructed responses
remain expressions/placeholders rather than fabricated literal text.

For the queries and calculations the new engine must express, see the
[condition vocabulary audit](conditions.md), including side-effecting functions
that need to be separated from availability predicates.

## Worked trace: Fwiffo recruitment

This is an actual slice of [Spathi / Fwiffo](catalog/spathi.md), traced from
[SpathiOnPluto](../../../sc2/src/uqm/comm/spathi/spathic.c#L209) and
[ExitConversation](../../../sc2/src/uqm/comm/spathi/spathic.c#L148).

```mermaid
flowchart TD
    Menu["SpathiOnPluto: response menu"]
    Monsters["HOW_TRUE<br/>DISABLE_PHRASE(full_of_monsters)"]
    Refuse["WONT_JOIN_1, then WONT_JOIN_2, then WONT_JOIN_3<br/>advance local refusal count for first two"]
    Exit["ExitConversation(join_us)<br/>setSegue(Segue_peace)"]
    Check{"EscortFeasibilityStudy(SPATHI_SHIP) == 0?"}
    NoShip["TOO_SCARY<br/>no recruitment"]
    Recruit["WILL_JOIN<br/>AlienTalkSegue(~0)<br/>AddEscortShips(SPATHI_SHIP, 1)<br/>SetEscortCrewComplement(SPATHI_SHIP, 1, NAME_OFFSET + NUM_CAPTAINS_NAMES)"]
    Done["End conversation; return to encounter"]
    Menu -->|"full_of_monsters, when offered"| Monsters
    Monsters -. "rebuild response menu" .-> Menu
    Menu -->|"join_us; PHRASE_ENABLED(full_of_monsters)"| Refuse
    Refuse -. "rebuild response menu" .-> Menu
    Menu -->|"join_us; !PHRASE_ENABLED(full_of_monsters)"| Exit
    Exit -.-> Check
    Check -->|"yes"| NoShip
    Check -->|"no"| Recruit
    NoShip -.-> Done
    Recruit -.-> Done
```

The same response ID, `join_us`, is registered with different destination handlers
based on phrase state. Before the monster topic is disabled, asking again cycles
through refusals; the third refusal can repeat. Once that topic is disabled,
selection reaches the recruitment branch. Its successful action adds one Spathi
escort and sets its crew/captain to the Fwiffo configuration. The complete menu
conditions, greeting selection, and post-encounter changes are in the catalog.

## Conversations

<!-- catalog:start -->
<!-- Generated by tools/conversation_catalog.py. -->

| Conversation | Handler/helper nodes | Response sites | Direct local calls | Phrase records | Live graph |
| --- | ---: | ---: | ---: | ---: | --- |
| [Arilou](catalog/arilou.md) | 4 | 0 | 0 | 97 | [graph-owned story: 129 nodes](../../../sc2/content/base/conversations/comm.arilou.dialogue.json) |
| [Kohr-Ah](catalog/blackur.md) | 4 | 0 | 0 | 76 | [graph-owned story: 106 nodes](../../../sc2/content/base/conversations/comm.kohrah.dialogue.json) |
| [Chmmr](catalog/chmmr.md) | 11 | 31 | 7 | 78 | [native handler bridge: 21 nodes](../../../sc2/content/base/conversations/comm.chmmr.dialogue.json) |
| [Commander Hayes: first contact](catalog/comandr.md) | 12 | 52 | 7 | 99 | [native handler bridge: 21 nodes](../../../sc2/content/base/conversations/comm.commander.dialogue.json) |
| [Druuge](catalog/druuge.md) | 13 | 29 | 5 | 105 | [native handler bridge: 21 nodes](../../../sc2/content/base/conversations/comm.druuge.dialogue.json) |
| [Ilwrath](catalog/ilwrath.md) | 4 | 0 | 0 | 108 | [graph-owned story: 115 nodes](../../../sc2/content/base/conversations/comm.ilwrath.dialogue.json) |
| [Melnorme](catalog/melnorm.md) | 36 | 61 | 46 | 282 | [native handler bridge: 21 nodes](../../../sc2/content/base/conversations/comm.melnorme.dialogue.json) |
| [Mycon](catalog/mycon.md) | 4 | 0 | 0 | 109 | [graph-owned story: 240 nodes](../../../sc2/content/base/conversations/comm.mycon.dialogue.json) |
| [Orz](catalog/orz.md) | 11 | 37 | 7 | 116 | [native handler bridge: 21 nodes](../../../sc2/content/base/conversations/comm.orz.dialogue.json) |
| [Pkunk](catalog/pkunk.md) | 4 | 0 | 0 | 182 | [graph-owned story: 192 nodes](../../../sc2/content/base/conversations/comm.pkunk.dialogue.json) |
| [Yehat rebels](catalog/rebel.md) | 4 | 0 | 0 | 34 | [graph-owned story: 51 nodes](../../../sc2/content/base/conversations/comm.yehat.rebel.dialogue.json) |
| [Shofixti](catalog/shofixt.md) | 4 | 0 | 0 | 95 | [graph-owned story: 83 nodes](../../../sc2/content/base/conversations/comm.shofixti.dialogue.json) |
| [Slylandro homeworld](catalog/slyhome.md) | 4 | 0 | 0 | 116 | [graph-owned story: 135 nodes](../../../sc2/content/base/conversations/comm.slylandro.dialogue.json) |
| [Slylandro probe](catalog/slyland.md) | 6 | 0 | 2 | 86 | [graph-owned story: 108 nodes](../../../sc2/content/base/conversations/comm.probe.dialogue.json) |
| [Spathi Safe Ones](catalog/spahome.md) | 14 | 64 | 7 | 145 | [native handler bridge: 21 nodes](../../../sc2/content/base/conversations/comm.safeones.dialogue.json) |
| [Spathi / Fwiffo](catalog/spathi.md) | 4 | 0 | 0 | 134 | [graph-owned story: 141 nodes](../../../sc2/content/base/conversations/comm.spathi.dialogue.json) |
| [Commander Hayes: starbase](catalog/starbas.md) | 21 | 57 | 13 | 300 | [native handler bridge: 21 nodes](../../../sc2/content/base/conversations/comm.starbase.dialogue.json) |
| [Supox](catalog/supox.md) | 4 | 0 | 0 | 97 | [graph-owned story: 107 nodes](../../../sc2/content/base/conversations/comm.supox.dialogue.json) |
| [Syreen](catalog/syreen.md) | 14 | 40 | 6 | 131 | [native handler bridge: 21 nodes](../../../sc2/content/base/conversations/comm.syreen.dialogue.json) |
| [Talking Pet](catalog/talkpet.md) | 13 | 33 | 6 | 113 | [native handler bridge: 21 nodes](../../../sc2/content/base/conversations/comm.talkingpet.dialogue.json) |
| [Thraddash](catalog/thradd.md) | 16 | 38 | 9 | 154 | [native handler bridge: 21 nodes](../../../sc2/content/base/conversations/comm.thraddash.dialogue.json) |
| [Umgah](catalog/umgah.md) | 4 | 0 | 0 | 87 | [graph-owned story: 111 nodes](../../../sc2/content/base/conversations/comm.umgah.dialogue.json) |
| [Ur-Quan](catalog/urquan.md) | 4 | 0 | 0 | 76 | [graph-owned story: 106 nodes](../../../sc2/content/base/conversations/comm.urquan.dialogue.json) |
| [Utwig](catalog/utwig.md) | 4 | 0 | 0 | 117 | [graph-owned story: 189 nodes](../../../sc2/content/base/conversations/comm.utwig.dialogue.json) |
| [VUX / ZEX](catalog/vux.md) | 7 | 31 | 2 | 102 | [native handler bridge: 21 nodes](../../../sc2/content/base/conversations/comm.vux.dialogue.json) |
| [Yehat royalists](catalog/yehat.md) | 4 | 0 | 0 | 75 | [graph-owned story: 89 nodes](../../../sc2/content/base/conversations/comm.yehat.dialogue.json) |
| [Zoq-Fot-Pik](catalog/zoqfot.md) | 13 | 28 | 48 | 337 | [native handler bridge: 21 nodes](../../../sc2/content/base/conversations/comm.zoqfotpik.dialogue.json) |
| **Total** | 243 | 501 | 165 | 3451 | |
<!-- catalog:end -->

## Regeneration and limits

From the repository root, with the development-only `pycparser` Python package:

```sh
python3 tools/conversation_catalog.py
python3 tools/conversation_catalog.py --check
```

The generator parses the C syntax without expanding story macros into packed
bit offsets. It resolves default phrase resources through `sc2/content/uqm.rmp`.
It checks that every parsed call is visited and every selectable response has
local destination callbacks, including assignments to callback variables. It
also catalogs script-scope variables and phrase/technology lookup tables. It fails on unfamiliar conditional compilation;
existing `#if 0` and `#ifdef NEVER` branches are excluded and reported.

This is a source-backed, stateful control-flow catalog. It is not an exhaustive
enumeration of every campaign-state combination, an execution simulator, or a
claim that every syntactically present branch is reachable in normal gameplay.
It includes helper/lifecycle nodes so initialization, delayed presentation,
recruitment, trading, and post-encounter effects are not lost. Excerpt trailing whitespace is removed for Markdown; source tokens and dialogue
wording are retained. No game code or conversation behavior is changed by
generating these pages.
