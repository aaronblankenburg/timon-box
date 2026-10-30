"""Software renderer v2: z-buffer, smooth per-face normals, outlines, cut faces, highlights, SSAO."""
import numpy as np, math
from PIL import Image
from scipy import ndimage

def nrm(v): v=np.asarray(v,float); return v/np.linalg.norm(v)
def camera(view, up=(0,0,1)):
    z = nrm(view); x = np.cross(up, z)
    if np.linalg.norm(x) < 1e-6: x = np.cross((0,1,0), z)
    x = nrm(x); y = np.cross(z, x); return np.stack([x,y,z])
def to_lin(c): c=np.asarray(c,float); return np.where(c<=0.04045, c/12.92, ((c+0.055)/1.055)**2.4)
def to_srgb(c): c=np.clip(c,0,1); return np.where(c<=0.0031308, 12.92*c, 1.055*np.power(c,1/2.4)-0.055)
def hexc(h): h=h.lstrip('#'); return np.array([int(h[i:i+2],16)/255 for i in (0,2,4)])

def clip_mesh(V, N, F, FID, p0, n):
    """keep part with dot(v-p0,n) <= 0"""
    n = nrm(n); d = (V - p0) @ n
    keep_v = d <= 0
    kf = keep_v[F]
    full = kf.all(1); none = ~kf.any(1); cross = ~full & ~none
    V2 = [V]; N2 = [N]; F2 = [F[full]]; FID2 = [FID[full]]
    nv = len(V); extraV=[]; extraN=[]; newF=[]; newFID=[]
    for t in np.nonzero(cross)[0]:
        idx = F[t]; poly=[]; 
        for i in range(3):
            a, b = idx[i], idx[(i+1)%3]
            da, db = d[a], d[b]
            if da <= 0: poly.append(('v', a))
            if (da <= 0) != (db <= 0):
                s = da/(da-db)
                extraV.append(V[a]+(V[b]-V[a])*s); nn = N[a]+(N[b]-N[a])*s
                extraN.append(nn/max(np.linalg.norm(nn),1e-9)); poly.append(('n', nv+len(extraV)-1))
        ids = [p[1] for p in poly]
        for k in range(1, len(ids)-1):
            newF.append((ids[0], ids[k], ids[k+1])); newFID.append(FID[t])
    if extraV:
        V2.append(np.array(extraV)); N2.append(np.array(extraN))
    if newF:
        F2.append(np.array(newF)); FID2.append(np.array(newFID))
    return np.concatenate(V2), np.concatenate(N2), np.concatenate(F2), np.concatenate(FID2)

def render(items, view=(1,-1.2,0.9), up=(0,0,1), size=(1600,1100), ss=2, margin=0.05, bg=(1,1,1),
           fit=None, scale=None, center=None, outline=True, crease_deg=35, ssao=True, line_rgb=(0.13,0.14,0.15),
           cut_rgb=hexc('#e07a5f'), hatch=True, transparent=False):
    R = camera(view, up)
    W, H = size[0]*ss, size[1]*ss
    allP=[]
    for it in items:
        V = it['V'] + np.asarray(it.get('offset',(0,0,0)),float)
        it['_P'] = V @ R.T; it['_Nc'] = it['N'] @ R.T
        allP.append(it['_P'])
    Pfit = np.concatenate(allP) if fit is None else np.asarray(fit,float) @ R.T
    mn, mx = Pfit[:,:2].min(0), Pfit[:,:2].max(0)
    s = (min((W*(1-2*margin))/(mx[0]-mn[0]), (H*(1-2*margin))/(mx[1]-mn[1])) if scale is None else scale*ss)
    c = (mn+mx)/2 if center is None else np.asarray(center,float)
    def proj(P):
        return (P[:,0]-c[0])*s + W/2, H/2 - (P[:,1]-c[1])*s, P[:,2]
    zbuf = np.full((H,W), -np.inf, np.float32); nbuf = np.zeros((H,W,3), np.float32)
    pid = np.full((H,W), -1, np.int16); fidb = np.full((H,W), -1, np.int32); back = np.zeros((H,W), bool)
    colb = np.zeros((H,W,3), np.float32)
    for k,it in enumerate(items):
        sx, sy, sz = proj(it['_P']); F = it['F']; Nc = it['_Nc']
        fidarr = it.get('FID', np.zeros(len(F),int))
        base = np.asarray(it['color'],float)
        fcol = np.tile(base,(len(F),1))
        for fid_set, col in it.get('face_colors', []):
            m = np.isin(fidarr, list(fid_set)); fcol[m] = col
        fcol_lin = to_lin(fcol)
        x0,x1,x2 = sx[F[:,0]], sx[F[:,1]], sx[F[:,2]]; y0,y1,y2 = sy[F[:,0]], sy[F[:,1]], sy[F[:,2]]
        area = (x1-x0)*(y2-y0) - (x2-x0)*(y1-y0)
        cull = not it.get('cut', False)
        keep = np.abs(area) > 1e-9
        if cull: keep &= area < 0
        for t in np.nonzero(keep)[0]:
            a,b,cc = F[t]
            xs = (x0[t],x1[t],x2[t]); ys=(y0[t],y1[t],y2[t])
            minx = max(int(math.floor(min(xs))),0); maxx = min(int(math.ceil(max(xs))),W-1)
            miny = max(int(math.floor(min(ys))),0); maxy = min(int(math.ceil(max(ys))),H-1)
            if minx>maxx or miny>maxy: continue
            gx, gy = np.meshgrid(np.arange(minx,maxx+1)+0.5, np.arange(miny,maxy+1)+0.5)
            ar = area[t]
            w0 = ((x1[t]-gx)*(y2[t]-gy) - (x2[t]-gx)*(y1[t]-gy))/ar
            w1 = ((x2[t]-gx)*(y0[t]-gy) - (x0[t]-gx)*(y2[t]-gy))/ar
            w2 = 1-w0-w1
            m = (w0>=-1e-6)&(w1>=-1e-6)&(w2>=-1e-6)
            if not m.any(): continue
            z = w0*sz[a]+w1*sz[b]+w2*sz[cc]
            sl = (slice(miny,maxy+1), slice(minx,maxx+1))
            sub = zbuf[sl]; upd = m & (z > sub)
            if not upd.any(): continue
            sub[upd] = z[upd]
            nbuf[sl][upd] = (w0[...,None]*Nc[a] + w1[...,None]*Nc[b] + w2[...,None]*Nc[cc])[upd]
            pid[sl][upd] = k; fidb[sl][upd] = fidarr[t]; back[sl][upd] = ar > 0
            colb[sl][upd] = fcol_lin[t]
    mask = pid >= 0
    nn = nbuf / np.maximum(np.linalg.norm(nbuf,axis=2,keepdims=True),1e-9)
    nn[back] *= -1
    L1 = nrm((-0.45, 0.65, 0.62)); L2 = nrm((0.75, 0.05, 0.65)); L3 = nrm((0.1,-0.8,0.35))
    d1 = np.clip(nn@L1,0,1); d2 = np.clip(nn@L2,0,1); d3 = np.clip(nn@L3,0,1)
    hemi = 0.5 + 0.5*nn[...,1]
    spec = np.clip(nn@nrm(L1+np.array([0,0,1.0])),0,1)**48
    ao = np.ones((H,W),np.float32)
    if ssao:
        z = np.where(mask, zbuf, -1e6).astype(np.float32)
        acc = np.zeros((H,W),np.float32); cnt=0
        for rad_mm in (1.5, 4.0):
            r = max(2, min(int(rad_mm*s), min(H,W)//6))
            for ang in np.linspace(0, 2*np.pi, 8, endpoint=False):
                dx, dy = int(round(r*np.cos(ang))), int(round(r*np.sin(ang)))
                zs = np.full_like(z, -1e6)
                ys_ = slice(max(dy,0), H+min(dy,0)); yd = slice(max(-dy,0), H+min(-dy,0))
                xs_ = slice(max(dx,0), W+min(dx,0)); xd = slice(max(-dx,0), W+min(-dx,0))
                zs[yd, xd] = z[ys_, xs_]
                diff = zs - z
                occ = np.clip(diff/(rad_mm*1.2), 0, 1) * (diff < rad_mm*4)
                acc += occ; cnt += 1
        ao = 1 - 0.55*np.clip(acc/cnt, 0, 1)
        ao = ndimage.uniform_filter(ao, size=3)
        ao[back] = 1.0
    shade = (0.28 + 0.12*hemi)*ao + (0.62*d1 + 0.20*d2 + 0.08*d3)*(0.55+0.45*ao)
    col = colb*shade[...,None] + 0.10*spec[...,None]*ao[...,None]
    # ghost / cut handling
    img = np.empty((H,W,3),np.float32); img[:] = np.asarray(bg,float)
    out = to_srgb(col)
    ghost = np.zeros((H,W),bool)
    for k,it in enumerate(items):
        if it.get('ghost'): ghost |= (pid==k)
    out[ghost] = out[ghost]*0.35 + 0.65*np.array([0.97,0.97,0.97])
    for k,it in enumerate(items):
        cutm = back & (pid==k)
        if not cutm.any(): continue
        cc = np.tile(np.asarray(it.get('cut_rgb', cut_rgb),float), (cutm.sum(),1))
        if hatch and it.get('hatch', True):
            yy, xx = np.nonzero(cutm)
            hm = ((xx + yy) % (9*ss)) < ss*1.4
            cc[hm] = cc[hm]*0.78
        out[cutm] = cc
    img[mask] = out[mask]
    if outline:
        e = np.zeros((H,W),bool); eg = np.zeros((H,W),bool)
        cosT = math.cos(math.radians(crease_deg))
        z = np.where(mask, zbuf, -1e9)
        dz_thr = max(0.5, 3.0/s*ss)
        for dy,dx in ((0,1),(1,0),(1,1),(1,-1)):
            a = (slice(0, H-dy), slice(max(0,-dx), W-max(dx,0)))
            b = (slice(dy, H), slice(max(dx,0), W-max(-dx,0)))
            both = mask[a] & mask[b]
            ee = (pid[a] != pid[b]) | (back[a] != back[b])
            nb = both & ~(back[a] & back[b])
            ee |= nb & (np.abs(z[a]-z[b]) > dz_thr*(1.0 + (dx!=0 and dy!=0)))
            ee |= nb & ((nn[a]*nn[b]).sum(-1) < cosT) & (fidb[a] != fidb[b])
            e[a] |= ee; e[b] |= ee
        e = ndimage.binary_dilation(e, iterations=max(1, ss//2))
        g = e & ghost
        img[e & ~g] = img[e & ~g]*0.12 + np.asarray(line_rgb)*0.88
        img[g] = img[g]*0.55 + np.array([0.55,0.56,0.58])*0.45
    if transparent:
        alpha = (mask | (e if outline else mask)).astype(np.float32)
        rgba = np.dstack([np.clip(img,0,1), alpha])
        im = Image.fromarray((rgba*255).astype(np.uint8), 'RGBA')
        if ss > 1: im = im.convert('RGBa').resize(size, Image.LANCZOS).convert('RGBA')
    else:
        im = Image.fromarray((np.clip(img,0,1)*255).astype(np.uint8), 'RGB')
        if ss > 1: im = im.resize(size, Image.LANCZOS)
    def project(pts):
        P = np.asarray(pts,float) @ R.T; x,y,_ = proj(P); return np.stack([x/ss, y/ss],1)
    return im, project
