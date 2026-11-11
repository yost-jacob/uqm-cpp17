# Editing the star catalogue

`sc2/content/base/starmap.json` is the runtime star catalogue. Edits are loaded
when a new campaign session is created; no executable rebuild is needed. It is
loaded through the game's content filesystem, so `--contentdir` and mounted addon
overrides still apply.

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
  502 and 519 are inserted by the C adapter and do not appear in JSON.
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

`StarMap` owns `std::vector<StarDesc>` containing the 518 real stars only.
Each star owns unpacked properties, a typed `StarId` and `StarSpace`, a position,
and its prefix/postfix/composed name as `std::string`. Nothing borrows parser or
game-string storage. The public C++ header includes only standard C++ headers.

C++ lookup uses `const StarDesc& at(StarId)`, which throws `std::out_of_range`
for unknown IDs. Access is read-only. Searches return an owned
`std::vector<StarId>`, never raw pointers, null results or pointer cursors:

```cpp
const auto matches = map.find({
    StarSpace::HyperSpace, {1752, 1450}, 5, 5
});
for (StarId id : matches) {
    const StarDesc& star = map.at(id);
    // Use star.name(), star.position(), star.properties(), ...
}
```

A search radius of zero requires an exact coordinate. `std::nullopt` means an
unbounded axis; negative C++ radii throw `std::invalid_argument`. An empty vector
means no match. IDs in search results remain ordinary values after the map is
destroyed. Star/name references require the owner to remain alive. The map
cannot be copied or moved and never resizes its published storage.

The campaign session's `GameDefinitions` owns the native map and compatibility
buffer of 520 plain `STAR_DESC` records. Arcade and frontend sessions do not load
a starmap. `star_map_c.cpp` borrows the active session's definitions.
Only this adapter packs enum fields, inserts the two sentinels, maps C pointers
and legacy indices, translates negative bounds/boolean space selection and
copies strings into caller-owned buffers. It translates the C "last result"
pointer to the next matching ID without exposing that convention on `StarMap`.
C buffers are read-only snapshots; they do not own or mutate native star data.
The extra in-memory catalogue index is never serialized; the eight saved fields
remain unchanged. Names are borrowed through `.c_str()` only at this boundary.

Searching scans the small native catalogue in ID order, avoiding another sorted
index and permitting coordinate edits without changing persistent identities.
The existing C `FindStar` wrapper selects the space from campaign state.

The current star is a player-owned `std::optional<StarId>` in navigation state.
C callers use `GetCurrentStar()` / `SetCurrentStar()` to translate that identity
at the boundary. No process-wide current-star pointer remains. Save/load still
uses the existing descriptor projection and restores the selection by identity.
See [session definitions](session-definitions.md) for loading/lifetime contracts.
