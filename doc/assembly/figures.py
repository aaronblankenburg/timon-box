"""Render all figures of doc/assembly.pdf from models/box.step.

Usage: python3 figures.py [name ...]    (without names: render everything)
Writes fig/<name>.jpg|png plus fig/<name>.json with the pixel positions of the label anchors.
Coordinates are model millimetres: x to the front (speaker), y to the right (next button), z up."""
import pickle, json, sys, os, time
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from lib.render import render, clip_mesh, hexc, nrm, camera
from lib import prim
from lib.step import load_parts

HERE = Path(__file__).resolve().parent
STEP = HERE.parent.parent / 'models' / 'box.step'
OUT = str(HERE / 'fig') + '/'
CACHE = HERE / '.cache' / 'parts.pkl'


def load_cached():
    """Tessellating the STEP takes a while, so keep the result until the file changes."""
    st = STEP.stat(); key = (st.st_size, int(st.st_mtime))
    if CACHE.exists():
        d = pickle.load(open(CACHE, 'rb'))
        if d.get('key') == key:
            return d['parts']
    parts = {k: {kk: v[kk] for kk in ('name', 'V', 'N', 'F', 'FID', 'color')} for k, v in load_parts(str(STEP)).items()}
    CACHE.parent.mkdir(exist_ok=True); pickle.dump({'key': key, 'parts': parts}, open(CACHE, 'wb'))
    return parts


P = load_cached()
os.makedirs(OUT, exist_ok=True)
# Lid, token and knob are tilted by 5 degrees about y; t is their axis.
t = np.array([0.0871557, 0, 0.9961947])
GLUE = hexc('#f08a24'); TAPE = hexc('#6f9ef0'); BRASS = hexc('#cfa64a'); STEEL = hexc('#3d4247')
PCB = hexc('#2f5fa8'); SILVER = hexc('#c8ccd1'); SPK = hexc('#45494f'); MAG = hexc('#8a8f96')
CERAMIC = hexc('#e2e2e2'); BED = hexc('#e3e8ec'); NFC = hexc('#c77d3a')
HOLES = [(5.43,13.13),(71.07,13.13),(5.43,76.87),(71.07,76.87)]

def part(name, **kw): d = dict(P[name]); d.update(kw); return d
def tri_info(p):
    V,F = p['V'], p['F']; tri = V[F]
    n = np.cross(tri[:,1]-tri[:,0], tri[:,2]-tri[:,0]); A = np.linalg.norm(n,axis=1)/2
    n = n/np.maximum(2*A[:,None],1e-12); return tri.mean(1), n, A
def fids(name, normal, pred=lambda c,e: True, tol=0.995):
    p = P[name]; C, n, A = tri_info(p); out=set(); normal = nrm(normal)
    for f in np.unique(p['FID']):
        m = p['FID']==f; nm = (n[m]*A[m,None]).sum(0); L = np.linalg.norm(nm)
        if L == 0 or (nm/L)@normal <= tol: continue
        c = (C[m]*A[m,None]).sum(0)/A[m].sum(); e = np.ptp(p['V'][p['F'][m]].reshape(-1,3), axis=0)
        if pred(c, e): out.add(int(f))
    return out
F_back_top = fids('back', (0,0,1), lambda c,e: c[2] > 14.4 and max(e[0],e[1]) > 60)
F_frame_top = fids('frame', t, lambda c,e: c@t > 87.5)
F_front_cup = fids('front', -t, lambda c,e: abs(c@t-87.988)<0.1 and max(e[0],e[1]) < 60)
F_prev_in = fids('prev button', (0,1,0)); F_next_in = fids('next button', (0,-1,0))
F_bumper_top = fids('bumper', (0,0,1), lambda c,e: c[2] > 6.9)
print('faces', F_back_top, F_frame_top, F_front_cup, F_prev_in, F_next_in, F_bumper_top, flush=True)

def ztop(x, y, h): return (h - t[0]*x)/t[2]
def save(im, name, anchors=None, prj=None, crop=False):
    off = (0, 0)
    if crop:
        a = np.asarray(im.convert('L')); ys, xs = np.nonzero(a < 248); pad = 26
        x0, y0 = max(int(xs.min())-pad, 0), max(int(ys.min())-pad, 0); x1, y1 = min(int(xs.max())+pad, im.width), min(int(ys.max())+pad, im.height)
        im = im.crop((x0, y0, x1, y1)); off = (x0, y0)
    if im.mode == 'RGBA': im.save(OUT+name+'.png')
    else: im.save(OUT+name+'.jpg', quality=93, subsampling=0)
    A = {}
    if anchors:
        ks = list(anchors); xy = prj([anchors[k] for k in ks]); A = {k: [round(float(xy[i][0]-off[0]),1), round(float(xy[i][1]-off[1]),1)] for i,k in enumerate(ks)}
    json.dump({'size': list(im.size), 'anchors': A}, open(OUT+name+'.json','w'), indent=1)
def rot_to_z(up):
    up = nrm(up); z = np.array([0,0,1.0]); v = np.cross(up, z); s = np.linalg.norm(v); c = up@z
    if s < 1e-9: return np.eye(3) if c > 0 else np.diag([1.0,-1.0,-1.0])
    vx = np.array([[0,-v[2],v[1]],[v[2],0,-v[0]],[-v[1],v[0],0]]); return np.eye(3) + vx + vx@vx*((1-c)/s**2)
def oriented(name, up, **kw):
    p = dict(P[name]); Rm = rot_to_z(up); V = p['V']@Rm.T; N = p['N']@Rm.T
    off = np.array([(V[:,0].min()+V[:,0].max())/2, (V[:,1].min()+V[:,1].max())/2, V[:,2].min()])
    p.update(V=V-off, N=N); p.update(kw); return p
def cutp(p, p0, n, **kw):
    V,N,F,FID = clip_mesh(p['V'],p['N'],p['F'],p.get('FID',np.zeros(len(p['F']),int)), np.asarray(p0,float), n)
    d = dict(p); d.update(V=V,N=N,F=F,FID=FID,cut=True); d.update(kw); return d
def autoscale(items, view, size, margin=0.08):
    R = camera(view); Pp = np.concatenate([(it['V']+np.asarray(it.get('offset',(0,0,0)))) @ R.T for it in items])
    d = Pp[:,:2].max(0)-Pp[:,:2].min(0); return min(size[0]*(1-2*margin)/d[0], size[1]*(1-2*margin)/d[1])
def tic(name, t0): print(f'{name}: {time.time()-t0:.1f}s', flush=True)

# ---- electronics stand-ins (lib/electronics.py), placed in the box ----------------------------------------
from lib.kicad import load_board
from lib import electronics as el
ROOT = HERE.parent.parent
U = np.array([t[2], 0, -t[0]]); YV = np.array([0, 1.0, 0])
LID = np.stack([U, YV, t])                                # lid frame: X along the tilted x, Z along t


def L(x, y, h):
    """Point at box-x/y on the lid plane with height h (along t)."""
    return np.array([x, y, (h - t[0]*x)/t[2]])


def lid_parts():
    return (el.place(el.led_pl9823(), LID, L(65, 17.5, 94.988)) + el.place(el.poti_16mm(), LID, L(10, 17.5, 95.988))
            + el.place(el.microswitch(), LID, L(12.9, 45, 88.5)))


SPACER = next(iter(load_parts(str(ROOT/'models'/'pcb_spacer.step')).values()))
P['pcb_spacer'] = {k: SPACER[k] for k in ('name', 'V', 'N', 'F', 'FID', 'color')}


def spacer_local(info):
    """Spacer in the stack frame: its slot holds the main board's free edge, its foot stands on the RC522."""
    k = info['inward']                                                  # +1/-1: from the free edge towards the header
    Rs = np.array([[-k, 0, 0], [0, 0, 1.0], [0, k, 0]])               # y across the boards: foot on the RC522, the main
    Ts = np.array([0.0, info['free_edge'] + 0.6*k, -12.6])           # board's edges sit between the bumps 11-13 mm up
    return el.place([dict(P['pcb_spacer'])], Rs, Ts)


def tie_local(info):
    """Cable tie around main board, spacer and RC522, right at the free end."""
    y = info['free_edge'] + 1.4*info['inward']; x0, x1, z0, z1, r = -23.2, 23.2, -14.9, 9.2, 1.8
    pts = []
    for cx, cz, a0 in ((x1-r, z1-r, 0), (x0+r, z1-r, 90), (x0+r, z0+r, 180), (x1-r, z0+r, 270)):
        pts += [(cx + r*np.cos(np.radians(a)), y, cz + r*np.sin(np.radians(a))) for a in np.linspace(a0, a0+90, 7)]
    pts.append(pts[0]); NYLON = hexc('#ece8dc')
    return [prim.ribbon(pts, 2.6, 1.1, NYLON, wdir=(0, 1, 0)), prim.box((8.0, y, z1 + 1.6), (5.0, 4.0, 3.2), NYLON),
            prim.box((12.5, y, z1 + 2.2), (6.0, 2.6, 1.1), NYLON)]


def stack(groups=('main', 'rc522'), spacer=True, tie=False):
    items, info = el.mainboard_stack(load_board(str(ROOT/'hardware'/'mainboard'/'timon-box.kicad_pcb')))
    R = np.stack([-U, YV, -t]); xr, yr = info['rc522_center']
    T = L(38, 45, 87.988) + xr*U - yr*YV + info['rc522_back']*t
    keep = [it for it in items if it.get('grp', 'main') in groups]
    if spacer and 'main' in groups: keep += spacer_local(info)
    if tie and 'main' in groups: keep += tie_local(info)
    return el.place(keep, R, T), (R, T, info)


def strap_path(apex, xc=39.7):
    """Centre line of the strap: through both loops, ends folded back, arch over the lid with its top at z=apex."""
    pts = [(-2.9, 99.0), (-3.0, 93.0), (-6.4, 86.0), (-6.9, 81.0), (-6.9, 72.0)]
    c = np.array([-4.2, 71.5]); r = 2.7
    pts += [tuple(c + r*np.array([np.cos(a), np.sin(a)])) for a in np.linspace(np.pi, 2*np.pi, 9)[1:]]
    pts += [(-1.5, 76.0), (-1.5, 86.0)]
    p0, p1, p2, p3 = map(np.array, ((-1.5, 86.0), (-3.6, 86.0 + 0.62*(apex-86.0)), (16.0, apex), (45.0, apex)))
    pts += [tuple((1-u)**3*p0 + 3*u*(1-u)**2*p1 + 3*u*u*(1-u)*p2 + u**3*p3) for u in np.linspace(0, 1, 24)[1:]]
    right = [(90.0 - y, z) for y, z in reversed(pts)]
    return [(xc, y, z) for y, z in pts + right[1:]]


def strap_apex(length):
    """Apex height (z) at which the strap centre line has the given length (mm)."""
    plen = lambda a: sum(np.linalg.norm(np.subtract(q, p)) for p, q in zip(strap_path(a)[:-1], strap_path(a)[1:]))
    lo, hi = 100.0, 260.0
    for _ in range(40):
        mid = (lo + hi)/2
        lo, hi = (mid, hi) if plen(mid) < length else (lo, mid)
    return (lo + hi)/2


STRAP_LENGTH = 300.0   # mm, as in step 14


def strap(length=STRAP_LENGTH, width=15.0):
    """Shutter strap of the given length; the arch height follows from it."""
    return prim.ribbon(strap_path(strap_apex(length)), width, 1.1, hexc('#c4b79a'))


def power_parts():
    lat, info = el.latch_stack(load_board(str(ROOT/'hardware'/'power-latch'/'power-latch-v2.kicad_pcb')))
    R = np.array([[0, 1.0, 0], [0, 0, 1.0], [1.0, 0, 0]])                # X -> y, Y -> z, Z -> +x (into the box)
    TL = np.array([0.6, 45.0, 33.0]); TB = np.array([-3.0, 17.0, 30.0])
    lx, ly, lz = info['led']; led = TL + lx*R[0] + ly*R[1] + lz*R[2]
    items = el.place(lat, R, TL) + el.place(el.boost_module(), R, TB)
    p0 = np.array([-9.8, 45, 24.25]); p1 = np.array([-6.0, 45, 24.25]); p3 = led + np.array([-0.15, 0, 0])
    c1 = p1 + np.array([2.2, 0, 0]); c2 = p3 + np.array([-2.2, 0, 0])
    curve = [((1-s)**3)*p1 + 3*s*(1-s)**2*c1 + 3*s*s*(1-s)*c2 + s**3*p3 for s in np.linspace(0, 1, 9)]
    items.append(prim.tube([p0] + curve, 0.875, hexc('#cfe8f2')))
    EPOX = hexc('#f2d49b')
    for c in ((-5.3, 31.5, 39.0), (-5.3, 58.5, 28.0), (-4.6, 8.0, 30.0), (-4.6, 26.0, 30.0)):
        items.append(prim.ellipsoid(c, (1.8, 2.6, 2.6), EPOX))
    anchors = {'latch': TL + np.array([1.8, 6.0, 5.0]), 'boost': TB + np.array([3.0, -4.0, 1.0]), 'pipe': curve[4], 'ledpcb': led}
    return items, anchors


def cable(T_stack, R_stack, info, lift=0.0, ends=None):
    """Seven-wire XH cable from J4 on the main board down to the power side."""
    b = load_board(str(ROOT/'hardware'/'mainboard'/'timon-box.kicad_pcb')); f = next(x for x in b['footprints'] if x['ref'] == 'J4')
    xs = [p['x'] for p in f['pads']]; cx = (min(xs)+max(xs))/2
    BB = el.Board(b); items = []; cols = ['#d23a2f', '#2b2b2b', '#2f6fd0', '#f2c230', '#f08a24', '#3a9a4a', '#f4f4f0']
    pins = sorted([BB.L(p['x'], p['y']) for p in f['pads']])
    up = np.array([0, 0, lift])
    plug_c = np.mean(pins, axis=0)
    items += el.place([el.box(min(q[0] for q in pins)-2.45, max(q[0] for q in pins)+2.45, plug_c[1]-2.9, plug_c[1]+2.85, 7.0, 12.5, el.BEIGE)], R_stack, T_stack + up)
    for i, (x, y) in enumerate(pins):
        a = T_stack + up + x*R_stack[0] + y*R_stack[1] + 12.5*R_stack[2]
        b2 = a + np.array([0, 0, -8.0])
        e = np.array(ends[i]); c = e + np.array([6.0, 0, 6.0])
        pts = [a, b2] + [((1-s)**3)*b2 + 3*s*(1-s)**2*(b2+np.array([0, 0, -12.0])) + 3*s*s*(1-s)*c + s**3*e for s in np.linspace(0.1, 1, 10)]
        items.append(prim.tube(pts, 0.5, hexc(cols[i]), seg=10))
    return items

which = sys.argv[1:] or ['hero','overview','exploded','tiles','token','inserts','shell','inside','lid','door','akkus','lid_full','lidopen','spacer_stack','strap','lid_stack','readme_hero']
VA = (1.0,-1.25,0.8); VB = (-1.0,1.3,0.7)
# print orientation: part -> direction (in assembly position) that points up on the bed
TILES = [('frame',(0,0,1)), ('front',t), ('back',(0,0,-1)), ('door',(0,0,-1)), ('bumper',(0,0,-1)),
         ('speaker grill',(-1,0,0)), ('prev button',(0,1,0)), ('next button',(0,-1,0)), ('volume',-t), ('token',-t), ('pcb_spacer',(0,0,1))]

if 'hero' in which:
    t0=time.time(); im, prj = render([part(k) for k in P if k != 'pcb_spacer'] + [strap()], view=VA, size=(1400,1250), ss=2, transparent=True, margin=0.03)
    save(im, 'hero'); tic('hero', t0)
if 'overview' in which:
    t0=time.time()
    im, prj = render([part(k) for k in P if k != 'pcb_spacer'], view=VA, size=(1000,920), ss=2, margin=0.07)
    save(im, 'overview_a', {'token':(37.7,45,ztop(37.7,45,103.988)), 'knob':(10.6,17.5,107.6), 'led':(65,17.5,ztop(65,17.5,98.988)),
         'grill':(87.2,45,49.8), 'prev':(38.6,-1,45.6), 'handle':(39.7,-6,77.2), 'lid':(70,60,ztop(70,60,98.988))}, prj)
    im, prj = render([part(k) for k in P if k != 'pcb_spacer'], view=VB, size=(1000,920), ss=2, margin=0.07)
    save(im, 'overview_b', {'usb':(-9.8,45,19.25), 'chg':(-9.8,45,24.25), 'next':(38.6,91,45.6), 'jack':(10,72.5,ztop(10,72.5,98.988)),
         'handle':(39.7,96,77.2), 'bumper':(-4,60,5.5), 'curved':(-9.8,70,50)}, prj)
    # both views side by side as one figure; anchors get a/b prefixes
    oa, ob = Image.open(OUT+'overview_a.jpg'), Image.open(OUT+'overview_b.jpg'); gap = 30
    ov = Image.new('RGB', (oa.width+gap+ob.width, max(oa.height, ob.height)), 'white'); ov.paste(oa, (0, 0)); ov.paste(ob, (oa.width+gap, 0))
    ov.save(OUT+'overview.jpg', quality=93, subsampling=0)
    ja, jb = json.load(open(OUT+'overview_a.json')), json.load(open(OUT+'overview_b.json'))
    anc = {f'a_{k}': v for k, v in ja['anchors'].items()}; anc.update({f'b_{k}': [v[0]+oa.width+gap, v[1]] for k, v in jb['anchors'].items()})
    json.dump({'size': [ov.width, ov.height], 'anchors': anc}, open(OUT+'overview.json', 'w'), indent=1)
    tic('overview', t0)
if 'exploded' in which:
    t0=time.time()
    off = {'bumper':(0,0,-62), 'door':(0,0,-44), 'back':(0,0,-22), 'frame':(0,0,0), 'front':(0,0,26), 'token':(0,0,50),
           'volume':(0,0,40), 'speaker grill':(36,0,0), 'prev button':(0,-34,0), 'next button':(0,34,0)}
    items = [part(k, offset=off[k]) for k in P]
    im, prj = render(items, view=(1.5,-0.55,0.85), size=(1500,1500), ss=2, margin=0.05)
    anc = {}
    for k in P:
        V = P[k]['V']; c = (V.min(0)+V.max(0))/2 + np.asarray(off[k]); anc[k] = c
    anc['frame'] = np.array([84.2, 70, 30.0]); anc['back'] = np.array([84.0, 60, 11.0-22]); anc['door'] = np.array([60, 10, 9.9-44])
    anc['bumper'] = np.array([78.5, 50, 6-62]); anc['front'] = np.array([82,70,ztop(82,70,95)+26])
    anc['token'] = np.array([37.7+20, 45, ztop(57,45,101)+50]); anc['volume'] = np.array([17.5,17.5,104+40])
    anc['speaker grill'] = np.array([87.2,45,70+0]) + np.array([36,0,0])
    save(im, 'exploded', anc, prj); tic('exploded', t0)
if 'tiles' in which:
    t0=time.time()
    TV = (1,-1.3,1.05); TS = (620,470)
    def tile_items(name, up):
        p = oriented(name, up); V = p['V']; lo, hi = V.min(0), V.max(0); m = 5 + 0.06*max(hi[0]-lo[0], hi[1]-lo[1])
        bed = prim.box(((lo[0]+hi[0])/2,(lo[1]+hi[1])/2,-0.75),(hi[0]-lo[0]+2*m, hi[1]-lo[1]+2*m, 1.5), BED)
        return [bed, p]
    s_frame = autoscale(tile_items('frame',(0,0,1)), TV, TS)
    for name, up in TILES:
        its = tile_items(name, up); s = min(autoscale(its, TV, TS), 2.3*s_frame)
        im, prj = render(its, view=TV, size=TS, ss=2, scale=s)
        save(im, 'tile_'+name.replace(' ','_'))
    tic('tiles', t0)
if 'token' in which:
    t0=time.time()
    tok = oriented('token', -t)
    sec = cutp(tok, (0,0,0), (0,1,0), cut_rgb=hexc('#eeb397'))
    tag = cutp(prim.cylinder((0,0,14.4),(0,0,1),12.5,0.28,NFC), (0,0,0), (0,1,0), cut_rgb=NFC, hatch=False)
    bed = cutp(prim.box((0,0,-0.75),(54,30,1.5),BED), (0,0,0), (0,1,0), cut_rgb=hexc('#cfd5da'), hatch=False)
    view = (0,1,0.10)
    im, prj = render([bed, sec, tag], view=view, size=(1600,620), ss=2, margin=0.04)
    save(im, 'token_section', {'top':(12,0,0.3), 'flange':(21.7,0,3.0), 'void':(-4,0,7.0), 'body':(19.7,0,10.0),
         'pocket':(-12.9,0,14.6), 'chamfer':(-19.75,0,14.85), 'bed':(-24,0,-0.75)}, prj)
    tic('token', t0)
if 'akkus' in which:
    t0=time.time()
    WRAP = hexc('#3e58a6'); NICKEL = hexc('#c9cdd2'); KAPTON = hexc('#d9952e'); GLUEC = hexc('#efe2a2'); RED = hexc('#c8322b'); BLK = hexc('#2b2b2b')
    CA = (71.1, 13.1); CB = (5.3, 76.95); Z0 = 14.55
    def cell(cx, cy):
        return [prim.cylinder((cx,cy,Z0),(0,0,1),9.2,64.9,WRAP), prim.cylinder((cx,cy,Z0+64.9),(0,0,1),9.0,0.35,NICKEL),
                prim.cylinder((cx,cy,Z0+65.25),(0,0,1),3.8,0.9,NICKEL), prim.cylinder((cx,cy,Z0+66.15),(0,0,1),5.8,0.3,KAPTON)]
    def blobs(beads): return [prim.ellipsoid((x,y,z),(rx,ry,6.0),GLUEC) for (x,y,rx,ry) in beads for z in (22, 35, 48, 61, 72)]
    gA = [(80.1,17.6,1.7,2.4),(67.6,3.8,2.4,1.5)]; gB = [(-3.9,72.6,1.7,2.4),(8.2,86.4,2.4,1.5)]
    wires = [prim.tube([(71.1,13.1,81.3),(71.1,13.1,82.8),(66.0,18.4,82.8),(59.5,25.0,81.0)],0.85,RED),
             prim.tube([(64.6,19.9,15.7),(57.6,27.1,15.7),(52.0,32.6,16.4)],0.85,BLK),
             prim.tube([(5.3,76.95,81.3),(5.3,76.95,82.8),(10.6,72.0,82.8),(18.2,65.1,81.0)],0.85,RED),
             prim.tube([(12.3,70.6,15.7),(19.8,63.7,15.7),(25.3,58.6,16.4)],0.85,BLK)]
    ctx = [prim.cylinder((38.6,1.05,45.6),(0,1,0),17.5,0.3,BRASS), prim.cylinder((38.6,1.35,45.6),(0,1,0),12.5,0.25,CERAMIC),
           prim.box((-1.6,45,16.9),(13.0,12.0,1.2),PCB), prim.box((-6.12,45,19.25),(7.35,8.94,3.26),SILVER)]
    p0 = (49.0,49.0,0); n = (1,1,0)
    pw, pa = power_parts()
    items = [cutp(part('frame'),p0,n), cutp(part('back'),p0,n), cutp(part('door'),p0,n)] + ctx + cell(*CA) + cell(*CB) + blobs(gA) + blobs(gB) + wires + pw
    im, prj = render(items, view=(0.78,0.72,0.85), size=(1500,1150), ss=2, margin=0.05)
    save(im, 'akkus', {'cellA':(77.8,19.4,50), 'cellB':(12.1,83.2,40), 'kapton':(71.1,13.1,81.0), 'red':(62.6,21.8,82.0),
         'black':(19.8,63.7,15.7), 'window':(30,1.2,24), 'usb':(-2.5,45,20.9), 'floor':(40,40,9.9), 'latch':pa['latch'], 'boost':pa['boost']}, prj, crop=True)
    det = [part('frame'), part('back')] + cell(*CA) + blobs(gA) + [wires[0]]
    fitp = [(54,-3,83),(88,-3,83),(54,33,83),(88,33,83),(54,-3,60),(88,33,60)]
    im, prj = render(det, view=(-0.15,0.15,1.0), up=(1,-1,0), size=(800,760), ss=2, fit=fitp, margin=0.03)
    save(im, 'akku_detail', {'glue1':(80.3,18.2,78.0), 'glue2':(67.0,3.6,78.0), 'kapton':(71.1,13.1,81.0), 'window':(59,1.1,74), 'cell':(63.1,17.7,70)}, prj)
    tic('akkus', t0)

if 'lid_full' in which:
    t0=time.time()
    st, (R, T, info) = stack(groups=('rc522',), spacer=False)
    items = [part('front')] + lid_parts() + st
    im, prj = render(items, view=(0.35,-0.45,-1.0), size=(1400,1100), ss=2, margin=0.05)
    hx, hy = info['header']
    save(im, 'lid_full', {'led':L(65,17.5,90.0), 'poti':L(10,17.5,90.5), 'switch':L(12.9,47,88.6), 'rc522':L(44,58,86.3),
         'header':T + hx*R[0] + hy*R[1] - 7.0*R[2], 'lugs':L(1.6,17.5,86.0)}, prj)
    tic('lid_full', t0)
if 'lidopen' in which:
    t0=time.time(); p0=(49.0,49.0,0); n=(1,1,0)
    box_items = [cutp(part('frame'),p0,n), cutp(part('back'),p0,n), cutp(part('door'),p0,n), cutp(part('speaker grill'),p0,n)]
    WRAP = hexc('#3e58a6'); NICKEL = hexc('#c9cdd2'); KAPTON = hexc('#d9952e')
    for cx_, cy_ in ((71.1, 13.1), (5.3, 76.95)):
        box_items += [prim.cylinder((cx_,cy_,14.55),(0,0,1),9.2,64.9,WRAP), prim.cylinder((cx_,cy_,79.45),(0,0,1),9.0,0.35,NICKEL),
                      prim.cylinder((cx_,cy_,79.8),(0,0,1),3.8,0.9,NICKEL), prim.cylinder((cx_,cy_,80.7),(0,0,1),5.8,0.3,KAPTON)]
    box_items += [prim.cylinder((38.6,1.05,45.6),(0,1,0),17.5,0.3,BRASS), prim.cylinder((38.6,1.35,45.6),(0,1,0),12.5,0.25,CERAMIC),
                  prim.box((-1.6,45,16.9),(13.0,12.0,1.2),PCB), prim.box((-6.12,45,19.25),(7.35,8.94,3.26),SILVER)]
    pw, pa = power_parts(); box_items += pw
    st, (R, T, info) = stack(tie=True)
    # lid swung open about its rear lower edge, underside towards the viewer
    H = np.array([-10.6, 0.0, 89.4]); ph = np.radians(-112)
    M = np.array([[np.cos(ph), 0, np.sin(ph)], [0, 1, 0], [-np.sin(ph), 0, np.cos(ph)]])
    rot = lambda v: (np.asarray(v, float) - H) @ M.T + H
    lid_items = el.place([part('front'), part('volume')] + lid_parts() + st, M.T, H - H @ M.T)
    b_ = load_board(str(ROOT/'hardware'/'mainboard'/'timon-box.kicad_pcb')); f = next(x for x in b_['footprints'] if x['ref'] == 'J4')
    BB = el.Board(b_); pins = sorted([BB.L(q['x'], q['y']) for q in f['pads']]); plug_c = np.mean(pins, axis=0)
    plug = el.place([el.box(min(q[0] for q in pins)-2.45, max(q[0] for q in pins)+2.45, plug_c[1]-2.9, plug_c[1]+2.85, 7.0, 12.5, el.BEIGE)], R, T)
    lid_items += el.place(plug, M.T, H - H @ M.T)
    d = R[2] @ M.T; cols = ['#d23a2f', '#2b2b2b', '#2f6fd0', '#f2c230', '#f08a24', '#3a9a4a', '#f4f4f0']; wires_ = []
    for i, (x, y) in enumerate(pins):
        a_ = rot(T + x*R[0] + y*R[1] + 12.5*R[2]); b2 = a_ + 7.0*d; e = np.array([2.5, 51.0 - 1.3*i, 44.0])
        c1 = b2 + 16.0*d; c2 = e + np.array([3.0, 0, 18.0])
        pts = [a_, b2] + [((1-s)**3)*b2 + 3*s*(1-s)**2*c1 + 3*s*s*(1-s)*c2 + s**3*e for s in np.linspace(0.08, 1, 14)]
        wires_.append(prim.tube(pts, 0.5, hexc(cols[i]), seg=10))
    items = box_items + lid_items + wires_
    im, prj = render(items, view=(0.85,0.62,0.5), size=(1500,1400), ss=2, margin=0.04)
    hx, hy = info['header']
    save(im, 'lidopen', {'mainboard':rot(T + 16*R[0] + 22*R[1] + 0.0*R[2]), 'rc522':rot(T - 2*R[0] - 31*R[1] - 13.4*R[2]),
         'modules':rot(T - 10*R[0] - 20*R[1] + 10.6*R[2]), 'esp32':rot(T + 14*R[0] + 6*R[1] + 3.2*R[2]), 'lid':rot(L(70,85,95.0)),
         'cable':np.array([14.0, 46.0, 66.0]), 'latch':pa['latch'], 'boost':pa['boost'], 'cellA':(77.8,19.4,50), 'cellB':(12.1,83.2,40),
         'led':rot(L(65,17.5,90.0)), 'poti':rot(L(10,17.5,90.5))}, prj, crop=True)
    tic('lidopen', t0)

if 'spacer_stack' in which:
    t0=time.time()
    items, info = el.mainboard_stack(load_board(str(ROOT/'hardware'/'mainboard'/'timon-box.kicad_pcb')))
    items = items + spacer_local(info)
    k = info['inward']; fe = info['free_edge']
    im, prj = render(items, view=(0.95, -1.45*k, 0.55), size=(1400,1000), ss=2, margin=0.05)
    save(im, 'spacer_stack', {'spacer':(8.0*k, fe+1.3*k, -6.0), 'arm':(21.3*k, fe+3.0*k, 1.0), 'mainboard':(-8.0*k, fe+14.0*k, 0.0),
         'rc522':(-10.0*k, fe+3.0*k, -13.4)}, prj, crop=True)
    tic('spacer_stack', t0)
if 'strap' in which:
    t0=time.time(); STRAP_APEX = strap_apex(STRAP_LENGTH)
    im, prj = render([part(k) for k in P if k != 'pcb_spacer'] + [strap()], view=(1.0,-1.25,0.8), size=(1400,1300), ss=2, margin=0.05)
    save(im, 'strap', {'apex':(39.7, 45.0, STRAP_APEX), 'loop':(39.7, -6.0, 77.0), 'fold':(39.7, -6.9, 84.0), 'loop_r':(39.7, 96.0, 77.0)}, prj, crop=True)
    tic('strap', t0)

if 'lid_stack' in which:
    t0=time.time()
    st, (R, T, info) = stack(tie=True)
    items = [part('front')] + lid_parts() + st
    im, prj = render(items, view=(0.35,-0.45,-1.0), size=(1400,1150), ss=2, margin=0.05)
    B = lambda X, Y, Z: T + X*R[0] + Y*R[1] + Z*R[2]
    fe = info['free_edge']; hx, hy = info['header']; k = info['inward']
    save(im, 'lid_stack', {'tie':B(12.0, fe+1.4*k, 9.7), 'spacer':B(21.6, fe+3.0*k, 1.0), 'mainboard':B(10.0, -5.0, 0.0), 'rc522':B(18.0, fe+6.0*k, -13.4),
         'header':B(hx, hy, -5.0), 'modules':B(-12.0, 4.0, 10.1)}, prj, crop=True)
    tic('lid_stack', t0)

if 'readme_hero' in which:
    t0=time.time()
    im, prj = render([part(k) for k in P if k != 'pcb_spacer'] + [strap()], view=VA, size=(1100,1200), ss=2, margin=0.04)
    save(im, 'readme_hero', {}, prj, crop=True)
    tic('readme_hero', t0)

if 'thingiverse' in which:
    # Images for the Thingiverse page (not part of the PDF): 4:3 on white, the object centred with room around it,
    # because Thingiverse crops previews to other ratios. Written to fig/thingiverse/.
    t0 = time.time(); TV = OUT + 'thingiverse/'; os.makedirs(TV, exist_ok=True); SZ = (1600, 1200)
    Ct, nt, At = tri_info(P['token'])
    TOP = {max(fids('token', t), key=lambda f: float((Ct[P['token']['FID'] == f] @ t).max()))}   # picture side
    PIC = [hexc('#e8a33d'), hexc('#d8643f'), hexc('#4f86c6'), hexc('#7bb662')]
    ZMIN = min(float(P[k]['V'][:, 2].min()) for k in P if k != 'pcb_spacer')
    def box_items(off=(0, 0, 0), body=None, pic=PIC[0], strap_=True):
        its = []
        for k in P:
            if k == 'pcb_spacer': continue
            kw = dict(offset=off)
            if body is not None and k in ('frame', 'door', 'volume'): kw['color'] = body
            if k == 'token': kw['face_colors'] = [(TOP, pic)]
            its.append(part(k, **kw))
        if strap_:
            st_ = strap(); st_ = dict(st_); st_['offset'] = off; its.append(st_)
        return its
    def loose_token(x, y, pic, turn=0.0):
        p = oriented('token', t, face_colors=[(TOP, pic)])
        c, s_ = np.cos(np.radians(turn)), np.sin(np.radians(turn)); Rz = np.array([[c, -s_, 0], [s_, c, 0], [0, 0, 1.0]])
        p['V'] = p['V'] @ Rz.T; p['N'] = p['N'] @ Rz.T; p['offset'] = (x, y, ZMIN); return p
    def fitpts(its): return np.concatenate([it['V'] + np.asarray(it.get('offset', (0, 0, 0)), float) for it in its])
    def canvas(im, name, fill=0.88):
        a = np.asarray(im.convert('L')); ys, xs = np.nonzero(a < 248)
        im = im.crop((int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1))
        s_ = min(SZ[0]*fill/im.width, SZ[1]*fill/im.height); im = im.resize((round(im.width*s_), round(im.height*s_)), Image.LANCZOS)
        c = Image.new('RGB', SZ, 'white'); c.paste(im, ((SZ[0]-im.width)//2, (SZ[1]-im.height)//2)); c.save(TV + name + '.jpg', quality=92, subsampling=0)
        return s_, ((SZ[0]-im.width)//2, (SZ[1]-im.height)//2), (int(xs.min()), int(ys.min()))
    # 1: two boxes and a few tokens
    A = box_items(pic=PIC[0]); B = box_items(off=(64, 128, 0), body=hexc('#c4573a'), pic=PIC[2], strap_=False)
    toks = [loose_token(150, -40, PIC[1], 10), loose_token(118, -92, PIC[3], 40), loose_token(196, 4, PIC[2], 70)]
    im, _ = render(A + B + toks, view=VA, size=SZ, ss=2, margin=0.09)
    im.save(TV + '01_cover.jpg', quality=92, subsampling=0)
    # 2: back with USB-C, charge light and the strap lugs
    A = box_items(pic=PIC[1])
    im, _ = render(A, view=(-1.5, 0.8, 0.75), size=SZ, ss=2, margin=0.1)
    im.save(TV + '02_back.jpg', quality=92, subsampling=0)
    # 3: exploded view
    canvas(Image.open(OUT + 'exploded.jpg'), '03_exploded', 0.92)
    # 5: open lid, electronics inside
    canvas(Image.open(OUT + 'lidopen.jpg'), '05_inside', 0.92)
    # 6: token in section with the NFC tag, labelled in English
    src = Image.open(OUT + 'token_section.jpg'); anc = json.load(open(OUT + 'token_section.json'))['anchors']
    s_, (ox, oy), (cx, cy) = canvas(src, '06_token', 0.86)
    im = Image.open(TV + '06_token.jpg'); dr = ImageDraw.Draw(im)
    fnt = ImageFont.truetype(str(HERE/'fonts'/'IBMPlexSans-Medium.ttf'), 34); sub = ImageFont.truetype(str(HERE/'fonts'/'IBMPlexSans-Regular.ttf'), 28)
    def at(k): return ((anc[k][0]-cx)*s_ + ox, (anc[k][1]-cy)*s_ + oy)
    ink = (34, 38, 42)
    for k, ty, head, line2 in (('pocket', 150, 'NFC tag', 'drop it in during a print pause before Z = 15.0 mm'),
                               ('top', 1040, 'Picture side on the bed', 'the top of the token prints face down')):
        x_, y_ = at(k); w = max(fnt.getlength(head), sub.getlength(line2))
        tx = min(max(100, x_ - 40), SZ[0] - 100 - w)               # keep the text on the canvas
        dr.line([(x_, y_), (x_, ty + (76 if ty < 600 else -14))], fill=ink, width=3); dr.ellipse([x_-7, y_-7, x_+7, y_+7], fill=ink)
        dr.text((tx, ty - 6), head, font=fnt, fill=ink); dr.text((tx, ty + 34), line2, font=sub, fill=(90, 96, 102))
    im.save(TV + '06_token.jpg', quality=92, subsampling=0)
    # 4: the ready-made plates from export_print_files.py, one per colour
    from export_print_files import PLATES, layout
    UPS = dict(TILES); CAPS = ['1 · Frame, hatch, knob', '2 · Lid, bottom ring, buttons', '3 · Grill, spacer',
                               '4 · Bumper, TPU', '5 · Tokens, pause at 15 mm']
    sheet = Image.new('RGB', SZ, 'white'); dr = ImageDraw.Draw(sheet); cap = ImageFont.truetype(str(HERE/'fonts'/'IBMPlexSans-Medium.ttf'), 30)
    cw, ch = SZ[0] // 3, SZ[1] // 2
    for i, (name, cols) in enumerate(PLATES):
        ps = {k: oriented(k, UPS[k]) for col in cols for k in col}
        if 'token' in ps: ps['token']['face_colors'] = [(TOP, PIC[0])]
        its = [dict(ps[k], offset=(xy[0], xy[1], 0.0)) for k, xy in layout(cols, ps)]
        bed = prim.box((120, 105, -0.75), (230, 220, 1.5), BED)
        im, _ = render(its + [bed], view=(0.55, -1.0, 1.25), size=(cw, ch - 60), ss=2, margin=0.06)
        x0, y0 = (i % 3) * cw, (i // 3) * ch; sheet.paste(im, (x0, y0))
        w = dr.textlength(CAPS[i], font=cap); dr.text((x0 + (cw - w) / 2, y0 + ch - 70), CAPS[i], font=cap, fill=(34, 38, 42))
    note_ = ImageFont.truetype(str(HERE/'fonts'/'IBMPlexSans-Regular.ttf'), 28); x0, y0 = 2 * cw + 30, ch + 150
    dr.text((x0, y0), 'Ready-made 3MF plates,', font=cap, fill=(34, 38, 42)); dr.text((x0, y0 + 42), 'one per colour', font=cap, fill=(34, 38, 42))
    for j, line in enumerate(['PrusaSlicer: open as project,', 'supports and the token pause', 'are already set.']):
        dr.text((x0, y0 + 110 + 38 * j), line, font=note_, fill=(90, 96, 102))
    sheet.save(TV + '04_plates.jpg', quality=92, subsampling=0)
    # 7: the board stack with the printed spacer
    canvas(Image.open(OUT + 'spacer_stack.jpg'), '07_electronics', 0.9)
    tic('thingiverse', t0)

if 'inserts' in which:
    t0=time.time()
    ins = [prim.cylinder((x,y,10.0-18),(0,0,1),2.3,4.0,BRASS) for x,y in HOLES]
    im, prj = render([part('back')]+ins, view=(0.62,-0.78,-0.95), size=(1400,900), ss=2, margin=0.06)
    A = {}
    for i,(x,y) in enumerate(HOLES):
        A[f'ins{i}'] = (x,y,14.0-18); A[f'insb{i}'] = (x,y,10.0-18); A[f'hole{i}'] = (x,y,10.0)
    A['ledge'] = (40, 87.6, 10.0); A['side'] = (-9.0, 45, 11)
    save(im, 'inserts', A, prj); tic('inserts', t0)
if 'shell' in which:
    t0=time.time()
    items = [part('back', offset=(0,0,-24), face_colors=[(F_back_top, GLUE)]), part('frame', face_colors=[(F_frame_top, GLUE)]),
             part('front', offset=(0,0,26)), part('prev button', offset=(0,-26,0), face_colors=[(F_prev_in, GLUE)]),
             part('next button', offset=(0,26,0), face_colors=[(F_next_in, GLUE)])]
    im, prj = render(items, view=(1.45,-0.6,0.85), size=(1500,1300), ss=2, margin=0.05)
    save(im, 'shell', {'back':(84,40,11-24), 'back_glue':(70,88,14.5-24), 'frame':(84.2,70,30), 'frame_top':(60,89.5,ztop(60,89.5,87.988)),
         'front':(84,30,ztop(84,30,92)+26), 'prev':(38.6,-27,45.6), 'prev_to':(38.6,-0.5,45.6), 'prev_from':(38.6,-24,45.6),
         'next':(38.6,116,45.6), 'next_from':(38.6,115.5,45.6), 'next_to':(38.6,100,45.6), 'curved':(-9.8,30,40)}, prj)
    tic('shell', t0)
if 'inside' in which:
    t0=time.time(); p0=(0,52,0); n=(0,1,0)
    items = [cutp(part('frame'),p0,n), cutp(part('back'),p0,n), cutp(part('door'),p0,n), cutp(part('speaker grill'),p0,n), part('prev button')]
    items += [prim.cylinder((38.6,1.05,45.6),(0,1,0),17.5,0.3,BRASS), prim.cylinder((38.6,1.35,45.6),(0,1,0),12.5,0.25,CERAMIC)]
    items += [prim.box((-1.6,45,16.9),(13.0,12.0,1.2),PCB), prim.box((-6.12,45,19.25),(7.35,8.94,3.26),SILVER)]
    items += [prim.cylinder((79.2,45,49.81),(-1,0,0),26.0,2.0,SPK), prim.cylinder((77.2,45,49.81),(-1,0,0),23.5,10.5,SPK,r2=11.0),
              prim.cylinder((66.7,45,49.81),(-1,0,0),14.5,8.5,MAG)]
    im, prj = render(items, view=(0.5,1.0,0.5), size=(1500,1150), ss=2, margin=0.05)
    save(im, 'inside', {'piezo':(38.6,1.6,45.6), 'wall':(14,1.0,24), 'usb':(-2.5,45,20.9), 'speaker':(60,45,58), 'grill':(83.5,46,30),
         'cut':(84,52,60), 'prev':(38.6,-1,40)}, prj)
    tic('inside', t0)
if 'lid' in which:
    t0=time.time()
    im, prj = render([part('front', face_colors=[(F_front_cup, TAPE)])], view=(0.35,-0.45,-1.0), size=(1400,1100), ss=2, margin=0.05)
    save(im, 'lid', {'led':(65,17.5,ztop(65,17.5,95.988)), 'poti':(10,17.5,ztop(10,17.5,95.988)), 'jack':(10,72.5,ztop(10,72.5,97.988)),
         'switch':(12.9,45,ztop(12.9,45,88.0)), 'cup':(42,52,ztop(42,52,87.988)), 'slot':(18.3,45,ztop(18.3,45,88.6)), 'skirt':(84,60,ztop(84,60,88.2))}, prj)
    tic('lid', t0)
if 'door' in which:
    t0=time.time()
    ins = [prim.cylinder((x,y,10.0),(0,0,1),2.3,4.0,BRASS) for x,y in HOLES]
    scr = []
    for x,y in HOLES:
        scr += [prim.cylinder((x,y,6.0-50),(0,0,1),2.75,3.0,STEEL), prim.cylinder((x,y,9.0-50),(0,0,1),1.5,5.0,STEEL)]
    items = [part('back')] + ins + [part('door', offset=(0,0,-18)), part('bumper', offset=(0,0,-34), face_colors=[(F_bumper_top, GLUE)])] + scr
    im, prj = render(items, view=(0.9,-1.15,0.72), size=(1400,1250), ss=2, margin=0.05)
    A = {'back':(84,30,12), 'door':(76,30,9.9-18), 'bumper':(78.5,60,7-34), 'bumper_glue':(40,5.9,7-34)}
    for i,(x,y) in enumerate(HOLES):
        A[f'scr{i}'] = (x,y,14-50); A[f'scrh{i}'] = (x,y,6-50); A[f'dhole{i}'] = (x,y,7-34)
    save(im, 'door', A, prj); tic('door', t0)
