# Editing the star catalogue

`sc2/content/base/starmap.json` is the runtime star catalogue. Edit it and restart
the game; no executable rebuild is needed. It is loaded through the game's
content filesystem, so `--contentdir` and mounted addon overrides still apply.

The original star catalogue is copyright Paul Reiche and Fred Ford, 1992–2002,
and remains covered by the project's GPL-2.0-or-later license.

## Names

Every record contains its own `prefix` and `postfix` display strings:

```json
{
  "catalogue_index": 0,
  "x": 5007,
  "y": 35,
  "type": "DWARF_STAR",
  "color": "GREEN_BODY",
  "owner": 7,
  "special_system": 0,
  "prefix_id": 6,
  "postfix_id": 74,
  "prefix": "Zeta",
  "postfix": "Serpentis"
}
```

This displays **Zeta Serpentis**. Change the strings to rename that star; no
shared string lookup is involved. An empty prefix displays only the postfix.
Stars with the same legacy name IDs can now have different display text.
Name search groups actual postfix text and retains numeric prefix order.
A single-word prefix works best with the existing search box's prefix/cluster
syntax. UTF-8 is supported. The combined name must fit 255 bytes, without NULs,
line breaks or other control characters, because existing search UI buffers are
256 bytes. Smaller display buffers safely truncate names at UTF-8 boundaries.

`quasispace_name` is the text used for a QuasiSpace portal without a selected
catalogue location (originally `UNKNOWN`).

Star names no longer come from `gamestrings.txt`. Its old 133 postfix and 14
prefix slots remain empty reserved entries so later game-string enum offsets,
and existing resources using those offsets, do not shift. Other game text stays
in that resource file. Language addons that rename stars should now provide an
override of `base/starmap.json`.

## Stable identities and properties

- Keep the two sections: 502 `hyperspace` records and 16 `quasispace` records.
- Keep record order and `catalogue_index` values unchanged. These identities
  index visited-star flags, scan records, encounter groups and portal endpoints.
- HyperSpace indices are 0–501; QuasiSpace indices are 503–518. Sentinel slots
  502 and 519 are inserted by the loader and do not appear in JSON.
- `x` and `y` are integers from 0 through 9999 in tenths of a map coordinate
  (1752 means 175.2). Coordinate edits need no reordering, but change procedural
  generation seeds and can affect compatibility with saves located at old coordinates.
- `type` accepts `DWARF_STAR`, `GIANT_STAR` or `SUPER_GIANT_STAR`.
- `color` accepts `BLUE_BODY`, `GREEN_BODY`, `ORANGE_BODY`, `RED_BODY`,
  `WHITE_BODY` or `YELLOW_BODY`.
- `special_system` preserves the numeric enum in `uqm/gendef.h`; zero selects
  default generation. Preserve it unless deliberately changing system behavior.
- `owner` preserves the existing three packed low bits (normally 7).
- `prefix_id` (0–14) and `postfix_id` (0–132) retain the old descriptor/save
  metadata. Leave them unchanged when renaming stars. They are not string indices
  used to fetch the new display text.

The loader validates the schema version, counts, positions, enum values, identity
order and name limits. Startup errors identify the file and record, or the JSON
parser's syntax error location. It never silently falls back to compiled data.

## Ownership and the C transition

`StarMap` owns `std::vector<StarDesc>`. Each `StarDesc` owns its prefix, postfix
and composed display name as `std::string`; it also contains the plain numeric
`STAR_DESC` used by remaining C gameplay code. Nothing borrows text from the JSON
parser or game-string resource.

The vector is populated before publication and never resized. `StarMap` cannot
be copied or moved. Borrowed descriptors and text remain valid until shutdown.
The C interface uses `GetStar`, `GetStarIndex` and name queries rather than raw
array arithmetic, so the C descriptor's layout need not match the C++ vector's
stride. Its additional in-memory catalogue identity is never serialized; the
existing eight saved star fields remain unchanged.

The class searches in catalogue order using `std::find_if` over the selected
space (at most 502 records). This avoids a second sorted index and allows edits
to coordinates without changing persistent identities. The existing C `FindStar`
wrapper selects HyperSpace or QuasiSpace from campaign state.
