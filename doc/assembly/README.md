# Quellen für doc/assembly.pdf

[`doc/assembly.pdf`](../assembly.pdf) ist die Bauanleitung: Elektronik, Druck und Zusammenbau. Das PDF wird aus diesem
Ordner gebaut und nie direkt bearbeitet: Quellen ändern, neu bauen, PDF im selben Commit mitnehmen.

Außer den Dateien hier liest der Build `models/box.step` (nur `figures.py`), die KiCad-Platinen, Schaltpläne und das
WireViz-SVG unter `hardware/` und die Fotos unter `assets/`. Kapitel 5 fasst `doc/hardware.md` zusammen; ändert sich
dort etwas, `content.py` nachziehen.

## Bauen

```bash
pip install -r requirements.txt     # einmalig, cadquery-ocp braucht man nur für --figures
./build.sh                          # PDF aus content.py und den Bildern in fig/
./build.sh --figures                # vorher alle Bilder neu aus models/box.step rendern, dauert einige Minuten
./build.sh --figures lid door       # nur diese Bilder neu rendern
```

Ohne `--figures` braucht der Build nur ReportLab, Pillow, svglib und pdfrw, die Bilder liegen fertig in `fig/`.

## Dateien

| Datei | Inhalt |
|---|---|
| `content.py` | Der ganze Text: Kapitel, Tabellen, Schritte, Hinweise, Bildunterschriften und die Beschriftungen der Bilder |
<<<<<<< HEAD
| `layout.py` | Seitenlayout, Schriften, Farben und Bausteine: Schritt, Hinweis, beschriftetes Bild, Diagramme, Bestückungsplan (`PcbView`), Fotos, Schaltplan- und SVG-Seiten |
=======
| `layout.py` | Seitenlayout, Schriften, Farben und Bausteine: Schritt, Hinweis, beschriftetes Bild, Schichtdiagramm, Parallelschaltung, Lichtleiter |
>>>>>>> 25e2960d7aa34102d2d0a27d80930b01dce578b1
| `figures.py` | Rendert die Bilder aus `models/box.step` nach `fig/` |
| `lib/step.py` | Liest das STEP-Modell mit OpenCascade (OCP) und vernetzt die Teile |
| `lib/kicad.py` | Liest die KiCad-Platinen (Umriss, Footprints, Pads, Beschriftung) für die Bestückungspläne |
| `lib/electronics.py` | 3D-Stellvertreter der Elektronik für die Bilder: Platinen aus den KiCad-Dateien, Module, LED, Poti, Mikroschalter, Step-up |
| `lib/render.py` | Kleiner Software-Renderer: Kanten, Schnitte, hervorgehobene Flächen |
| `lib/prim.py` | Einfache Körper für Zukaufteile: Akku, Schraube, Einsatz, Kabel, Kleberpunkte |
| `fig/` | Gerenderte Bilder und je eine `.json` mit den Bildpositionen der Beschriftungsanker |
| `fonts/` | IBM Plex Sans und Mono, SIL Open Font License (`OFL.txt`) |

## Erweitern

- **Text ändern:** in `content.py`. Kapitel sind `H1(...)`, Unterkapitel `P('…', 'h2')`, Absätze `P('…')`, Tabellen
  `table([...], Spaltenbreiten)`, Hinweise `note('…', 'tip' | 'warn' | 'info')`.
- **Schritt einfügen:** `step(Nummer, 'Titel', [Chips], ['Punkt', …], [('warn', '…')])`. Die Nummern stehen fest im
  Aufruf, beim Einfügen die folgenden hochzählen. Chips (Kleber, Werkzeug) und ihre Farben stehen in `CHIPS` in
  `layout.py`. Abbildungen nummerieren sich selbst. Schritte der Elektronik heißen E1, E2 … und haben blaue Nummern
  (`color=EB`).
- **Bild beschriften:** `fig('name', [dict(a='anker', t='Text', s='Unterzeile')], frac=0.6)`. `frac` ist die
  Bildbreite als Anteil der Textbreite, links und rechts davon stehen die Beschriftungen. Die Anker sind Punkte im
  Modell. Sie stehen in `figures.py` beim jeweiligen Bild und landen beim Rendern als Pixelposition in `fig/name.json`.
- **Neues Bild:** in `figures.py` einen Block nach dem Muster der anderen anlegen: Teile mit `part()` holen, verschieben
  (`offset`), schneiden (`cutp`), Flächen färben (`face_colors` mit `fids(...)`), Zukaufteile aus `lib/prim.py` dazu,
  dann `render(...)` und `save(...)` mit den Ankern.
- **Elektronik in den Bildern:** Die Teile baut `lib/electronics.py`, ihre Lage im Gehäuse steht in `figures.py`
  (`lid_parts`, `stack`, `power_parts`). Ändert sich eine Platine, rendern `./build.sh --figures lid_full akkus lidopen`
  sie neu.
- **Modell geändert:** `./build.sh --figures`, danach die Bilder ansehen. Anker und Hilfskörper (Akkus, Schrauben,
  Kabel) sind feste Koordinaten und wandern nicht mit. Die Maße im Text und in Kapitel 6 stehen von Hand in
  `content.py`.

Koordinaten im Modell, in mm: x nach vorn (Lautsprecher), y nach rechts (Taste „vor“), z nach oben. Deckel, Token und
Drehknopf sind um 5° um y geneigt, ihre Achse ist `t` in `figures.py`.
