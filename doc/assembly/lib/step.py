"""Load a STEP assembly with OpenCascade (OCP) and tessellate every part.

Each part comes back with vertices, smooth per-face normals, triangles, B-rep face ids and its sRGB colour."""
import numpy as np
from OCP.STEPCAFControl import STEPCAFControl_Reader
from OCP.TDocStd import TDocStd_Document
from OCP.TCollection import TCollection_ExtendedString
from OCP.XCAFDoc import XCAFDoc_DocumentTool, XCAFDoc_ColorType
from OCP.TDF import TDF_Label
from OCP.OCP.collections import Sequence_TDF_Label
from OCP.TDataStd import TDataStd_Name
from OCP.Quantity import Quantity_Color
from OCP.IFSelect import IFSelect_RetDone
from OCP.BRepMesh import BRepMesh_IncrementalMesh
from OCP.TopExp import TopExp_Explorer
from OCP.TopAbs import TopAbs_FACE
from OCP.BRep import BRep_Tool
from OCP.TopLoc import TopLoc_Location
from OCP.TopoDS import TopoDS


def _name(lab):
    n = TDataStd_Name()
    return n.Get().ToExtString() if lab.FindAttribute(TDataStd_Name.GetID_s(), n) else '?'


def _colour(ct, shape):
    col = Quantity_Color()
    for t in (XCAFDoc_ColorType.XCAFDoc_ColorSurf, XCAFDoc_ColorType.XCAFDoc_ColorGen):
        try:
            if ct.GetColor(shape, t, col):
                return (col.Red(), col.Green(), col.Blue())
        except Exception:
            pass
    exp = TopExp_Explorer(shape, TopAbs_FACE)
    if exp.More():
        try:
            if ct.GetColor(exp.Current(), XCAFDoc_ColorType.XCAFDoc_ColorSurf, col):
                return (col.Red(), col.Green(), col.Blue())
        except Exception:
            pass
    return None


def _walk(st, ct, lab, loc, out):
    if st.IsReference_s(lab):
        ref = TDF_Label(); st.GetReferredShape_s(lab, ref)
        _walk(st, ct, ref, loc.Multiplied(st.GetLocation_s(lab)), out)
        return
    if st.IsAssembly_s(lab):
        comps = Sequence_TDF_Label(); st.GetComponents_s(lab, comps)
        for i in range(1, comps.Length() + 1):
            _walk(st, ct, comps.Value(i), loc, out)
        return
    shape = st.GetShape_s(lab)
    out.append((_name(lab), shape.Moved(loc), _colour(ct, shape)))


def _srgb(c):
    c = np.asarray(c, float)
    return np.where(c <= 0.0031308, 12.92 * c, 1.055 * np.power(c, 1 / 2.4) - 0.055)


def _tessellate(shape, lin, ang):
    BRepMesh_IncrementalMesh(shape, lin, False, ang, True)
    V, N, F, FID = [], [], [], []
    off = fi = 0
    exp = TopExp_Explorer(shape, TopAbs_FACE)
    while exp.More():
        f = TopoDS.Face(exp.Current()); loc = TopLoc_Location()
        tri = BRep_Tool.Triangulation_s(f, loc)
        if tri is not None:
            tr = loc.Transformation()
            if not tri.HasNormals():
                tri.ComputeNormals()
            rev = f.Orientation() == 1
            s = -1.0 if rev else 1.0
            for i in range(1, tri.NbNodes() + 1):
                p = tri.Node(i).Transformed(tr); d = tri.Normal(i).Transformed(tr)
                V.append((p.X(), p.Y(), p.Z())); N.append((s * d.X(), s * d.Y(), s * d.Z()))
            for i in range(1, tri.NbTriangles() + 1):
                a, b, c = tri.Triangle(i).Get()
                F.append((off + a - 1, off + c - 1, off + b - 1) if rev else (off + a - 1, off + b - 1, off + c - 1))
                FID.append(fi)
            off += tri.NbNodes()
        fi += 1
        exp.Next()
    return np.array(V, float), np.array(N, float), np.array(F, int), np.array(FID, int)


def load_parts(path, lin=0.03, ang=0.12):
    """Return {part name: dict(V, N, F, FID, color)} with all parts in assembly position (mm)."""
    doc = TDocStd_Document(TCollection_ExtendedString('doc'))
    r = STEPCAFControl_Reader(); r.SetNameMode(True); r.SetColorMode(True)
    if r.ReadFile(path) != IFSelect_RetDone:
        raise RuntimeError(f'cannot read {path}')
    r.Transfer(doc)
    st = XCAFDoc_DocumentTool.ShapeTool_s(doc.Main()); ct = XCAFDoc_DocumentTool.ColorTool_s(doc.Main())
    roots = Sequence_TDF_Label(); st.GetFreeShapes(roots)
    out = []
    for i in range(1, roots.Length() + 1):
        _walk(st, ct, roots.Value(i), TopLoc_Location(), out)
    parts = {}
    for name, shape, c in out:
        V, N, F, FID = _tessellate(shape, lin, ang)
        parts[name] = dict(name=name, V=V, N=N, F=F, FID=FID, color=_srgb(c) if c is not None else np.array([.7, .7, .7]))
    return parts
