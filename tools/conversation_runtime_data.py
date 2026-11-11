#!/usr/bin/env python3
"""Refresh indexed text metadata while preserving authored live conversation graphs.

Development only: source phrase indexing uses pycparser; the game never does.
"""

import argparse
import json
from pathlib import Path
import re
import sys
from conversation_catalog import ROOT, active_source, mask, phrases

sys.path.insert(0, str(ROOT / "docs/design/conversation/engine"))
from json_format import dumps

OUTPUT = ROOT / "sc2/content/base/conversations"


def fingerprint(text):
    value = 14695981039346656037
    for byte in text.encode("utf-8"):
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return value


def definition(script, resource, bank, symbols):
    return {
        "id": script, "resource": resource,
        "phrases": [{"id": name, "index": index, "text": bank[name]["text"],
                     "voice": bank[name]["voice"] or None,
                     "original_text_hash": fingerprint(bank[name]["text"])}
                    for index, name in enumerate(symbols)]
    }


def generate(check=False):
    resources = dict(
        re.findall(
            r"^(\S+)\s*=\s*CONVERSATION:([^:\n]+)",
            (ROOT / "sc2/content/uqm.rmp").read_text(),
            re.M,
        )
    )
    macros = {}
    for p in (ROOT / "sc2/src/uqm/comm").glob("*/resinst.h"):
        macros.update(re.findall(r'#define\s+(\w+)\s+"([^"]+)"', p.read_text()))
    count = total = 0
    seen = set()
    for path in sorted((ROOT / "sc2/src/uqm/comm").glob("*/*.c")):
        source = active_source(path.read_text())[0]
        # The resource ID is the descriptor's conversation phrase field.
        macro = re.search(r"(\w+CONVERSATION_PHRASES)\s*,", source)[1]
        resource = macros[macro]
        bank = phrases(ROOT / "sc2/content" / resources[resource])
        header = mask((path.parent / "strings.h").read_text())
        enum = active_source(re.search(r"enum\s*\{(.*?)\}", header, re.S)[1])[0]
        symbols = [x.strip() for x in enum.split(",") if x.strip()]
        assert symbols.pop(0) == "NULL_PHRASE"
        assert all(re.fullmatch(r"\w+", name) for name in symbols), path
        assert set(symbols) == set(bank), (path, set(symbols) ^ set(bank))
        output = OUTPUT / (resource + ".json")
        data = definition(path.parent.name, resource, bank, symbols)
        # Migrated JSON is the source of truth. Refresh indexed text metadata,
        # but never regenerate its story graph from another programming language.
        if not output.exists():
            raise ValueError(f"missing authored conversation graph: {output}")
        if output.exists():
            previous = json.loads(output.read_text())
            if previous.get("_migration", {}).get("status") != "graph_with_native_media":
                raise ValueError(f"conversation is not graph-owned: {output}")
            previous["phrases"] = data["phrases"]
            data = previous
            data["node_count"] = len(data["nodes"]) + 2 * sum(
                "action" in edge
                for node in data["nodes"]
                for edge in node.get("responses", [])
            )
        encoded = dumps(data)
        if check:
            assert (
                json.loads(output.read_text()) == data
            ), f"stale source-generated definition: {output}"
        else:
            OUTPUT.mkdir(parents=True, exist_ok=True)
            output.write_text(encoded)
        assert resource not in seen
        seen.add(resource)
        count += 1
        total += len(symbols)
    print(
        f"{count} authored live graphs; {total} indexed source phrases; all story turns are graph-owned"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    generate(parser.parse_args().check)
