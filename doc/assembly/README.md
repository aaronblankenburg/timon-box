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
python3 export_print_files.py       # Druckteile als STL nach models/stl/, Druckbetten als 3MF nach models/plates/
BAMBU_STUDIO=…/AppRun python3 export_bambu_project.py --check   # Bambu-Studio-Projekt mit allen Betten
python3 figures.py thingiverse      # Bilder für die Thingiverse-Seite nach fig/thingiverse/
```

Ohne `--figures` braucht der Build nur ReportLab, Pillow, svglib und pdfrw, die Bilder liegen fertig in `fig/`.

## Dateien

| Datei | Inhalt |
|---|---|
| `content.py` | Der ganze Text: Kapitel, Tabellen, Schritte, Hinweise, Bildunterschriften und die Beschriftungen der Bilder |
| `layout.py` | Seitenlayout, Schriften, Farben und Bausteine: Schritt, Hinweis, beschriftetes Bild, Diagramme, Bestückungsplan (`PcbView`), Fotos, Schaltplan- und SVG-Seiten |
| `figures.py` | Rendert die Bilder aus `models/box.step` nach `fig/`, mit dem Ziel `thingiverse` auch die Galerie für Thingiverse |
| `export_print_files.py` | Schreibt jedes Druckteil als STL nach `models/stl/`, gedreht wie in der Tabelle in Kapitel 3, und die Druckbetten nach `models/plates/`: eine 3MF pro Farbe, für PrusaSlicer mit den Einstellungen aus Kapitel 3 und der Pause für die Tokens. Welche Teile auf welchem Bett liegen, steht in `PLATES` |
| `export_bambu_project.py` | Baut mit der Kommandozeile von Bambu Studio `models/plates/timon-box_bambu-studio.3mf`: alle fünf Betten in einem Projekt für den X1 Carbon, mit Filamenten, Stützen und Pause. Das ist auch das Druckprofil für MakerWorld. `--check` sliced danach alle Betten zur Kontrolle. Vorschaubilder kann die Kommandozeile nicht erzeugen: Das Projekt danach einmal in Bambu Studio öffnen und speichern, möglichst mit der ältesten Version, die es öffnen soll. Die Datei im Repo ist mit 2.8.0 gespeichert |
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
- **Modell geändert:** `./build.sh --figures` und `python3 export_print_files.py`, danach die Bilder ansehen. Anker und Hilfskörper (Akkus, Schrauben,
  Kabel) sind feste Koordinaten und wandern nicht mit. Die Maße im Text und in Kapitel 6 stehen von Hand in
  `content.py`.

Koordinaten im Modell, in mm: x nach vorn (Lautsprecher), y nach rechts (Taste „vor“), z nach oben. Deckel, Token und
Drehknopf sind um 5° um y geneigt, ihre Achse ist `t` in `figures.py`.
