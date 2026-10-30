"""Render all figures of doc/assembly.pdf from models/box.step.

Usage: python3 figures.py [name ...]    (without names: render everything)
Writes fig/<name>.jpg|png plus fig/<name>.json with the pixel positions of the label anchors.
Coordinates are model millimetres: x to the front (speaker), y to the right (next button), z up."""
import pickle, json, sys, os, time
from pathlib import Path
import numpy as np
from PIL import Image
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

which = sys.argv[1:] or ['hero','overview','exploded','tiles','token','inserts','shell','inside','lid','door','akkus']
VA = (1.0,-1.25,0.8); VB = (-1.0,1.3,0.7)

if 'hero' in which:
    t0=time.time(); im, prj = render([part(k) for k in P], view=VA, size=(1400,1250), ss=2, transparent=True, margin=0.03)
    save(im, 'hero'); tic('hero', t0)
if 'overview' in which:
    t0=time.time()
    im, prj = render([part(k) for k in P], view=VA, size=(1000,920), ss=2, margin=0.07)
    save(im, 'overview_a', {'token':(37.7,45,ztop(37.7,45,103.988)), 'knob':(10.6,17.5,107.6), 'led':(65,17.5,ztop(65,17.5,98.988)),
         'grill':(87.2,45,49.8), 'prev':(38.6,-1,45.6), 'handle':(39.7,-6,77.2), 'lid':(70,60,ztop(70,60,98.988))}, prj)
    im, prj = render([part(k) for k in P], view=VB, size=(1000,920), ss=2, margin=0.07)
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
    TILES = [('frame',(0,0,1)), ('front',t), ('back',(0,0,-1)), ('door',(0,0,-1)), ('bumper',(0,0,-1)),
             ('speaker grill',(-1,0,0)), ('prev button',(0,1,0)), ('next button',(0,-1,0)), ('volume',-t), ('token',-t)]
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
    items = [cutp(part('frame'),p0,n), cutp(part('back'),p0,n), cutp(part('door'),p0,n)] + ctx + cell(*CA) + cell(*CB) + blobs(gA) + blobs(gB) + wires
    im, prj = render(items, view=(0.78,0.72,0.85), size=(1500,1150), ss=2, margin=0.05)
    save(im, 'akkus', {'cellA':(77.8,19.4,50), 'cellB':(12.1,83.2,40), 'kapton':(71.1,13.1,81.0), 'red':(62.6,21.8,82.0),
         'black':(19.8,63.7,15.7), 'window':(30,1.2,24), 'usb':(-2.5,45,20.9), 'floor':(40,40,9.9)}, prj, crop=True)
    det = [part('frame'), part('back')] + cell(*CA) + blobs(gA) + [wires[0]]
    fitp = [(54,-3,83),(88,-3,83),(54,33,83),(88,33,83),(54,-3,60),(88,33,60)]
    im, prj = render(det, view=(-0.15,0.15,1.0), up=(1,-1,0), size=(800,760), ss=2, fit=fitp, margin=0.03)
    save(im, 'akku_detail', {'glue1':(80.3,18.2,78.0), 'glue2':(67.0,3.6,78.0), 'kapton':(71.1,13.1,81.0), 'window':(59,1.1,74), 'cell':(63.1,17.7,70)}, prj)
    tic('akkus', t0)

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
