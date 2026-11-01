"""Write the print files to models/: one STL per part and ready-made print plates as 3MF.

models/stl/     one file per part, already turned the way it goes on the print bed (table in chapter 3 of the guide)
models/plates/  one 3MF per colour: the parts that share a colour, laid out on one bed. For PrusaSlicer the files
                also carry the settings from the guide (0.2 mm layers, 3 walls, 15 % infill, supports for frame and
                lid) and, for the tokens, the pause for the NFC tags. Other slicers take the geometry and layout.

Run from this folder:  python3 export_print_files.py
"""
import io, pathlib, zipfile
import numpy as np
from PIL import Image
from lib.step import load_parts
from lib.render import render
from lib import prim

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
T = np.array([0.0871557, 0, 0.9961947])          # axis of lid, token and knob, which sit tilted by 5 degrees
# Direction of each part (in assembly position) that has to point up on the print bed.
UP = {'frame': (0, 0, 1), 'front': T, 'back': (0, 0, -1), 'door': (0, 0, -1), 'bumper': (0, 0, -1),
      'speaker grill': (-1, 0, 0), 'prev button': (0, 1, 0), 'next button': (0, -1, 0), 'volume': -T, 'token': -T,
      'pcb_spacer': (0, 0, 1)}
FILE = {'bumper': 'bumper-tpu', 'volume': 'volume-knob', 'pcb_spacer': 'pcb-spacer'}
LIN, ANG = 0.02, 0.1                              # tessellation: chord error in mm, angle in rad

# Plates: columns placed left to right, the parts of a column stacked from the back to the front of the bed.
# Grouped by colour as in the pictures: body colour, lid colour, black, TPU, tokens.
CENTER = np.array([120.0, 105.0])                 # fits 220 x 220 and 250 x 210 beds and everything larger
GAP = 12.0
PLATES = [
    ('plate-1_frame-door-knob', [['frame'], ['door', 'volume']]),
    ('plate-2_lid-bottom-buttons', [['front', 'back'], ['prev button', 'next button']]),
    ('plate-3_grill-spacer', [['speaker grill'], ['pcb_spacer']]),
    ('plate-4_bumper-tpu', [['bumper']]),
    ('plate-5_9-tokens', [['token'] * 3] * 3),
]
BASE = {'layer_height': '0.2', 'perimeters': '3', 'fill_density': '15%'}
EXTRA = {'frame': {'support_material': '1', 'support_material_auto': '1'},
         'front': {'support_material': '1', 'support_material_auto': '1', 'support_material_buildplate_only': '1',
                   'support_material_style': 'organic'}}
PAUSE = {'plate-5_9-tokens': (15.0, 'Insert the NFC tags, adhesive side down')}


def rot_to_z(up):
    """Rotation matrix that turns the vector up onto +z."""
    up = np.asarray(up, float) / np.linalg.norm(up); z = np.array([0, 0, 1.0]); v = np.cross(up, z); s = np.linalg.norm(v)
    if s < 1e-9:
        return np.eye(3) if up @ z > 0 else np.diag([1.0, -1.0, -1.0])
    vx = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
    return np.eye(3) + vx + vx @ vx * ((1 - up @ z) / s**2)


def oriented(p, up):
    """Part turned into print orientation, centred on x/y, standing on z = 0, triangles facing outwards."""
    R = rot_to_z(up); V = p['V'] @ R.T; N = p['N'] @ R.T; F = p['F'].copy()
    V = V - [(V[:, 0].min() + V[:, 0].max()) / 2, (V[:, 1].min() + V[:, 1].max()) / 2, V[:, 2].min()]
    tri = V[F]; fn = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    flip = (fn * N[F].sum(1)).sum(1) < 0
    F[flip] = F[flip][:, ::-1]
    return dict(p, V=V, N=N, F=F)


def weld(V, F):
    """Shared vertices instead of one set per CAD face, as 3MF wants it. Drops triangles that collapse."""
    _, first, idx = np.unique(np.round(V / 1e-3).astype(np.int64), axis=0, return_index=True, return_inverse=True)
    G = idx.reshape(-1)[F]; keep = (G[:, 0] != G[:, 1]) & (G[:, 1] != G[:, 2]) & (G[:, 0] != G[:, 2])
    return V[first], G[keep]


def check(Vw, Fw):
    """Every edge used exactly once in each direction: closed and consistently oriented."""
    E = np.concatenate([Fw[:, [0, 1]], Fw[:, [1, 2]], Fw[:, [2, 0]]])
    _, cnt = np.unique(E, axis=0, return_counts=True)
    rev = set(map(tuple, E[:, ::-1].tolist())); missing = sum(1 for e in map(tuple, E.tolist()) if e not in rev)
    return int((cnt != 1).sum()) + missing


def write_stl(path, V, F, header):
    tri = V[F]; n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
    rec = np.zeros(len(F), dtype=[('n', '<f4', 3), ('v', '<f4', (3, 3)), ('a', '<u2')])
    rec['n'] = n; rec['v'] = tri
    with open(path, 'wb') as f:
        f.write(header.encode('ascii')[:80].ljust(80, b' ')); f.write(np.uint32(len(F)).tobytes()); f.write(rec.tobytes())


def layout(columns, parts):
    """Positions (x, y) of every part on the bed for a list of columns."""
    dims = {k: parts[k]['V'].max(0) - parts[k]['V'].min(0) for col in columns for k in col}
    widths = [max(dims[k][0] for k in col) for col in columns]
    heights = [sum(dims[k][1] for k in col) + GAP * (len(col) - 1) for col in columns]
    x = CENTER[0] - (sum(widths) + GAP * (len(columns) - 1)) / 2; out = []
    for col, w, h in zip(columns, widths, heights):
        y = CENTER[1] + h / 2
        for k in col:
            out.append((k, np.array([x + w / 2, y - dims[k][1] / 2]))); y -= dims[k][1] + GAP
        x += w + GAP
    return out


def thumbnail(placed, parts):
    items = []
    for k, xy in placed:
        items.append(dict(parts[k], offset=(xy[0], xy[1], 0.0)))
    P = np.concatenate([it['V'] + it['offset'] for it in items]); lo, hi = P.min(0), P.max(0); m = 10
    items.append(prim.box(((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, -0.75), (hi[0] - lo[0] + 2 * m, hi[1] - lo[1] + 2 * m, 1.5),
                          np.array([0.89, 0.91, 0.93])))
    im, _ = render(items, view=(0.55, -1.0, 1.25), size=(512, 512), ss=2, margin=0.04)
    b = io.BytesIO(); im.save(b, 'PNG'); return b.getvalue()


def write_3mf(path, name, placed, parts, welded):
    # One object per part with the mesh. Further copies of a part (the tokens) are small objects that only reference
    # the mesh, the way PrusaSlicer writes instances; other slicers read them as plain components.
    order = []
    for k, _ in placed:
        if k not in order: order.append(k)
    mesh_id = {k: i + 1 for i, k in enumerate(order)}; next_id = len(order) + 1; item_ids = []; used = set()
    for k, _ in placed:
        if k in used:
            item_ids.append(next_id); next_id += 1
        else:
            item_ids.append(mesh_id[k]); used.add(k)
    model = ['<?xml version="1.0" encoding="UTF-8"?>',
             '<model unit="millimeter" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02"'
             ' xmlns:slic3rpe="http://schemas.slic3r.org/3mf/2017/06">',
             ' <metadata name="slic3rpe:Version3mf">1</metadata>',          # PrusaSlicer reads the copies as instances
             f' <metadata name="Title">Timon Box – {name}</metadata>', ' <metadata name="Designer">Aaron Blankenburg</metadata>',
             ' <metadata name="LicenseTerms">CC BY-NC-SA 4.0</metadata>',
             ' <metadata name="Description">https://github.com/aaronblankenburg/timon-box</metadata>', ' <resources>']
    cfg = ['<?xml version="1.0" encoding="UTF-8"?>', '<config>']
    for k in order:
        V, F = welded[k]; oname = 'timon-box_' + FILE.get(k, k.replace(' ', '-'))
        model.append(f'  <object id="{mesh_id[k]}" name="{oname}" type="model">\n   <mesh>\n    <vertices>')
        model.append('\n'.join(f'     <vertex x="{x:.4f}" y="{y:.4f}" z="{z:.4f}"/>' for x, y, z in V))
        model.append('    </vertices>\n    <triangles>')
        model.append('\n'.join(f'     <triangle v1="{a}" v2="{b}" v3="{c}"/>' for a, b, c in F))
        model.append('    </triangles>\n   </mesh>\n  </object>')
        n_inst = sum(1 for kk, _ in placed if kk == k)
        cfg.append(f' <object id="{mesh_id[k]}" instances_count="{n_inst}">')
        cfg.append(f'  <metadata type="object" key="name" value="{oname}"/>')
        for key, val in {**BASE, **EXTRA.get(k, {})}.items():
            cfg.append(f'  <metadata type="object" key="{key}" value="{val}"/>')
        cfg.append(f'  <volume firstid="0" lastid="{len(F) - 1}">')
        cfg.append(f'   <metadata type="volume" key="name" value="{oname}"/>')
        cfg.append('   <metadata type="volume" key="volume_type" value="ModelPart"/>')
        cfg.append('   <metadata type="volume" key="matrix" value="1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1"/>')
        cfg.append('  </volume>\n </object>')
    for (k, _), oid in zip(placed, item_ids):
        if oid != mesh_id[k]:
            model.append(f'  <object id="{oid}" type="model">\n   <components>\n    <component objectid="{mesh_id[k]}"/>\n   </components>\n  </object>')
    model.append(' </resources>\n <build>')
    for (k, xy), oid in zip(placed, item_ids):
        model.append(f'  <item objectid="{oid}" transform="1 0 0 0 1 0 0 0 1 {xy[0]:.4f} {xy[1]:.4f} 0" printable="1"/>')
    model.append(' </build>\n</model>\n'); cfg.append('</config>\n')
    types = ('<?xml version="1.0" encoding="UTF-8"?>\n<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">\n'
             ' <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>\n'
             ' <Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>\n'
             ' <Default Extension="png" ContentType="image/png"/>\n</Types>\n')
    rels = ('<?xml version="1.0" encoding="UTF-8"?>\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">\n'
            ' <Relationship Target="/3D/3dmodel.model" Id="rel-1" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>\n'
            ' <Relationship Target="/Metadata/thumbnail.png" Id="rel-2" Type="http://schemas.openxmlformats.org/package/2006/relationships/metadata/thumbnail"/>\n'
            '</Relationships>\n')
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('[Content_Types].xml', types); z.writestr('_rels/.rels', rels)
        z.writestr('3D/3dmodel.model', '\n'.join(model))
        z.writestr('Metadata/thumbnail.png', thumbnail(placed, parts))
        z.writestr('Metadata/Slic3r_PE_model.config', '\n'.join(cfg))
        if name in PAUSE:
            zz, msg = PAUSE[name]
            z.writestr('Metadata/Prusa_Slicer_custom_gcode_per_print_z.xml',
                       '<?xml version="1.0" encoding="utf-8"?>\n<custom_gcodes_per_print_z>\n'
                       f'<code print_z="{zz}" type="1" extruder="1" color="" extra="{msg}" gcode="M601"/>\n'
                       '<mode value="SingleExtruder"/>\n</custom_gcodes_per_print_z>\n')


def main():
    raw = load_parts(str(ROOT / 'models' / 'box.step'), lin=LIN, ang=ANG)
    raw['pcb_spacer'] = next(iter(load_parts(str(ROOT / 'models' / 'pcb_spacer.step'), lin=LIN, ang=ANG).values()))
    parts = {k: oriented(raw[k], up) for k, up in UP.items()}
    stl = ROOT / 'models' / 'stl'; plates = ROOT / 'models' / 'plates'
    stl.mkdir(parents=True, exist_ok=True); plates.mkdir(parents=True, exist_ok=True)
    welded = {}
    for k, p in parts.items():
        fname = 'timon-box_' + FILE.get(k, k.replace(' ', '-')) + '.stl'
        write_stl(stl / fname, p['V'], p['F'], f'timon-box {k} - CC BY-NC-SA 4.0 - github.com/aaronblankenburg/timon-box')
        welded[k] = weld(p['V'], p['F']); size = p['V'].max(0) - p['V'].min(0)
        print(f'{fname:34s} {len(p["F"]):7d} triangles  {size[0]:5.1f} x {size[1]:5.1f} x {size[2]:5.1f} mm'
              f'  mesh errors {check(*welded[k])}')
    for name, columns in PLATES:
        placed = layout(columns, parts)
        P = np.concatenate([parts[k]['V'][:, :2] + xy for k, xy in placed]); lo, hi = P.min(0), P.max(0)
        write_3mf(plates / f'timon-box_{name}.3mf', name, placed, parts, welded)
        print(f'timon-box_{name}.3mf: {len(placed)} parts, x {lo[0]:.0f}..{hi[0]:.0f}, y {lo[1]:.0f}..{hi[1]:.0f} mm')


if __name__ == '__main__':
    main()
