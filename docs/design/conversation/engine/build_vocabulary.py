from json_format import dumps
import re, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))
from conversation_catalog import active_source, PREFIX, TYPES, Calls, expression
from pycparser import c_ast, c_parser

D = Path(__file__).resolve().parent
ROOT = D.parents[3]
s = (D / "verb_factory.cpp").read_text()


def specs(section):
    text = re.search(
        r"const std::vector<Spec>\s+" + re.escape(section) + r"\s*=\s*\{(.*?)\};",
        s,
        re.S,
    )[1]
    return [
        (n, int(a), int(b))
        for n, a, b in re.findall(r'\{\s*"([^"]+)"\s*,\s*(\d+)\s*,\s*(\d+)\s*\}', text)
    ]


qargs = {
    "fleet.contains": ["race"],
    "fleet.slots": ["race"],
    "fleet.count": ["race_or_all"],
    "fleet.value": [],
    "galaxy.relationship": ["race"],
    "galaxy.available": ["race"],
    "inventory.has": ["item"],
    "inventory.quantity": ["resource"],
    "ship.fuel": [],
    "ship.fuel_capacity": [],
    "ship.free_fuel": [],
    "currency.balance": ["currency"],
    "ship.equipment_count": ["equipment"],
    "ship.salvage_value": [],
    "ship.capability": ["profile"],
    "technology.owned": ["technology"],
    "market.available": ["merchant", "category"],
    "calendar.reached": ["saved_date", "months", "days", "inclusive"],
    "report.available": ["report"],
    "encounter.read": ["field"],
    "location.distance": ["location"],
}
aargs = {
    "fleet.give": ["race [, count] OR race, captain, crew"],
    "fleet.take": ["race [, count]"],
    "fleet.set_crew": ["ship_id", "crew", "captain"],
    "inventory.transfer": ["item [, signed_quantity]"],
    "ship.adjust": ["property", "signed_quantity"],
    "ship.configure": ["profile", "configuration"],
    "currency.adjust": ["currency", "signed_amount"],
    "trade.purchase": ["merchant", "offer", "quantity_or_fill"],
    "trade.sell": ["merchant", "category", "quantity_or_all"],
    "rescue.prepare": ["merchant"],
    "rescue.accept": ["quote_token"],
    "ship.strip": ["policy"],
    "technology.grant": ["technology"],
    "galaxy.set_relationship": ["race", "relationship"],
    "galaxy.reveal": ["race"],
    "calendar.schedule": ["event", "months", "days", "years"],
    "calendar.remember": ["date_key"],
    "report.prepare": ["report", "parameters"],
    "encounter.set_outcome": ["outcome"],
    "random.draw": ["exclusive_upper_bound"],
}
qmethod = [
    "hasShip",
    "availableSlots",
    "shipCount",
    "fleetValue",
    "relationship",
    "raceAvailable",
    "hasItem",
    "quantity",
    "fuel",
    "fuelCapacity",
    "balance",
    "equipmentCount",
    "salvageValue",
    "capability",
    "hasTechnology",
    "marketAvailable",
    "dateReached",
    "reportAvailable",
    "encounter",
    "distanceTo",
    "fuelCapacity - fuel",
]
amethod = [
    "createShip / giveShips",
    "removeShips",
    "setShipCrew",
    "transferItem",
    "adjustShip",
    "configureShip",
    "adjustCurrency",
    "purchase",
    "sell",
    "prepareRescue",
    "acceptRescue",
    "stripShip",
    "grantTechnology",
    "setRelationship",
    "revealRace",
    "scheduleEvent",
    "rememberDate",
    "prepareReport",
    "setOutcome",
    "randomBelow",
]
assert len(specs("queries")) == len(qmethod) == len(qargs)
assert len(specs("actions")) == len(amethod) == len(aargs)
registry = {
    "version": 1,
    "queries": [],
    "actions": [],
    "values": [
        "get",
        "literal",
        "query",
        "add",
        "subtract",
        "multiply",
        "divide",
        "mod",
        "min",
        "max",
        "bit_and",
    ],
    "predicates": ["eq", "ne", "gt", "ge", "lt", "le", "all", "any", "not"],
}
for (n, lo, hi), method in zip(specs("queries"), qmethod):
    registry["queries"].append(
        {
            "verb": n,
            "min_args": lo,
            "max_args": hi,
            "arguments": qargs[n],
            "state_accessor": method,
            "effect": "read_only",
        }
    )
for n, lo, hi, args in [
    ("state.set", 3, 3, ["scope", "key", "value"]),
    ("state.add", 3, 3, ["scope", "key", "signed_delta"]),
    ("state.bits", 4, 4, ["scope", "key", "set_or_clear", "mask"]),
    (
        "sequence.next",
        4,
        5,
        ["scope", "key", "items", "repeat_last_or_cycle", "optional_base"],
    ),
]:
    registry["actions"].append(
        {
            "verb": n,
            "min_args": lo,
            "max_args": hi,
            "arguments": args,
            "state_accessor": "set / local memory",
            "effect": "mutating",
        }
    )
for (n, lo, hi), method in zip(specs("actions"), amethod):
    registry["actions"].append(
        {
            "verb": n,
            "min_args": lo,
            "max_args": hi,
            "arguments": aargs[n],
            "state_accessor": method,
            "effect": "mutating",
        }
    )
(D / "verbs.json").write_text(dumps(registry))
external = {
    "AddEscortShips": ("action", ["fleet.give"]),
    "AddEvent": ("action", ["calendar.schedule"]),
    "AlienTalkSegue": ("presentation", ["speech boundary / wait cue"]),
    "AnyButtonPress": ("presentation", ["input skip policy"]),
    "CalculateEscortsWorth": ("query", ["fleet.value"]),
    "CheckAlliance": ("query", ["galaxy.relationship"]),
    "ClearSISRect": ("presentation", ["view refresh"]),
    "DISABLE_PHRASE": ("action", ["state.set (local topic availability)"]),
    "DeltaSISGauges": ("action", ["ship.adjust", "currency.adjust"]),
    "DoResponsePhrase": ("graph", ["response edge / composed text"]),
    "DrawCargoStrings": ("presentation", ["sale progress cue"]),
    "DrawLanders": ("presentation", ["view refresh"]),
    "DrawStatusMessage": ("presentation", ["status display cue"]),
    "EnableTalkingAnim": ("presentation", ["talk animation cue"]),
    "EscortFeasibilityStudy": ("query", ["fleet.slots"]),
    "GET_GAME_STATE": ("query", ["get (story)"]),
    "GET_GAME_STATE_32": ("query", ["get (story)"]),
    "GLOBAL": ("state projection", ["get / validated state action"]),
    "GLOBAL_SIS": (
        "state projection",
        ["get / ship.adjust / ship.configure / inventory.transfer"],
    ),
    "GetAllianceName": ("presentation", ["text insertion / formatter"]),
    "GetColorMapAddress": ("presentation", ["palette cue"]),
    "GetConversationData": ("presentation", ["composite / palette / text layout"]),
    "GetConversationPhraseBuffer": ("graph", ["immutable text composition"]),
    "GetHeadLink": ("query", ["fleet.count / encounter.read"]),
    "GetPlayerFleetQueue": ("query", ["fleet.count"]),
    "GetPlayerSISState": ("state projection", ["get / ship.strip / rescue.accept"]),
    "GetPlayerTravelPosition": (
        "state projection",
        ["location.distance / get / validated state.set"],
    ),
    "GetTimeCounter": ("presentation", ["playback / progress timing"]),
    "HIBYTE": ("encoding", ["decoded state value"]),
    "HaveEscortShip": ("query", ["fleet.contains"]),
    "LOBYTE": ("encoding", ["decoded state / encounter.read"]),
    "LOGX_TO_UNIVERSE": ("implementation", ["location units adapter"]),
    "LOGY_TO_UNIVERSE": ("implementation", ["location units adapter"]),
    "LOWORD": ("encoding", ["decoded state value"]),
    "MAKE_WORD": ("encoding", ["decoded state value"]),
    "NPCNumber": ("presentation", ["numeric speech segment"]),
    "NPCPhrase": ("presentation", ["dialogue speech"]),
    "NPCPhrase_cb": ("presentation", ["speech segment cue"]),
    "NPCPhrase_splice": ("presentation", ["speech composition"]),
    "PHRASE_ENABLED": ("query", ["get (local topic availability)"]),
    "PLAYER_SAID": ("graph", ["destination edge"]),
    "RandomContext_Random": ("implementation", ["rescue.prepare seeded proposal"]),
    "RandomContext_SeedRandom": ("implementation", ["rescue.prepare seeded proposal"]),
    "RemoveEscortShips": ("action", ["fleet.take"]),
    "Response": ("graph", ["response edge"]),
    "SET_GAME_STATE": ("action", ["state.set / state.add / state.bits"]),
    "SET_GAME_STATE_32": ("action", ["state.set / state.bits"]),
    "SetAbsColorMapIndex": ("presentation", ["palette cue"]),
    "SetCommIntroMode": ("presentation", ["entry transition cue"]),
    "SetEscortCrewComplement": (
        "action",
        ["fleet.set_crew / fleet.give explicit overload"],
    ),
    "SetRaceAllied": ("action", ["galaxy.set_relationship"]),
    "SetStatusMessageMode": ("presentation", ["status display binding"]),
    "ShowRemainingCapacity": ("presentation", ["sale progress cue"]),
    "SleepThread": ("presentation", ["playback / progress timing"]),
    "SleepThreadUntil": ("presentation", ["playback / progress timing"]),
    "StartSphereTracking": (
        "action",
        ["galaxy.reveal (separate from galaxy.available)"],
    ),
    "TFB_Random": ("action", ["random.draw"]),
    "TaskSwitch": ("presentation", ["backend scheduling"]),
    "UNIVERSE_TO_LOGX": ("implementation", ["location units adapter"]),
    "UNIVERSE_TO_LOGY": ("implementation", ["location units adapter"]),
    "XFormColorMap": ("presentation", ["palette transition cue"]),
    "abs": ("implementation", ["distance / domain arithmetic"]),
    "assert": ("implementation", ["loader / domain invariant"]),
    "construct_response": ("presentation", ["text composition"]),
    "getSegue": ("query", ["encounter.read"]),
    "memset": ("implementation", ["owned local container initialization"]),
    "setSegue": ("graph/action", ["end outcome / encounter.set_outcome"]),
    "square_root": ("implementation", ["location.distance / rescue.prepare"]),
}
helper = {
    "AnalyzeCondition": ("action", ["report.prepare (hayes.flagship_analysis)"]),
    "AnyInfoLeftToSell": ("query", ["market.available"]),
    "CheckBulletins": (
        "action",
        ["report.prepare (hayes.arrival / repeat_bulletins / departure)"],
    ),
    "CheckTiming": ("query", ["calendar.reached"]),
    "DeltaCredit": ("action", ["currency.adjust / trade.purchase"]),
    "DiscussDevices": (
        "query/action split",
        ["report.available", "report.prepare (hayes.devices)"],
    ),
    "DoTransaction": ("action", ["trade.purchase / trade.sell"]),
    "GetAvailableCredits": ("query", ["currency.balance"]),
    "GetCultureName": ("presentation", ["text formatter"]),
    "GetLineSafe": ("data", ["sequence table / clamped cursor"]),
    "GetNextTechForSale": ("query", ["market.available / next-offer report"]),
    "GetNumLines": ("data", ["sequence table size"]),
    "GetShofixtiName": ("presentation", ["text insertion"]),
    "GetStripModuleRef": ("presentation", ["rescue offer equipment labels"]),
    "GetTechData": ("data", ["technology catalog lookup"]),
    "GrantStateTech": ("action", ["technology.grant"]),
    "GrantTech": ("action", ["technology.grant"]),
    "HasStateTech": ("query", ["technology.owned"]),
    "HasTech": ("query", ["technology.owned"]),
    "LikeYouPhrase": ("presentation", ["text formatter"]),
    "PigLatinPhrase": ("presentation", ["text formatter"]),
    "PolitePhrase": ("presentation", ["text formatter"]),
    "RhymePhrase": ("presentation", ["text formatter"]),
    "MindControlStrobe": ("presentation", ["one-shot composite cue"]),
    "PrepareShip": (
        "graph",
        ["fleet.give action -> result decision -> calendar state actions"],
    ),
    "SelectAlienZOQ": ("presentation", ["speaker/composite cue"]),
    "SetAvailableCredits": ("action", ["state.set / currency.adjust"]),
    "ShipWorth": ("query", ["ship.salvage_value"]),
    "ShipsReady": ("query", ["story condition AND calendar.reached"]),
    "StripShip": ("action", ["rescue.prepare / rescue.accept / ship.strip"]),
    "ZFPTalkSegue": ("presentation", ["speaker/composite sequence"]),
    "getStripRandomSeed": ("implementation", ["rescue.prepare deterministic seed"]),
    "sayCoord": ("presentation", ["coordinate speech segments"]),
}
rows = []
for p in sorted((ROOT / "sc2/src/uqm/comm").glob("*/*.c")):
    clean, _ = active_source(p.read_text())
    ast = c_parser.CParser().parse(PREFIX + clean)
    functions = {f.decl.name for f in ast.ext if isinstance(f, c_ast.FuncDef)}
    for f in ast.ext:
        if not isinstance(f, c_ast.FuncDef):
            continue
        calls = Calls()
        calls.visit(f.body)
        for call in calls.nodes:
            name = expression(call.name)
            line = call.coord.line - len(TYPES)
            if name in functions:
                kind, targets = helper.get(
                    name,
                    (
                        "graph",
                        ["handler subgraph; expand dialogue/decision/action nodes"],
                    ),
                )
                if name == "AlienRaces" and p.parent.name == "melnorm":
                    kind, targets = "action", [
                        "trade.purchase (alien_races information)"
                    ]
                if name in ("CurrentEvents", "History") and p.parent.name == "melnorm":
                    kind, targets = "action", ["trade.purchase (information category)"]
            elif name.startswith("*stack_func["):
                kind, targets = "graph", [
                    "computed ordering of first-meeting topic responses"
                ]
            else:
                if name not in external:
                    raise ValueError("unclassified " + name)
                kind, targets = external[name]
            rows.append(
                {
                    "script": p.parent.name,
                    "handler": f.decl.name,
                    "call": name,
                    "line": line,
                    "kind": kind,
                    "targets": targets,
                }
            )
unique = {}
for row in rows:
    k = row["script"] + "::" + row["call"]
    entry = unique.setdefault(
        k, {key: row[key] for key in ("script", "call", "kind", "targets")}
    )
    entry.setdefault("sites", []).append(
        {"handler": row["handler"], "line": row["line"]}
    )
mapping = {
    "version": 1,
    "scope": "All active function-call expressions across all 27 conversation scripts; direct record assignments additionally use state actions or presentation fields.",
    "occurrences": len(rows),
    "mappings": list(unique.values()),
}
(D / "legacy-verbs.json").write_text(dumps(mapping))
print(
    len(registry["queries"]),
    "queries",
    len(registry["actions"]),
    "actions",
    len(unique),
    "qualified legacy mappings",
    len(rows),
    "call sites",
)
