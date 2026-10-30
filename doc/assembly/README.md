# Quellen für doc/assembly.pdf

[`doc/assembly.pdf`](../assembly.pdf) beschreibt Druck und Zusammenbau des Gehäuses. Das PDF wird aus diesem Ordner
gebaut und nie direkt bearbeitet: Quellen ändern, neu bauen, PDF im selben Commit mitnehmen.

## Bauen

```bash
pip install -r requirements.txt     # einmalig, cadquery-ocp braucht man nur für --figures
./build.sh                          # PDF aus content.py und den Bildern in fig/
./build.sh --figures                # vorher alle Bilder neu aus models/box.step rendern, dauert einige Minuten
./build.sh --figures lid door       # nur diese Bilder neu rendern
```

Ohne `--figures` braucht der Build nur ReportLab und Pillow, die Bilder liegen fertig in `fig/`.

## Dateien

| Datei | Inhalt |
|---|---|
| `content.py` | Der ganze Text: Kapitel, Tabellen, Schritte, Hinweise, Bildunterschriften und die Beschriftungen der Bilder |
| `layout.py` | Seitenlayout, Schriften, Farben und Bausteine: Schritt, Hinweis, beschriftetes Bild, Schichtdiagramm, Parallelschaltung, Lichtleiter |
| `figures.py` | Rendert die Bilder aus `models/box.step` nach `fig/` |
| `lib/step.py` | Liest das STEP-Modell mit OpenCascade (OCP) und vernetzt die Teile |
| `lib/render.py` | Kleiner Software-Renderer: Kanten, Schnitte, hervorgehobene Flächen |
| `lib/prim.py` | Einfache Körper für Zukaufteile: Akku, Schraube, Einsatz, Kabel, Kleberpunkte |
| `fig/` | Gerenderte Bilder und je eine `.json` mit den Bildpositionen der Beschriftungsanker |
| `fonts/` | IBM Plex Sans und Mono, SIL Open Font License (`OFL.txt`) |

## Erweitern

- **Text ändern:** in `content.py`. Kapitel sind `H1(...)`, Unterkapitel `P('…', 'h2')`, Absätze `P('…')`, Tabellen
  `table([...], Spaltenbreiten)`, Hinweise `note('…', 'tip' | 'warn' | 'info')`.
- **Schritt einfügen:** `step(Nummer, 'Titel', [Chips], ['Punkt', …], [('warn', '…')])`. Die Nummern stehen fest im
  Aufruf, beim Einfügen die folgenden hochzählen. Chips (Kleber, Werkzeug) und ihre Farben stehen in `CHIPS` in
  `layout.py`. Abbildungen nummerieren sich selbst.
- **Bild beschriften:** `fig('name', [dict(a='anker', t='Text', s='Unterzeile')], frac=0.6)`. `frac` ist die
  Bildbreite als Anteil der Textbreite, links und rechts davon stehen die Beschriftungen. Die Anker sind Punkte im
  Modell. Sie stehen in `figures.py` beim jeweiligen Bild und landen beim Rendern als Pixelposition in `fig/name.json`.
- **Neues Bild:** in `figures.py` einen Block nach dem Muster der anderen anlegen: Teile mit `part()` holen, verschieben
  (`offset`), schneiden (`cutp`), Flächen färben (`face_colors` mit `fids(...)`), Zukaufteile aus `lib/prim.py` dazu,
  dann `render(...)` und `save(...)` mit den Ankern.
- **Modell geändert:** `./build.sh --figures`, danach die Bilder ansehen. Anker und Hilfskörper (Akkus, Schrauben,
  Kabel) sind feste Koordinaten und wandern nicht mit. Die Maße im Text und in Kapitel 6 stehen von Hand in
  `content.py`.

Koordinaten im Modell, in mm: x nach vorn (Lautsprecher), y nach rechts (Taste „vor“), z nach oben. Deckel, Token und
Drehknopf sind um 5° um y geneigt, ihre Achse ist `t` in `figures.py`.
