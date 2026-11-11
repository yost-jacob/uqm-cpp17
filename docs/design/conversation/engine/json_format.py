"""Readable JSON for design data: keep records expanded and short expressions inline."""

import json


def dumps(value, width=120):
    """Preserve values and order; width is a soft limit for indivisible strings.

    Nodes, responses, phrase records and verb records keep their fields on separate
    lines. Short expression trees and argument lists fit on one line when possible.
    """

    def render(item, depth):
        compact = json.dumps(item, ensure_ascii=False)
        record = isinstance(item, dict) and any(
            key in item for key in ("id", "kind", "text", "verb", "action", "when", "speech")
        )
        if not isinstance(item, (dict, list)) or not item:
            return compact
        if not record and depth * 2 + len(compact) <= width:
            return compact
        indent = "  " * (depth + 1)
        if isinstance(item, dict):
            parts = [
                indent
                + json.dumps(key, ensure_ascii=False)
                + ": "
                + render(child, depth + 1)
                for key, child in item.items()
            ]
            opening, closing = "{", "}"
        else:
            parts = [indent + render(child, depth + 1) for child in item]
            opening, closing = "[", "]"
        return opening + "\n" + ",\n".join(parts) + "\n" + "  " * depth + closing

    return render(value, 0) + "\n"
