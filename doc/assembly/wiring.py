"""English version of the wiring diagram in the appendix.

hardware/wiring/power-side.yml is labelled in German. For the English guide the labels below replace the German
ones and WireViz renders hardware/wiring/power-side-en.svg. The English build does that whenever wireviz is
installed, otherwise it uses the committed SVG. Labels missing here show up as a build warning.
"""
import html, pathlib, re, shutil, subprocess, tempfile

from i18n import GERMAN

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
YML = ROOT / 'hardware' / 'wiring' / 'power-side.yml'
SVG_EN = YML.with_name('power-side-en.svg')

# German label in power-side.yml -> English
WIRING_EN = {
    'Hauptplatine J4': 'Main board J4',
    'JST XH, 2,5 mm': 'JST XH, 2.5 mm',
    'Buchse, 7-polig': 'socket, 7 pins',
    'Kabel 7-polig': 'Cable, 7 pins',
    'Boost-Modul': 'Boost module',
    'feste 5 V': 'fixed 5 V',
    'Lötpads J1': 'solder pads J1',
    'Power Latch Rückseite': 'Power Latch back',
    'Footprint des TP4056': 'TP4056 footprint',
    'Lademodul': 'Charger module',
    'mit Schutzschaltung': 'with protection',
    'Li-Ion-Zelle': 'Li-ion cell',
    'USB-C-Buchse': 'USB-C socket',
    '5,1 kΩ an CC1 und CC2': '5.1 kΩ on CC1 and CC2',
    'Mikroschalter': 'Microswitch',
    '12,8 × 5,8 mm': '12.8 × 5.8 mm',
    'Token drückt ihn': 'token presses it',
    'hält die Box, solange der ESP32 HOLD setzt': 'holds the box while the ESP32 sets HOLD',
    'hält die Box, solange an D− Spannung liegt': 'holds the box while D− carries voltage',
}


def render_english():
    """Render power-side-en.svg from the German YAML with the labels above. Returns False without wireviz."""
    if not shutil.which('wireviz'):
        return False
    text = YML.read_text(encoding='utf-8')
    for de in sorted(WIRING_EN, key=len, reverse=True):
        text = text.replace(de, WIRING_EN[de])
    with tempfile.TemporaryDirectory() as tmp:
        src = pathlib.Path(tmp) / 'power-side-en.yml'
        src.write_text(text, encoding='utf-8')
        subprocess.run(['wireviz', '-f', 's', str(src)], check=True, capture_output=True)
        shutil.copy(src.with_suffix('.svg'), SVG_EN)
    return True


def leftovers():
    """German-looking labels in the English SVG."""
    svg = SVG_EN.read_text(encoding='utf-8')
    texts = {html.unescape(t).strip() for t in re.findall(r'<text[^>]*>([^<]*)</text>', svg)}
    return sorted(t for t in texts if GERMAN.search(t) or re.search(r'\d,\d', t))
