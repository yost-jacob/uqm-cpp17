#!/usr/bin/env python3
"""Cross-check design data against source and the accepted verb registry.

The C++ tests exercise the actual loader/runtime. This validates independent
source coverage, phrase/resource references, presentation profiles and native
call classification, not a second implementation of graph execution.
"""

from collections import Counter
import json
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(ROOT / "tools"))
from conversation_catalog import (
    active_source,
    PREFIX,
    TYPES,
    Calls,
    expression,
    phrases,
)
from pycparser import c_ast, c_parser


def load(name):
    return json.loads((HERE / name).read_text())


registry = load("verbs.json")
query_names = {v["verb"] for v in registry["queries"]}
action_names = {v["verb"] for v in registry["actions"]}
assert len(query_names) == len(registry["queries"])
assert len(action_names) == len(registry["actions"])
assert all(v["effect"] == "read_only" for v in registry["queries"])
assert all(v["effect"] == "mutating" for v in registry["actions"])
profiles = load("presentation.json")
for composite in profiles["composites"].values():
    assert (ROOT / composite["_source"]).is_file()
    for layer in composite["ambient"] + [composite["talk"], composite["transition"]]:
        assert layer["first_frame"] >= 0 and layer["frame_count"] >= 0
        assert all(0 <= i < len(composite["ambient"]) for i in layer["blocks_layers"])
        for rate in ("frame_seconds", "restart_seconds"):
            for part in ("base", "random_range"):
                numerator, denominator = layer[rate][part]
                assert numerator >= 0 and denominator > 0
for profile in profiles["profiles"].values():
    assert profile["composite"] in profiles["composites"]
manifest = load("legacy-verbs.json")
expected = Counter()
literal_responses = {}
for path in sorted((ROOT / "sc2/src/uqm/comm").glob("*/*.c")):
    source, _ = active_source(path.read_text())
    ast = c_parser.CParser().parse(PREFIX + source)
    choices = set()
    for fn in ast.ext:
        if not isinstance(fn, c_ast.FuncDef):
            continue
        calls = Calls()
        calls.visit(fn.body)
        for call in calls.nodes:
            name = expression(call.name)
            line = call.coord.line - len(TYPES)
            expected[path.parent.name, fn.decl.name, line, name] += 1
            if name in ("Response", "DoResponsePhrase") and isinstance(
                call.args.exprs[0], c_ast.ID
            ):
                choices.add(call.args.exprs[0].name)
    literal_responses[path.parent.name] = choices
actual = Counter()
for mapping in manifest["mappings"]:
    assert mapping["kind"] and mapping["targets"]
    for site in mapping["sites"]:
        actual[mapping["script"], site["handler"], site["line"], mapping["call"]] += 1
assert expected == actual, "legacy call-site mapping is stale or incomplete"
assert sum(expected.values()) == manifest["occurrences"]


def validate_example(name, scripts, banks):
    d = load("examples/" + name + ".json")
    nodes = d["nodes"]
    expected_count = len(nodes) + 2 * sum(
        "action" in edge for node in nodes for edge in node.get("responses", [])
    )
    assert type(d["node_count"]) is int and d["node_count"] == expected_count
    by_id = {n["id"]: n for n in nodes}
    assert len(by_id) == len(nodes) and d["entry"] in by_id
    assert d["initial_presentation"] in profiles["profiles"]
    bank = d["phrases"]
    for bank_name in banks:
        original = phrases(ROOT / f"sc2/content/base/comm/{bank_name}/{bank_name}.txt")
        for symbol, data in original.items():
            assert bank[bank_name + "." + symbol]["text"] == data["text"]
            assert bank[bank_name + "." + symbol]["voice"] == (data["voice"] or None)
    selected = set()
    links = {id: [] for id in by_id}

    def phrase(value):
        if isinstance(value, str):
            assert value in bank, ("missing phrase", value)
        elif isinstance(value, list):
            for item in value:
                phrase(item)
        elif isinstance(value, dict):
            if "phrase" in value:
                phrase(value["phrase"])
            elif "compose" in value:
                phrase(value["compose"])
            else:
                assert any(k in value for k in ("get", "insert", "number")), (
                    "unknown speech request",
                    value,
                )
        else:
            raise AssertionError(("invalid text", value))

    def inspect(value):
        if isinstance(value, dict):
            if "query" in value:
                assert value["query"] in query_names
            if "verb" in value or isinstance(value.get("action"), str):
                assert value.get("verb", value.get("action")) in action_names
            for v in value.values():
                inspect(v)
        elif isinstance(value, list):
            for v in value:
                inspect(v)

    for node in nodes:
        id = node["id"]
        if "next" in node:
            links[id].append(node["next"])
        if "otherwise" in node:
            links[id].append(node["otherwise"])
        links[id].extend(b["next"] for b in node.get("branches", []))
        for edge in node.get("responses", []):
            links[id].extend(edge.get("then", {}).values())
            links[id].extend(edge[key] for key in ("next", "otherwise") if key in edge)
            if "action" in edge:
                assert "next" not in edge and edge.get("then"), (
                    "invalid transaction",
                    id,
                )
                assert all(isinstance(code, str) and code for code in edge["then"])
        assert all(target in by_id for target in links[id]), ("missing node", id)
        for variant in node.get("variants", []):
            phrase(variant["speech"])
            profile = variant.get("presentation", {}).get("composite", "continue")
            assert profile == "continue" or profile in profiles["profiles"], profile
        for edge in node.get("responses", []):
            selected.add(edge["id"])
            phrase(edge["text"])
        actions = [node.get("action", {})] + [
            edge["action"] for edge in node.get("responses", []) if "action" in edge
        ]
        for action in actions:
            if action.get("action", action.get("verb")) == "sequence.next":
                phrase(action["args"][2])
            if action.get("action", action.get("verb")) == "handlers.prepare_report":
                assert action["args"][0] in d["_coverage"]["native_reports"]
    inspect(d)
    reached = set()
    pending = [d["entry"]]
    while pending:
        n = pending.pop()
        if n not in reached:
            reached.add(n)
            pending.extend(links[n])
    unreachable = set(by_id) - reached
    assert not unreachable, ("unreachable design nodes", name, sorted(unreachable))
    for script in scripts:
        missing = literal_responses[script] - selected
        assert not missing, ("unmapped literal response IDs", script, missing)
    # Market speech and technology grants are data consumed by the native market adapter.
    if "markets" in d:
        market = d["markets"]["melnorme"]
        assert len(market["technology"]) == 13
        for offer in market["technology"]:
            phrase(offer["pitch"])
            phrase(offer["sold"])
            assert offer["price"] == 150
        for category, count in [("events", 8), ("alien_races", 16), ("history", 9)]:
            book = market["information"]["categories"][category]
            assert len(book["speech"]) == count
            phrase(book["speech"])
    print(
        name + ":",
        len(nodes),
        "reachable nodes;",
        len(bank),
        "source-matched phrases; all literal registered response IDs represented",
    )


validate_example("commander_hayes", ["comandr", "starbas"], ["commander", "starbase"])
validate_example("melnorme", ["melnorm"], ["melnorme"])
print(
    len(query_names),
    "queries;",
    len(action_names),
    "actions;",
    len(manifest["mappings"]),
    "qualified legacy mappings;",
    sum(expected.values()),
    "classified call sites",
)
