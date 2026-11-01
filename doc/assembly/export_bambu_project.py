"""Build models/plates/timon-box_bambu-studio.3mf: one Bambu Studio project with all five plates.

Same plates as export_print_files.py, but as a Bambu Studio project for an X1 Carbon (Bambu Studio offers the other
printers with a 0.4 nozzle as compatible): PETG HF in three colours plus TPU 95A HF, textured PEI plate, 3 walls,
15 % infill, tree supports for frame and lid, and the pause for the NFC tags on the token plate. This is also the
print profile for MakerWorld.

The command line cannot render the plate previews. After building, open the project once in Bambu Studio
(File > Open Project) and save it: that adds the previews and turns it into a project saved by the app itself.
Use the oldest Bambu Studio that should open it, newer files make older versions ask first.

Needs the STL files from export_print_files.py and Bambu Studio's command line, for example the extracted AppImage:
    BAMBU_STUDIO=/path/to/squashfs-root/AppRun python3 export_bambu_project.py [--check]
--check slices all plates afterwards and prints supports, pauses and print time per plate.
"""
import glob, json, os, pathlib, re, shutil, struct, subprocess, sys, tempfile, zipfile
import numpy as np
import export_print_files as ex

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
APP = os.environ.get('BAMBU_STUDIO', 'bambu-studio')
PROFILES = os.environ.get('BAMBU_PROFILES', str(pathlib.Path(APP).resolve().parent / 'resources' / 'profiles' / 'BBL'))
OUT = ROOT / 'models' / 'plates' / 'timon-box_bambu-studio.3mf'
MACHINE, PROCESS = 'Bambu Lab X1 Carbon 0.4 nozzle', '0.20mm Standard @BBL X1C'
# filament slots: 1 body colour, 2 lid colour (and tokens), 3 black, 4 TPU
FILAMENTS = [('Bambu PETG HF @BBL X1C', '#3E7564'), ('Bambu PETG HF @BBL X1C', '#B8B8B8'),
             ('Bambu PETG HF @BBL X1C', '#1C1C1C'), ('Bambu TPU 95A HF @BBL X1C', '#7A7F85')]
PLATE = {'plate-1_frame-door-knob': ('Body colour', 1), 'plate-2_lid-bottom-buttons': ('Lid colour', 2),
         'plate-3_grill-spacer': ('Black', 3), 'plate-4_bumper-tpu': ('TPU', 4), 'plate-5_9-tokens': ('Tokens', 2)}
BASE = {'wall_loops': '3', 'sparse_infill_density': '15%'}
EXTRA = {'frame': {'enable_support': '1', 'support_type': 'tree(auto)'},
         'front': {'enable_support': '1', 'support_type': 'tree(auto)', 'support_on_build_plate_only': '1'}}
PAUSE = {'mode': 'SingleExtruder', 'gcodes': [{'type': 'PausePrint', 'print_z': 15.0, 'color': '', 'extruder': 1,
                                               'extra': 'Insert the NFC tags, adhesive side down'}]}
TOKEN_PLATE = 5
GAP = 25.0                                        # Bambu's path check wants more room around the frame's tree supports


def preset(kind, name):
    """System preset with everything it inherits, so the project carries complete settings (start G-code etc.)."""
    d = json.load(open(f'{PROFILES}/{kind}/{name}.json'))
    full = preset(kind, d['inherits']) if d.get('inherits') else {}
    full.update(d)
    if kind == 'machine':
        for t in sorted(glob.glob(f'{PROFILES}/machine/{name} template *.json')):
            full.update({k: v for k, v in json.load(open(t)).items() if k not in ('name', 'instantiation')})
    full.update(inherits='', name=name, **{'from': 'system'})
    return full


def stl_vertices(path):
    data = open(path, 'rb').read(); n = struct.unpack('<I', data[80:84])[0]
    rec = np.frombuffer(data[84:84 + 50 * n], dtype=[('n', '<f4', 3), ('v', '<f4', (3, 3)), ('a', '<u2')])
    return rec['v'].reshape(-1, 3).astype(float)


def run(args, tmp):
    env = dict(os.environ, HOME=str(tmp / 'home'))           # fresh settings, older versions have no --datadir
    r = subprocess.run([APP, '--debug', '1'] + args, capture_output=True, text=True, cwd=tmp, env=env)
    if r.returncode != 0:
        raise SystemExit(f'bambu-studio failed ({r.returncode}):\n{r.stdout[-2000:]}\n{r.stderr[-2000:]}')


def main():
    tmp = pathlib.Path(tempfile.mkdtemp(prefix='bambu-'))
    paths = {}
    for kind, name, key in (('machine', MACHINE, 'machine'), ('process', PROCESS, 'process')):
        paths[key] = tmp / f'{key}.json'; json.dump(preset(kind, name), open(paths[key], 'w'), indent=1)
    fil = []
    for i, (name, colour) in enumerate(FILAMENTS, 1):
        p = tmp / f'filament{i}.json'; d = preset('filament', name); d['filament_colour'] = [colour]
        json.dump(d, open(p, 'w'), indent=1); fil.append(str(p))
    # plates: same layout as the PrusaSlicer plates, centred on a 256 x 256 bed, one entry per part (copies are
    # listed one by one; the command line puts counted copies on the wrong plates)
    ex.CENTER = np.array([128.0, 128.0]); ex.GAP = GAP
    parts = {}
    for k in ex.UP:
        f = ROOT / 'models' / 'stl' / ('timon-box_' + ex.FILE.get(k, k.replace(' ', '-')) + '.stl')
        parts[k] = {'file': str(f), 'V': stl_vertices(f)}
    plates = []
    for name, columns in ex.PLATES:
        title, slot = PLATE[name]
        objs = [{'path': parts[k]['file'], 'count': 1, 'filaments': [slot], 'pos_x': [float(xy[0])], 'pos_y': [float(xy[1])],
                 'pos_z': [0.0], 'print_params': {**BASE, **EXTRA.get(k, {})}} for k, xy in ex.layout(columns, parts)]
        plates.append({'plate_name': title, 'need_arrange': False, 'plate_params': {'curr_bed_type': 'Textured PEI Plate'},
                       'objects': objs})
    json.dump({'plates': plates}, open(tmp / 'assemble.json', 'w'), indent=1)
    json.dump(PAUSE, open(tmp / 'pause.json', 'w'))
    run(['--load-settings', f'{paths["machine"]};{paths["process"]}', '--load-filaments', ';'.join(fil),
         '--load-assemble-list', str(tmp / 'assemble.json'), '--export-3mf', 'stage1.3mf', '--outputdir', str(tmp)], tmp)
    # The command line attaches a pause only to the plate it slices, and that export then holds only this plate.
    # So take the pause record from it and add it to the full project.
    run(['--load-custom-gcodes', str(tmp / 'pause.json'), '--slice', str(TOKEN_PLATE), '--export-3mf', 'stage2.3mf',
         '--outputdir', str(tmp), str(tmp / 'stage1.3mf')], tmp)
    pause = zipfile.ZipFile(tmp / 'stage2.3mf').read('Metadata/custom_gcode_per_layer.xml')
    src = zipfile.ZipFile(tmp / 'stage1.3mf')
    with zipfile.ZipFile(OUT, 'w', zipfile.ZIP_DEFLATED) as out:
        for item in src.infolist():
            data = src.read(item.filename)
            if item.filename == 'Metadata/project_settings.config':
                d = json.loads(data); d['curr_bed_type'] = 'Textured PEI Plate'; data = json.dumps(d, indent=4).encode()
            elif item.filename == 'Metadata/model_settings.config':
                count = iter(range(1, 100))                    # the nine tokens all come in as token_1
                data = re.sub(r'value="timon-box_token_1"(?=/>\s*<metadata key="(?:enable|extruder|sparse|wall|support))',
                              lambda m: f'value="timon-box_token_{next(count)}"', data.decode()).encode()
            out.writestr(item.filename, data)
        out.writestr('Metadata/custom_gcode_per_layer.xml', pause)
    print(f'wrote {OUT}')
    if '--check' in sys.argv:
        run(['--slice', '0', '--export-3mf', 'check.3mf', '--outputdir', str(tmp / 'check'), str(OUT)], tmp)
        z = zipfile.ZipFile(tmp / 'check' / 'check.3mf')
        for i in range(1, len(plates) + 1):
            g = z.read(f'Metadata/plate_{i}.gcode').decode(errors='ignore')
            t = re.search(r'model printing time: ([^;\n]+)', g)
            print(f'plate {i} {plates[i - 1]["plate_name"]:12s} supports {len(re.findall("; FEATURE: Support", g)):4d}  '
                  f'pauses {g.count(chr(10) + "M400 U1" + chr(10))}  time {t.group(1).strip() if t else "?"}')


if __name__ == '__main__':
    main()
