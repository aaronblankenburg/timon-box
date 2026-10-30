"""Simple 3D stand-ins for the electronics.

The two custom boards come from the KiCad files (outline, part bodies from the footprints); modules and loose parts
are boxes and cylinders in their typical sizes. Every builder returns a list of render items in a local frame:
X/Y like the KiCad front view (Y up), Z up out of the front side, millimetres."""
import math
import numpy as np
from . import prim
from .render import hexc

PCB_GREEN = hexc('#2e6b52'); PCB_BLUE = hexc('#2f5fa8'); PCB_PURPLE = hexc('#6b46a8'); PCB_BLACK = hexc('#2b2d2f')
BLACK = hexc('#252627'); SILVER = hexc('#c9cdd2'); GOLD = hexc('#d4af5a'); TAN = hexc('#c9a46a'); BEIGE = hexc('#eee5cc')
GREY = hexc('#6d7276'); GLASS = hexc('#c9773e'); RED = hexc('#e0302a'); LEDBLUE = hexc('#3d7be0'); WHITE = hexc('#f2f2ee')


def _earclip(poly):
    pts = [tuple(p) for p in poly]
    if math.dist(pts[0], pts[-1]) < 1e-6:
        pts = pts[:-1]
    n = len(pts); area = sum(pts[i][0]*pts[(i+1) % n][1] - pts[(i+1) % n][0]*pts[i][1] for i in range(n))
    if area < 0:
        pts.reverse()
    def cross(o, a, b): return (a[0]-o[0])*(b[1]-o[1]) - (a[1]-o[1])*(b[0]-o[0])
    def inside(p, a, b, c): return cross(a, b, p) >= -1e-9 and cross(b, c, p) >= -1e-9 and cross(c, a, p) >= -1e-9
    idx = list(range(len(pts))); tris = []
    while len(idx) > 3:
        for k in range(len(idx)):
            i0, i1, i2 = idx[k-1], idx[k], idx[(k+1) % len(idx)]
            if cross(pts[i0], pts[i1], pts[i2]) <= 1e-9:
                continue
            if any(inside(pts[j], pts[i0], pts[i1], pts[i2]) for j in idx if j not in (i0, i1, i2)):
                continue
            tris.append((i0, i1, i2)); idx.pop(k); break
        else:
            break
    if len(idx) == 3:
        tris.append(tuple(idx))
    return pts, tris


def prism(poly, z0, z1, color):
    """Extrude a simple polygon (x, y) between z0 and z1, flat-shaded."""
    pts, tris = _earclip(poly); V = []; N = []; F = []; FID = []
    for z, nz, flip in ((z1, 1.0, False), (z0, -1.0, True)):
        base = len(V)
        V += [(x, y, z) for x, y in pts]; N += [(0, 0, nz)] * len(pts)
        F += [(base+a, base+c, base+b) if flip else (base+a, base+b, base+c) for a, b, c in tris]; FID += [0 if nz > 0 else 1] * len(tris)
    n = len(pts)
    for i in range(n):
        (x0, y0), (x1, y1) = pts[i], pts[(i+1) % n]; d = math.hypot(x1-x0, y1-y0)
        if d < 1e-9: continue
        nx, ny = (y1-y0)/d, -(x1-x0)/d; base = len(V)
        V += [(x0, y0, z0), (x1, y1, z0), (x1, y1, z1), (x0, y0, z1)]; N += [(nx, ny, 0)] * 4
        F += [(base, base+1, base+2), (base, base+2, base+3)]; FID += [2+i, 2+i]
    return prim.fix_winding(dict(V=np.array(V, float), N=np.array(N, float), F=np.array(F), FID=np.array(FID), color=np.asarray(color, float)))


def box(x0, x1, y0, y1, z0, z1, color):
    return prim.box(((x0+x1)/2, (y0+y1)/2, (z0+z1)/2), (abs(x1-x0), abs(y1-y0), abs(z1-z0)), color)


class Board:
    """KiCad board in a local frame centred on its outline."""
    def __init__(self, kb):
        self.kb = kb
        xs = [x for q in kb['outline'] for x, _ in q]; ys = [y for q in kb['outline'] for _, y in q]
        self.cx, self.cy = (min(xs)+max(xs))/2, (min(ys)+max(ys))/2
    def L(self, kx, ky): return kx - self.cx, -(ky - self.cy)
    def fp(self, ref): return next(f for f in self.kb['footprints'] if f['ref'] == ref)
    def bbox(self, f):
        for want in (('F.Fab', 'B.Fab'), ('F.SilkS', 'B.SilkS', 'Cmts.User')):
            pts = [p for g in f['gfx'] if g['layer'] in want for p in g['pts']]
            if pts: break
        if not pts:
            pts = [(p['x'], p['y']) for p in f['pads']]
        a = [self.L(*p) for p in pts]
        return min(p[0] for p in a), max(p[0] for p in a), min(p[1] for p in a), max(p[1] for p in a)
    def pads(self, f): return [self.L(p['x'], p['y']) for p in f['pads']]
    def outline(self, color, t=1.6): return [prism([self.L(*p) for p in poly], -t, 0.0, color) for poly in self.kb['outline']]


def _smd(b, f, top):
    """Generic body for small parts, from the footprint name."""
    lib = f['lib'].lower(); x0, x1, y0, y1 = b.bbox(f)
    h, col = 1.0, BLACK
    if 'r_0805' in lib or lib.startswith('resistor'): h, col = 0.55, BLACK
    elif lib.startswith('capacitor'): h, col = 0.85, TAN
    elif 'sot-223' in lib: h = 1.7
    elif 'soic' in lib: h = 1.65
    elif 'sot-23' in lib: h = 1.1
    elif 'sod' in lib: h = 0.9
    shrink = 0.15
    z0, z1 = (0.0, h) if top else (-1.6 - h, -1.6)
    return [box(x0+shrink, x1-shrink, y0+shrink, y1-shrink, z0, z1, col)]


def _header(b, f, h_plastic=2.5, pin=6.0, top=True):
    items = []; pts = b.pads(f)
    for x, y in pts:
        z0 = 0.0 if top else -1.6
        items.append(box(x-1.27, x+1.27, y-1.27, y+1.27, z0, z0+h_plastic if top else z0-h_plastic, BLACK))
        items.append(box(x-0.32, x+0.32, y-0.32, y+0.32, z0+h_plastic if top else z0-h_plastic, z0+h_plastic+pin if top else z0-h_plastic-pin, GOLD))
    return items


def _socket(b, f, h=8.5, top=True):
    pts = b.pads(f); xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
    z0, z1 = (0.0, h) if top else (-1.6 - h, -1.6)
    return [box(min(xs)-1.27, max(xs)+1.27, min(ys)-1.27, max(ys)+1.27, z0, z1, BLACK)]


def mainboard_stack(kb):
    """Main board with ESP32, modules on sockets (front) and the RC522 below (back).

    Returns (items, info) with info['rc522_center'] = (X, Y) and info['rc522_back'] = Z of its flat outer side."""
    b = Board(kb); items = b.outline(PCB_GREEN)
    for f in kb['footprints']:
        ref, lib = f['ref'], f['lib'].lower(); top = f['layer'] == 'F.Cu'
        if ref in ('C5', 'C6'):
            continue
        x0, x1, y0, y1 = b.bbox(f)
        if 'esp32' in lib:
            pc = np.mean(b.pads(f), axis=0); ant_left = abs(pc[0]-x0) > abs(pc[0]-x1)
            items.append(box(x0, x1, y0, y1, 0.0, 0.8, PCB_BLACK))
            sx0, sx1 = (x0+6.8, x1-0.6) if ant_left else (x0+0.6, x1-6.8)
            items.append(box(sx0, sx1, y0+0.5, y1-0.5, 0.8, 3.2, SILVER))
        elif 'pts645' in lib or 'sw_spst' in lib:
            items.append(box(x0, x1, y0, y1, 0.0, 3.5, BLACK))
            items.append(prim.cylinder(((x0+x1)/2, (y0+y1)/2, 3.5), (0, 0, 1), 1.75, 1.0, GREY))
        elif ref == 'J4':
            pts = b.pads(f); xs = [p[0] for p in pts]; y = pts[0][1]
            items.append(box(min(xs)-2.45, max(xs)+2.45, y-2.9, y+2.85, 0.0, 7.0, BEIGE))
        elif 'pinheader' in lib:
            items += _header(b, f)
        elif 'max98357' in lib:
            items += _socket(b, f)
            items.append(box(x0, x1, y0, y1, 8.5, 10.1, PCB_PURPLE))
            cx, cy = (x0+x1)/2, (y0+y1)/2
            items.append(box(cx-1.5, cx+1.5, cy-1.5, cy+1.5, 10.1, 11.0, BLACK))
        elif 'sdcard' in lib:
            items += _socket(b, f)
            items.append(box(x0, x1, y0, y1, 8.5, 10.1, PCB_BLUE))
            items.append(box(x0+2.0, x1-4.0, y0+1.5, y1-1.5, 10.1, 11.9, SILVER))
        elif 'rc522' in lib:
            items += _socket(b, f, top=False)
            cx, cy = (x0+x1)/2, (y0+y1)/2; pts = b.pads(f); px = [q[0] for q in pts]; py = [q[1] for q in pts]
            # 11 mm between the boards: female header 8.5 mm on the main board, plastic of the male header 2.5 mm
            rc = [box(x0, x1, y0, y1, -14.2, -12.6, PCB_BLUE), box(cx-2.5, cx+2.5, y0+8.0, y0+13.0, -12.6, -11.7, BLACK),
                  box(cx+6.0, cx+10.0, y0+6.0, y0+16.0, -12.6, -9.6, SILVER),
                  box(min(px)-1.27, max(px)+1.27, min(py)-1.27, max(py)+1.27, -12.6, -10.1, BLACK)]
            rc += [box(x-0.32, x+0.32, y-0.32, y+0.32, -10.1, -4.6, GOLD) for x, y in pts]
            for it in rc: it['grp'] = 'rc522'
            items += rc
            hy = (min(py)+max(py))/2; ys = [b.L(x, y)[1] for q in kb['outline'] for x, y in q]
            free = max(ys) if abs(max(ys)-hy) > abs(min(ys)-hy) else min(ys)       # the edge opposite the header
            info = dict(rc522_center=(cx, cy), rc522_back=-14.2, header=((min(px)+max(px))/2, hy),
                        free_edge=free, inward=1.0 if hy > free else -1.0)
        else:
            items += _smd(b, f, top)
    return items, info


def latch_stack(kb):
    """Power Latch V2 with parts on the front (+Z) and the TP4056 module on the back (-Z).

    info['led'] = (X, Y, Z) of the module's charge LED, info['back'] = Z of the module's outer side."""
    b = Board(kb); items = b.outline(PCB_GREEN)
    for f in kb['footprints']:
        ref, lib = f['ref'], f['lib'].lower(); x0, x1, y0, y1 = b.bbox(f)
        if 'tp4056' in lib:
            z1 = -1.6 - 0.3; z0 = z1 - 1.0
            items.append(box(x0, x1, y0, y1, z0, z1, PCB_BLUE))
            cx, cy = (x0+x1)/2, (y0+y1)/2
            items.append(box(cx-2.5, cx+2.5, cy-2.0, cy+2.0, z0-1.5, z0, BLACK))
            items.append(box(x0+1.0, x0+8.5, cy-3.8, cy+3.8, z0-2.6, z0, SILVER))
            led = (cx-7.0, cy-5.0)
            items.append(box(led[0]-0.8, led[0]+0.8, led[1]-0.4, led[1]+0.4, z0-0.6, z0, RED))
            items.append(box(led[0]-0.8, led[0]+0.8, led[1]+1.4, led[1]+2.2, z0-0.6, z0, LEDBLUE))
            info = dict(led=(led[0], led[1], z0-0.6), back=z0-1.5)
        elif 'do-35' in lib:
            items.append(prim.cylinder(((x0+x1)/2-2.0, (y0+y1)/2, 1.0), (1, 0, 0), 0.95, 4.0, GLASS))
        elif ref == 'J1':
            continue
        else:
            items += _smd(b, f, True)
    return items, info


def boost_module():
    """Generic fixed 5 V boost module, 22 x 12 mm, parts on +Z."""
    return [box(-11, 11, -6, 6, -1.2, 0.0, PCB_BLUE), box(-6.5, -1.5, -2.5, 2.5, 0.0, 3.0, BLACK),
            box(1.5, 4.5, -1.5, 1.5, 0.0, 1.1, BLACK), box(6.0, 8.0, -4.0, -2.0, 0.0, 1.2, TAN), box(6.0, 8.0, 2.0, 4.0, 0.0, 1.2, TAN)]


def led_pl9823():
    """8 mm LED along +Z: flange at Z 0..1, body and dome above, legs below."""
    items = [prim.cylinder((0, 0, 0), (0, 0, 1), 4.5, 1.0, WHITE), prim.cylinder((0, 0, 1.0), (0, 0, 1), 4.0, 5.6, WHITE),
             prim.ellipsoid((0, 0, 6.6), (4.0, 4.0, 4.0), WHITE)]
    for dx in (-1.9, -0.63, 0.63, 1.9):
        items.append(box(dx-0.25, dx+0.25, -0.25, 0.25, -9.0, 0.0, SILVER))
    return items


def poti_16mm():
    """16 mm pot: body below Z=0 (mounting face), bushing and shaft along +Z."""
    items = [prim.cylinder((0, 0, -7.0), (0, 0, 1), 8.0, 7.0, SILVER), prim.cylinder((0, 0, 0), (0, 0, 1), 3.5, 6.0, SILVER),
             prim.cylinder((0, 0, 6.0), (0, 0, 1), 3.0, 6.5, GREY)]
    for dy in (-5.0, 0.0, 5.0):
        items.append(box(-9.0, -7.6, dy-1.0, dy+1.0, -6.0, -1.0, SILVER))
        items.append(box(-9.0, -8.6, dy-1.0, dy+1.0, -11.0, -6.0, SILVER))
    return items


def microswitch():
    """Small lever switch: body along +Z, lever towards +X."""
    return [box(-2.2, 2.2, -6.0, 6.0, 0.0, 6.5, hexc('#2f3133')), box(2.2, 6.5, -2.5, 2.5, 1.2, 1.5, SILVER),
            box(-1.5, 1.5, -5.0, 5.0, -3.5, 0.0, SILVER)]


def place(items, R, T):
    """Rotate (rows of R = images of the local X, Y, Z axes) and translate a list of items."""
    R = np.asarray(R, float); T = np.asarray(T, float); out = []
    for it in items:
        d = dict(it); d['V'] = it['V'] @ R + T; d['N'] = it['N'] @ R; out.append(d)
    return out
