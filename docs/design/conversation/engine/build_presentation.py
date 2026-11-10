#!/usr/bin/env python3
"""Extract the existing Hayes/Melnorme sprite-layer defaults into design data."""

from fractions import Fraction
import json
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(ROOT / "tools"))
from conversation_catalog import active_source, PREFIX, expression
from pycparser import c_parser, c_ast


def number(n):
    if isinstance(n, c_ast.Constant):
        return Fraction(int(n.value, 0))
    if isinstance(n, c_ast.ID) and n.name == "ONE_SECOND":
        return Fraction(1)
    if isinstance(n, c_ast.BinaryOp):
        a, b = number(n.left), number(n.right)
        if n.op == "/":
            return a / b
        if n.op == "*":
            return a * b
        if n.op == "+":
            return a + b
        if n.op == "<<":
            return Fraction(int(a) << int(b))
        if n.op == "|":
            return Fraction(int(a) | int(b))
    raise ValueError(expression(n))


def seconds(n):
    v = number(n)
    return [v.numerator, v.denominator]


def layer(n):
    a = n.exprs
    flags = expression(a[2]).replace("(", "").replace(")", "").split("|")
    return {
        "first_frame": int(number(a[0])),
        "frame_count": int(number(a[1])),
        "flags": [
            x.strip().lower().replace("_anim", "") for x in flags if x.strip() != "0"
        ],
        "frame_seconds": {"base": seconds(a[3]), "random_range": seconds(a[4])},
        "restart_seconds": {"base": seconds(a[5]), "random_range": seconds(a[6])},
        "blocks_layers": [i for i in range(32) if int(number(a[7])) & (1 << i)],
    }


def composite(path):
    clean, _ = active_source(path.read_text())
    ast = c_parser.CParser().parse(PREFIX + clean)
    desc = next(
        d
        for d in ast.ext
        if isinstance(d, c_ast.Decl) and d.name and d.name.endswith("_desc") and d.init
    )
    a = desc.init.exprs
    return {
        "_source": str(path.relative_to(ROOT)),
        "ambient": [layer(x) for x in a[17].exprs],
        "transition": layer(a[18]),
        "talk": layer(a[19]),
    }


data = {
    "version": 1,
    "_status": "Presentation data and service contract; SDL compositor/audio backend not implemented by this prototype.",
    "time_units": "rational seconds [numerator, denominator]; backend preserves legacy base-plus-random-range semantics",
    "composites": {
        "hayes.first_contact": composite(ROOT / "sc2/src/uqm/comm/comandr/comandr.c"),
        "hayes.starbase": composite(ROOT / "sc2/src/uqm/comm/starbas/starbas.c"),
        "melnorme": composite(ROOT / "sc2/src/uqm/comm/melnorm/melnorm.c"),
    },
    "profiles": {},
}
for key, base, palette, music in [
    ("hayes.normal", "hayes.first_contact", 0, "comm.commander.music"),
    ("hayes.low_power", "hayes.first_contact", 1, "comm.commander.lowpower.music"),
    ("hayes.power_restored", "hayes.first_contact", 0, "continue"),
    ("hayes.starbase", "hayes.starbase", 0, "comm.starbase.music"),
    ("melnorme.yellow", "melnorme", 0, "comm.melnorme.music"),
    ("melnorme.purple", "melnorme", 1, "continue"),
    ("melnorme.blue", "melnorme", 2, "continue"),
]:
    hayes = key.startswith("hayes")
    data["profiles"][key] = {
        "composite": base,
        "graphics": "comm.commander.graphics" if hayes else "comm.melnorme.graphics",
        "font": "comm.commander.font" if hayes else "comm.melnorme.font",
        "palette_resource": (
            "comm.commander.colortable" if hayes else "comm.melnorme.colortable"
        ),
        "palette_index": palette,
        "palette_transition_seconds": [1, 2],
        "music": music,
        "music_policy": "continue_if_same",
        "voice_policy": "optional; use phrase voice metadata when installed",
        "text_layout": (
            {"width": 143, "baseline": [164, 20]}
            if hayes
            else {"width": "SIS_TEXT_WIDTH - 16", "alignment": "center"}
        ),
        "disabled_ambient_layers": [2] if key == "hayes.power_restored" else [],
    }
(HERE / "presentation.json").write_text(json.dumps(data, indent=2) + "\n")
print("Extracted 3 composites /", len(data["profiles"]), "presentation profiles")
