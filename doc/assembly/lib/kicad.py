"""Minimal reader for KiCad 8 boards (.kicad_pcb): outline, footprints with pads and graphics, board texts.

Coordinates stay in KiCad board millimetres (y points down)."""
import math
import re

_TOK = re.compile(r'\(|\)|"(?:[^"\\]|\\.)*"|[^\s()"]+')


def parse(text):
    stack = [[]]
    for m in _TOK.finditer(text):
        t = m.group(0)
        if t == '(':
            stack.append([])
        elif t == ')':
            node = stack.pop(); stack[-1].append(node)
        elif t[0] == '"':
            stack[-1].append(t[1:-1].replace('\\"', '"').replace('\\\\', '\\'))
        else:
            stack[-1].append(t)
    return stack[0][0]


def _get(node, key):
    for c in node[1:]:
        if isinstance(c, list) and c and c[0] == key:
            return c
    return None


def _all(node, key):
    return [c for c in node[1:] if isinstance(c, list) and c and c[0] == key]


def _xy(node):
    return float(node[1]), float(node[2])


def _arc(s, m, e, n=24):
    (x1, y1), (x2, y2), (x3, y3) = s, m, e
    d = 2 * (x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2))
    if abs(d) < 1e-12:
        return [s, e]
    ux = ((x1*x1 + y1*y1) * (y2 - y3) + (x2*x2 + y2*y2) * (y3 - y1) + (x3*x3 + y3*y3) * (y1 - y2)) / d
    uy = ((x1*x1 + y1*y1) * (x3 - x2) + (x2*x2 + y2*y2) * (x1 - x3) + (x3*x3 + y3*y3) * (x2 - x1)) / d
    r = math.hypot(x1 - ux, y1 - uy)
    a1, a2, a3 = (math.atan2(y - uy, x - ux) for x, y in (s, m, e))
    full = 2 * math.pi
    d13, d12 = (a3 - a1) % full, (a2 - a1) % full
    sweep = d13 if d12 <= d13 else d13 - full
    return [(ux + r * math.cos(a1 + sweep * i / n), uy + r * math.sin(a1 + sweep * i / n)) for i in range(n + 1)]


def _shape(g, kind):
    k = kind[3:]
    if k == 'line':
        return [_xy(_get(g, 'start')), _xy(_get(g, 'end'))], False
    if k == 'rect':
        (x1, y1), (x2, y2) = _xy(_get(g, 'start')), _xy(_get(g, 'end'))
        return [(x1, y1), (x2, y1), (x2, y2), (x1, y2), (x1, y1)], True
    if k == 'circle':
        (cx, cy), (ex, ey) = _xy(_get(g, 'center')), _xy(_get(g, 'end')); r = math.hypot(ex - cx, ey - cy)
        return [(cx + r * math.cos(2 * math.pi * i / 48), cy + r * math.sin(2 * math.pi * i / 48)) for i in range(49)], True
    if k == 'arc':
        return _arc(_xy(_get(g, 'start')), _xy(_get(g, 'mid')), _xy(_get(g, 'end'))), False
    pts = [_xy(p) for p in _all(_get(g, 'pts'), 'xy')]
    return pts + pts[:1], True


def _text(s):
    return re.sub(r'\\+n', '\n', s)


def _transform(x, y, rot):
    a = math.radians(rot); c, s = math.cos(a), math.sin(a)
    return lambda lx, ly: (x + lx * c + ly * s, y - lx * s + ly * c)


def _chain(polys, eps=1e-3):
    """Join outline segments into closed polygons."""
    polys = [list(p) for p in polys]; out = []
    while polys:
        cur = polys.pop(0)
        grown = True
        while grown and math.dist(cur[0], cur[-1]) > eps:
            grown = False
            for i, p in enumerate(polys):
                if math.dist(cur[-1], p[0]) < eps:
                    cur += p[1:]
                elif math.dist(cur[-1], p[-1]) < eps:
                    cur += p[::-1][1:]
                else:
                    continue
                polys.pop(i); grown = True; break
        out.append(cur)
    return out


def load_board(path):
    root = parse(open(path, encoding='utf-8').read())
    edges = []
    for kind in ('gr_line', 'gr_arc', 'gr_rect', 'gr_circle', 'gr_poly'):
        for g in _all(root, kind):
            lay = _get(g, 'layer')
            if lay and lay[1] == 'Edge.Cuts':
                edges.append(_shape(g, kind)[0])
    texts = []
    for g in _all(root, 'gr_text'):
        at = _get(g, 'at'); eff = _get(g, 'effects'); font = _get(eff, 'font') if eff else None
        size = _get(font, 'size') if font else None; just = _get(eff, 'justify') if eff else None
        texts.append(dict(text=_text(g[1]), x=float(at[1]), y=float(at[2]), rot=float(at[3]) if len(at) > 3 else 0.0,
                          layer=_get(g, 'layer')[1], size=float(size[1]) if size else 1.0, justify=list(just[1:]) if just else []))
    fps = []
    for f in _all(root, 'footprint'):
        at = _get(f, 'at'); x, y = float(at[1]), float(at[2]); rot = float(at[3]) if len(at) > 3 else 0.0
        T = _transform(x, y, rot)
        props = {p[1]: p[2] for p in _all(f, 'property') if len(p) > 2 and isinstance(p[2], str)}
        gfx = []
        for kind in ('fp_line', 'fp_rect', 'fp_circle', 'fp_arc', 'fp_poly'):
            for g in _all(f, kind):
                pts, closed = _shape(g, kind)
                gfx.append(dict(layer=_get(g, 'layer')[1], local=pts, pts=[T(*p) for p in pts], closed=closed))
        pads = []
        for p in _all(f, 'pad'):
            pat = _get(p, 'at'); sz = _get(p, 'size'); dr = _get(p, 'drill'); lays = _get(p, 'layers')
            px, py = float(pat[1]), float(pat[2]); bx, by = T(px, py)
            drill = 0.0
            if dr:
                vals = [v for v in dr[1:] if isinstance(v, str) and v not in ('oval',)]
                nums = [float(v) for v in vals if re.match(r'^-?[\d.]+$', v)]
                drill = min(nums) if nums else 0.0
            rr = _get(p, 'roundrect_rratio')
            pads.append(dict(num=p[1], kind=p[2], shape=p[3], x=bx, y=by, lx=px, ly=py,
                             angle=float(pat[3]) if len(pat) > 3 else 0.0, w=float(sz[1]), h=float(sz[2]),
                             drill=drill, layers=list(lays[1:]) if lays else [], rr=float(rr[1]) if rr else 0.0))
        fps.append(dict(lib=f[1], ref=props.get('Reference', '?'), value=props.get('Value', ''), layer=_get(f, 'layer')[1],
                        x=x, y=y, rot=rot, T=T, gfx=gfx, pads=pads))
    return dict(outline=_chain(edges), texts=texts, footprints=fps)
