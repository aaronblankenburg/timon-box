import numpy as np
def _frame(axis):
    a = np.asarray(axis,float); a/=np.linalg.norm(a)
    t = np.array([1,0,0]) if abs(a[0])<0.9 else np.array([0,1,0])
    u = np.cross(a,t); u/=np.linalg.norm(u); v = np.cross(a,u); return a,u,v
def cylinder(center, axis, r, h, color, seg=72, r2=None):
    """center = center of bottom cap; axis direction; optional top radius r2 (cone frustum)"""
    a,u,v = _frame(axis); c = np.asarray(center,float); r2 = r if r2 is None else r2
    ang = np.linspace(0,2*np.pi,seg,endpoint=False)
    ring = np.cos(ang)[:,None]*u + np.sin(ang)[:,None]*v
    V=[];N=[];F=[];FID=[]
    # side
    b = c + r*ring; tp = c + a*h + r2*ring
    slope = (r-r2)/h
    ns = ring + slope*a; ns /= np.linalg.norm(ns,axis=1,keepdims=True)
    V += list(b)+list(tp); N += list(ns)+list(ns)
    for i in range(seg):
        j=(i+1)%seg; F += [(i,j,seg+j),(i,seg+j,seg+i)]; FID += [0,0]
    off = len(V)
    # bottom cap
    V += [c] + list(b); N += [-a]*(seg+1)
    for i in range(seg):
        j=(i+1)%seg; F.append((off, off+1+j, off+1+i)); FID.append(1)
    off = len(V)
    V += [c+a*h] + list(tp); N += [a]*(seg+1)
    for i in range(seg):
        j=(i+1)%seg; F.append((off, off+1+i, off+1+j)); FID.append(2)
    return dict(V=np.array(V),N=np.array(N),F=np.array(F),FID=np.array(FID),color=np.asarray(color,float))
def box(center, size, color, axes=np.eye(3)):
    c=np.asarray(center,float); hs=np.asarray(size,float)/2; A=np.asarray(axes,float)
    V=[];N=[];F=[];FID=[]
    for fi,(ax,sgn) in enumerate([(0,1),(0,-1),(1,1),(1,-1),(2,1),(2,-1)]):
        n = A[ax]*sgn; o1, o2 = [k for k in range(3) if k!=ax]
        u = A[o1]*hs[o1]; v = A[o2]*hs[o2]; p = c + n*hs[ax]
        quad = [p-u-v, p+u-v, p+u+v, p-u+v]
        base=len(V); V+=quad; N+=[n]*4
        if np.dot(np.cross(quad[1]-quad[0], quad[2]-quad[0]), n) > 0: F+=[(base,base+1,base+2),(base,base+2,base+3)]
        else: F+=[(base,base+2,base+1),(base,base+3,base+2)]
        FID+=[fi,fi]
    return dict(V=np.array(V),N=np.array(N),F=np.array(F),FID=np.array(FID),color=np.asarray(color,float))
def merge(*ms, color=None):
    V=[];N=[];F=[];FID=[];off=0;foff=0
    for m in ms:
        V.append(m['V']);N.append(m['N']);F.append(m['F']+off);FID.append(m['FID']+foff);off+=len(m['V']);foff+=m['FID'].max()+1
    return dict(V=np.concatenate(V),N=np.concatenate(N),F=np.concatenate(F),FID=np.concatenate(FID),color=ms[0]['color'] if color is None else color)

def fix_winding(m):
    V, F, N = m['V'], m['F'], m['N']; tri = V[F]
    fn = np.cross(tri[:,1]-tri[:,0], tri[:,2]-tri[:,0]); vn = N[F].sum(1)
    bad = (fn*vn).sum(1) < 0; F = F.copy(); F[bad] = F[bad][:, [0,2,1]]; m['F'] = F; return m
def ellipsoid(center, radii, color, nu=28, nv=14):
    c = np.asarray(center, float); a, b, cc = radii; V=[]; N=[]
    for v in np.linspace(0, np.pi, nv+1):
        for u in np.linspace(0, 2*np.pi, nu, endpoint=False):
            x, y, z = np.cos(u)*np.sin(v), np.sin(u)*np.sin(v), np.cos(v)
            V.append(c + np.array([a*x, b*y, cc*z])); n = np.array([x/a, y/b, z/cc]); L = np.linalg.norm(n)
            N.append(n/L if L > 0 else np.array([0, 0, 1.0]))
    F=[]
    for i in range(nv):
        for j in range(nu):
            p, q, r, s = i*nu+j, i*nu+(j+1)%nu, (i+1)*nu+j, (i+1)*nu+(j+1)%nu; F += [(p, r, q), (q, r, s)]
    return fix_winding(dict(V=np.array(V), N=np.array(N), F=np.array(F), FID=np.zeros(len(F), int), color=np.asarray(color, float)))
def tube(points, r, color, seg=16):
    P = [np.asarray(p, float) for p in points]; parts = []
    for a, b in zip(P[:-1], P[1:]):
        d = b-a; L = np.linalg.norm(d); parts.append(cylinder(a, d/L, r, L, color, seg=seg))
    for p in P[1:-1]: parts.append(ellipsoid(p, (r, r, r), color, nu=16, nv=8))
    return merge(*parts, color=np.asarray(color, float))
