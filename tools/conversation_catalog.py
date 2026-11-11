#!/usr/bin/env python3
"""Catalog legacy conversation control flow without executing story scripts.

Development-only dependency: pycparser. No game/build dependency is introduced.
Keep original expressions, local updates and source bodies: a structural guard
is not a fully resolved predicate over every possible campaign state.
"""
import argparse
from collections import Counter, defaultdict
import html
from pathlib import Path
import re

from pycparser import c_ast, c_generator, c_parser

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'docs/design/conversation'
TYPES = '''LOCDATA RESPONSE_REF BYTE SBYTE UBYTE COUNT UWORD DWORD SIZE COORD
BOOLEAN UNICODE RECT POINT STAMP FRAME COLORMAP MUSIC_REF SOUND SHIP_FRAGMENT
HFLEETINFO HSHIPFRAG FLEET_INFO RESOURCE RESPONSE_FUNC CONTEXT ANIMATION_DESC
STAR_DESC SHIP_INFO RaceType TimeCount CallbackFunction Segue BYTEPTR PUNICODE
HEVENT EVENT STARSHIP DATE SIS_STATE STRING STRING_TABLE GAME_STATE HSTARSHIP
PLANET_DESC SOLARSYS_STATE NUMBER_SPEECH_DESC CallbackArg SWORD size_t bool StatMsgMode RandomContext PVOIDFUNC'''.split()
PREFIX = ''.join(f'typedef int {name};\n' for name in TYPES)
GEN = c_generator.CGenerator()
LEXICAL = re.compile(r'''"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|/\*.*?\*/|//[^\n]*''', re.S)
NAMES = dict(arilou='Arilou', blackur='Kohr-Ah', chmmr='Chmmr',
    comandr='Commander Hayes: first contact', druuge='Druuge', ilwrath='Ilwrath',
    melnorm='Melnorme', mycon='Mycon', orz='Orz', pkunk='Pkunk',
    rebel='Yehat rebels', shofixt='Shofixti', slyhome='Slylandro homeworld',
    slyland='Slylandro probe', spahome='Spathi Safe Ones', spathi='Spathi / Fwiffo',
    starbas='Commander Hayes: starbase', supox='Supox', syreen='Syreen',
    talkpet='Talking Pet', thradd='Thraddash', umgah='Umgah', urquan='Ur-Quan',
    utwig='Utwig', vux='VUX / ZEX', yehat='Yehat royalists', zoqfot='Zoq-Fot-Pik')


def blank(value):
    return ''.join('\n' if c == '\n' else ' ' for c in value)


def mask(source, strings=False):
    return LEXICAL.sub(lambda m: blank(m[0]) if strings or m[0].startswith('/') else m[0], source)


def active_source(source):
    """Preserve coordinates; omit only the disabled blocks present in these scripts.

    Fail on unfamiliar conditional compilation rather than silently choosing a
    build variant. Includes/macros stay symbolic, not expanded into bit offsets.
    """
    result, stack, disabled = [], [], []
    enabled, continuation = True, False
    for line_number, line in enumerate(mask(source).splitlines(keepends=True), 1):
        directive = re.match(r'\s*#\s*(\w+)\s*(.*)', line)
        if continuation:
            continuation = line.rstrip().endswith('\\')
            result.append(blank(line))
            continue
        if directive:
            command, argument = directive.groups()
            argument = argument.strip()
            if command in ('if', 'ifdef', 'ifndef'):
                if (command, argument) not in [('if', '0'), ('ifdef', 'NEVER')]:
                    raise ValueError(f'unsupported conditional at line {line_number}: {line.strip()}')
                stack.append((enabled, False))
                enabled = False
            elif command == 'else':
                parent, selected = stack[-1]
                stack[-1] = (parent, not selected)
                enabled = parent and not selected
            elif command == 'endif':
                enabled = stack.pop()[0]
            elif command == 'elif':
                raise ValueError(f'unsupported elif at line {line_number}')
            continuation = line.rstrip().endswith('\\')
            result.append(blank(line))
        else:
            result.append(line if enabled else blank(line))
            if not enabled and line.strip():
                disabled.append(line_number)
    if stack:
        raise ValueError('unterminated preprocessor conditional')
    return ''.join(result), disabled


def expression(node):
    return ' '.join(GEN.visit(node).split()) if node is not None else ''


def code(value):
    return '<code>' + html.escape(value).replace('|', '&#124;') + '</code>'


def anchor(value):
    return re.sub(r'[^a-z0-9_-]', '-', value.lower())


def source_link(path, line, label=None):
    relative = path.relative_to(ROOT).as_posix()
    return f'[{label or str(line)}](../../../../{relative}#L{line})'


class Calls(c_ast.NodeVisitor):
    def __init__(self):
        self.nodes = []

    def visit_FuncCall(self, node):
        self.nodes.append(node)
        self.generic_visit(node)


class Flow:
    """Collect structural conditions and all operations, without guessing effects."""
    def __init__(self):
        self.events = []

    def line(self, node):
        return node.coord.line - len(TYPES)

    def event(self, kind, node, guard, value):
        self.events.append(dict(kind=kind, line=self.line(node), guard=list(guard), value=value, node=node))

    def walk(self, node, guard=()):
        if node is None:
            return
        if isinstance(node, c_ast.If):
            self.walk(node.cond, guard)
            condition = expression(node.cond)
            self.walk(node.iftrue, (*guard, condition))
            self.walk(node.iffalse, (*guard, f'!({condition})'))
        elif isinstance(node, c_ast.TernaryOp):
            self.walk(node.cond, guard)
            self.walk(node.iftrue, (*guard, expression(node.cond)))
            self.walk(node.iffalse, (*guard, f'!({expression(node.cond)})'))
        elif isinstance(node, c_ast.BinaryOp) and node.op in ('&&', '||'):
            self.walk(node.left, guard)
            left = expression(node.left)
            self.walk(node.right, (*guard, left if node.op == '&&' else f'!({left})'))
        elif isinstance(node, c_ast.Switch):
            self.walk(node.cond, guard)
            self.walk(node.stmt, (*guard, f'switch ({expression(node.cond)})'))
        elif isinstance(node, (c_ast.Case, c_ast.Default)):
            label = f'case {expression(node.expr)}' if isinstance(node, c_ast.Case) else 'default'
            for statement in node.stmts or []:
                self.walk(statement, (*guard, label + ' (see source for fallthrough)'))
        elif isinstance(node, c_ast.For):
            self.walk(node.init, guard)
            self.walk(node.cond, guard)
            loop = f'for ({expression(node.init)}; {expression(node.cond)}; {expression(node.next)})'
            self.walk(node.stmt, (*guard, loop))
            self.walk(node.next, (*guard, loop))
        elif isinstance(node, (c_ast.While, c_ast.DoWhile)):
            self.walk(node.cond, guard)
            label = 'while' if isinstance(node, c_ast.While) else 'do / repeat while'
            self.walk(node.stmt, (*guard, f'{label} ({expression(node.cond)})'))
        elif isinstance(node, c_ast.FuncCall):
            name = expression(node.name)
            kind = ('edge' if name in ('Response', 'DoResponsePhrase') else
                    'speech' if name in ('NPCPhrase', 'NPCPhrase_cb', 'NPCPhrase_splice', 'NPCNumber', 'DoNPCPhrase') else 'call')
            self.event(kind, node, guard, expression(node))
            if node.args:
                self.walk(node.args, guard)
        elif isinstance(node, c_ast.Assignment):
            self.event('update', node, guard, expression(node))
            self.walk(node.lvalue, guard)
            self.walk(node.rvalue, guard)
        elif isinstance(node, c_ast.Decl) and node.init is not None:
            self.event('local', node, guard, expression(node))
            self.walk(node.init, guard)
        elif isinstance(node, c_ast.UnaryOp) and node.op in ('p++', 'p--', '++', '--'):
            self.event('update', node, guard, expression(node))
            self.walk(node.expr, guard)
        elif isinstance(node, (c_ast.Return, c_ast.Break, c_ast.Continue, c_ast.Goto)):
            self.event('flow', node, guard, expression(node))
            for _, child in node.children():
                self.walk(child, guard)
        else:
            for _, child in node.children():
                self.walk(child, guard)


def phrases(path):
    source = path.read_text(encoding='utf-8-sig')
    entries = list(re.finditer(r'^#\(([^)]+)\)([^\n]*)', source, re.M))
    if len({m[1] for m in entries}) != len(entries):
        raise ValueError(f'{path}: duplicate phrase identifiers')
    return {m[1]: dict(text=source[m.end():entries[i + 1].start() if i + 1 < len(entries) else len(source)].strip(),
        voice=m[2].strip(), line=source.count('\n', 0, m.start()) + 1) for i, m in enumerate(entries)}


def phrase_reference(value, bank):
    if value in bank:
        return f'[{code(value)}](#phrase-{anchor(value)})'
    return code(value)


def body_source(source, function):
    line = function.body.coord.line - len(TYPES)
    lines = source.splitlines(keepends=True)
    start = sum(map(len, lines[:line - 1])) + function.body.coord.column - 1
    lexical = mask(source, strings=True)
    depth = 0
    for end in range(start, len(source)):
        depth += (lexical[end] == '{') - (lexical[end] == '}')
        if depth == 0:
            return source[start:end + 1]
    raise ValueError('unclosed function body')


def catalog(path, resources, macros):
    source = path.read_text()
    clean, disabled = active_source(source)
    ast = c_parser.CParser().parse(PREFIX + clean, filename=str(path.relative_to(ROOT)))
    functions = {n.decl.name: n for n in ast.ext if isinstance(n, c_ast.FuncDef)}
    banks = re.findall(r'(\w+)\s*,\s*/\*\s*PlayerPhrases\s*\*/', source)
    if len(banks) != 1:
        raise ValueError(f'{path}: expected one default phrase bank, got {banks}')
    phrase_path = ROOT / 'sc2/content' / resources[macros[banks[0]]]
    bank = phrases(phrase_path)
    flows = {}
    for name, function in functions.items():
        flow = Flow()
        flow.walk(function.body)
        flows[name] = sorted(flow.events, key=lambda e: e['line'])
    all_events = [e for events in flows.values() for e in events]
    # Independently count call syntax in the parsed source to detect lost traversal.
    calls = Calls()
    for function in functions.values():
        calls.visit(function.body)
    expected = Counter(expression(n.name) for n in calls.nodes)
    actual = Counter(expression(e['node'].name) for e in all_events if e['kind'] in ('call', 'edge', 'speech'))
    if expected != actual:
        raise ValueError(f'{path}: incomplete call traversal')
    guards = dict.fromkeys(g for e in all_events for g in e['guard'])
    guard_ids = {g: f'C{i + 1}' for i, g in enumerate(guards)}
    def conditions(event):
        return ' ∧ '.join(f'[{guard_ids[g]}](#condition-{guard_ids[g].lower()})' for g in event['guard']) or 'No enclosing branch condition'
    bindings = []
    for name, events in flows.items():
        for e in events:
            if isinstance(e['node'], c_ast.Assignment) and isinstance(e['node'].lvalue, c_ast.StructRef):
                field = expression(e['node'].lvalue.field)
                if field in ('init_encounter_func', 'post_encounter_func', 'uninit_encounter_func'):
                    bindings.append((field, expression(e['node'].rvalue), name, e))
    edges, automatic, indirect, references, incoming = [], [], [], [], defaultdict(list)
    for name, events in flows.items():
        for e in events:
            node = e['node']
            if e['kind'] == 'edge':
                args = node.args.exprs
                if len(args) < 2:
                    raise ValueError(f'{path}:{e["line"]}: response without destination')
                destination = args[1]
                while isinstance(destination, c_ast.Cast):
                    destination = destination.expr
                target = expression(destination)
                targets = [(target, None)] if target in functions else []
                if not targets:
                    for assignment in events:
                        a = assignment['node']
                        if isinstance(a, c_ast.Assignment) and expression(a.lvalue) == target and assignment['line'] < e['line']:
                            value = a.rvalue
                            while isinstance(value, c_ast.Cast):
                                value = value.expr
                            if expression(value) in functions:
                                targets.append((expression(value), assignment))
                if not targets:
                    raise ValueError(f'{path}:{e["line"]}: unresolved callback {target}')
                edge = dict(source=name, target=target, targets=targets, phrase=expression(args[0]), event=e)
                edges.append(edge)
                for candidate, _ in targets:
                    incoming[candidate].append(edge)
            elif e['kind'] == 'call' and expression(node.name) in functions:
                automatic.append((name, expression(node.name), e))
    # Function-valued arguments are references, not immediate calls (for example
    # the speaker callback queued by NPCPhrase_cb).
    for caller, events in flows.items():
        for e in events:
            if e['kind'] in ('call', 'speech') and e['node'].args:
                for argument in e['node'].args.exprs:
                    if isinstance(argument, c_ast.ID) and argument.name in functions:
                        references.append((caller, argument.name, e))
            if e['kind'] == 'call' and not isinstance(e['node'].name, c_ast.ID):
                identifiers = set(re.findall(r'\b[A-Za-z_]\w*\b', expression(e['node'].name)))
                candidates = []
                for item in events:
                    decl = item['node']
                    if isinstance(decl, c_ast.Decl) and decl.name in identifiers and isinstance(decl.init, c_ast.InitList):
                        candidates.extend(expression(value) for value in decl.init.exprs if expression(value) in functions)
                if not candidates:
                    raise ValueError(f'{path}:{e["line"]}: unresolved indirect call {e["value"]}')
                indirect.extend((caller, target, e) for target in dict.fromkeys(candidates))
    name = path.parent.name
    title = NAMES[name]
    out = [f'# {title}: existing conversation graph', '',
        '> Generated by `tools/conversation_catalog.py`; do not edit this page by hand.', '',
        '[Catalog and interpretation rules](../README.md) · [Proposed engine](../model.md)', '',
        f'Script: {source_link(path, 1, path.name)}. Phrase bank: {source_link(phrase_path, 1, phrase_path.name)}.', '',
        f'{len(functions)} handler/helper nodes; {len(edges)} response registration sites; '
        f'{len(automatic)} direct local calls; {len(bank)} phrase records.', '',
        '## Initialization and lifecycle', '', '| Phase | Callback | Bound by | Conditions / source |', '| --- | --- | --- | --- |']
    for phase, callback, caller, e in bindings:
        out.append(f'| {code(phase)} | [{code(callback)}](#node-{anchor(callback)}) | {code(caller)} | {conditions(e)}; {source_link(path, e["line"])} |')
    out += ['', '## Handler graph', '',
        'Solid arrows are selectable responses; dashed arrows are direct C calls. Conditions, arguments, speech and effects are cataloged below. Parallel response sites are grouped only in this diagram.', '', '```mermaid', 'flowchart LR']
    ids = {fn: f'n{i}' for i, fn in enumerate(functions)}
    for fn in functions:
        out.append(f'    {ids[fn]}["{fn}"]')
    grouped = defaultdict(list)
    for edge in edges:
        for target, _ in edge['targets']:
            grouped[edge['source'], target].append(edge['phrase'] + (' via ' + edge['target'] if len(edge['targets']) > 1 else ''))
    for (src, dest), labels in grouped.items():
        unique = list(dict.fromkeys(labels))
        label = ', '.join(unique[:3]) + (f' (+{len(unique)-3} more)' if len(unique) > 3 else '')
        label = html.escape(label, quote=True).replace('|', '&#124;')
        out.append(f'    {ids[src]} -->|"{label}"| {ids[dest]}')
    for src, dest in dict.fromkeys((src, dest) for src, dest, _ in automatic):
        out.append(f'    {ids[src]} -.-> {ids[dest]}')
    for src, dest in dict.fromkeys((src, dest) for src, dest, _ in indirect):
        out.append(f'    {ids[src]} -. "computed call: candidate" .-> {ids[dest]}')
    for src, dest in dict.fromkeys((src, dest) for src, dest, _ in references):
        out.append(f'    {ids[src]} -. "callback reference" .-> {ids[dest]}')
    for phase, callback, caller, _ in bindings:
        if callback in functions:
            out.append(f'    {ids[caller]} -. "{phase}" .-> {ids[callback]}')
    out += ['```', '', '## Condition catalog', '',
        'Condition IDs describe enclosing source structure. Branch predicates combine with ∧; case labels mark source locations that can also be reached by fallthrough. Values refer to their original evaluation points. Local assignments and early control flow remain in the decision bodies.', '',
        '| ID | Source expression / structural condition |', '| --- | --- |']
    for condition, identifier in guard_ids.items():
        out.append(f'| <a id="condition-{identifier.lower()}"></a>{identifier} | {code(condition)} |')
    out += ['', '## Script-scope state and lookup tables', '',
        'These declarations supply shared local state, computed phrase alternatives, number speech, or lookup data used by the nodes. LOCDATA presentation defaults are linked separately in the source.', '']
    for declaration in ast.ext:
        if isinstance(declaration, c_ast.Decl) and not isinstance(declaration.type, c_ast.FuncDecl):
            value = GEN.visit(declaration)
            line = declaration.coord.line - len(TYPES)
            if line < 1:
                continue
            if 'LOCDATA' in value.split('=')[0]:
                out += [f'Presentation descriptor {code(declaration.name)}: {source_link(path, line, "source")}.', '']
            else:
                out += ['<details>', f'<summary>{html.escape(declaration.name or "declaration")}</summary>', '',
                        source_link(path, line, 'Source'), '', '```c', value + ';', '```', '', '</details>', '']
    out += ['## Node catalog', '']
    for fn, function in functions.items():
        events = flows[fn]
        line = function.decl.coord.line - len(TYPES)
        out += [f'<a id="node-{anchor(fn)}"></a>', '', f'### {fn}', '',
            f'{code(expression(function.decl))} — {source_link(path, line, "source")}.', '']
        variants = list(dict.fromkeys(e['phrase'] for e in incoming[fn]))
        if variants:
            out += ['Incoming player-response variants: ' + ', '.join(phrase_reference(p, bank) for p in variants) + '.', '']
        direct_in = [(src, e) for src, dest, e in automatic if dest == fn]
        if direct_in:
            out += ['Also called directly by: ' + ', '.join(f'[{code(src)}](#node-{anchor(src)}) ({source_link(path, e["line"])})' for src, e in direct_in) + '.', '']
        extra_in = [('Computed-call candidate', src, e) for src, dest, e in indirect if dest == fn]
        extra_in += [('Callback reference', src, e) for src, dest, e in references if dest == fn]
        for label, src, e in extra_in:
            out += [f'{label} from [{code(src)}](#node-{anchor(src)}): {code(e["value"])} ({source_link(path, e["line"])}).', '']
        speech = [e for e in events if e['kind'] == 'speech']
        if speech:
            out += ['#### Dialogue / audio emissions', '', '| Phrase or expression | Enclosing conditions | Source |', '| --- | --- | --- |']
            for e in speech:
                args = e['node'].args.exprs if e['node'].args else []
                first = expression(args[0]) if args else ''
                label = phrase_reference(first, bank)
                label += '<br>' + code(e['value'])
                out.append(f'| {label} | {conditions(e)} | {source_link(path, e["line"])} |')
            out.append('')
        outgoing = [e for e in edges if e['source'] == fn]
        if outgoing:
            out += ['#### Response edges', '', '| Player response | Destination on selection | Enclosing availability conditions | Source |', '| --- | --- | --- | --- |']
            for edge in outgoing:
                e = edge['event']
                label = phrase_reference(edge['phrase'], bank)
                if edge['phrase'] in bank:
                    text = ' '.join(bank[edge['phrase']]['text'].split())
                    label += '<br>' + html.escape(text).replace('|', '&#124;')
                else:
                    label += '<br>Computed phrase: resolve using the local updates / decision body below.'
                if expression(e['node'].name) == 'DoResponsePhrase':
                    label += '<br>' + code(e['value'])
                destinations = []
                for target, assignment in edge['targets']:
                    destination = f'[{code(target)}](#node-{anchor(target)})'
                    if assignment:
                        destination += f'<br>via {code(edge["target"])} assigned under {conditions(assignment)} ({source_link(path, assignment["line"])})'
                    destinations.append(destination)
                out.append(f'| {label} | {"<br>OR<br>".join(destinations)} | {conditions(e)} | {source_link(path, e["line"])} |')
            out.append('')
        operations = [e for e in events if e['kind'] not in ('speech', 'edge')]
        if operations:
            out += ['#### Actions, queries, local updates, and control flow', '',
                'Calls are listed without assuming they are pure or mutating. Local updates are retained because they determine dynamic menus and counter-selected speech.', '',
                '| Kind | Operation | Enclosing conditions | Source |', '| --- | --- | --- | --- |']
            for e in operations:
                op = code(e['value'])
                if e['kind'] == 'call' and expression(e['node'].name) in functions:
                    target = expression(e['node'].name)
                    op += f' → [{code(target)}](#node-{anchor(target)})'
                out.append(f'| {e["kind"]} | {op} | {conditions(e)} | {source_link(path, e["line"])} |')
            out.append('')
        out += ['<details>', '<summary>Exact decision body (local calculations, early returns, fallthrough and ordering)</summary>', '',
                '```c', body_source(source, function), '```', '', '</details>', '']
    out += ['## Phrase catalog', '',
        'Full base-content text, keyed by the symbols used above. Voice filenames are resource metadata; playback also depends on installed addons and settings. Dynamic insertions retain their original placeholders.', '']
    for phrase, item in bank.items():
        out += [f'<a id="phrase-{anchor(phrase)}"></a>', '', '<details>',
            f'<summary>{html.escape(phrase)}' + (f' — {html.escape(item["voice"])}' if item['voice'] else '') + '</summary>', '',
            source_link(phrase_path, item['line'], 'Text source'), '', '<pre>' + html.escape(item['text']) + '</pre>', '', '</details>', '']
    if disabled:
        out += ['## Inactive source', '',
            '`#if 0` and `#ifdef NEVER` branches are excluded from the graph and operation tables. '
            'Exact decision bodies retain those directives for context. Nonblank inactive source lines: ' + ', '.join(map(str, disabled)) + '.', '']
    rendered = '\n'.join(line.rstrip() for line in '\n'.join(out).splitlines()) + '\n'
    return rendered, dict(name=name, title=title, functions=len(functions), edges=len(edges),
        automatic=len(automatic), phrases=len(bank), bindings=bindings, path=path, source=source,
        events=len(all_events), disabled=len(disabled))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='fail if generated catalog pages are stale')
    args = parser.parse_args()
    resources = dict(re.findall(r'^(\S+)\s*=\s*CONVERSATION:([^:\n]+)', (ROOT / 'sc2/content/uqm.rmp').read_text(), re.M))
    macros = {}
    for path in (ROOT / 'sc2/src/uqm/comm').glob('*/resinst.h'):
        macros.update(re.findall(r'#define\s+(\w+)\s+"([^"]+)"', path.read_text()))
    results, stale = [], []
    for path in sorted((ROOT / 'sc2/src/uqm/comm').glob('*/*.c')):
        text, stats = catalog(path, resources, macros)
        results.append(stats)
        output = OUTPUT / 'catalog' / f'{stats["name"]}.md'
        if args.check:
            if not output.exists() or output.read_text() != text:
                stale.append(output.relative_to(ROOT).as_posix())
        else:
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(text)
    index = ['<!-- Generated by tools/conversation_catalog.py. -->', '',
        '| Conversation | Handler/helper nodes | Response sites | Direct local calls | Phrase records |', '| --- | ---: | ---: | ---: | ---: |']
    for r in results:
        index.append(f'| [{r["title"]}](catalog/{r["name"]}.md) | {r["functions"]} | {r["edges"]} | {r["automatic"]} | {r["phrases"]} |')
    index += ['| **Total** | ' + ' | '.join(str(sum(r[k] for r in results)) for k in ('functions', 'edges', 'automatic', 'phrases')) + ' |', '']
    # Keep the human-authored README contract; replace only its generated index.
    readme = OUTPUT / 'README.md'
    original = readme.read_text()
    start, end = '<!-- catalog:start -->', '<!-- catalog:end -->'
    before, rest = original.split(start, 1)
    _, after = rest.split(end, 1)
    updated = before + start + '\n' + '\n'.join(index) + end + after
    if args.check:
        if original != updated:
            stale.append(readme.relative_to(ROOT).as_posix())
        if stale:
            raise SystemExit('Stale catalog: ' + ', '.join(stale))
    else:
        readme.write_text(updated)
    print(f'{len(results)} scripts; ' + '; '.join(f'{sum(r[k] for r in results)} {k}' for k in ('functions', 'edges', 'automatic', 'phrases', 'events')))


if __name__ == '__main__':
    main()
