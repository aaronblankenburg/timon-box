"""Text and structure of doc/assembly.pdf (German). Build with ./build.sh, layout lives in layout.py."""
from layout import *
from lib.kicad import load_board

META = dict(
    title='Timon Box – Bauanleitung: Elektronik, Druck und Zusammenbau', subject='Hardware-Doku: Platinen, Verkabelung, Druckteile, Zusammenbau',
    kicker='HARDWARE-DOKU · BAUANLEITUNG', name='Timon Box', subtitle='Elektronik, Druck und Zusammenbau',
    intro='Platinen bestücken, verkabeln, Gehäuse drucken und alles Schritt für Schritt zusammenbauen.',
    facts=[repo('#cfe3db'), 'Modell: models/box.step', 'Platinen: hardware/ (KiCad 8)', 'Hintergründe: doc/hardware.md', 'Stand: Oktober 2026', 'Lizenz: CC BY-NC-SA 4.0', '© 2026 Aaron Blankenburg'],
    header='Timon Box · Bauanleitung', footer='Stand Oktober 2026 · © Aaron Blankenburg · Lizenz CC BY-NC-SA 4.0')


def glance():
    items = [('DRUCK', 'PETG oder PLA, Bumper TPU, 0,2 mm Schichthöhe'), ('VERBINDUNGEN', 'Sekundenkleber, 2K-Kleber, UV-Lack, Heißkleber, Klebeband'),
             ('SCHRAUBEN', '4 × M3 × 5 in Einschmelzgewinden M3'), ('AKKUS', '2 × 18650, parallel geschaltet')]
    sl = ParagraphStyle('gl', fontName='Plex-SB', fontSize=6.9, leading=9, textColor=ACCENT)
    sv = ParagraphStyle('gv', fontName='Plex-Med', fontSize=8.8, leading=11.6, textColor=INK)
    cells = [[Paragraph(a, sl), Spacer(1, 2), Paragraph(b, sv)] for a, b in items]
    t = Table([cells], colWidths=[(FW-9)/4]*4, hAlign='LEFT')
    t.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,-1),TINT), ('VALIGN',(0,0),(-1,-1),'TOP'), ('LEFTPADDING',(0,0),(-1,-1),8),
        ('RIGHTPADDING',(0,0),(-1,-1),6), ('TOPPADDING',(0,0),(-1,-1),7), ('BOTTOMPADDING',(0,0),(-1,-1),8),
        ('LINEAFTER',(0,0),(2,0),3,white)]))
    return t

story = []
toc = TableOfContents(); toc.levelStyles = [S['toc']]; toc.dotsMinLevel = 0
story += [P('<font name="Plex-SB" size="11" color="#2f6b5a">Inhalt</font>'), Spacer(1, 4), toc, NextPageTemplate('body'), PageBreak()]

# 1 Überblick
story += [H1(1, 'Überblick'),
  P('Das Gehäuse besteht aus zehn Druckteilen. Rahmen, Boden und Deckel werden zu einer Schale verklebt. Unten bleibt eine Bodenklappe, die mit vier M3-Schrauben in Einschmelzgewinden hält. Durch sie kommt man nach dem Zusammenbau wieder an die Elektronik.'),
  P('Vorn ist die Lautsprecherseite. Von vorn gesehen liegt die Taste „zurück“ links und „vor“ rechts, passend zu J2 und J3 (Kapitel 5.4). Der Deckel ist um 5° nach vorn geneigt, die Rückseite mit der USB-C-Buchse ist leicht gewölbt.'), Spacer(1, 4)]
story += [fig('overview', [
    dict(a='a_token', t='Token-Mulde', s='RC522 darunter', side='L'), dict(a='a_knob', t='Lautstärke', side='L'),
    dict(a='a_led', t='LED', side='L'), dict(a='a_handle', t='Lasche', s='für den Tragegurt', side='L'), dict(a='a_prev', t='Taste „zurück“', s='Piezo innen', side='L'),
    dict(a='a_grill', t='Lautsprecher', side='L', dy=-6),
    dict(a='b_jack', t='Kopfhörerbuchse', s='noch verschlossen', side='R'), dict(a='b_next', t='Taste „vor“', side='R'),
    dict(a='b_curved', t='Rückseite', s='leicht gewölbt', side='R'), dict(a='b_usb', t='USB-C', s='Laden und Flashen', side='R'), dict(a='b_chg', t='Ladeanzeige', s='Lichtleiter zur Lade-LED', side='R'),
    dict(a='b_bumper', t='Bumper (TPU)', side='R')], frac=0.70, gap=6),
  cap('Links von vorn, rechts von hinten.'),
  table([['Maß', 'Wert'], ['Grundfläche', '94 × 90 mm, mit Laschen 102 mm breit'], ['Höhe mit Bumper', 'hinten 94,5 mm, vorn 87 mm'],
         ['Darüber', 'Token 5 mm, Drehknopf 9 mm über der Deckeloberfläche']], [42*mm, FW-42*mm]), Spacer(1, 10),
  P('Auf einen Blick', 'h2'), glance(), Spacer(1, 10),
  note('Firmware, Platinen, Modell und diese Anleitung liegen im Repo ' + repo() + '. Kapitel 5 beschreibt den Aufbau der Elektronik, Kapitel 6 den Zusammenbau. Hintergründe zur Schaltung stehen in <b>doc/hardware.md</b>, die Pinbelegung der Firmware in <b>doc/architecture.md</b>, das Abstimmen der Klopfsensoren in <b>doc/development.md</b>.', 'info'),
  PageBreak()]

# 2 Druckteile
rows = [['Teil', 'Funktion', 'Material', 'Befestigung'],
 [pn('front'), 'Deckel mit Token-Mulde, LED, Poti, Kopfhörerbuchse und Tasche für den Mikroschalter', 'PETG/PLA', 'Sekundenkleber auf ' + pn('frame')],
 [pn('frame'), 'Rahmen: Seitenwände mit Laschen für den Tragegurt, Lautsprecheröffnung, USB-C-Ausschnitt. An den Tasten ist die Wand innen auf 1 mm gedünnt', 'PETG/PLA', '–'],
 [pn('back'), 'Boden: Rahmen um die Klappe mit vier Gewindeeinsätzen', 'PETG/PLA', 'Sekundenkleber an ' + pn('frame')],
 [pn('door'), 'Bodenklappe', 'PETG/PLA', '4 × M3 × 5 in ' + pn('back')],
 [pn('bumper'), 'Fuß unter der Klappe', 'TPU', 'Sekundenkleber an ' + pn('door')],
 [pn('speaker grill'), 'Lautsprechergitter, mit Stoff bezogen', 'PETG/PLA', 'eingesetzt, Sekundenkleber'],
 [pn('prev button'), 'Taste „zurück“, Ø 20 × 1 mm', 'PETG/PLA', 'Sekundenkleber, außen auf ' + pn('frame')],
 [pn('next button'), 'Taste „vor“, Ø 20 × 1 mm', 'PETG/PLA', 'Sekundenkleber, außen auf ' + pn('frame')],
 [pn('volume'), 'Drehknopf', 'PETG/PLA', 'auf die Poti-Achse gesteckt'],
 [pn('token'), 'Token mit eingedrucktem NFC-Tag, einer je Album', 'PETG/PLA', '–'],
 [pn('pcb_spacer'), 'Abstandshalter zwischen Hauptplatine und RC522, eigene Datei <font name="Mono" size="7.9">models/pcb_spacer.step</font>', 'PETG/PLA', 'eingeclipst']]
story += [H1(2, 'Druckteile'), P('Die Teilenamen sind die aus dem STEP-Modell <font name="Mono" size="8.3">models/box.step</font>. Dort liegen alle Teile in Einbaulage. Der Abstandshalter für die Elektronik liegt als eigene Datei in <font name="Mono" size="8.3">models/pcb_spacer.step</font>.'),
  table(rows, [27*mm, 82*mm, 20*mm, FW-129*mm]), Spacer(1, 8),
  fig('exploded', [dict(a='token', t='token'), dict(a='volume', t='volume', s='Drehknopf'), dict(a='front', t='front', s='Deckel'),
     dict(a='frame', t='frame', s='Rahmen'), dict(a='speaker grill', t='speaker grill', s='Lautsprechergitter'),
     dict(a='prev button', t='prev button', s='Taste „zurück“'), dict(a='next button', t='next button', s='Taste „vor“'),
     dict(a='back', t='back', s='Boden'), dict(a='door', t='door', s='Bodenklappe'), dict(a='bumper', t='bumper', s='Fuß, TPU')], frac=0.54),
  cap('Explosionszeichnung. Unten die Klappe mit Bumper, seitlich die Tasten, vorn das Lautsprechergitter.'), PageBreak()]

# 3 Drucken
orient = [['Teil', 'Aufs Druckbett', 'Stützen', 'Hinweis'],
 [pn('frame'), 'Unterkante', 'im Lautsprecherloch und unter den Laschen', 'Das Gitter hat nur 0,2 mm Spiel: Loch entgraten'],
 [pn('front'), 'Unterkante, Oberseite nach oben', 'innen unter der Decke, nur vom Druckbett (Baumstützen)', 'Taschen für Mikroschalter und Buchse freimachen'],
 [pn('back'), 'Seite zum Rahmen (kopfüber)', 'keine', ''],
 [pn('door'), 'Innenseite', 'keine', 'Senkungen zeigen nach oben'],
 [pn('bumper'), 'Klebeseite', 'keine', 'TPU, langsam drucken'],
 [pn('speaker grill'), 'Außenseite', 'keine', ''],
 [pn('prev button') + ', ' + pn('next button'), 'Sichtseite', 'keine', '1 mm dünn'],
 [pn('volume'), 'Oberseite', 'keine', ''],
 [pn('token'), 'Oberseite (Bildseite)', 'keine', 'Pause vor der letzten Schicht für den NFC-Tag, siehe unten'],
 [pn('pcb_spacer'), 'flache Seite', 'keine', 'Eigene Datei, siehe Kapitel 5.2']]
tiles = [('frame','Unterkante'), ('front','Unterkante'), ('back','kopfüber'), ('door','Innenseite'), ('bumper','Klebeseite, TPU'),
         ('speaker grill','Außenseite'), ('prev button','Sichtseite'), ('next button','Sichtseite'), ('volume','Oberseite'), ('token','Oberseite'), ('pcb_spacer','flache Seite')]
cw = FW/6; cells = []
for name, bed in tiles:
    im = PILImage.open(FIG+'tile_'+name.replace(' ','_')+'.jpg'); iw = cw-4; ih = iw*im.height/im.width
    cells.append([Image(FIG+'tile_'+name.replace(' ','_')+'.jpg', iw, ih), Paragraph(f'{pn(name, 7.4)}<br/>{bed}', S['tile'])])
grid = Table([cells[0:6], cells[6:] + [''] * (12 - len(cells))], colWidths=[cw]*6)
grid.setStyle(TableStyle([('ALIGN',(0,0),(-1,-1),'CENTER'), ('VALIGN',(0,0),(-1,-1),'TOP'), ('LEFTPADDING',(0,0),(-1,-1),2), ('RIGHTPADDING',(0,0),(-1,-1),2), ('BOTTOMPADDING',(0,0),(-1,-1),6)]))
story += [H1(3, 'Drucken'),
  P('Alle Teile außer dem Bumper aus PETG oder PLA, den Bumper aus TPU. PETG verträgt Wärme besser, etwa im Auto oder auf der Fensterbank.'),
  P('Am schnellsten geht es mit den fertigen Druckbetten in <font name="Mono" size="8.3">models/plates/</font>: eine 3MF-Datei pro Farbe, die Teile schon angeordnet, für Betten ab 220 × 220 mm. In PrusaSlicer öffnet man sie als Projekt (Datei › Projekt öffnen), dann sind Schichthöhe, Wände, Füllung, Stützen und die Pause für die Tokens schon eingestellt. Für Bambu Studio liegt dort ein Projekt mit allen fünf Betten, ebenfalls fertig eingestellt. Andere Slicer übernehmen nur die Anordnung. Einzelne Teile liegen als STL in <font name="Mono" size="8.3">models/stl/</font>, benannt wie in der Tabelle und schon so gedreht, wie sie aufs Bett kommen.'),
  P('Einstellungen: 0,2 mm Schichthöhe, 3 Wände, 15–20 % Füllung. Die NFC-Tasche im Token ist auf 0,2 mm Schichthöhe ausgelegt.'),
  table(orient, [33*mm, 38*mm, 46*mm, FW-117*mm]), Spacer(1, 8), grid, cap('Druckausrichtung, die graue Fläche ist das Druckbett.'),
  note('Den Deckel mit der Oberseite nach oben drucken. Andersherum lägen die gerundeten Kanten (R 7,5) direkt auf dem Bett und würden unsauber. So bleiben Oberseite und Rundungen glatt, die Stützen sitzen innen, wo man sie nicht sieht. Die Bohrung der Kopfhörerbuchse ist oben mit einer 0,2-mm-Schicht verschlossen und bleibt zu, solange keine Buchse eingebaut ist.', 'tip'),
  PageBreak()]
tok_steps = [bullets([
  'Den Token mit der Oberseite (Ø 44 mm) aufs Bett legen. Die Bildfläche wird so glatt, und der breite Rand haftet gut.',
  'Im Slicer eine Pause vor der letzten Schicht einfügen, bei 0,2 mm Schichthöhe vor <b>Z = 15,0 mm</b>. Dann ist die NFC-Tasche oben offen.',
  'In der Pause den NFC-Tag mit der Klebeseite nach unten in die Tasche legen und andrücken. Weiterdrucken, die letzte Schicht schließt die Tasche. Der Tag liegt dann 0,2 mm unter der Unterseite, direkt über dem RC522.',
  'Warnt der Slicer vor langen Brücken, ist das die Decke über dem Hohlraum im Token. Damit ist zu rechnen.',
  'Das Bild aus dem Token-Bogen (<font name="Mono" size="8.3">tools/token-sheet.html</font>) grob ausschneiden, zum Beispiel mit doppelseitigem Klebeband auf die Oberseite (Ø 44 mm) kleben und ringsum an der Kante abschneiden.'])]
two = Table([[tok_steps, LayerDiagram()]], colWidths=[FW-92*mm, 92*mm]); two.setStyle(TableStyle([('VALIGN',(0,0),(-1,-1),'TOP'), ('LEFTPADDING',(0,0),(-1,-1),0), ('RIGHTPADDING',(0,0),(-1,-1),0)]))
story += [P('Token mit NFC-Tag', 'h2'),
  P('Der Token ist 15 mm hoch und innen hohl. 0,2 mm über der Unterseite liegt eine Tasche (Ø 26 × 0,4 mm) für einen runden NFC-Klebetag: 25 mm, NTAG, höchstens 0,4 mm dick.'),
  fig('token_section', [dict(a='top', t='Oberseite Ø 44 mm', s='liegt auf dem Bett'), dict(a='flange', t='Rand', s='liegt im Gehäuse auf dem Deckel'),
       dict(a='body', t='Körper Ø 39,7 mm', s='passt in die Mulde Ø 40'), dict(a='void', t='Hohlraum'),
       dict(a='pocket', t='NFC-Tasche Ø 26 × 0,4 mm', s='0,2 mm unter der Unterseite'), dict(a='chamfer', t='Fase'), dict(a='bed', t='Druckbett')], frac=0.64),
  cap('Token im Schnitt, so wie er gedruckt wird.'), two, Spacer(1, 8),
  note('Sondertokens für Bluetooth und Management werden genauso gedruckt. Ihre UIDs kommen in die <font name="Mono" size="8.3">platformio.ini</font>, siehe Kapitel 5.5.', 'info'), PageBreak()]

# 4 Material
story += [H1(4, 'Material und Werkzeug'), P('Zukaufteile fürs Gehäuse', 'h2'),
  table([['Teil', 'Anzahl', 'Anmerkung'],
   ['Gewindeeinsatz M3', '4', 'Für Bohrung Ø 4,0 mm, 4 mm lang. Bis 5,7 mm geht, ragt dann innen 1,2 mm heraus'],
   ['Zylinderkopfschraube M3 × 5, ISO 4762', '4', 'Kopf Ø 5,5 mm versinkt in der Senkung Ø 5,6 mm und im Bumper. Nicht länger als 5 mm: In zwei Ecken steht direkt über dem Einsatz ein Akku'],
   ['18650-Zelle', '2', 'Einfache Zellen ohne Schutzschaltung (65 mm) mit Lötfahnen, gleicher Typ und gleiche Spannung. Parallel geschaltet'],
   ['Stoff', '1 Stück, 8 × 8 cm', 'Dünn und dehnbar, zum Beispiel eine Socke'],
   ['NFC-Klebetag, rund, 25 mm', 'je Token', 'NTAG, höchstens 0,4 mm dick'], ['Rolladengurt, ca. 15 mm breit', 'ca. 30 cm', 'Tragegurt durch die Laschen, aus dem Baumarkt'], ['Kabelbinder, schmal', '1', 'Um Hauptplatine, Abstandshalter und RC522'], ['Transparentes Filament, 1,75 mm', 'ein paar cm', 'Lichtleiter von der Lade-LED nach außen, darf gebogen verlaufen']], [55*mm, 26*mm, FW-81*mm]),
  P('Kleber und Kleinkram', 'h2'),
  table([['Material', 'wofür'], ['Sekundenkleber', 'Gehäuseschale, Tasten, Lautsprechergitter, Bumper'], ['2-Komponenten-Kleber (Epoxid)', 'Lautsprecher, Ladeschaltung, Step-up'],
   ['UV-Lack mit UV-Lampe', 'LED, USB-C-Buchse, Mikroschalter'], ['Heißkleber', 'Akkus'], ['Dünnes doppelseitiges Klebeband', 'Piezos, RC522, Bilder auf den Token'],
   ['Kapton-Band, Schrumpfschlauch', 'Akkukontakte und Lötstellen isolieren'], ['Nadel und reißfestes Garn oder Nähmaschine', 'Tragegurt'], ['Schleifpapier, Grundierung, Lack', 'optional, für die Oberfläche']], [62*mm, FW-62*mm]),
  P('Elektronik im Gehäuse', 'h2'), P('Die vollständige Teileliste steht in Kapitel 5.1. Diese Teile müssen zum Gehäuse passen:'),
  table([['Teil', 'Ort', 'Passt, wenn …'],
   ['Lautsprecher, 3 W', 'innen am Gitter', 'Ø 50–57 mm liegt mit dem Rand auf dem Ring (Ø 48,6–54,6 mm); kleinere sitzen im Ring direkt am Gitter'],
   ['Piezo-Scheibe, ca. 35 mm', 'innen an den Seitenwänden', 'gedünnte Fläche 56 × 50 mm'],
   ['PL9823, 8 mm', 'Deckel vorn links', 'Bohrung Ø 8 mm'],
   ['Poti 10 kΩ', 'Deckel hinten links', 'Gewinde M7 (Bohrung Ø 7 mm), Achse 6 mm rund. Gewindehülse und Mutter müssen in die 3 mm tiefe Aussparung im Knopf passen'],
   ['RC522', 'unter der Token-Mulde', 'Muldenboden 1 mm'], ['Mikroschalter', 'Tasche neben der Mulde', 'Hebel reicht durch den Schlitz in die Mulde'],
   ['USB-C-Platine', 'Rückwand unten', 'Ausschnitt 9,2 × 3,5 mm'], ['Power Latch mit TP4056', 'Wandinnenseite, nahe der USB-C-Buchse', 'Lichtleiter vom Loch Ø 2 mm bis an die Lade-LED'], ['Step-up 5 V', 'freie Stelle an der Wandinnenseite', 'nicht auf den gedünnten Tastenflächen'], ['Kopfhörerbuchse 3,5 mm', 'Deckel hinten rechts, optional', 'Bohrung Ø 6 mm, Tasche 12 × 7 × 2 mm']],
   [38*mm, 44*mm, FW-82*mm]),
  P('Werkzeug', 'h2'),
  P('Lötkolben (für die Einsätze eine Einschmelzspitze oder eine alte Spitze), Innensechskant 2,5 mm, UV-Lampe, Heißklebepistole, Multimeter, Säge oder Seitenschneider für die Poti-Achse, Cutter, Schleifpapier. Für die Elektronik dazu: Lötkolben mit feiner Spitze, Lötzinn, Flussmittel, Entlötlitze, Pinzette, Abisolierzange und ein USB-C-Kabel zum Flashen.'),
  CondPageBreak(160*mm)]

# 5 Zusammenbau

# 5 Elektronik
EB = '#2f5fa8'   # badge colour of the electronics steps
MB = load_board(str(ROOT / 'hardware' / 'mainboard' / 'timon-box.kicad_pcb'))
PL = load_board(str(ROOT / 'hardware' / 'power-latch' / 'power-latch-v2.kicad_pcb'))
G_MB = [('#f0a23b', 'E1 SMD-Teile', ['U2', 'U3', 'Q1', 'Q2', 'D1', 'D2', 'R1', 'R2', 'R3', 'R4', 'R5', 'C1', 'C2', 'C3', 'C4', 'C7', 'C8']),
        ('#6f9ef0', 'E2 ESP32 und Taster', ['U1', 'SW1', 'SW2']), ('#b48ae0', 'E3 XH-Buchse', ['J4']),
        ('#5cc0b5', 'E4 Buchsenleisten', ['MAX1', 'U4', 'P1'])]
G_PL = [('#f0a23b', 'E7 SMD-Teile', ['Q1', 'Q2', 'Q3', 'Q4', 'D1', 'R1', 'R2', 'R3', 'R4', 'R5', 'R6', 'R7', 'C1', 'C2']),
        ('#6f9ef0', 'E7 D2 zuletzt', ['D2']), ('#b48ae0', 'E7 TP4056-Modul', ['J4']), ('#5cc0b5', 'E10 Kabel an J1', ['J1'])]


def boards(board, k, groups, dnp=(), titles=('Vorderseite', 'Rückseite, von unten gesehen')):
    a = PcbView(board, 'F', k, groups, dnp, titles[0]); b = PcbView(board, 'B', k, groups, dnp, titles[1])
    t = Table([[a, b]], colWidths=[a.width + 10*mm, b.width], hAlign='CENTER')
    t.setStyle(TableStyle([('VALIGN', (0, 0), (-1, -1), 'TOP'), ('LEFTPADDING', (0, 0), (-1, -1), 0), ('RIGHTPADDING', (0, 0), (-1, -1), 0)]))
    return t


story += [CondPageBreak(160*mm), H1(5, 'Elektronik'),
  P('Die Elektronik besteht aus zwei Platinen und ein paar lose verdrahteten Teilen. Die Bestückungspläne sind direkt aus den KiCad-Dateien unter <font name="Mono" size="8.3">hardware/</font> gezeichnet, die Schaltpläne stehen im Anhang.'),
  bullets(['<b>Hauptplatine</b> (40,9 × 60,5 mm): ESP32, USB-seriell-Wandler CH340C, 3,3-V-Regler und drei Module auf Buchsenleisten: I²S-Verstärker MAX98357A und SD-Karte oben, RFID-Leser RC522 unten.',
           '<b>Power Latch V2</b> (31,3 × 18,3 mm): Lademodul TP4056, Umschaltung zwischen USB und Akku und die Selbsthaltung. Ein Mikroschalter schaltet die Box ein, wenn ein Token aufgelegt wird. Danach hält der ESP32 die Box selbst an. Anders lässt sie sich nicht einschalten, auch nicht über USB.',
           '<b>Lose verdrahtet:</b> 18650-Zellen, USB-C-Buchse, Boost-Wandler auf 5 V, zwei 2N7000, Mikroschalter, zwei Piezos, Poti, LED und Lautsprecher.']),
  P('Ein 7-poliges Kabel verbindet die Hauptplatine mit der Stromseite. Über dieses Kabel laufen auch die USB-Datenleitungen. Die USB-C-Buchse am Gehäuse lädt also den Akku und dient zugleich zum Flashen.'),
  P('5.1  Teileliste', 'h2'),
  table([['Teil', 'Anzahl', 'Anmerkung'],
   ['ESP32-WROOM-32, -32D oder -32E', '1', 'Ohne PSRAM. Ein WROVER geht nicht, dort belegt der PSRAM GPIO 16 und 17, die SD-Karte braucht sie'],
   ['MAX98357A-Modul', '1', 'I²S-Verstärker. 7 Pins: LRC, BCLK, DIN, GAIN, SD, GND, VIN'],
   ['microSD-Modul für 3,3 V', '1', '6 Pins: 3V3, CS, MOSI, CLK, MISO, GND. Die Platine wird gekürzt (Schritt E5)'],
   ['RC522-Modul', '1', 'RFID mit 13,56 MHz. 8 Pins, sitzt unter der Hauptplatine'],
   ['TP4056-Lademodul mit Schutzschaltung', '1', 'Wird auf die Rückseite der Power Latch gelötet'],
   ['Boost-Modul mit festen 5 V', '1', 'Manche Module stellt man erst mit Lötbrücken auf 5 V'],
   ['18650-Zelle', '1–2', 'Einfache Li-Ion-Zellen, 65 mm lang. Im gedruckten Gehäuse zwei, parallel. Die Schutzschaltung sitzt auf dem TP4056-Modul'],
   ['USB-C-Buchse auf kleiner Platine', '1', 'Mit 5,1 kΩ an CC1 und CC2, sonst liefern USB-C-Netzteile keine 5 V. D+ und D− müssen herausgeführt sein'],
   ['2N7000 (TO-92)', '2', 'Umbau an der Power Latch (Schritt E8)'],
   ['Mikroschalter 12,8 × 5,8 mm', '1', 'Wechsler mit C, NO und NC. Der Token drückt ihn'],
   ['Piezo-Scheibe, etwa 35 mm', '2', 'Klopfsensoren für Vor und Zurück'],
   ['Poti 10 kΩ linear', '1', 'Lautstärke'],
   ['PL9823, 8 mm', '1', 'Bedrahtete RGB-LED mit eingebautem Controller, Farbreihenfolge RGB'],
   ['Lautsprecher, 3 W', '1', 'Klein, 4 oder 8 Ω'],
   ['JST-XH-Buchse 7-polig, stehend, mit Stecker', '1', '2,5 mm Raster, für das Kabel zur Stromseite'],
   ['Buchsen- und Stiftleisten, 2,54 mm', '', '7-polig für den Verstärker, 6-polig für die SD-Karte, 8-polig für das RC522'],
   ['microSD-Karte', '1', 'FAT32'],
   ['NFC-Klebetags, rund, 25 mm', 'je Token', '13,56 MHz, NXP NTAG mit 7-Byte-UID. Sie werden beim Druck in den Token eingelegt (Kapitel 3)']],
   [55*mm, 18*mm, FW-73*mm]),
  CondPageBreak(120*mm)]

story += [P('5.2  Hauptplatine', 'h2'),
  table([['Bauteil', 'Wert', 'Bauform', 'Aufgabe'],
   ['U1', 'ESP32-WROOM-32(D/E)', 'Modul', ''], ['U2', 'AMS1117-3.3', 'SOT-223', '3,3 V aus den 5 V vom Kabel'],
   ['U3', 'CH340C', 'SOIC-16', 'USB-seriell, ohne Quarz'], ['Q1, Q2', '2N7002', 'SOT-23', 'Auto-Reset: DTR und RTS steuern EN und IO0'],
   ['D1, D2', 'BZT52C3V3', 'SOD-323', 'Zenerdioden, begrenzen die Piezo-Spannung auf 3,3 V'],
   ['R1, R2', '100 kΩ', '0805', 'Last parallel zu den Piezos'], ['R3', '10 kΩ', '0805', 'Pull-up an EN'],
   ['R4, R5', '10 kΩ', '0805', 'Pull-ups für I²C (Reserve)'], ['C1, C8', '100 nF', '0805', 'Abblockkondensatoren CH340C und ESP32'],
   ['C2', '1 µF', '0805', 'Mit R3 die Startverzögerung an EN'], ['C3', '10 µF', '0805', 'Eingang AMS1117'],
   ['C4, C7', '22 µF', '1206', '3,3 V'], ['C5, C6', '–', '0805', '<b>Nicht bestücken</b>'], ['SW1, SW2', 'Taster 6 × 6 mm', 'PTS645', 'RST und BOOT'],
   ['J4', 'JST XH, 7-polig, stehend', '', 'Kabel zur Stromseite, siehe Schritt E3'], ['J1, J5', '3-polig', '', 'Poti und LED, Stiftleiste oder Kabel direkt'],
   ['J2, J3', '2-polig', '', 'Piezos, Stiftleiste oder Kabel direkt'], ['J6', '4-polig', '', 'I²C, Reserve für Erweiterungen. Bleibt frei'],
   ['MAX1', 'Buchsenleiste 7-polig', '', 'MAX98357A-Modul'], ['U4', 'Buchsenleiste 6-polig', '', 'SD-Modul'],
   ['P1', 'Buchsenleiste 8-polig, Rückseite', '', 'RC522-Modul']], [18*mm, 44*mm, 20*mm, FW-82*mm]),
  Spacer(1, 6),
  KeepTogether([boards(MB, 1.75, G_MB, dnp=['C5', 'C6']), Spacer(1, 4),
    ChipRow([(g[1], g[0]) for g in G_MB] + [('nicht bestücken', '#e05a4f')]),
    cap('Bestückungsplan der Hauptplatine aus <font name="Mono" size="7.4">hardware/mainboard/timon-box.kicad_pcb</font>. Die Rückseite ist gespiegelt, so wie man von unten draufschaut; das RC522 deckt sie fast ganz ab.')]),
  step('E1', 'SMD-Teile bestücken', ['LK'], [
    'CH340C (U3), AMS1117 (U2), die beiden 2N7002 (Q1, Q2), Widerstände, Kondensatoren und Zenerdioden bestücken. Werte in der Tabelle, Lage im Bestückungsplan.',
    'C5 und C6 bleiben frei.'], color=EB),
  step('E2', 'ESP32 und Taster einlöten', ['LK'], [
    'Das ESP32-Modul (U1) und die beiden Taster RST (SW1) und BOOT (SW2) einlöten.',
    'Nur ein WROOM-Modul ohne PSRAM nehmen. Beim WROVER belegt der PSRAM GPIO 16 und 17, die braucht die SD-Karte.'], color=EB),
  step('E3', 'XH-Buchse J4 einsetzen', ['LK'], [
    'Der Footprint von J4 ist eine 2,54-mm-Stiftleiste. Eingebaut wird eine JST-XH-Buchse mit 2,5 mm Raster, sie passt in dieselben Löcher.',
    'Ihr Gehäuse ist tiefer als eine Stiftleiste und stößt an den AMS1117 (U2). Das Gehäuse deshalb an der Unterseite über dem Regler ausklinken, dann einlöten.',
    'Pin 1 (+5 V) liegt an der Platinenkante neben C4, Pin 7 (D−) zum ESP32 hin.'], color=EB),
  step('E4', 'Buchsenleisten einlöten', ['LK'], [
    'Oben die Buchsenleisten für den Verstärker (MAX1, 7-polig) und das SD-Modul (U4, 6-polig) einlöten, unten die für das RC522 (P1, 8-polig).'], color=EB),
  step('E5', 'SD-Modul kürzen', [], [
    'Die Platine des SD-Moduls ragt über die XH-Buchse. Die Kante zur Buchse hin absägen oder abschleifen, bis das Modul neben der Buchse steckt (Foto unten).'], color=EB),
  step('E6', 'Module einstecken', ['LK'], [
    'Verstärker und SD-Modul oben einstecken.',
    'Am RC522 die Stiftleiste so einlöten, dass es unter der Hauptplatine sitzt.',
    'Genau gegenüber der Stiftleiste sichert der gedruckte Abstandshalter (' + pn('pcb_spacer') + ') die frei hängende Hauptplatine, damit sie nicht nur an der Stiftleiste hängt. Sie rastet mit ihren Längskanten zwischen seinen Armen ein, sein Fuß steht auf dem RC522. Im Gehäuse kommt noch ein Kabelbinder darum (Kapitel 6, Schritt 11).',
    'Im Gehäuse klebt das RC522 mit der flachen Rückseite unter der Token-Mulde (Kapitel 6, Schritt 9).'],
    [('info', 'Am MAX98357A liegt GAIN auf GND, das sind 12 dB. SD bleibt offen. Die Firmware schickt ohnehin mono, der Kanal ist also egal.')], color=EB),
  KeepTogether([fig('spacer_stack', [dict(a='spacer', t='pcb_spacer', s='Abstandshalter'), dict(a='arm', t='Arme mit Rastnasen', s='halten die Hauptplatine'),
     dict(a='mainboard', t='Hauptplatine'), dict(a='rc522', t='RC522')], frac=0.6),
     cap('Der Abstandshalter genau gegenüber der Stiftleiste, zwischen Hauptplatine und RC522. Die Stiftleiste sitzt am anderen Ende, hinten im Bild.')]),
  KeepTogether([photos([('hw_overview_top.jpeg', 'Von oben: Verstärker (lila), SD-Modul, darunter die XH-Buchse J4, rechts der ESP32.'),
                        ('hw_overview_buttom.jpeg', 'Von unten: das RC522.'),
                        ('hw_cutpcb.jpeg', 'Das gekürzte SD-Modul (oben eingekreist) und die ausgeklinkte XH-Buchse über dem AMS1117 (unten eingekreist).')], cols=3),
                photos([('hw_overview_side.jpeg', 'Von der Seite: Verstärker und SD-Modul auf Buchsenleisten, das RC522 unter der Hauptplatine.')], cols=1, width=110*mm)])]

story += [CondPageBreak(140*mm), P('5.3  Stromseite', 'h2'),
  table([['Bauteil', 'Wert', 'Bauform', 'Aufgabe'],
   ['Q1', 'AO4606', 'SOIC-8', 'N- und P-Kanal in einem Gehäuse: P schaltet Vcc, N hält ihn'], ['Q2', 'BSS138', 'SOT-23', 'TrigOut, Open Drain zum ESP32'],
   ['Q3', 'BSS138', 'SOT-23', 'Einschaltimpuls'], ['Q4', 'AO3401A', 'SOT-23', 'Akku-Zweig der Umschaltung USB/Akku'],
   ['D1', 'BAT85', 'SOD-323', 'Klemmt den Einschaltimpuls nach unten'], ['D2', 'BAT85', 'DO-35, bedrahtet', 'USB-Zweig der Umschaltung'],
   ['R1, R4–R7', '10 kΩ', '0805', 'R4/R5: Spannungsteiler für Sense'], ['R2', '1 MΩ', '0805', 'Pull-down an Hold'],
   ['R3', '100 kΩ', '0805', 'Entlädt C1'], ['C1', '10 µF', '0805', 'Einschaltimpuls'], ['C2', '1 µF', '0805', 'Gate des P-Kanals'],
   ['J4', 'TP4056-Modul', '', 'Auf der Rückseite aufgelötet'], ['J1', '–', '', 'Hier werden die sieben Kabel direkt eingelötet']],
   [18*mm, 30*mm, 30*mm, FW-78*mm]),
  Spacer(1, 6),
  KeepTogether([boards(PL, 2.5, G_PL), Spacer(1, 4), ChipRow([(g[1], g[0]) for g in G_PL]),
    cap('Bestückungsplan der Power Latch V2 aus <font name="Mono" size="7.4">hardware/power-latch/power-latch-v2.kicad_pcb</font>. Die Beschriftung von J1 steht auf beiden Seiten.')]),
  P('So schaltet die Power Latch', 'h3'),
  bullets(['<b>USB oder Akku:</b> Die 5 V vom USB kommen über D2 zur Last, der Akku über Q4. Steckt USB, sperrt Q4. Die Box läuft dann vom USB, und das TP4056 lädt den Akku.',
           '<b>Einschalten:</b> Ein Token drückt den Mikroschalter, der verbindet ' + code('Trig+') + ' mit ' + code('TrigIn') + '. Über C1 schaltet Q3 für etwa eine Sekunde durch, genug, damit der ESP32 startet und HOLD setzt.',
           '<b>Halten:</b> Liegt ' + code('Hold') + ' hoch, hält der N-Kanal von Q1 die Box an. Sonst zieht R2 ' + code('Hold') + ' auf GND.',
           '<b>Token-Meldung:</b> Solange der Schalter geschlossen ist, zieht Q2 ' + code('TrigOut') + ' auf GND. Der ESP32 liest das an GPIO 32.',
           '<b>Akkuspannung:</b> R4 und R5 teilen ' + code('Vcc') + ' durch 2 und geben das auf ' + code('Sense') + '. Der ESP32 misst es an GPIO 39.']),
  step('E7', 'Power Latch bestücken', ['LK'], [
    'Die SMD-Teile bestücken, die bedrahtete Diode D2 zuletzt.',
    'Dann das TP4056-Modul auf die Rückseite auflöten (J4). Seine vier Eckpads IN+, IN−, OUT+ und OUT− sitzen direkt auf dem Footprint.'], color=EB),
  step('E8', 'Zwei 2N7000 nachrüsten', ['LK'], [
    code('Hold') + ' der Latch hängt nicht direkt am ESP32. Zwei 2N7000 liegen parallel: Drain an ' + code('Vcc') + ', Source an ' + code('Hold') + '.',
    'Gate des einen („ESP“) an HOLD vom ESP32, Kabel Pin 3 (blau). Gate des anderen („USB“) an D− der USB-C-Buchse.',
    'Die beiden bilden ein logisches Oder: ' + code('Hold') + ' liegt hoch, wenn der ESP32 HOLD setzt oder an D− Spannung liegt. Einschalten kann keiner der beiden, denn ist die Box aus, liegt an ' + code('Vcc') + ' keine Spannung.'], color=EB),
  step('E9', 'Boost-Modul prüfen', [], [
    'Das Boost-Modul vor dem Anschließen allein und ohne Last messen: Es muss 5 V liefern. Manche Module stellt man erst mit Lötbrücken auf 5 V.'], color=EB),
  step('E10', 'Stromseite verdrahten', ['LK'], [
    'USB-C-Buchse, Mikroschalter, Boost-Modul, die beiden 2N7000 und die sieben Adern des XH-Kabels nach den Tabellen in 5.4 anlöten. Die Kabel lang genug für den Einbau lassen.',
    'Der NC-Kontakt des Mikroschalters bleibt frei. Die Akkus kommen erst beim Einbau dazu (Kapitel 6, Schritt 11).'], color=EB),
  photos([('hw_bat.jpeg', 'Power Latch V2 mit dem TP4056-Modul auf der Rückseite, rechts die USB-C-Buchse, unten die 18650-Zelle.'),
          ('hw_hack.jpeg', 'Einer der beiden 2N7000 auf den Pads Hold und Vcc der Latch.'),
          ('hw_peripherals.jpeg', 'Stromseite mit Mikroschalter und Kabel zur Hauptplatine.')], cols=3)]

j1 = table([['Pin', 'Beschriftung', 'an'], ['1', code('Vcc'), 'Boost IN+ und Drain beider 2N7000'], ['2', code('Hold'), 'Source beider 2N7000'],
            ['3', code('Sense 50%'), 'Kabel Pin 4 (gelb)'], ['4', code('Trigger +'), 'Mikroschalter C'], ['5', code('Trigger In'), 'Mikroschalter NO'],
            ['6', code('Trigger Out'), 'Kabel Pin 5 (orange)'], ['7', code('GND'), 'Boost IN−']], [12*mm, 28*mm, 62*mm])
conn = [photo('hw_connector.jpeg', 58*mm), Paragraph('Der XH-Stecker mit den Aderfarben, links Pin 1.', S['pcap'])]
side = Table([[[P('Power Latch, Anschluss J1', 'h3'), j1, Spacer(1, 3), P('Der NC-Kontakt des Mikroschalters bleibt frei.', 'cap')], conn]], colWidths=[FW-62*mm, 62*mm])
side.setStyle(TableStyle([('VALIGN', (0, 0), (-1, -1), 'TOP'), ('LEFTPADDING', (0, 0), (-1, -1), 0), ('RIGHTPADDING', (0, 0), (0, 0), 6*mm), ('RIGHTPADDING', (1, 0), (1, 0), 0)]))
story += [CondPageBreak(120*mm), P('5.4  Verkabelung', 'h2'),
  P('Jede Zeile nennt beide Enden. Das vollständige Verdrahtungsdiagramm der Stromseite steht im Anhang.'),
  P('Kabel zwischen Hauptplatine und Stromseite', 'h3'),
  P('Auf der Hauptplatine steckt das Kabel in J4, auf der Stromseite gehen die Adern einzeln an verschiedene Teile. Pin 1 (+5 V) liegt an der Platinenkante neben C4, Pin 7 (D−) zum ESP32 hin.'),
  table([['Pin J4', 'Signal', 'Ader', 'ESP32', 'Stromseite'], ['1', '+5V', Wire('rot'), '–', 'Boost OUT+'], ['2', 'GND', Wire('schwarz'), '–', 'Boost OUT−'],
         ['3', 'HOLD', Wire('blau'), 'GPIO 33', 'Gate des 2N7000 „ESP“'], ['4', 'SENSE', Wire('gelb'), 'GPIO 39', 'Latch J1 Pin 3, ' + code('Sense 50%')],
         ['5', 'SW_TOKEN', Wire('orange'), 'GPIO 32', 'Latch J1 Pin 6, ' + code('Trigger Out')], ['6', 'D+', Wire('grün'), 'CH340C', 'USB-C D+'],
         ['7', 'D−', Wire('weiß'), 'CH340C', 'USB-C D− und Gate des 2N7000 „USB“']], [16*mm, 24*mm, 26*mm, 22*mm, FW-88*mm]),
  Spacer(1, 4), side,
  P('Akku, Laden und USB-C', 'h3'),
  table([['von', 'an'], ['USB-C VBUS, GND', 'TP4056 IN+, IN−. Auf der Latch sind das die Pads 5v + und GND'], ['18650 Plus, Minus (beide Zellen parallel)', 'TP4056 B+, B−'],
         ['TP4056 OUT+, OUT−', 'Latch Bat+, Bat−. Das Modul ist dort aufgelötet'], ['USB-C D+, D−', 'Kabel Pin 6 und 7, D− zusätzlich an das Gate des 2N7000 „USB“']],
        [62*mm, FW-62*mm]),
  P('Bedienteile an der Hauptplatine', 'h3'),
  table([['Anschluss', 'Pins', 'GPIO', 'an'], ['J1 „Volume“', '+3V3, SIG, GND', '36', 'Poti: Enden an +3V3 und GND, Schleifer an SIG'],
         ['J2 „R Btn“', 'BTN, GND', '34', 'Piezo an der <b>linken</b> Wand, von vorn gesehen: zurück'], ['J3 „L Btn“', 'BTN, GND', '35', 'Piezo an der <b>rechten</b> Wand: vor'],
         ['J5 „NeoPixel“', '+5V, DATA, GND', '2', 'PL9823: VDD, DIN, GND. DOUT bleibt frei'], ['J6 „I2C“', '+3V3, GND, SDA, SCL', '14, 13', 'Reserve, frei'],
         ['MAX98357A', '+, −', '', 'Lautsprecher']], [28*mm, 34*mm, 16*mm, FW-78*mm]),
  Spacer(1, 6),
  note('Die Beschriftung von J2 und J3 auf der Platine ist seitenverkehrt, in der Firmware ebenso. Wer den Piezo von J2 an die linke Wand klebt, bekommt links zurück und rechts vor.', 'info'),
  Spacer(1, 6),
  note('Die Piezos sind rückwärts gepolt: die Messingseite (meist die schwarze Ader) an BTN, die Keramikseite (meist rot) an GND. Löst ein Piezo erst beim Loslassen aus, seine Adern tauschen. Wird die Box beim Drehen nach links lauter statt leiser, die beiden äußeren Drähte am Poti tauschen.', 'tip')]

ENV_EXAMPLE = r'''[env:meinebox]
build_flags =
    ${env.build_flags}
    -D BOX_NAME=\"Meine-Box\"
    -D BOX_COLOR=\"#2f6b5a\"
    -D BLUETOOTH_TAG=\"04c6d4b26e6780\"
    -D MANAGEMENT_SERVER_TAG=\"042041122a6781\"
    -D BATTERY_ADC_LOW=1994
    -D BATTERY_MV_LOW=3685
    -D BATTERY_ADC_HIGH=2390
    -D BATTERY_MV_HIGH=4322
    -D RKS_THRESHOLD=10000
    -D LKS_THRESHOLD=10000
    -D KNOCK_ON_LEVEL=100
    -D KNOCK_MIN_PRESS_MS=25
    -D KNOCK_MAX_GAP_US=700'''
envbox = Table([[codeblock(ENV_EXAMPLE, 84*mm), [P('Im Code stehen nur Ersatzwerte. Für eine neue Box eine bestehende Env kopieren, umbenennen und die Werte anpassen. Die Werte von Timon sind ein guter Anfang.'),
                                                   P('Zeichenketten stehen in maskierten Anführungszeichen, wie bei ' + code('BOX_NAME') + '. Nach jeder Änderung neu flashen.')]]],
               colWidths=[90*mm, FW-90*mm])
envbox.setStyle(TableStyle([('VALIGN', (0, 0), (-1, -1), 'TOP'), ('LEFTPADDING', (0, 0), (-1, -1), 0), ('RIGHTPADDING', (0, 0), (-1, -1), 0)]))
story += [CondPageBreak(110*mm), P('5.5  Inbetriebnahme', 'h2'),
  P('Firmware, Platinen und diese Anleitung liegen im Repo ' + repo() + '. Gebaut und geflasht wird mit PlatformIO, in VS Code mit der PlatformIO-Erweiterung oder mit ' + code('pio') + ' auf der Kommandozeile. Am besten einmal vor dem Einbau mit offen liegender Elektronik in Betrieb nehmen und noch einmal im Gehäuse (Kapitel 6, Schritt 12).'),
  P('Werte pro Box in der platformio.ini', 'h3'),
  P('Alles, was sich von Box zu Box unterscheidet, steht in einer eigenen Env in der ' + code('platformio.ini') + ':'),
  envbox, Spacer(1, 8),
  table([['Wert', 'Bedeutung', 'So findet man ihn heraus'],
         [code('BOX_NAME', 7.6), 'Name im WLAN, bei Bluetooth und in der Web-UI', 'frei wählen'],
         [code('BOX_COLOR', 7.6), 'Farbe des Symbols im Browser-Tab', 'frei wählen'],
         [code('BLUETOOTH_TAG', 7.6) + '<br/>' + code('MANAGEMENT_SERVER_TAG', 7.6), 'UIDs der beiden Sondertokens', 'Token auflegen, die Konsole meldet ' + code('tag: <uid>', 7.6) + ' (Schritt 4)'],
         [code('BATTERY_ADC_LOW', 7.6) + '<br/>' + code('BATTERY_MV_LOW', 7.6) + '<br/>' + code('BATTERY_ADC_HIGH', 7.6) + '<br/>' + code('BATTERY_MV_HIGH', 7.6), 'Zwei Messpunkte für die Akkuanzeige: ADC-Wert und Spannung in mV', code('battery:', 7.6) + ' oder die Management-Seite zeigt den ADC-Wert, ein Multimeter die Spannung (Schritt 6)'],
         [code('RKS_THRESHOLD', 7.6) + '<br/>' + code('LKS_THRESHOLD', 7.6) + '<br/>' + code('KNOCK_…', 7.6), 'Klopfsensoren', code('knockdebug:1', 7.6) + ' und ' + code('knockdebug:2', 7.6) + ', siehe doc/development.md (Schritt 5)']],
        [50*mm, 52*mm, FW-102*mm]),
  P('Serielle Konsole', 'h3'),
  P('USB-C-Buchse an den Rechner stecken, die Box mit einem Token einschalten und ' + code('pio device monitor -b 115200') + ' starten, oder den Serial Monitor in VS Code. Befehle mit Doppelpunkt eintippen, etwa ' + code('battery:') + ', dann Enter.'),
  P('Schritt für Schritt', 'h3'),
  ListFlowable([Paragraph(t, S['li']) for t in [
    '<b>SD-Karte:</b> FAT32. Ins Wurzelverzeichnis gehören ' + code('tap.mp3') + ' (Klick beim Klopfen) und ' + code('alert.mp3') + ' (Warnton bei schwachem Akku), beide aus ' + code('sdcard/') + '. Den Aufbau der Karte beschreibt architecture.md, Abschnitt 4.',
    '<b>Eigene Env:</b> In der ' + code('platformio.ini') + ' eine bestehende Env wie ' + code('[env:timon]') + ' kopieren und umbenennen, ' + code('BOX_NAME') + ' und ' + code('BOX_COLOR') + ' anpassen. Die übrigen Werte folgen in den nächsten Schritten.',
    '<b>Flashen:</b> Die Box mit dem Mikroschalter einschalten, über USB geht sie nicht an. Dann die USB-C-Buchse an den Rechner stecken und ' + code('~/.platformio/penv/bin/pio run -e <box> -t upload --upload-port /dev/ttyUSBn') + ' ausführen.',
    '<b>Sondertokens:</b> Ein Token für den Bluetooth-Modus und einer für den Management-Modus. Jeden auflegen, die Konsole meldet ' + code('tag: <uid>') + '. Die UIDs als ' + code('BLUETOOTH_TAG') + ' und ' + code('MANAGEMENT_SERVER_TAG') + ' in die Env eintragen und neu flashen. Jede andere UID ist ein Album.',
    '<b>Klopfsensoren abstimmen:</b> ' + code('knockdebug:1') + ' zeigt pro Sekunde Pegel und Flächen, ' + code('knockdebug:2') + ' zusätzlich den Verlauf des nächsten Drucks. Die Schwellen in der Env so setzen, dass ein Druck sicher auslöst und laute Musik nicht (doc/development.md). Neu flashen.',
    '<b>Akkuanzeige kalibrieren:</b> ESP32-ADCs streuen. Die Box rechnet den ADC-Wert über zwei Messpunkte in Millivolt um. Jeder Punkt ist ein ADC-Wert und die Spannung, die ein Multimeter im selben Moment zwischen ' + code('Vcc') + ' und ' + code('GND') + ' an J1 der Latch misst. '
    'Am USB zeigt ' + code('battery:') + ' den ADC-Wert, er und die Spannung kommen in ' + code('BATTERY_ADC_HIGH') + ' und ' + code('BATTERY_MV_HIGH') + '. '
    'Im Akkubetrieb ohne USB, am besten mit halb leerem Akku, den Management-Token auflegen: Die Management-Seite zeigt unter „Battery“ den ADC-Wert, er und die Spannung kommen in ' + code('BATTERY_ADC_LOW') + ' und ' + code('BATTERY_MV_LOW') + '. '
    'Neu flashen. ' + code('battery:') + ' zeigt danach Spannung und Prozent, die Spannung sollte zum Multimeter passen.']],
    bulletType='1', leftIndent=14, bulletFontName='Plex-SB', bulletFontSize=8.6, bulletColor=ACCENT),
  Spacer(1, 6),
  note('Steckt USB, misst die Box die USB-Spannung statt des Akkus, die Akkuanzeige steht dann auf voll. Am USB-Netzteil bleibt die Box außerdem an, auch wenn die Firmware HOLD abschaltet.', 'info'),
  CondPageBreak(170*mm)]

story += [H1(6, 'Zusammenbau'),
  P('Erst werden die Teile vorbereitet, dann Rahmen und Boden verklebt und alles eingebaut, was an den Wänden sitzt. Der Deckel wird separat bestückt und erst zum Schluss aufgeklebt. Bis dahin kommt man von oben an alles, danach nur noch durch die Bodenklappe.'),
  ChipRow(['CA','EP','UV','HG','TB'], label='Farben in den Schritten:'), Spacer(1, 10)]
story += [KeepTogether([P('6.1  Vorbereiten', 'h2'), fig('inserts', [dict(a='ins0', t='Gewindeeinsatz M3', s='4 Stück', side='L'),
     dict(a='hole1', t='Bohrung Ø 4,0 mm', s='4,5 mm tief, durchgehend', side='R'), dict(a='ledge', t='Auflage der Klappe', side='R')],
     arrows=[('ins0','hole0'),('ins1','hole1'),('ins2','hole2'),('ins3','hole3')], frac=0.6), cap(pn('back') + ' von unten, die Einsätze kommen von der Seite der Klappe.')]),
  step(1, 'Teile vorbereiten', ['SL'], [
    'Stützen entfernen, Löcher entgraten. Gitter, Token und Klappe probehalber einsetzen: Das Gitter muss ohne Kraft in die Öffnung passen.',
    'Wer lackieren will, schleift und lackiert ' + pn('back') + ', ' + pn('frame') + ', ' + pn('front') + ' und die Tasten jetzt. Die Klebeflächen dabei abkleben und blank lassen, Sekundenkleber hält auf blankem Kunststoff besser als auf Lack.']),
  step(2, 'Gewindeeinsätze einschmelzen', ['LK'], [
    'Die vier M3-Einsätze von unten in die Bohrungen in ' + pn('back') + ' setzen, also von der Seite, an der später die Klappe anliegt.',
    'Mit dem Lötkolben gerade einschmelzen, bis sie bündig mit der Auflagefläche sind. Den Lötkolben etwa auf Drucktemperatur des Materials stellen.'])]
story += [CondPageBreak(120*mm), KeepTogether([P('6.2  Unterteil', 'h2'), fig('shell', [dict(a='front', t='front', s='erst in Schritt 12', side='R'),
     dict(a='frame_top', t='Klebekante oben', s='für den Deckel', side='R'), dict(a='frame', t='frame', side='R'), dict(a='back_glue', t='Klebefläche back', side='R'),
     dict(a='back', t='back', s='Einsätze nach unten', side='R'), dict(a='prev', t='prev button', s='links', side='L'), dict(a='next', t='next button', s='rechts', side='R')],
     arrows=[('prev_from','prev_to')], frac=0.56), cap('Klebeflächen orange. Die Tasten kleben außen auf der Seitenwand.')]),
  step(3, 'Tasten aufkleben', ['CA'], [
    pn('prev button') + ' auf die linke, ' + pn('next button') + ' auf die rechte Seitenwand kleben, von vorn gesehen. Beide sitzen mittig unter der Lasche, die Mitte 31 mm über der Unterkante des Rahmens.',
    'Das Symbol waagerecht ausrichten. Kleber sparsam auftragen, er darf nicht aus den Aussparungen des Symbols quellen.']),
  step(4, 'Boden an den Rahmen kleben', ['CA'], [
    pn('back') + ' mit den Einsätzen nach unten an die Unterkante von ' + pn('frame') + ' setzen. Die gewölbte Seite gehört nach hinten, zur USB-C-Buchse.',
    'Auf einer ebenen Fläche verkleben und die Kanten bündig ausrichten. Den Deckel noch nicht aufkleben.'])]
story += [CondPageBreak(120*mm), KeepTogether([P('6.3  Wände', 'h2'), fig('inside', [dict(a='piezo', t='Piezo', s='Messingseite zur Wand', side='L'),
     dict(a='wall', t='Wand 1 mm', s='Klebeband', side='L'), dict(a='usb', t='USB-C-Platine', s='UV-Lack', side='R'),
     dict(a='speaker', t='Lautsprecher', s='2K-Kleber auf dem Ring', side='L'), dict(a='grill', t='Gitter mit Stoff', s='Sekundenkleber', side='L')], frac=0.6),
     cap('Rahmen im Schnitt: was innen an den Wänden sitzt.')]),
  step(5, 'Lautsprechergitter mit Stoff beziehen', ['CA'], [
    'Ein Stück dünnen Stoff, etwa eine Socke, straff über die Außenseite des Gitters ziehen.',
    'Das Gitter von außen in die Öffnung drücken. Der Stoff wird im Spalt zwischen Rand (Ø 54,6 mm) und Öffnung (Ø 55 mm) eingeklemmt.',
    'Von innen mit Sekundenkleber fixieren und den überstehenden Stoff innen abschneiden.'],
    [('tip', 'Kleber nur innen auftragen. Sekundenkleber zieht in Stoff ein, wird hart und hinterlässt helle Flecken.')]),
  step(6, 'Lautsprecher einkleben', ['EP'], [
    'Die Kabel vorher anlöten, lang genug bis zur Hauptplatine unter dem Deckel.',
    'Den Lautsprecher von innen mit 2K-Kleber auf das Gitter kleben. Mit 50–57 mm Durchmesser liegt er mit seinem Rand auf dem Ring an der Innenseite des Gitters, ein kleinerer sitzt im Ring direkt am Gitter.',
    'Kleber nur auf den Rand, nicht auf die Membran. Aushärten lassen.']),
  step(7, 'Piezos ankleben', ['TB'], [
    'Je eine Piezo-Scheibe mit dünnem doppelseitigem Klebeband von innen auf die gedünnte Wand hinter ' + pn('prev button') + ' und ' + pn('next button') + ' kleben, mittig hinter der Taste, Messingseite zur Wand.',
    'Die Kabel beschriften: links ist „zurück“ (J2), rechts „vor“ (J3), siehe Kapitel 5.4.'],
    [('tip', 'Dünnes, festes Klebeband überträgt den Druck besser als dickes Schaumklebeband. Die Schwellen werden in Schritt 12 abgestimmt.')]),
  step(8, 'USB-C-Buchse einkleben', ['UV'], [
    'Die Kabel vorher anlöten: VBUS und GND zum TP4056, D+ und D− zum Kabel zur Hauptplatine (Kapitel 5.4).',
    'Die Platine von innen in den Ausschnitt unten in der Rückwand setzen, die Buchse bündig mit der Außenseite.',
    'Ein USB-C-Kabel einstecken, damit die Buchse gerade sitzt. Dann mit UV-Lack von innen fixieren und aushärten. Kein Lack in die Buchse.'])]
story += [CondPageBreak(120*mm), KeepTogether([P('6.4  Deckel', 'h2'), fig('lid', [dict(a='led', t='LED Ø 8 mm', s='UV-Lack', side='R'),
     dict(a='poti', t='Poti Ø 7 mm', s='Mutter von außen', side='L'), dict(a='jack', t='Kopfhörerbuchse', s='Tasche 12 × 7 mm\nBohrung bleibt zu', side='L'),
     dict(a='switch', t='Mikroschalter', s='Hebel durch den Schlitz\nUV-Lack', side='L'), dict(a='cup', t='RC522 hier aufkleben', s='Muldenboden 1 mm', side='R'),
     dict(a='skirt', t='Klebekante', s='zum Rahmen', side='R')], frac=0.6),
     cap(pn('front') + ' von innen. Blau: Fläche für das RC522.')]),
  step(9, 'Deckel bestücken', ['UV', 'TB'], [
    '<b>LED:</b> Die PL9823 von innen bis zum Kragen in die 8-mm-Bohrung vorn links stecken und mit UV-Lack fixieren.',
    '<b>Poti:</b> Von innen durch die 7-mm-Bohrung hinten links stecken, außen Scheibe und Mutter. Den Verdrehschutz-Nocken am Poti abbrechen, falls vorhanden; der Deckel hat dafür kein Loch. Die Achse so kürzen, dass sie 8,5 mm über die Deckeloberfläche ragt, dann sitzt der Knopf mit 0,5 mm Luft. Den Knopf ' + pn('volume') + ' aufstecken.',
    '<b>Mikroschalter:</b> In die Tasche an der Rückseite der Token-Mulde setzen, sodass der Hebel durch den Schlitz in die Mulde ragt. Prüfen, dass ein aufgelegter Token ihn hörbar drückt, dann mit UV-Lack fixieren und aushärten. Lack nur ans Gehäuse des Schalters, nicht an Hebel und Stößel.',
    '<b>RC522:</b> Mit dünnem doppelseitigem Klebeband von innen unter die Token-Mulde kleben, flache Rückseite nach oben, Antenne mittig unter der Mulde. Die Stiftleiste zeigt nach unten, dort steckt später die Hauptplatine.',
    '<b>Kopfhörerbuchse:</b> Bleibt vorerst leer, die Bohrung hinten rechts bleibt verschlossen. Für eine spätere Buchse ist innen eine Tasche; dann die 0,2-mm-Schicht mit 6 mm aufbohren.']),
  KeepTogether([fig('lid_full', [dict(a='rc522', t='RC522', s='flache Rückseite an der Mulde'), dict(a='header', t='Stiftleiste', s='für die Hauptplatine'),
     dict(a='led', t='LED', s='UV-Lack'), dict(a='poti', t='Poti', s='Mutter von außen'), dict(a='switch', t='Mikroschalter', s='UV-Lack')], frac=0.58),
     cap('Deckel nach Schritt 9. Die Hauptplatine kommt in Schritt 11 auf die Stiftleiste des RC522.')])]
story += [CondPageBreak(110*mm), KeepTogether([P('6.5  Elektronik und Akkus', 'h2'), LightPipeDiagram(),
     cap('Schnitt durch die Rückwand an der USB-C-Buchse: Das Filament führt das Licht der Lade-LED nach außen und darf gebogen verlaufen.')]),
  step(10, 'Ladeschaltung und Step-up einkleben', ['EP'], [
    'Die Ladeschaltung, also die Power Latch mit dem TP4056-Modul auf der Rückseite, mit 2K-Kleber innen an die Wand kleben, in die Nähe der USB-C-Buchse. Heißkleber reicht hier nicht: Der Laderegler wird beim Laden sehr warm, und der Heißkleber kann sich dann lösen.',
    'Schnell härtenden 2K-Kleber nehmen und die Platine halten, bis er anzieht. Kein Kleber auf die Lade-LED.',
    'Ein Stück transparentes Filament (1,75 mm) von außen durch das 2-mm-Loch über der USB-C-Buchse stecken und innen bis an die Lade-LED führen. Es darf länger sein und in einem sanften Bogen verlaufen, so ist man bei der Lage der Ladeschaltung frei. Außen bündig abschneiden. Hält es nicht von selbst, mit einem Tropfen UV-Lack sichern.',
    'Den Step-up-Konverter ebenfalls mit 2K-Kleber an eine freie Stelle der Wandinnenseite kleben, nicht auf die gedünnten Tastenflächen. Auch er wird unter Last warm.'],
    [('tip', 'Beim Laden leuchtet das Filament neben der USB-C-Buchse in der Farbe der LED: je nach Modul rot beim Laden, blau oder grün, wenn der Akku voll ist.')]),
  CondPageBreak(110*mm), KeepTogether([fig('akkus', [
     dict(a='cellA', t='18650 vorn links', s='stehend, Pluspol oben'), dict(a='cellB', t='18650 hinten rechts', s='diagonal gegenüber'),
     dict(a='kapton', t='Kapton über der Lötstelle'), dict(a='red', t='Plus, rot', s='zu B+ am TP4056'), dict(a='black', t='Minus, schwarz', s='zu B− am TP4056'),
     dict(a='window', t='Tastenwand', s='kein Kleber'), dict(a='usb', t='USB-C'), dict(a='latch', t='Ladeschaltung', s='2K-Kleber'), dict(a='boost', t='Step-up', s='2K-Kleber')], frac=0.6),
     cap('Akkus stehend in zwei diagonal gegenüberliegenden Ecken, hier vorn links und hinten rechts. An der Rückwand Ladeschaltung und Step-up. Rahmen aufgeschnitten, Deckel noch nicht aufgeklebt.')]),
  step(11, 'Elektronik verdrahten und Akkus einkleben', ['EL', 'HG'], [
    'Die Hauptplatine von unten auf die Stiftleiste des RC522 stecken und am freien Ende den Abstandshalter einclipsen (Kapitel 5.2, Schritt E6).',
    'Zur Sicherheit einen Kabelbinder um Hauptplatine, Abstandshalter und RC522 legen, am Ende gegenüber der Stiftleiste. Dort ist zwischen RC522 und Deckel Platz, denn die Token-Mulde trägt das RC522 nur in der Mitte.',
    'Alles nach den Tabellen in Kapitel 5.4 verdrahten: Kabel zu J4, Lautsprecher an den MAX98357A, Piezos an J2 und J3, Poti an J1, LED an J5, Mikroschalter mit C an Trigger + und NO an Trigger In der Latch.',
    'Die beiden 18650-Zellen stehend in zwei diagonal gegenüberliegende Ecken setzen, Pluspol oben, zum Beispiel vorn links und hinten rechts. Hinten links sitzt direkt darüber das Poti.',
    'Die Zellen stehen auf dem Boden direkt über den Gewindeeinsätzen und reichen bis knapp unter den Deckel. Einfache Zellen ohne Schutzschaltung (65 mm) passen, die Schutzschaltung sitzt auf dem TP4056-Modul.',
    'Mit Heißkleber in den Spalten zwischen Zelle und Ecke fixieren. Kein Kleber auf die gedünnten Wandflächen der Tasten, er dämpft sonst die Piezos.',
    'Parallel schalten, Plus an Plus und Minus an Minus, und an B+ und B− des TP4056 anschließen. Alle Lötstellen mit Kapton-Band oder Schrumpfschlauch isolieren.'],
    [('warn', 'Vor dem Zusammenschalten beide Zellen auf gleiche Spannung bringen (höchstens etwa 0,1 V Unterschied), sonst fließt ein hoher Ausgleichsstrom. Nur gleiche Zellen verwenden, Zellen mit Lötfahnen nehmen und nicht direkt an die Zelle löten. Nichts Blankes darf Schrauben, Einsätze oder andere Bauteile berühren.')])]
story += [KeepTogether([fig('lid_stack', [dict(a='tie', t='Kabelbinder', s='am freien Ende'), dict(a='spacer', t='Abstandshalter'), dict(a='mainboard', t='Hauptplatine'),
     dict(a='rc522', t='RC522', s='an der Token-Mulde'), dict(a='header', t='Stiftleiste')], frac=0.56),
     cap('Deckel mit Elektronik von innen. Gegenüber der Stiftleiste halten Abstandshalter und Kabelbinder die Hauptplatine.')])]
left = [fig('akku_detail', [dict(a='glue1', t='Heißkleber', s='in beiden Spalten zur Wand'), dict(a='kapton', t='Kapton'),
        dict(a='cell', t='Zelle in der Ecke'), dict(a='window', t='Tastenwand', s='hier kein Kleber')], frac=0.58, width=88*mm, place='left', gap=7),
        cap('Ecke vorn links von oben.')]
right = [Spacer(1, 6), ParallelDiagram(), Spacer(1, 4), cap('Parallelschaltung: Plus an Plus, Minus an Minus.')]
pair = Table([[left, right]], colWidths=[88*mm, FW-88*mm])
pair.setStyle(TableStyle([('VALIGN',(0,0),(-1,-1),'TOP'), ('LEFTPADDING',(0,0),(-1,-1),0), ('RIGHTPADDING',(0,0),(-1,-1),0)]))
story += [KeepTogether([pair])]
story += [CondPageBreak(120*mm), KeepTogether([P('6.6  Abschluss', 'h2'), fig('lidopen', [dict(a='mainboard', t='Hauptplatine', s='steckt auf dem RC522'),
     dict(a='modules', t='Verstärker und SD-Modul'), dict(a='rc522', t='RC522', s='unter der Token-Mulde'), dict(a='cable', t='7-poliges Kabel', s='nach innen legen'),
     dict(a='latch', t='Ladeschaltung'), dict(a='boost', t='Step-up'), dict(a='cellB', t='Akkus')], frac=0.5),
     cap('Vor dem Aufkleben: Der Deckel trägt RC522, Hauptplatine mit Modulen, LED, Poti und Mikroschalter. Das 7-polige Kabel verbindet ihn mit der Stromseite.')]),
  step(12, 'Testen, dann Deckel aufkleben', ['CA'], [
    'Vor dem Verkleben in Betrieb nehmen, wie in Kapitel 5.5 beschrieben: Token auflegen, Box startet, flashen, Lautstärke, LED und Akkuanzeige prüfen.',
    'Die Klopfsensoren mit <font name="Mono" size="8.3">knockdebug:1</font> und <font name="Mono" size="8.3">knockdebug:2</font> abstimmen (doc/development.md). Die Schwellen stehen pro Box in der <font name="Mono" size="8.3">platformio.ini</font>, eine neue Box bekommt dort eine eigene Env.',
    'Die Kabel nach innen legen, auf der Klebekante darf nichts liegen. Den Deckel mit Sekundenkleber auf den Rahmen kleben, die gewölbte Seite nach hinten. Danach die Klopfsensoren kurz gegenprüfen.']),
  KeepTogether([fig('door', [dict(a='back', t='back', s='mit Einschmelzgewinden', side='R'),
     dict(a='door', t='door', s='Bodenklappe', side='R'), dict(a='bumper_glue', t='bumper (TPU)', s='Klebefläche zur Klappe', side='L'),
     dict(a='scr0', t='4 × M3 × 5', s='ISO 4762', side='L')], arrows=[('scr0','dhole0'),('scr1','dhole1'),('scr3','dhole3')], frac=0.56),
     cap('Bodenklappe mit Bumper und Schrauben. Orange: Klebefläche.')]),
  step(13, 'Bodenklappe anschrauben', ['CA'], [
    'Den Bumper mit Sekundenkleber unter die Klappe kleben. Zum Ausrichten zwei Schrauben durch Bumper und Klappe stecken.',
    'Die Klappe mit vier Schrauben M3 × 5 anschrauben. Die Köpfe versinken im Bumper. Durch die Klappe kommt man wieder an die Elektronik.'],
    [('warn', 'Keine längeren Schrauben als M3 × 5 verwenden. In zwei Ecken steht direkt über dem Einsatz ein Akku.')]),
  KeepTogether([fig('strap', [dict(a='apex', t='Rolladengurt', s='ca. 15 mm breit, ca. 30 cm'), dict(a='fold', t='Ende umgeschlagen', s='und vernäht'),
     dict(a='loop', t='Lasche', s='Öffnung 20 × 3 mm')], frac=0.5), cap('Zum Tragen: ein Stück Rolladengurt durch beide Laschen.')]),
  step(14, 'Tragegurt anbringen', [('Nadel und Faden', '#9aa5a0')], [
    'Etwa 30 cm Rolladengurt aus dem Baumarkt nehmen, ca. 15 mm breit.',
    'Je ein Ende durch eine Lasche führen, umschlagen und festnähen, zum Beispiel mit einem Kästchen mit Kreuz.',
    'Dann lässt sich die Box am Gurt tragen.'],
    [('tip', 'Die Schnittkanten kurz mit dem Feuerzeug anschmelzen, dann franst der Gurt nicht aus.')]),
  CondPageBreak(150*mm)]

# 6 Maße
story += [H1(7, 'Maße und Schnittstellen'), P('Alle Werte aus dem STEP-Modell, in Millimetern. Pfade beziehen sich auf das Repo ' + repo() + '.'),
  table([['Stelle', 'Maß'],
   ['Außen', '94 × 90 mm, mit Laschen 102 mm breit; Höhe mit Bumper hinten 94,5 mm, vorn 87 mm'],
   ['Deckel', '5° nach vorn geneigt, 3 mm Deckplatte, 8 mm Rand'],
   ['Wände', '3 mm, an den Tasten auf einer Fläche von 56 × 50 mm auf 1 mm gedünnt'],
   ['Laschen', 'Öffnung 20 × 3 mm, für Gurte bis knapp 20 mm Breite'],
   ['Token-Mulde', 'Ø 40 mm, 10 mm tief, Boden 1 mm'],
   ['Token', 'Körper Ø 39,7 mm, Rand Ø 44 mm, 15 mm hoch; NFC-Tasche Ø 26 × 0,4 mm, 0,2 mm über der Unterseite'],
   ['LED', 'Bohrung Ø 8 mm'],
   ['Poti und Knopf', 'Bohrung Ø 7 mm; Knopf Ø 14 × 9 mm, Achsloch Ø 6,2 mm (8 mm tief ab Unterkante), Aussparung Ø 12 × 3 mm'],
   ['Kopfhörerbuchse', 'Bohrung Ø 6 mm, oben mit 0,2 mm verschlossen; Tasche 12 × 7 × 2 mm'],
   ['USB-C', 'Ausschnitt 9,2 × 3,5 mm, Ecken R 1,5; Mitte 5 mm darüber ein Loch Ø 2 mm für den Lichtleiter'],
   ['Lautsprecher', 'Öffnung Ø 55 mm; Gitter mit Rand Ø 54,6 mm, Auflagering Ø 48,6–54,6 mm, steht 3 mm vor'],
   ['Tasten', 'Ø 20 × 1 mm, Mitte 31 mm über der Unterkante des Rahmens'],
   ['Gewindeeinsätze', '4 Bohrungen Ø 4,0 mm, 4,5 mm tief, durchgehend'],
   ['Klappe', '2,9 mm dick, Senkung Ø 5,6 × 2 mm, Durchgang Ø 3,1 mm'],
   ['Bumper', '2 mm, TPU'],
   ['Abstandshalter', '44,2 × 16,8 × 10 mm; zwischen den Armen 41,2 mm breit, Rastnasen auf 39,9 mm'],
   ['Platinenstapel', 'etwa 11 mm zwischen Hauptplatine und RC522: Buchsenleiste 8,5 mm und Kunststoff der Stiftleiste 2,5 mm']], [36*mm, FW-36*mm]),
  P('Dateien', 'h2'),
  table([['Pfad', 'Inhalt'], ['<font name="Mono" size="7.9">models/box.step</font>', 'CAD-Modell, alle zehn Teile in Einbaulage'],
   ['<font name="Mono" size="7.9">models/pcb_spacer.step</font>', 'Abstandshalter zwischen Hauptplatine und RC522'],
   ['<font name="Mono" size="7.9">models/plates/</font>', 'Fertige Druckbetten als 3MF, eins pro Farbe, und ein Bambu-Studio-Projekt mit allen'], ['<font name="Mono" size="7.9">models/stl/</font>', 'Jedes Druckteil als STL, in Druckausrichtung'],
   ['<font name="Mono" size="7.9">hardware/</font>', 'KiCad-Projekte, Schaltpläne und Gerber der Platinen, Verdrahtung der Stromseite (WireViz)'],
   ['<font name="Mono" size="7.9">doc/hardware.md</font>', 'Elektronik: Teileliste, Platinen, Verkabelung, Inbetriebnahme'],
   ['<font name="Mono" size="7.9">README.md</font>', 'Überblick und Schnellstart'], ['<font name="Mono" size="7.9">doc/usage.md</font>', 'Bedienung'], ['<font name="Mono" size="7.9">doc/development.md</font>', 'Klopfsensoren abstimmen, Entwicklung'], ['<font name="Mono" size="7.9">tools/token-sheet.html</font>', 'Token-Bilder als Fotoabzug 10 × 15 cm'],
   ['<font name="Mono" size="7.9">doc/assembly/</font>', 'Quellen dieser Anleitung: Text, Bilder aus dem Modell, Build-Skript']],
   [52*mm, FW-52*mm])]

# 8 Anhang
AH = 218*mm
story += [PageBreak(), H1(8, 'Anhang'), P('Schaltpläne und Verdrahtung aus <font name="Mono" size="8.3">hardware/</font>, unverändert eingebunden. Am Bildschirm lassen sie sich beliebig vergrößern.'),
  P('Schaltplan Hauptplatine', 'h2'), PdfPage(ROOT / 'hardware' / 'mainboard' / 'timon-box-schematic.pdf', FW, 196*mm), PageBreak(),
  P('Schaltplan Power Latch V2', 'h2'), PdfPage(ROOT / 'hardware' / 'power-latch' / 'power-latch-v2-schematic.pdf', FW, AH), PageBreak(),
  P('Verdrahtung der Stromseite', 'h2'), P('Aus <font name="Mono" size="8.3">hardware/wiring/power-side.yml</font> mit WireViz erzeugt. Jede Zeile in einem Kabel nennt beide Enden.'),
  SvgPage(ROOT / 'hardware' / 'wiring' / 'power-side.svg', FW, 205*mm)]

out = str(HERE.parent / 'assembly.pdf')
Doc(out, META).multiBuild(story, canvasmaker=NumberedCanvas)
print('written', out)
