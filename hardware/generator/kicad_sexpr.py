"""Minimal S-expression reader and writer for KiCad symbol libraries and schematics."""
import re


class Sym(str):
    """An unquoted KiCad atom."""


TOKEN = re.compile(r'\s*(?:(\()|(\))|"((?:[^"\\]|\\.)*)"|([^\s()"]+))', re.S)


def parse(text):
    stack = [[]]
    pos = 0
    while True:
        m = TOKEN.match(text, pos)
        if not m or m.end() == pos:
            break
        pos = m.end()
        opened, closed, quoted, atom = m.groups()
        if opened:
            stack.append([])
        elif closed:
            node = stack.pop()
            stack[-1].append(node)
        elif quoted is not None:
            stack[-1].append(re.sub(r'\\(.)', r'\1', quoted))
        elif atom is not None:
            stack[-1].append(Sym(atom))
    if text[pos:].strip():
        raise ValueError('Trailing data in S-expression')
    return stack[0]


def quote(s):
    return '"' + str(s).replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"'


def dump(node, indent=0):
    if isinstance(node, Sym):
        return str(node)
    if isinstance(node, bool):
        return 'yes' if node else 'no'
    if isinstance(node, (int, float)):
        return fmt(node)
    if isinstance(node, str):
        return quote(node)
    simple = all(not isinstance(x, list) for x in node)
    if simple:
        return '(' + ' '.join(dump(x) for x in node) + ')'
    head = []
    rest = list(node)
    while rest and not isinstance(rest[0], list):
        head.append(dump(rest.pop(0)))
    pad = '\t' * (indent + 1)
    body = '\n'.join(pad + dump(x, indent + 1) for x in rest)
    return '(' + ' '.join(head) + '\n' + body + '\n' + '\t' * indent + ')'


def fmt(v):
    if isinstance(v, int):
        return str(v)
    s = f'{v:.4f}'.rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


def find(node, key):
    return [x for x in node if isinstance(x, list) and x and x[0] == key]


def first(node, key):
    found = find(node, key)
    return found[0] if found else None


def library_symbols(path):
    """Return the top-level symbols of a .kicad_sym file by name."""
    with open(path, encoding='utf-8') as f:
        root = parse(f.read())[0]
    return {x[1]: x for x in find(root, 'symbol')}
