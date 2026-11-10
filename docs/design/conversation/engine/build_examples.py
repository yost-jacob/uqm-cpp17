#!/usr/bin/env python3
"""Author the reviewable design graphs; not a mechanical C-to-JSON translator."""

import json
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(ROOT / "tools"))
from conversation_catalog import phrases


def get(scope, key, default=0):
    return {"get": [scope, key, default]}


def flag(key):
    return get("story", key, False)


def local(key, default=0):
    return get("local", key, default)


def query(name, *args):
    return {"query": name, "args": list(args)}


def eq(a, b):
    return {"eq": [a, b]}


def ne(a, b):
    return {"ne": [a, b]}


def gt(a, b):
    return {"gt": [a, b]}


def ge(a, b):
    return {"ge": [a, b]}


def lt(a, b):
    return {"lt": [a, b]}


def both(*xs):
    return {"all": list(xs)}


def anyof(*xs):
    return {"any": list(xs)}


def not_(x):
    return {"not": x}


def result(name, field):
    return {"get": ["result", name + "." + field]}


def set_(scope, key, value):
    return ("state.set", [scope, key, value])


class Graph:
    def __init__(self, name, banks, entry):
        self.name = name
        self.nodes = []
        self.ids = set()
        self.prefix = banks[0]
        self.bank = {}
        self.locals = {}
        self.contracts = {}
        for bank in banks:
            for symbol, entrydata in phrases(
                ROOT / f"sc2/content/base/comm/{bank}/{bank}.txt"
            ).items():
                self.bank[f"{bank}.{symbol}"] = {
                    "text": entrydata["text"],
                    "voice": entrydata["voice"] or None,
                }
        self.entry = entry

    def p(self, name, bank=None):
        if isinstance(name, dict):
            return name
        if name in self.bank:
            return name
        return (bank or self.prefix) + "." + name

    def node(self, id, kind, **fields):
        if id in self.ids:
            raise ValueError("duplicate " + id)
        self.ids.add(id)
        self.nodes.append(dict(id=id, kind=kind, **fields))
        return id

    def edge(self, id, next, when=True, bank=None):
        return dict(id=id, text=self.p(id, bank), when=when, next=next)

    def say(
        self,
        id,
        lines,
        next=None,
        responses=None,
        variants=None,
        bank=None,
        profile=None,
    ):
        if variants is None:
            speech = (
                lines
                if isinstance(lines, dict)
                else [self.p(x, bank) if isinstance(x, str) else x for x in lines]
            )
            variants = [{"speech": speech}]
        for v in variants:
            v.setdefault(
                "presentation",
                {
                    "composite": profile or "continue",
                    "music": "continue",
                    "voice": "if_available",
                },
            )
        data = {"variants": variants}
        if responses is not None:
            data["responses"] = responses
        if next is not None:
            data["next"] = next
        return self.node(id, "dialogue", **data)

    def decide(self, id, cases, otherwise):
        return self.node(
            id,
            "decision",
            branches=[{"when": c, "next": n} for c, n in cases],
            otherwise=otherwise,
        )

    def act(self, id, verb, args, next, save=None):
        return self.node(
            id,
            "action",
            action={"verb": verb, "args": args},
            save_as=save or id.replace(".", "_"),
            next=next,
        )

    def chain(self, id, actions, next):
        for i, (verb, args) in reversed(list(enumerate(actions))):
            target = id if i == 0 else id + "." + str(i)
            self.act(target, verb, args, next)
            next = target
        return next

    def reply(self, id, lines, next, actions=(), bank=None, profile=None):
        if actions:
            self.chain(id + ".effects", actions, next)
            next = id + ".effects"
        return self.say(id, lines, next, bank=bank, profile=profile)

    def report(self, id, topic, next, args=None):
        self.contracts[topic] = (
            "Native report contract; returns speech data, never starts playback."
        )
        self.say(id + ".speech", result(id.replace(".", "_"), "speech"), next)
        self.decide(
            id + ".check",
            [(eq(result(id.replace(".", "_"), "code"), "ok"), id + ".speech")],
            "error",
        )
        return self.act(
            id, "report.prepare", [topic, {"literal": args or {}}], id + ".check"
        )

    def end(self, id="end", outcome="peace"):
        return self.node(id, "end", outcome=outcome)

    def write(self):
        self.end("error", "design_error")
        for n in self.nodes:
            refs = []
            if "next" in n:
                refs.append(n["next"])
            refs.extend(e["next"] for e in n.get("responses", []))
            refs.extend(b["next"] for b in n.get("branches", []))
            if "otherwise" in n:
                refs.append(n["otherwise"])
            for ref in refs:
                if ref not in self.ids:
                    raise ValueError(f'{self.name}: {n["id"]} -> {ref}')
        data = {
            "version": 1,
            "id": self.name,
            "entry": self.entry,
            "_status": "Design definition with real phrase banks and explicit native-operation contracts; not a completed gameplay port.",
            "_coverage": {
                "native_reports": self.contracts,
                "known_differences": [
                    "First-contact question ordering is normalized; legacy dynamic buffer formatting becomes presentation templates.",
                    "Timing, bulletin/device eligibility, capability analysis and rescue mechanics require native adapters; no legacy callback execution is hidden in this JSON.",
                    "Flagship strip/sale reports return prepared speech data; legacy per-frame sale animation is a presentation-service contract.",
                ],
            },
            "phrase_banks": {
                b: {
                    "resource": "comm." + b + ".dialogue",
                    "base_text": f"base/comm/{b}/{b}.txt",
                    "voice": "optional addon",
                }
                for b in sorted({key.split(".")[0] for key in self.bank})
            },
            "presentation_library": "../presentation.json",
            "initial_presentation": (
                "hayes.normal" if self.name == "commander_hayes" else "melnorme.yellow"
            ),
            "locals": self.locals,
            "phrases": self.bank,
            "nodes": self.nodes,
        }
        if self.name == "commander_hayes":
            data["ship_assessment"] = {
                "minimum": {
                    "thrusters": 7,
                    "turning_jets": 5,
                    "crew": 50,
                    "fuel_displayed": 50,
                    "storage_bays": 1,
                    "landers": 1,
                    "battery_score": 1,
                    "gun_score": 2,
                },
                "maximum": {
                    "thrusters": 11,
                    "turning_jets": 8,
                    "crew": 150,
                    "fuel_displayed": 150,
                    "landers": 3,
                    "battery_score": 4,
                    "gun_score": 7,
                    "point_defense": 2,
                },
                "scoring": {
                    "gun_weapon": 1,
                    "blaster_weapon": 2,
                    "cannon_weapon": 3,
                    "tracking_bonus_if_armed": 2,
                    "dynamo": 1,
                    "shiva_furnace": 2,
                },
                "advice_thresholds": {
                    "cash_ru": 3000,
                    "allied_nonhuman_races": 2,
                    "adequate_escort_value": 6000,
                    "sa_matra_escort_value": 20000,
                },
            }
        if self.name == "melnorme":
            data["markets"] = {
                "melnorme": {
                    "currency": "melnorme_credit",
                    "fuel": {
                        "unit_price": 1,
                        "unit": "displayed_fuel",
                        "storage_units_per_unit": 100,
                        "quantities": [1, 5, 10, 25, "fill"],
                        "fill_rounding": "ceil",
                        "capacity_comparison": "legacy_whole_units",
                    },
                    "sales": {
                        "biomass": {"credits_per_unit": 2},
                        "rainbows": {"credits_per_new_location": 500},
                    },
                    "technology": technology_catalog(),
                    "information": {
                        "price": 75,
                        "categories": {
                            "events": {
                                "cursor": "MELNORME_EVENTS_INFO_STACK",
                                "speech": [
                                    [self.p(f"OK_BUY_EVENT_{i}")] for i in range(1, 9)
                                ],
                            },
                            "alien_races": {
                                "cursor": "MELNORME_ALIEN_INFO_STACK",
                                "speech": [
                                    [self.p(f"OK_BUY_ALIEN_RACE_{i}")]
                                    for i in range(1, 17)
                                ],
                                "special_effects": {
                                    "14": [
                                        {
                                            "when": not_(flag("FOUND_PLUTO_SPATHI")),
                                            "actions": [
                                                {
                                                    "verb": "state.set",
                                                    "args": [
                                                        "story",
                                                        "KNOW_SPATHI_PASSWORD",
                                                        True,
                                                    ],
                                                },
                                                {
                                                    "verb": "state.set",
                                                    "args": [
                                                        "story",
                                                        "SPATHI_HOME_VISITS",
                                                        7,
                                                    ],
                                                },
                                            ],
                                        }
                                    ],
                                    "15": [
                                        {
                                            "when": lt(
                                                get("story", "KNOW_ABOUT_SHATTERED"), 2
                                            ),
                                            "actions": [
                                                {
                                                    "verb": "state.set",
                                                    "args": [
                                                        "story",
                                                        "KNOW_ABOUT_SHATTERED",
                                                        2,
                                                    ],
                                                }
                                            ],
                                        },
                                        {
                                            "when": True,
                                            "actions": [
                                                {
                                                    "verb": "state.set",
                                                    "args": [
                                                        "story",
                                                        "KNOW_SYREEN_WORLD_SHATTERED",
                                                        True,
                                                    ],
                                                }
                                            ],
                                        },
                                    ],
                                },
                            },
                            "history": {
                                "cursor": "MELNORME_HISTORY_INFO_STACK",
                                "speech": [
                                    [self.p(f"OK_BUY_HISTORY_{i}")]
                                    for i in range(1, 10)
                                ],
                            },
                        },
                    },
                    "rescue": {
                        "destination": "sol",
                        "extra_fuel_units": 2,
                        "seed_policy": "legacy_location_and_equipment",
                        "quote_lifetime": "conversation",
                        "accept_once": True,
                    },
                    "penance": {
                        "minimum_value": 80,
                        "valuation_units": "legacy_module_cost_units",
                        "strip_policy": "preserve_required_crew_pods; clamp_fuel_to_reserve",
                    },
                }
            }
        data["_coverage"][
            "dialogue_action_timing"
        ] = "Some reply effects occur after playback in this draft; audit source enqueue-versus-commit timing before a production port."
        (HERE / "examples" / f"{self.name}.json").write_text(
            json.dumps(data, indent=2, ensure_ascii=False) + "\n"
        )
        print(self.name, len(self.nodes), "nodes", len(self.bank), "phrases")


def hayes():
    g = Graph("commander_hayes", ["commander", "starbase"], "entry")
    g.end()
    g.decide(
        "entry",
        [
            (
                eq(query("encounter.read", "kind"), "commander_first_contact"),
                "first.entry",
            )
        ],
        "starbase.entry",
    )
    # First-contact entry and reactor/radioactive supply branch.
    g.decide(
        "first.entry",
        [
            (flag("PROBE_ILWRATH_ENCOUNTER"), "first.victory"),
            (flag("STARBASE_VISITED"), "first.return"),
        ],
        "first.start",
    )
    g.chain("first.start", [set_("story", "STARBASE_VISITED", True)], "first.greeting")
    g.say(
        "first.greeting",
        ["ARE_YOU_SUPPLY_SHIP"],
        responses=[
            g.edge("no_but_well_help0", "first.help"),
            g.edge("yes_this_is_supply_ship", "first.supply"),
            g.edge("what_slave_planet", "first.slave"),
        ],
        profile="hayes.low_power",
    )
    for id, line in [
        ("help", "THE_WHAT_FROM_WHERE"),
        ("supply", "ABOUT_TIME"),
        ("slave", "DONT_KNOW_WHO_YOU_ARE"),
    ]:
        g.reply("first." + id, [line], "first.radios_check")
    g.decide(
        "first.return",
        [(flag("RADIOACTIVES_PROVIDED"), "first.moon_check")],
        "first.radio_return",
    )
    g.reply(
        "first.radio_return",
        ["DO_YOU_HAVE_RADIO_THIS_TIME"],
        "first.radios_check",
        profile="hayes.low_power",
    )
    g.decide(
        "first.radios_check",
        [(gt(query("inventory.quantity", "radioactive"), 0), "first.radios")],
        "first.no_radios",
    )

    def aid_menu(id, finish, question, reply_to):
        choices = []
        no_lander = both(
            eq(query("inventory.quantity", "landers"), 0),
            lt(get("story", "CHMMR_BOMB_STATE"), 2),
        )
        for response, condition in [
            ("i_lost_my_lander", both(no_lander, not_(flag("LANDERS_LOST")))),
            ("i_lost_another_lander", both(no_lander, flag("LANDERS_LOST"))),
            (
                "need_fuel_mercury",
                both(lt(query("ship.fuel"), 2), not_(flag("GIVEN_FUEL_BEFORE"))),
            ),
            (
                "need_fuel_again",
                both(lt(query("ship.fuel"), 2), flag("GIVEN_FUEL_BEFORE")),
            ),
        ]:
            if id == "first.moon" and response == "need_fuel_mercury":
                response = "need_fuel_luna"
            target = id + "." + response
            choices.append(g.edge(response, target, condition))
            lander = response.startswith("i_lost")
            line = (
                (
                    "HERE_IS_A_NEW_LANDER"
                    if response == "i_lost_my_lander"
                    else "HERE_IS_ANOTHER_LANDER"
                )
                if lander
                else (
                    "GIVE_FUEL_AGAIN" if response == "need_fuel_again" else "GIVE_FUEL"
                )
            )
            actions = (
                [("ship.adjust", ["landers", 1]), ("ship.adjust", ["crew", 4])]
                if lander
                else [("ship.adjust", ["fuel", 5])]
            )
            if response == "i_lost_my_lander":
                actions.append(set_("story", "LANDERS_LOST", True))
            if not lander:
                actions.append(set_("story", "GIVEN_FUEL_BEFORE", True))
            g.reply(target, [line], reply_to, actions)
        choices.extend(finish)
        if question:
            r, line = question
            g.locals[id + ".question"] = True
            choices.append(g.edge(r, id + ".question", local(id + ".question", True)))
            g.reply(
                id + ".question",
                [line],
                reply_to,
                [set_("local", id + ".question", False)],
            )
        return choices

    choices = aid_menu(
        "first.no_radios",
        [g.edge("ok_i_will_get_radios", "first.radios_bye")],
        ("where_can_i_get_radios", "RADIOS_ON_MERCURY"),
        "first.radios_check",
    )
    g.say("first.no_radios", [], responses=choices, profile="hayes.low_power")
    g.reply("first.radios_bye", ["THANKS_FOR_HELPING"], "end")
    g.say(
        "first.radios",
        [],
        responses=[
            g.edge("we_will_transfer_now", "first.restore_power"),
            g.edge("what_will_you_give_us", "first.garbled1"),
            g.edge("before_radios_we_need_info", "first.garbled2"),
        ],
    )
    g.reply("first.garbled1", ["MESSAGE_GARBLED_1"], "first.radios")
    g.reply("first.garbled2", ["MESSAGE_GARBLED_2"], "first.radios")
    g.chain(
        "first.restore_power",
        [set_("story", "RADIOACTIVES_PROVIDED", True)],
        "first.power_speech",
    )
    g.say(
        "first.power_speech",
        ["FUEL_UP0", "FUEL_UP1"],
        "first.reveal",
        profile="hayes.power_restored",
    )
    # Identification and informational topic progression.
    knowledge = [
        "first_give_info",
        "whats_this_starbase",
        "what_about_earth",
        "where_are_urquan",
        "what_was_red_thing",
    ]
    replies = [
        "ASK_AWAY",
        "STARBASE_IS",
        "HAPPENED_TO_EARTH",
        "URQUAN_LEFT",
        "THAT_WAS_PROBE",
    ]
    reveal = []
    for i, (response, line) in enumerate(zip(knowledge, replies)):
        key = "first.topic." + response
        g.locals[key] = True
        guard = both(
            local(key, True),
            *(not_(local("first.topic." + prior, True)) for prior in knowledge[:i]),
        )
        reveal.append(g.edge(response, "first.topic." + response, guard))
        target = (
            "first.probe_menu" if response == "what_was_red_thing" else "first.reveal"
        )
        g.reply("first.topic." + response, [line], target, [set_("local", key, False)])
    g.locals["first.identify"] = 0
    reveal.extend(
        [
            g.edge(
                "we_are_vindicator0",
                "first.identification",
                eq(local("first.identify"), 0),
            ),
            g.edge(
                "our_mission_was_secret", "first.secret", eq(local("first.identify"), 1)
            ),
            g.edge(
                "we_are_here_to_help", "first.moon_help", ge(local("first.identify"), 2)
            ),
        ]
    )
    g.say("first.reveal", [], responses=reveal)
    g.reply(
        "first.identification",
        ["THATS_IMPOSSIBLE"],
        "first.reveal",
        [set_("local", "first.identify", 1)],
    )
    g.reply(
        "first.secret",
        ["ACKNOWLEDGE_SECRET"],
        "first.reveal",
        [set_("local", "first.identify", 2)],
    )
    g.say(
        "first.probe_menu",
        [],
        responses=[
            g.edge(r, "first.probe." + r)
            for r in ["it_went_away", "we_destroyed_it", "what_probe"]
        ],
    )
    for r, line in [
        ("it_went_away", "DEEP_TROUBLE"),
        ("we_destroyed_it", "GOOD_NEWS"),
        ("what_probe", "SURE_HOPE"),
    ]:
        g.reply("first.probe." + r, [line], "first.reveal")
    g.decide(
        "first.moon_check",
        [(flag("MOONBASE_DESTROYED"), "first.base_destroyed")],
        "first.moon_greeting",
    )
    g.reply("first.moon_greeting", ["DEALT_WITH_BASE_YET"], "first.moon")
    g.reply("first.moon_help", ["BASE_ON_MOON"], "first.moon_check")
    moon = aid_menu("first.moon", [], None, "first.moon")
    moon.extend(
        [
            g.edge(
                "we_will_take_care_of_base",
                "first.promise",
                not_(flag("WILL_DESTROY_BASE")),
            ),
            g.edge(
                "take_care_of_base_again",
                "first.promise_again",
                flag("WILL_DESTROY_BASE"),
            ),
            g.edge(
                "tell_me_about_base",
                "first.base_info",
                eq(get("story", "STARBASE_YACK_STACK1"), 0),
            ),
            g.edge(
                "tell_me_again",
                "first.base_info_again",
                ne(get("story", "STARBASE_YACK_STACK1"), 0),
            ),
        ]
    )
    g.say("first.moon", [], responses=moon)
    g.reply(
        "first.promise",
        ["GOOD_LUCK_WITH_BASE"],
        "end",
        [set_("story", "WILL_DESTROY_BASE", True)],
    )
    g.reply("first.promise_again", ["GOOD_LUCK_AGAIN"], "end")
    g.reply(
        "first.base_info",
        ["ABOUT_BASE"],
        "first.moon",
        [set_("story", "STARBASE_YACK_STACK1", 1)],
    )
    g.reply("first.base_info_again", ["ABOUT_BASE_AGAIN"], "first.moon")
    g.say(
        "first.base_destroyed",
        ["DEALT_WITH_BASE_YET"],
        responses=[
            g.edge("base_was_abandoned", "first.abandoned"),
            g.edge("we_fought_them", "first.claim_fight"),
        ],
    )
    g.say(
        "first.claim_fight",
        ["YOU_REALLY_FOUGHT_BASE"],
        responses=[
            g.edge(r, "first.claim." + r)
            for r in [
                "oh_yes_big_fight",
                "i_lied_it_was_abandoned",
                "i_cant_talk_about_it",
            ]
        ],
    )
    for r, line in [
        ("oh_yes_big_fight", "IM_GLAD_YOU_WON"),
        ("i_lied_it_was_abandoned", "IT_WAS_ABANDONED"),
        ("i_cant_talk_about_it", "IM_SURE_IT_WAS_DIFFICULT"),
    ]:
        g.reply(
            "first.claim." + r,
            [line, "HERE_COMES_ILWRATH"],
            "end",
            [set_("story", "PROBE_ILWRATH_ENCOUNTER", True)],
        )
    g.reply(
        "first.abandoned",
        ["IT_WAS_ABANDONED", "HERE_COMES_ILWRATH"],
        "end",
        [set_("story", "PROBE_ILWRATH_ENCOUNTER", True)],
    )
    g.say(
        "first.victory",
        ["VERY_IMPRESSIVE"],
        responses=[
            g.edge(r, "first.victory." + r)
            for r in [
                "cook_their_butts",
                "overthrow_evil_aliens",
                "annihilate_those_monsters",
            ]
        ],
    )
    for r, line in [
        ("cook_their_butts", "COOK_BUTTS"),
        ("overthrow_evil_aliens", "OVERTHROW_ALIENS"),
        ("annihilate_those_monsters", "KILL_MONSTERS"),
    ]:
        g.reply(
            "first.victory." + r,
            [line],
            "first.alliance_names",
            [
                set_("story", "PROBE_ILWRATH_ENCOUNTER", False),
                set_("story", "STARBASE_AVAILABLE", True),
            ],
        )
    g.say(
        "first.alliance_names",
        ["THIS_MAY_SEEM_SILLY"],
        responses=[
            g.edge(r, "first.name." + r)
            for r in ["name_1", "name_2", "name_3", "name_40"]
        ],
    )
    for i, (r, line) in enumerate(
        [
            ("name_1", "OK_THE_NAFS"),
            ("name_2", "OK_THE_CAN"),
            ("name_3", "OK_THE_UFW"),
            ("name_40", "OK_THE_NAME_IS_EMPIRE0"),
        ]
    ):
        g.reply(
            "first.name." + r,
            [line, "STARBASE_WILL_BE_READY"],
            "end",
            [set_("story", "NEW_ALLIANCE_NAME", i)],
        )
    # Operational starbase. Complex reports are explicit native contracts.
    g.report("starbase.entry", "hayes.arrival", "starbase.menu")
    g.say(
        "starbase.menu",
        [],
        responses=[
            g.edge(
                "have_minerals",
                "starbase.sell",
                gt(query("inventory.quantity", "minerals"), 0),
                "starbase",
            ),
            g.edge(
                "new_devices",
                "starbase.devices",
                query("report.available", "hayes.devices"),
                "starbase",
            ),
            g.edge(
                "repeat_bulletins",
                "starbase.bulletins",
                query("report.available", "hayes.repeat_bulletins"),
                "starbase",
            ),
            g.edge("need_info", "starbase.info", True, "starbase"),
            g.edge("goodbye_commander", "starbase.goodbye", bank="starbase"),
        ],
        bank="starbase",
    )
    for id, topic in [
        ("devices", "hayes.devices"),
        ("bulletins", "hayes.repeat_bulletins"),
    ]:
        g.report("starbase." + id, topic, "starbase.menu")
    g.act(
        "starbase.sell",
        "trade.sell",
        ["starbase", "minerals", "all"],
        "starbase.sell.check",
        save="mineral_sale",
    )
    g.decide(
        "starbase.sell.check",
        [(eq(result("mineral_sale", "code"), "ok"), "starbase.sell.speech")],
        "error",
    )
    g.say(
        "starbase.sell.speech",
        result("mineral_sale", "speech"),
        "starbase.menu",
        bank="starbase",
    )
    g.report("starbase.goodbye", "hayes.departure", "starbase.post")
    g.chain(
        "starbase.post",
        [set_("story", "MOONBASE_ON_SHIP", False)],
        "starbase.bomb_post",
    )
    g.decide(
        "starbase.bomb_post",
        [(eq(get("story", "CHMMR_BOMB_STATE"), 2), "starbase.bomb_done")],
        "end",
    )
    g.chain("starbase.bomb_done", [set_("story", "CHMMR_BOMB_STATE", 3)], "end")
    g.say(
        "starbase.info",
        ["WHAT_KIND_OF_INFO"],
        responses=[
            g.edge("starbase_functions", "info.functions.start", bank="starbase"),
            g.edge("history", "info.history.start", bank="starbase"),
            g.edge("our_mission", "info.mission.start", bank="starbase"),
            g.edge("no_need_info", "info.exit", bank="starbase"),
        ],
        bank="starbase",
    )
    g.reply("info.exit", ["OK_NO_NEED_INFO"], "starbase.menu", bank="starbase")

    def topic_menu(id, prompt, topics, back, back_target, back_line, reset=True):
        choices = []
        for response, line in topics:
            key = id + "." + response
            g.locals[key] = False
            choices.append(
                g.edge(response, key, not_(local(key, False)), bank="starbase")
            )
            g.reply(
                key, [line], id + ".menu", [set_("local", key, True)], bank="starbase"
            )
        choices.append(g.edge(back, id + ".back", bank="starbase"))
        g.say(id + ".menu", [], responses=choices, bank="starbase")
        g.reply(id + ".back", [back_line], back_target, bank="starbase")
        actions = [set_("local", id + "." + r, False) for r, _ in topics]
        g.chain(id + ".start", actions, id + ".intro")
        g.reply(id + ".intro", [prompt], id + ".menu", bank="starbase")

    topic_menu(
        "info.alliance",
        "WHICH_ALLIANCE",
        [
            (x, "ABOUT_" + x.upper())
            for x in ["shofixti", "yehat", "arilou", "chenjesu", "mmrnmhrm", "syreen"]
        ],
        "enough_alliance",
        "info.races.menu",
        "OK_ENOUGH_ALLIANCE",
    )
    topic_menu(
        "info.hierarchy",
        "WHICH_HIERARCHY",
        [
            (x, "ABOUT_" + x.upper())
            for x in [
                "urquan",
                "mycon",
                "spathi",
                "umgah",
                "androsynth",
                "ilwrath",
                "vux",
            ]
        ],
        "enough_hierarchy",
        "info.races.menu",
        "OK_ENOUGH_HIERARCHY",
    )
    topic_menu(
        "info.war",
        "WHICH_WAR",
        [
            ("what_started_war", "URQUAN_STARTED_WAR"),
            ("what_was_war_like", "WAR_WAS_LIKE_SO"),
            ("why_lose_war", "LOST_WAR_BECAUSE"),
            ("what_after_war", "AFTER_WAR"),
        ],
        "enough_war",
        "info.history.menu",
        "OK_ENOUGH_WAR",
    )
    topic_menu(
        "info.ancient",
        "WHICH_ANCIENT",
        [
            ("precursors", "ABOUT_PRECURSORS"),
            ("old_races", "ABOUT_OLD_RACES"),
            ("aliens_on_earth", "ABOUT_ALIENS_ON_EARTH"),
        ],
        "enough_ancient",
        "info.history.menu",
        "OK_ENOUGH_ANCIENT",
    )
    topic_menu(
        "info.defeat",
        "DEFEAT_LIKE_SO",
        [
            ("how_find_urquan", "FIND_URQUAN"),
            ("how_fight_urquan", "FIGHT_URQUAN"),
            ("how_ally", "ALLY_LIKE_SO"),
            ("how_get_strong", "STRONG_LIKE_SO"),
        ],
        "enough_defeat",
        "info.mission.menu",
        "OK_ENOUGH_DEFEAT",
    )
    g.reply("info.races.start", ["WHICH_ALIEN"], "info.races.menu", bank="starbase")
    g.say(
        "info.races.menu",
        [],
        responses=[
            g.edge("what_about_alliance", "info.alliance.start", bank="starbase"),
            g.edge("what_about_hierarchy", "info.hierarchy.start", bank="starbase"),
            g.edge("what_about_other", "info.races.other", bank="starbase"),
            g.edge("enough_aliens", "info.races.back", bank="starbase"),
        ],
        bank="starbase",
    )
    g.reply("info.races.other", ["ABOUT_OTHER"], "info.races.menu", bank="starbase")
    g.reply(
        "info.races.back", ["OK_ENOUGH_ALIENS"], "info.history.menu", bank="starbase"
    )
    g.reply(
        "info.history.start", ["WHICH_HISTORY"], "info.history.menu", bank="starbase"
    )
    g.say(
        "info.history.menu",
        [],
        responses=[
            g.edge(r, dest, bank="starbase")
            for r, dest in [
                ("alien_races", "info.races.start"),
                ("the_war", "info.war.start"),
                ("ancient_history", "info.ancient.start"),
                ("enough_history", "info.history.back"),
            ]
        ],
        bank="starbase",
    )
    g.reply(
        "info.history.back", ["OK_ENOUGH_HISTORY"], "starbase.info", bank="starbase"
    )
    g.reply(
        "info.mission.start", ["WHICH_MISSION"], "info.mission.menu", bank="starbase"
    )
    mission = []
    for r, line in [
        ("where_get_minerals", "GET_MINERALS"),
        ("what_about_aliens", "ABOUT_ALIENS"),
        ("what_about_urquan", "MUST_DEFEAT"),
    ]:
        mission.append(g.edge(r, "info.mission." + r, bank="starbase"))
        g.reply("info.mission." + r, [line], "info.mission.menu", bank="starbase")
    mission.extend(
        [
            g.edge("what_do_now", "info.analysis", bank="starbase"),
            g.edge("how_defeat", "info.defeat.start", bank="starbase"),
            g.edge("enough_mission", "info.mission.back", bank="starbase"),
        ]
    )
    g.say("info.mission.menu", [], responses=mission, bank="starbase")
    g.report("info.analysis", "hayes.flagship_analysis", "info.mission.menu")
    g.reply(
        "info.mission.back", ["OK_ENOUGH_MISSION"], "starbase.info", bank="starbase"
    )
    # Functions have sequential ship/crew and RU/mineral/life questions.
    g.locals["functions.ships"] = 0
    g.locals["functions.resources"] = 0
    function_choices = []
    for r, line in [
        ("tell_me_about_fuel0", "ABOUT_FUEL"),
        ("tell_me_about_modules0", "ABOUT_MODULES"),
    ]:
        key = "functions." + r
        g.locals[key] = False
        function_choices.append(
            g.edge(r, key, not_(local(key, False)), bank="starbase")
        )
        g.reply(
            key,
            [line],
            "info.functions.menu",
            [set_("local", key, True)],
            bank="starbase",
        )
    for key, entries in [
        (
            "functions.ships",
            [
                ("tell_me_about_ships", "ABOUT_SHIPS"),
                ("tell_me_about_crew", "ABOUT_CREW0"),
            ],
        ),
        (
            "functions.resources",
            [
                ("tell_me_about_ru", "ABOUT_RU"),
                ("tell_me_about_minerals", "ABOUT_MINERALS"),
                ("tell_me_about_life", "ABOUT_LIFE"),
            ],
        ),
    ]:
        for i, (r, line) in enumerate(entries):
            function_choices.append(
                g.edge(r, "functions." + r, eq(local(key), i), bank="starbase")
            )
            g.reply(
                "functions." + r,
                [line],
                "info.functions.menu",
                [set_("local", key, i + 1)],
                bank="starbase",
            )
    function_choices.append(
        g.edge("enough_starbase", "info.functions.back", bank="starbase")
    )
    g.chain(
        "info.functions.start",
        [
            set_("local", "functions.ships", 0),
            set_("local", "functions.resources", 0),
            set_("local", "functions.tell_me_about_fuel0", False),
            set_("local", "functions.tell_me_about_modules0", False),
        ],
        "info.functions.intro",
    )
    g.reply(
        "info.functions.intro",
        ["WHICH_FUNCTION"],
        "info.functions.menu",
        bank="starbase",
    )
    g.say("info.functions.menu", [], responses=function_choices, bank="starbase")
    g.reply(
        "info.functions.back", ["OK_ENOUGH_STARBASE"], "starbase.info", bank="starbase"
    )
    # Restore the original local topic progression rather than repeating every topic.
    lookup = {n["id"]: n for n in g.nodes}

    def after(id, actions, next):
        g.chain(id + ".after", actions, next)
        lookup[id]["next"] = id + ".after"

    def reset_after(id, keys, next):
        after(id, [set_("local", key, False) for key in keys], next)

    history_keys = ["history.aliens", "history.war", "history.ancient"]
    reset_after("info.history.start", history_keys, "info.history.menu")
    for edge, key in zip(lookup["info.history.menu"]["responses"][:3], history_keys):
        edge["when"] = not_(local(key, False))
    for id, key in [
        ("info.races.back", "history.aliens"),
        ("info.war.back", "history.war"),
        ("info.ancient.back", "history.ancient"),
    ]:
        after(id, [set_("local", key, True)], "info.history.menu")
    race_keys = ["races.alliance", "races.hierarchy", "races.other"]
    reset_after("info.races.start", race_keys, "info.races.menu")
    for edge, key in zip(lookup["info.races.menu"]["responses"][:3], race_keys):
        edge["when"] = not_(local(key, False))
    for id, key in [
        ("info.alliance.back", "races.alliance"),
        ("info.hierarchy.back", "races.hierarchy"),
        ("info.races.other", "races.other"),
    ]:
        after(id, [set_("local", key, True)], "info.races.menu")
    mission_keys = ["mission.minerals", "mission.aliens", "mission.analysis"]
    reset_after("info.mission.start", mission_keys, "info.mission.reset")
    g.chain(
        "info.mission.reset", [set_("local", "mission.urquan", 0)], "info.mission.menu"
    )
    choices = lookup["info.mission.menu"]["responses"]
    for edge, key in zip(choices[:2], mission_keys):
        edge["when"] = not_(local(key, False))
    choices[2]["when"] = eq(local("mission.urquan"), 0)
    choices[3]["when"] = not_(local("mission.analysis", False))
    choices[4]["when"] = eq(local("mission.urquan"), 1)
    after(
        "info.mission.where_get_minerals",
        [set_("local", "mission.minerals", True)],
        "info.mission.menu",
    )
    after(
        "info.mission.what_about_aliens",
        [set_("local", "mission.aliens", True)],
        "info.mission.menu",
    )
    after(
        "info.mission.what_about_urquan",
        [set_("local", "mission.urquan", 1)],
        "info.mission.menu",
    )
    after("info.defeat.back", [set_("local", "mission.urquan", 2)], "info.mission.menu")
    lookup["info.analysis.speech"]["next"] = "info.analysis.mark"
    g.chain(
        "info.analysis.mark",
        [set_("local", "mission.analysis", True)],
        "info.mission.menu",
    )
    # The supply-ship answer has its own immediate lie/out-of-stock responses.
    lookup["first.supply"]["next"] = "first.supply_check"
    g.decide(
        "first.supply_check",
        [(gt(query("inventory.quantity", "radioactive"), 0), "first.radios")],
        "first.supply_missing",
    )
    g.say(
        "first.supply_missing",
        [],
        responses=[
            g.edge("i_lied", "first.i_lied"),
            g.edge("plumb_out", "first.plumb_out"),
        ],
        profile="hayes.low_power",
    )
    g.reply("first.i_lied", ["DONT_KNOW_WHO_YOU_ARE"], "first.radios_check")
    g.reply("first.plumb_out", ["WHAT_KIND_OF_IDIOT"], "first.radios_check")
    lookup["first.radio_return"]["next"] = "first.return_radios_check"
    g.decide(
        "first.return_radios_check",
        [(gt(query("inventory.quantity", "radioactive"), 0), "first.radios")],
        "first.ask_after_radios",
    )
    asks = aid_menu(
        "first.ask_after_radios",
        [g.edge("well_go_get_them_now", "first.return_bye")],
        ("where_get_radios", "RADIOS_ON_MERCURY"),
        "first.return_radios_check",
    )
    g.say("first.ask_after_radios", [], responses=asks, profile="hayes.low_power")
    g.reply("first.return_bye", ["GLAD_WHEN_YOU_COME_BACK"], "end")
    # A destroyed moon base shares its menu, without adding a second greeting.
    lookup["first.base_destroyed"]["next"] = "first.base_destroyed.menu"
    base_choices = lookup["first.base_destroyed"].pop("responses")
    g.say("first.base_destroyed.menu", [], responses=base_choices)
    lookup["first.moon_help"]["next"] = "first.moon_help_route"
    g.decide(
        "first.moon_help_route",
        [(flag("MOONBASE_DESTROYED"), "first.base_destroyed.menu")],
        "first.moon",
    )
    # Crew explanation has voice-specific fragments and a literal name insertion.
    crew = lookup["functions.tell_me_about_crew"]
    crew["variants"] = [
        {
            "when": get("config", "speech", True),
            "speech": [
                g.p(x, "starbase")
                for x in ["ABOUT_CREW0", "YOUR_FLAGSHIP_3DO2", "ABOUT_CREW1"]
            ],
            "presentation": {"composite": "continue"},
        },
        {
            "speech": [
                g.p("ABOUT_CREW0", "starbase"),
                g.p("YOUR_FLAGSHIP_PC", "starbase"),
                {"insert": get("player", "ship_name", "Vindicator")},
                g.p("ABOUT_CREW1", "starbase"),
            ],
            "presentation": {"composite": "continue"},
        },
    ]
    lookup["starbase.entry.speech"]["variants"][0]["presentation"][
        "composite"
    ] = "hayes.starbase"

    def composed(menu, response, parts):
        item = next(n for n in g.nodes if n["id"] == menu)
        edge = next(e for e in item["responses"] if e["id"] == response)
        edge["text"] = {"compose": parts}

    def fragment(id, bank="commander"):
        return {"phrase": g.p(id, bank)}

    ship = {"insert": get("player", "ship_name", "Vindicator")}
    captain = {"insert": get("player", "captain_name", "Captain")}
    composed(
        "first.greeting",
        "no_but_well_help0",
        [fragment("no_but_well_help0"), ship, fragment("no_but_well_help1")],
    )
    composed(
        "first.reveal",
        "we_are_vindicator0",
        [
            fragment("we_are_vindicator0"),
            captain,
            fragment("we_are_vindicator1"),
            ship,
            fragment("we_are_vindicator2"),
        ],
    )
    composed(
        "first.alliance_names",
        "name_40",
        [fragment("name_40"), captain, fragment("name_41")],
    )
    for r in ["tell_me_about_fuel", "tell_me_about_modules"]:
        composed(
            "info.functions.menu",
            r + "0",
            [fragment(r + "0", "starbase"), ship, fragment(r + "1", "starbase")],
        )
    name = lookup["first.name.name_40"]
    name["variants"][0]["speech"] = [
        g.p("OK_THE_NAME_IS_EMPIRE0"),
        captain,
        g.p("OK_THE_NAME_IS_EMPIRE1"),
        g.p("STARBASE_WILL_BE_READY"),
    ]
    g.write()


def melnorme():
    g = Graph("melnorme", ["melnorme"], "entry")
    g.end()
    g.end("combat", "hostile")
    g.locals.update(
        {
            "asked_to_buy": False,
            "sell_enabled": True,
            "bluster.strength": 0,
            "bluster.testing": 0,
        }
    )
    g.decide("entry", [(not_(flag("MET_MELNORME")), "first.mark")], "anger")
    g.chain("first.mark", [set_("story", "MET_MELNORME", True)], "first.greeting")
    g.decide(
        "anger",
        [
            (eq(get("story", "MELNORME_ANGER"), 1), "miffed.greeting"),
            (eq(get("story", "MELNORME_ANGER"), 2), "pissed.greeting"),
            (ge(get("story", "MELNORME_ANGER"), 3), "hate.greeting"),
            (lt(get("story", "MELNORME_YACK_STACK2"), 6), "first.greeting"),
        ],
        "business.greeting",
    )

    def sequence(id, key, lines, next, base=0):
        save = id.replace(".", "_")
        g.act(
            id,
            "sequence.next",
            ["story", key, [[g.p(x)] for x in lines], "repeat_last"]
            + ([base] if base else []),
            id + ".speech",
            save=save,
        )
        g.say(
            id + ".speech",
            result(save, "speech"),
            next,
            profile=(
                "melnorme.blue"
                if id.startswith(("miffed.", "pissed.", "hate."))
                else None
            ),
        )

    sequence(
        "first.greeting",
        "MELNORME_BUSINESS_COUNT",
        [f"HELLO_NOW_DOWN_TO_BUSINESS_{i}" for i in range(1, 4)],
        "first.menu",
    )
    first = []
    stages = [
        ("we_are_from_alliance0", "KNOW_OF_YOU", "MELNORME_YACK_STACK0", 0, 1),
        ("how_know", "KNOW_BECAUSE", "MELNORME_YACK_STACK0", 1, 2),
        (
            "what_about_yourselves",
            "NO_TALK_ABOUT_OURSELVES",
            "MELNORME_YACK_STACK1",
            0,
            1,
        ),
        ("what_factors", "FACTORS_ARE", "MELNORME_YACK_STACK1", 1, 2),
        ("what_about_universe", "NO_FREE_LUNCH", "MELNORME_YACK_STACK2", 0, 1),
        ("giving_is_good_1", "GIVING_IS_BAD_1", "MELNORME_YACK_STACK2", 1, 2),
        ("giving_is_good_2", "GIVING_IS_BAD_2", "MELNORME_YACK_STACK2", 2, 3),
    ]
    for r, line, key, index, updated in stages:
        first.append(g.edge(r, "first." + r, eq(get("story", key), index)))
        g.reply("first." + r, [line], "first.menu", [set_("story", key, updated)])
    first.extend(
        [
            g.edge(
                "get_on_with_business",
                "business.start",
                ge(get("story", "MELNORME_YACK_STACK1"), 1),
            ),
            g.edge(
                "trade_is_for_the_weak",
                "bluster.start",
                eq(get("story", "MELNORME_YACK_STACK2"), 3),
            ),
            g.edge("no_trade_now", "exit.no_trade_now"),
        ]
    )
    g.say("first.menu", [], responses=first)
    g.reply("exit.no_trade_now", ["OK_NO_TRADE_NOW_BYE"], "end")
    g.chain(
        "business.start",
        [set_("story", "MELNORME_YACK_STACK2", 5)],
        "business.greeting",
    )
    # A sequence base preserves the original shared yack-stack encoding.
    sequence(
        "business.greeting",
        "MELNORME_YACK_STACK2",
        [f"HELLO_AND_DOWN_TO_BUSINESS_{i}" for i in range(1, 11)],
        "business.eligibility",
        base=5,
    )
    g.decide(
        "business.eligibility",
        [
            (
                anyof(
                    gt(query("ship.fuel"), 0),
                    gt(query("inventory.quantity", "biomass"), 0),
                    gt(query("currency.balance", "melnorme_credit"), 0),
                    gt(get("player", "unreported_rainbows"), 0),
                ),
                "business.trading_intro_check",
            )
        ],
        "rescue.entry",
    )
    g.decide(
        "business.trading_intro_check",
        [(not_(flag("TRADED_WITH_MELNORME")), "business.trading_intro")],
        "business.ask",
    )
    g.reply(
        "business.trading_intro",
        ["TRADING_INFO"],
        "business.ask",
        [set_("story", "TRADED_WITH_MELNORME", True)],
    )
    g.reply("business.ask", ["BUY_OR_SELL"], "business.menu", profile="melnorme.purple")
    g.say(
        "business.menu",
        [],
        responses=[
            g.edge(
                "why_turned_purple",
                "business.purple",
                not_(flag("WHY_MELNORME_PURPLE")),
            ),
            g.edge("buy", "buy.entry", not_(local("asked_to_buy", False))),
            g.edge("sell", "sell.entry", local("sell_enabled", True)),
            g.edge("goodbye", "exit.goodbye"),
        ],
        profile="melnorme.purple",
    )
    g.reply(
        "business.purple",
        ["TURNED_PURPLE_BECAUSE"],
        "business.menu",
        [set_("story", "WHY_MELNORME_PURPLE", True)],
    )
    g.reply("exit.goodbye", ["FRIENDLY_GOODBYE"], "end")
    # Buying: response availability is independent of operation failure outcomes.
    g.decide(
        "buy.entry",
        [(eq(query("currency.balance", "melnorme_credit"), 0), "buy.no_credit")],
        "buy.ask",
    )
    g.reply(
        "buy.no_credit",
        ["NEED_CREDIT"],
        "business.menu",
        [set_("local", "asked_to_buy", True)],
    )
    g.reply("buy.ask", ["WHAT_TO_BUY"], "buy.menu")
    g.say(
        "buy.menu",
        [],
        responses=[
            g.edge(
                "buy_fuel",
                "fuel.entry",
                lt(query("ship.fuel"), query("ship.fuel_capacity")),
            ),
            g.edge(
                "buy_technology",
                "tech.entry",
                query("market.available", "melnorme", "technology"),
            ),
            g.edge(
                "buy_info",
                "info.entry",
                query("market.available", "melnorme", "information"),
            ),
            g.edge("done_buying", "buy.done"),
            g.edge("be_leaving_now", "exit.goodbye"),
        ],
    )
    g.reply("buy.done", ["OK_DONE_BUYING"], "business.menu")
    g.decide(
        "fuel.entry",
        [(not_(flag("MELNORME_FUEL_PROCEDURE")), "fuel.intro")],
        "fuel.ask",
    )
    g.reply(
        "fuel.intro",
        ["BUY_FUEL_INTRO"],
        "fuel.ask",
        [set_("story", "MELNORME_FUEL_PROCEDURE", True)],
    )
    fuel_responses = []
    for r, quantity in [
        ("buy_1_fuel", 1),
        ("buy_5_fuel", 5),
        ("buy_10_fuel", 10),
        ("buy_25_fuel", 25),
        ("fill_me_up", "fill"),
    ]:
        node = "fuel." + r
        fuel_responses.append(g.edge(r, node))
        g.act(
            node,
            "trade.purchase",
            ["melnorme", "fuel", quantity],
            "fuel.outcome",
            save="fuel_purchase",
        )
    fuel_responses.append(g.edge("done_buying_fuel", "fuel.done"))
    g.say("fuel.ask", ["HOW_MUCH_FUEL"], responses=fuel_responses)
    g.decide(
        "fuel.outcome",
        [
            (eq(result("fuel_purchase", "code"), "ok"), "fuel.success"),
            (eq(result("fuel_purchase", "code"), "no_room"), "fuel.no_room"),
            (eq(result("fuel_purchase", "code"), "no_credit"), "fuel.no_credit"),
        ],
        "error",
    )
    g.reply("fuel.success", ["GOT_FUEL"], "fuel.capacity_check")
    g.decide(
        "fuel.capacity_check",
        [(ge(query("ship.fuel"), query("ship.fuel_capacity")), "buy.menu")],
        "fuel.ask",
    )
    g.reply("fuel.no_room", ["NO_ROOM_FOR_FUEL"], "fuel.ask")
    g.say(
        "fuel.no_credit",
        [
            g.p("NEED_MORE_CREDIT0"),
            {"number": result("fuel_purchase", "missing")},
            g.p("NEED_MORE_CREDIT1"),
        ],
        "fuel.ask",
    )
    g.reply("fuel.done", ["OK_DONE_BUYING_FUEL"], "buy.menu")
    # Technology: quote contents are data, purchasing is one atomic operation.
    g.decide(
        "tech.entry",
        [(not_(flag("MELNORME_TECH_PROCEDURE")), "tech.intro")],
        "tech.pitch",
    )
    g.reply(
        "tech.intro",
        ["BUY_NEW_TECH_INTRO"],
        "tech.pitch",
        [set_("story", "MELNORME_TECH_PROCEDURE", True)],
    )
    g.report("tech.pitch", "melnorme.technology_pitch", "tech.menu")
    g.say(
        "tech.menu",
        [],
        responses=[
            g.edge(
                "buy_new_tech",
                "tech.buy",
                query("market.available", "melnorme", "technology"),
            ),
            g.edge("no_buy_new_tech", "tech.done"),
        ],
    )
    g.act(
        "tech.buy",
        "trade.purchase",
        ["melnorme", "next_technology", 1],
        "tech.outcome",
        save="tech_purchase",
    )
    g.decide(
        "tech.outcome",
        [
            (eq(result("tech_purchase", "code"), "ok"), "tech.success"),
            (eq(result("tech_purchase", "code"), "no_credit"), "tech.no_credit"),
            (eq(result("tech_purchase", "code"), "unavailable"), "tech.none"),
        ],
        "error",
    )
    g.say("tech.success", result("tech_purchase", "speech"), "tech.remaining")
    g.decide(
        "tech.remaining",
        [(query("market.available", "melnorme", "technology"), "tech.pitch")],
        "tech.none",
    )
    g.reply("tech.none", ["NEW_TECH_ALL_GONE"], "buy.menu")
    g.say(
        "tech.no_credit",
        [
            g.p("NEED_MORE_CREDIT0"),
            {"number": result("tech_purchase", "missing")},
            g.p("NEED_MORE_CREDIT1"),
        ],
        "buy.menu",
    )
    g.reply("tech.done", ["OK_NO_BUY_NEW_TECH"], "buy.menu")
    # Information purchases keep story effects (e.g. Spathi password) in the sale contract.
    g.decide(
        "info.entry",
        [(not_(flag("MELNORME_INFO_PROCEDURE")), "info.intro")],
        "info.ask",
    )
    g.reply(
        "info.intro",
        ["BUY_INFO_INTRO"],
        "info.menu",
        [set_("story", "MELNORME_INFO_PROCEDURE", True)],
    )
    g.reply("info.ask", ["OK_BUY_INFO"], "info.menu")
    choices = []
    for r, category in [
        ("buy_current_events", "events"),
        ("buy_alien_races", "alien_races"),
        ("buy_history", "history"),
    ]:
        choices.append(
            g.edge(
                r, "info." + category, query("market.available", "melnorme", category)
            )
        )
        g.act(
            "info." + category,
            "trade.purchase",
            ["melnorme", category, 1],
            "info.outcome",
            save="info_purchase",
        )
    choices.append(g.edge("done_buying_info", "info.done"))
    g.say("info.menu", [], responses=choices)
    g.decide(
        "info.outcome",
        [
            (eq(result("info_purchase", "code"), "ok"), "info.success"),
            (eq(result("info_purchase", "code"), "no_credit"), "info.no_credit"),
            (eq(result("info_purchase", "code"), "unavailable"), "info.none"),
        ],
        "error",
    )
    g.say("info.success", result("info_purchase", "speech"), "info.remaining")
    g.decide(
        "info.remaining",
        [(query("market.available", "melnorme", "information"), "info.menu")],
        "info.none",
    )
    g.reply("info.none", ["INFO_ALL_GONE"], "buy.menu")
    g.say(
        "info.no_credit",
        [
            g.p("NEED_MORE_CREDIT0"),
            {"number": result("info_purchase", "missing")},
            g.p("NEED_MORE_CREDIT1"),
        ],
        "buy.menu",
    )
    g.reply("info.done", ["OK_DONE_BUYING_INFO"], "buy.menu")
    # Selling and reports retain typed outcome data for numbers and phrase segments.
    g.decide(
        "sell.entry",
        [
            (
                anyof(
                    gt(query("inventory.quantity", "biomass"), 0),
                    gt(get("player", "unreported_rainbows"), 0),
                ),
                "sell.ask",
            )
        ],
        "sell.none",
    )
    g.reply("sell.ask", ["WHAT_TO_SELL"], "sell.menu")
    g.say(
        "sell.menu",
        [],
        responses=[
            g.edge(
                "sell_life_data",
                "sell.biomass",
                gt(query("inventory.quantity", "biomass"), 0),
            ),
            g.edge(
                "sell_rainbow_locations",
                "sell.rainbows",
                gt(get("player", "unreported_rainbows"), 0),
            ),
            g.edge("done_selling", "sell.done"),
        ],
    )
    for category in ["biomass", "rainbows"]:
        g.act(
            "sell." + category,
            "trade.sell",
            ["melnorme", category, "all"],
            "sell.outcome",
            save="sale",
        )
    g.decide(
        "sell.outcome", [(eq(result("sale", "code"), "ok"), "sell.success")], "error"
    )
    g.say("sell.success", result("sale", "speech"), "sell.reset")
    g.chain("sell.reset", [set_("local", "asked_to_buy", False)], "sell.remaining")
    g.decide(
        "sell.remaining",
        [
            (
                anyof(
                    gt(query("inventory.quantity", "biomass"), 0),
                    gt(get("player", "unreported_rainbows"), 0),
                ),
                "sell.ask",
            )
        ],
        "sell.disable",
    )
    g.reply("sell.none", ["NOTHING_TO_SELL"], "sell.disable")
    g.chain("sell.disable", [set_("local", "sell_enabled", False)], "business.menu")
    g.reply("sell.done", ["OK_DONE_SELLING"], "business.menu")
    # Rescue is an exceptional action-result subgraph, not a predicate with mutations.
    g.decide(
        "rescue.entry",
        [(flag("MELNORME_RESCUE_REFUSED"), "rescue.changed")],
        "rescue.explain",
    )
    g.say(
        "rescue.changed",
        ["CHANGED_MIND"],
        responses=[
            g.edge("yes_changed_mind", "rescue.prepare"),
            g.edge("no_changed_mind", "rescue.no_changed"),
        ],
    )
    sequence(
        "rescue.explain",
        "MELNORME_RESCUE_COUNT",
        ["RESCUE_EXPLANATION"] + [f"RESCUE_AGAIN_{i}" for i in range(1, 6)],
        "rescue.ask",
    )
    g.say(
        "rescue.ask",
        ["SHOULD_WE_HELP_YOU"],
        responses=[
            g.edge("yes_help", "rescue.prepare"),
            g.edge("no_help", "rescue.no_help"),
        ],
    )
    g.act(
        "rescue.prepare",
        "rescue.prepare",
        ["melnorme"],
        "rescue.quote_check",
        save="rescue_quote",
    )
    g.decide(
        "rescue.quote_check",
        [
            (eq(result("rescue_quote", "code"), "offer"), "rescue.offer"),
            (eq(result("rescue_quote", "code"), "free"), "rescue.free"),
        ],
        "error",
    )
    g.say(
        "rescue.offer",
        result("rescue_quote", "speech"),
        responses=[
            g.edge("take_it", "rescue.accept"),
            g.edge("leave_it", "rescue.leave"),
        ],
    )
    g.act(
        "rescue.accept",
        "rescue.accept",
        [result("rescue_quote", "token")],
        "rescue.accept_check",
        save="rescue_accept",
    )
    g.decide(
        "rescue.accept_check",
        [(eq(result("rescue_accept", "code"), "ok"), "rescue.thanks")],
        "error",
    )
    g.say("rescue.free", result("rescue_quote", "speech"), "end")
    g.reply("rescue.thanks", ["HAPPY_TO_HAVE_RESCUED"], "end")
    for id, line, refused in [
        ("leave", "MAYBE_SEE_YOU_LATER", True),
        ("no_help", "GOODBYE_AND_GOODLUCK", True),
        ("no_changed", "GOODBYE_AND_GOODLUCK_AGAIN", False),
    ]:
        g.reply(
            "rescue." + id,
            [line],
            "end",
            [set_("story", "MELNORME_RESCUE_REFUSED", True)] if refused else [],
        )
    # Hostility and apology branches.
    g.reply(
        "bluster.start",
        ["WERE_NOT_AFRAID"],
        "bluster.menu",
        [set_("story", "MELNORME_YACK_STACK2", 4)],
        profile="melnorme.blue",
    )
    bluster = [g.edge("why_blue_light", "bluster.why", not_(flag("WHY_MELNORME_BLUE")))]
    g.reply(
        "bluster.why",
        ["BLUE_IS_MAD"],
        "bluster.menu",
        [set_("story", "WHY_MELNORME_BLUE", True)],
    )
    for i in range(1, 4):
        r = f"we_strong_{i}"
        bluster.append(g.edge(r, "bluster." + r, eq(local("bluster.strength"), i - 1)))
        g.reply(
            "bluster." + r,
            [f"YOU_NOT_STRONG_{i}"],
            "bluster.menu",
            [set_("local", "bluster.strength", i)],
        )
    bluster.extend(
        [
            g.edge("just_testing", "bluster.testing", eq(local("bluster.testing"), 0)),
            g.edge(
                "yes_really_testing",
                "bluster.accept_testing",
                eq(local("bluster.testing"), 1),
            ),
            g.edge("youre_on", "exit.youre_on"),
        ]
    )
    g.say("bluster.menu", [], responses=bluster, profile="melnorme.blue")
    g.reply(
        "bluster.testing",
        ["REALLY_TESTING"],
        "bluster.menu",
        [set_("local", "bluster.testing", 1)],
    )
    g.reply(
        "bluster.accept_testing",
        ["TEST_RESULTS"],
        "first.menu",
        profile="melnorme.yellow",
    )
    for id, count, prefix, menu in [
        ("miffed", "MELNORME_MIFFED_COUNT", "HELLO_SLIGHTLY_ANGRY", "miffed.menu"),
        ("pissed", "MELNORME_PISSED_COUNT", "HELLO_PISSED_OFF", "pissed.menu"),
        ("hate", "MELNORME_HATE_COUNT", "HELLO_HATE_YOU", "hate.menu"),
    ]:
        sequence(id + ".greeting", count, [f"{prefix}_{i}" for i in range(1, 4)], menu)
    miffed = []
    for i, (r, line) in enumerate(
        [
            ("explore_relationship", "EXAMPLE_OF_RELATIONSHIP"),
            ("excuse_1", "NO_EXCUSE_1"),
            ("excuse_2", "NO_EXCUSE_2"),
            ("excuse_3", "NO_EXCUSE_3"),
        ]
    ):
        miffed.append(
            g.edge(r, "miffed." + r, eq(get("story", "MELNORME_YACK_STACK3"), i))
        )
        g.reply(
            "miffed." + r,
            [line],
            "miffed.menu",
            [set_("story", "MELNORME_YACK_STACK3", i + 1)],
        )
    miffed.extend(
        [
            g.edge("we_apologize", "miffed.apologize"),
            g.edge("so_we_can_attack", "exit.so_we_can_attack"),
            g.edge("bye_melnorme_slightly_angry", "exit.miffed"),
        ]
    )
    g.say("miffed.menu", [], responses=miffed, profile="melnorme.blue")
    g.reply(
        "miffed.apologize",
        ["APOLOGY_ACCEPTED"],
        "first.menu",
        [set_("story", "MELNORME_ANGER", 0)],
        profile="melnorme.yellow",
    )
    g.say(
        "pissed.menu",
        [],
        responses=[
            g.edge(
                "beg_forgiveness",
                "pissed.beg",
                eq(get("story", "MELNORME_YACK_STACK4"), 0),
            ),
            g.edge(
                "you_are_so_right",
                "pissed.agree",
                eq(get("story", "MELNORME_YACK_STACK4"), 1),
            ),
            g.edge(
                "ok_strip_me",
                "pissed.strip",
                eq(get("story", "MELNORME_YACK_STACK4"), 2),
            ),
            g.edge("fight_some_more", "exit.fight_some_more"),
            g.edge("bye_melnorme_pissed_off", "exit.pissed"),
        ],
        profile="melnorme.blue",
    )
    g.reply(
        "pissed.beg",
        ["LOTS_TO_MAKE_UP_FOR"],
        "pissed.menu",
        [set_("story", "MELNORME_YACK_STACK4", 1)],
    )
    g.reply(
        "pissed.agree",
        ["ONE_LAST_CHANCE"],
        "pissed.menu",
        [set_("story", "MELNORME_YACK_STACK4", 2)],
    )
    g.act(
        "pissed.strip",
        "ship.strip",
        ["melnorme.penance"],
        "pissed.strip_check",
        save="penance",
    )
    g.decide(
        "pissed.strip_check",
        [
            (eq(result("penance", "code"), "not_worth_stripping"), "pissed.not_worth"),
            (eq(result("penance", "code"), "ok"), "pissed.forgiven"),
        ],
        "error",
    )
    g.reply("pissed.not_worth", ["NOT_WORTH_STRIPPING"], "end")
    g.reply(
        "pissed.forgiven", ["FAIR_JUSTICE"], "end", [set_("story", "MELNORME_ANGER", 0)]
    )
    g.say(
        "hate.menu",
        [],
        responses=[
            g.edge("well_if_thats_the_way_you_feel", "exit.fight_again"),
            g.edge("you_hate_us_so_we_go_away", "exit.hate"),
        ],
        profile="melnorme.blue",
    )
    for id, line, anger in [
        ("youre_on", "YOU_GIVE_US_NO_CHOICE", 1),
        ("so_we_can_attack", "DECEITFUL_HUMAN", 2),
        ("fight_some_more", "OK_FIGHT_SOME_MORE", 3),
    ]:
        g.reply(
            "exit." + id, [line], "combat", [set_("story", "MELNORME_ANGER", anger)]
        )
    for id, line, target in [
        ("miffed", "MELNORME_SLIGHTLY_ANGRY_GOODBYE", "end"),
        ("pissed", "MELNORME_PISSED_OFF_GOODBYE", "end"),
        ("fight_again", "WE_FIGHT_AGAIN", "combat"),
        ("hate", "HATE_YOU_GOODBYE", "end"),
    ]:
        g.reply("exit." + id, [line], target)
    for n in g.nodes:
        if n["id"] == "first.menu":
            edge = next(e for e in n["responses"] if e["id"] == "we_are_from_alliance0")
            edge["text"] = {
                "compose": [
                    {"phrase": g.p("we_are_from_alliance0")},
                    {"insert": get("player", "alliance_name", "Alliance")},
                ]
            }
    g.write()


def technology_catalog():
    import re

    text = (ROOT / "sc2/src/uqm/comm/melnorm/melnorm.c").read_text()
    sales = re.search(r"tech_sale_catalog\[\]\s*=\s*\{(.*?)\};", text, re.S)[1]
    grants = re.search(r"tech_data_table\[\]\s*=\s*\{(.*?)\};", text, re.S)[1]
    rules = {}
    for id, kind, key, value in re.findall(
        r"\{\s*(TECH_\w+)\s*,\s*(TECH_TYPE_\w+)\s*,\s*(\w+)\s*,\s*(-?\d+)\s*\}", grants
    ):
        if kind == "TECH_TYPE_MODULE":
            rules[id] = {
                "kind": "module_unlock",
                "module": key,
                "build_cost_ru": int(value),
            }
        elif kind == "TECH_TYPE_STATE":
            rules[id] = {"kind": "state", "key": key, "value": int(value)}
        else:
            rules[id] = {"kind": "lander_shield", "hazard": key}
    return [
        {
            "id": id,
            "price": 150,
            "pitch": ["melnorme." + pitch],
            "sold": ["melnorme." + sold],
            "grant": rules[id],
        }
        for id, pitch, sold in re.findall(
            r"\{\s*(TECH_\w+)\s*,\s*TECHPRICE\s*,\s*(\w+)\s*,\s*(\w+)\s*\}", sales
        )
    ]


if __name__ == "__main__":
    hayes()
    melnorme()
