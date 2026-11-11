#!/usr/bin/env python3
"""Generate the checked data-name index for the existing packed save fields."""

from pathlib import Path
import argparse
import re

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--check", action="store_true")
args = parser.parse_args()
fields = re.findall(
    r"ADD_GAME_STATE\s*\(\s*(\w+)\s*,\s*(\d+)\s*\)",
    (ROOT / "sc2/src/uqm/globdata.h").read_text(),
)
assert len({name for name, _ in fields}) == len(fields)
output = "// Generated from globdata.h by tools/story_field_names.py.\n" + "".join(
    '{"' + name + '", {' + name + ", END_" + name + "}},\n"
    for name, width in sorted(fields)
)
path = ROOT / "sc2/src/uqm/story_field_names.inc"
if args.check:
    assert path.read_text() == output, "story name index is stale"
else:
    path.write_text(output)
print(len(fields), "checked story field names")
