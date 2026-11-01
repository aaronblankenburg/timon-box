"""Text and structure of the build guide: doc/assembly.pdf (German) and doc/assembly-en.pdf (English).

Every text is a pair T('deutsch', 'English'), see i18n.py. Build with ./build.sh, layout lives in layout.py.
"""
from layout import *
from i18n import T, LANG, MISSING, check_untranslated
import wiring

if LANG != 'de' and not wiring.render_english():     # the appendix needs the English wiring diagram first
    print('  wireviz not installed, using the committed', wiring.SVG_EN.name)
from lib.kicad import load_board

META = dict(
    title=T('Timon Box – Bauanleitung: Elektronik, Druck und Zusammenbau', 'Timon Box – Build guide: electronics, printing and assembly'), subject=T('Hardware-Doku: Platinen, Verkabelung, Druckteile, Zusammenbau', 'Hardware docs: boards, wiring, printed parts, assembly'),
    kicker=T('HARDWARE-DOKU · BAUANLEITUNG', 'HARDWARE DOCS · BUILD GUIDE'), name='Timon Box', subtitle=T('Elektronik, Druck und Zusammenbau', 'Electronics, printing and assembly'),
    intro=T('Platinen bestücken, verkabeln, Gehäuse drucken und alles Schritt für Schritt zusammenbauen.', 'Populate the boards, wire them up, print the case and put everything together step by step.'),
    facts=[repo('#cfe3db'), T('Modell: models/box.step', 'Model: models/box.step'), T('Platinen: hardware/ (KiCad 8)', 'Boards: hardware/ (KiCad 8)'), T('Hintergründe: doc/hardware.md', 'Background: doc/hardware.md'), T('Stand: Oktober 2026', 'As of: October 2026'), T('Lizenz: CC BY-NC-SA 4.0', 'License: CC BY-NC-SA 4.0'), '© 2026 Aaron Blankenburg'],
    header=T('Timon Box · Bauanleitung', 'Timon Box · Build guide'), footer=T('Stand Oktober 2026 · © Aaron Blankenburg · Lizenz CC BY-NC-SA 4.0', 'As of October 2026 · © Aaron Blankenburg · License CC BY-NC-SA 4.0'))


def glance():
    items = [(T('DRUCK', 'PRINTING'), T('PETG oder PLA, Bumper TPU, 0,2 mm Schichthöhe', 'PETG or PLA, bumper TPU, 0.2 mm layer height')), (T('VERBINDUNGEN', 'JOINING'), T('Sekundenkleber, 2K-Kleber, UV-Lack, Heißkleber, Klebeband', 'Super glue, 2-part epoxy, UV resin, hot glue, tape')),
             (T('SCHRAUBEN', 'SCREWS'), T('4 × M3 × 5 in Einschmelzgewinden M3', '4 × M3 × 5 in M3 heat-set inserts')), (T('AKKUS', 'CELLS'), T('2 × 18650, parallel geschaltet', '2 × 18650, wired in parallel'))]
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
story += [P(T('<font name="Plex-SB" size="11" color="#2f6b5a">Inhalt</font>', '<font name="Plex-SB" size="11" color="#2f6b5a">Contents</font>')), Spacer(1, 4), toc, NextPageTemplate('body'), PageBreak()]

# 1 Überblick
story += [H1(1, T('Überblick', 'Overview')),
  P(T('Das Gehäuse besteht aus zehn Druckteilen. Rahmen, Boden und Deckel werden zu einer Schale verklebt. Unten bleibt eine Bodenklappe, die mit vier M3-Schrauben in Einschmelzgewinden hält. Durch sie kommt man nach dem Zusammenbau wieder an die Elektronik.', 'The case consists of ten printed parts. Frame, bottom and lid are glued into one shell. At the bottom there is a hatch held by four M3 screws in heat-set inserts. Through it you can still reach the electronics after assembly.')),
  P(T('Vorn ist die Lautsprecherseite. Von vorn gesehen liegt die Taste „zurück“ links und „vor“ rechts, passend zu J2 und J3 (Kapitel 5.4). Der Deckel ist um 5° nach vorn geneigt, die Rückseite mit der USB-C-Buchse ist leicht gewölbt.', 'The speaker side is the front. Seen from the front, the “previous” button is on the left and “next” on the right, matching J2 and J3 (chapter 5.4). The lid is tilted forward by 5°, the back with the USB-C socket is slightly curved.')), Spacer(1, 4)]
story += [fig('overview', [
    dict(a='a_token', t=T('Token-Mulde', 'Token cup'), s=T('RC522 darunter', 'RC522 underneath'), side='L'), dict(a='a_knob', t=T('Lautstärke', 'Volume'), side='L'),
    dict(a='a_led', t='LED', side='L'), dict(a='a_handle', t=T('Lasche', 'Lug'), s=T('für den Tragegurt', 'for the carrying strap'), side='L'), dict(a='a_prev', t=T('Taste „zurück“', '“Previous” button'), s=T('Piezo innen', 'piezo inside'), side='L'),
    dict(a='a_grill', t=T('Lautsprecher', 'Speaker'), side='L', dy=-6),
    dict(a='b_jack', t=T('Kopfhörerbuchse', 'Headphone jack'), s=T('noch verschlossen', 'still closed'), side='R'), dict(a='b_next', t=T('Taste „vor“', '“Next” button'), side='R'),
    dict(a='b_curved', t=T('Rückseite', 'Back'), s=T('leicht gewölbt', 'slightly curved'), side='R'), dict(a='b_usb', t='USB-C', s=T('Laden und Flashen', 'charging and flashing'), side='R'), dict(a='b_chg', t=T('Ladeanzeige', 'Charge indicator'), s=T('Lichtleiter zur Lade-LED', 'light pipe to the charge LED'), side='R'),
    dict(a='b_bumper', t='Bumper (TPU)', side='R')], frac=0.70, gap=6),
  cap(T('Links von vorn, rechts von hinten.', 'Left from the front, right from the back.')),
  table([[T('Maß', 'Dimension'), T('Wert', 'Value')], [T('Grundfläche', 'Footprint'), T('94 × 90 mm, mit Laschen 102 mm breit', '94 × 90 mm, 102 mm wide with lugs')], [T('Höhe mit Bumper', 'Height with bumper'), T('hinten 94,5 mm, vorn 87 mm', 'back 94.5 mm, front 87 mm')],
         [T('Darüber', 'On top'), T('Token 5 mm, Drehknopf 9 mm über der Deckeloberfläche', 'token 5 mm, knob 9 mm above the lid surface')]], [42*mm, FW-42*mm]), Spacer(1, 10),
  P(T('Auf einen Blick', 'At a glance'), 'h2'), glance(), Spacer(1, 10),
  note(T('Firmware, Platinen, Modell und diese Anleitung liegen im Repo ', 'Firmware, boards, model and this guide live in the repo ') + repo() + T('. Kapitel 5 beschreibt den Aufbau der Elektronik, Kapitel 6 den Zusammenbau. Hintergründe zur Schaltung stehen in <b>doc/hardware.md</b>, die Pinbelegung der Firmware in <b>doc/architecture.md</b>, das Abstimmen der Klopfsensoren in <b>doc/development.md</b>.', '. Chapter 5 covers the electronics, chapter 6 the assembly. Background on the circuit is in <b>doc/hardware.md</b>, the firmware’s pin assignment in <b>doc/architecture.md</b>, tuning the knock sensors in <b>doc/development.md</b>.'), 'info'),
  PageBreak()]

# 2 Druckteile
rows = [[T('Teil', 'Part'), T('Funktion', 'Function'), T('Material', 'Material'), T('Befestigung', 'Fixing')],
 [pn('front'), T('Deckel mit Token-Mulde, LED, Poti, Kopfhörerbuchse und Tasche für den Mikroschalter', 'Lid with token cup, LED, potentiometer, headphone jack and pocket for the microswitch'), 'PETG/PLA', T('Sekundenkleber auf ', 'Super glue onto ') + pn('frame')],
 [pn('frame'), T('Rahmen: Seitenwände mit Laschen für den Tragegurt, Lautsprecheröffnung, USB-C-Ausschnitt. An den Tasten ist die Wand innen auf 1 mm gedünnt', 'Frame: side walls with lugs for the carrying strap, speaker opening, USB-C cutout. At the buttons the wall is thinned to 1 mm on the inside'), 'PETG/PLA', '–'],
 [pn('back'), T('Boden: Rahmen um die Klappe mit vier Gewindeeinsätzen', 'Bottom: frame around the hatch with four threaded inserts'), 'PETG/PLA', T('Sekundenkleber an ', 'Super glue to ') + pn('frame')],
 [pn('door'), T('Bodenklappe', 'Bottom hatch'), 'PETG/PLA', T('4 × M3 × 5 in ', '4 × M3 × 5 into ') + pn('back')],
 [pn('bumper'), T('Fuß unter der Klappe', 'Foot under the hatch'), 'TPU', T('Sekundenkleber an ', 'Super glue to ') + pn('door')],
 [pn('speaker grill'), T('Lautsprechergitter, mit Stoff bezogen', 'Speaker grill, covered with fabric'), 'PETG/PLA', T('eingesetzt, Sekundenkleber', 'pressed in, super glue')],
 [pn('prev button'), T('Taste „zurück“, Ø 20 × 1 mm', '“Previous” button, Ø 20 × 1 mm'), 'PETG/PLA', T('Sekundenkleber, außen auf ', 'Super glue, outside onto ') + pn('frame')],
 [pn('next button'), T('Taste „vor“, Ø 20 × 1 mm', '“Next” button, Ø 20 × 1 mm'), 'PETG/PLA', T('Sekundenkleber, außen auf ', 'Super glue, outside onto ') + pn('frame')],
 [pn('volume'), T('Drehknopf', 'Volume knob'), 'PETG/PLA', T('auf die Poti-Achse gesteckt', 'pushed onto the potentiometer shaft')],
 [pn('token'), T('Token mit eingedrucktem NFC-Tag, einer je Album', 'Token with a printed-in NFC tag, one per album'), 'PETG/PLA', '–'],
 [pn('pcb_spacer'), T('Abstandshalter zwischen Hauptplatine und RC522, eigene Datei <font name="Mono" size="7.9">models/pcb_spacer.step</font>', 'Spacer between main board and RC522, separate file <font name="Mono" size="7.9">models/pcb_spacer.step</font>'), 'PETG/PLA', T('eingeclipst', 'clipped in')]]
story += [H1(2, T('Druckteile', 'Printed parts')), P(T('Die Teilenamen sind die aus dem STEP-Modell <font name="Mono" size="8.3">models/box.step</font>. Dort liegen alle Teile in Einbaulage. Der Abstandshalter für die Elektronik liegt als eigene Datei in <font name="Mono" size="8.3">models/pcb_spacer.step</font>.', 'The part names are the ones from the STEP model <font name="Mono" size="8.3">models/box.step</font>, where all parts sit in their assembled position. The spacer for the electronics is a separate file, <font name="Mono" size="8.3">models/pcb_spacer.step</font>.')),
  table(rows, [27*mm, 82*mm, 20*mm, FW-129*mm]), Spacer(1, 8),
  fig('exploded', [dict(a='token', t='token'), dict(a='volume', t='volume', s=T('Drehknopf', 'Volume knob')), dict(a='front', t='front', s=T('Deckel', 'Lid')),
     dict(a='frame', t='frame', s=T('Rahmen', 'Frame')), dict(a='speaker grill', t='speaker grill', s=T('Lautsprechergitter', 'Speaker grill')),
     dict(a='prev button', t='prev button', s=T('Taste „zurück“', '“Previous” button')), dict(a='next button', t='next button', s=T('Taste „vor“', '“Next” button')),
     dict(a='back', t='back', s=T('Boden', 'Bottom')), dict(a='door', t='door', s=T('Bodenklappe', 'Bottom hatch')), dict(a='bumper', t='bumper', s=T('Fuß, TPU', 'Foot, TPU'))], frac=0.54),
  cap(T('Explosionszeichnung. Unten die Klappe mit Bumper, seitlich die Tasten, vorn das Lautsprechergitter.', 'Exploded view. At the bottom the hatch with the bumper, at the sides the buttons, at the front the speaker grill.')), PageBreak()]

# 3 Drucken
orient = [[T('Teil', 'Part'), T('Aufs Druckbett', 'On the bed'), T('Stützen', 'Supports'), T('Hinweis', 'Note')],
 [pn('frame'), T('Unterkante', 'Bottom edge'), T('im Lautsprecherloch und unter den Laschen', 'in the speaker hole and under the lugs'), T('Das Gitter hat nur 0,2 mm Spiel: Loch entgraten', 'The grill has only 0.2 mm clearance: deburr the hole')],
 [pn('front'), T('Unterkante, Oberseite nach oben', 'Bottom edge, top side up'), T('innen unter der Decke, nur vom Druckbett (Baumstützen)', 'inside under the ceiling, from the build plate only (tree supports)'), T('Taschen für Mikroschalter und Buchse freimachen', 'Clear the pockets for microswitch and jack')],
 [pn('back'), T('Seite zum Rahmen (kopfüber)', 'Side facing the frame (upside down)'), T('keine', 'none'), ''],
 [pn('door'), T('Innenseite', 'Inner side'), T('keine', 'none'), T('Senkungen zeigen nach oben', 'Counterbores face up')],
 [pn('bumper'), T('Klebeseite', 'Glue side'), T('keine', 'none'), T('TPU, langsam drucken', 'TPU, print slowly')],
 [pn('speaker grill'), T('Außenseite', 'Outer side'), T('keine', 'none'), ''],
 [pn('prev button') + ', ' + pn('next button'), T('Sichtseite', 'Visible side'), T('keine', 'none'), T('1 mm dünn', '1 mm thin')],
 [pn('volume'), T('Oberseite', 'Top side'), T('keine', 'none'), ''],
 [pn('token'), T('Oberseite (Bildseite)', 'Top side (picture side)'), T('keine', 'none'), T('Pause vor der letzten Schicht für den NFC-Tag, siehe unten', 'Pause before the last layer for the NFC tag, see below')],
 [pn('pcb_spacer'), T('flache Seite', 'Flat side'), T('keine', 'none'), T('Eigene Datei, siehe Kapitel 5.2', 'Separate file, see chapter 5.2')]]
tiles = [('frame',T('Unterkante', 'Bottom edge')), ('front',T('Unterkante', 'Bottom edge')), ('back',T('kopfüber', 'upside down')), ('door',T('Innenseite', 'Inner side')), ('bumper',T('Klebeseite, TPU', 'glue side, TPU')),
         ('speaker grill',T('Außenseite', 'Outer side')), ('prev button',T('Sichtseite', 'Visible side')), ('next button',T('Sichtseite', 'Visible side')), ('volume',T('Oberseite', 'Top side')), ('token',T('Oberseite', 'Top side')), ('pcb_spacer',T('flache Seite', 'Flat side'))]
cw = FW/6; cells = []
for name, bed in tiles:
    im = PILImage.open(FIG+'tile_'+name.replace(' ','_')+'.jpg'); iw = cw-4; ih = iw*im.height/im.width
    cells.append([Image(FIG+'tile_'+name.replace(' ','_')+'.jpg', iw, ih), Paragraph(f'{pn(name, 7.4)}<br/>{bed}', S['tile'])])
grid = Table([cells[0:6], cells[6:] + [''] * (12 - len(cells))], colWidths=[cw]*6)
grid.setStyle(TableStyle([('ALIGN',(0,0),(-1,-1),'CENTER'), ('VALIGN',(0,0),(-1,-1),'TOP'), ('LEFTPADDING',(0,0),(-1,-1),2), ('RIGHTPADDING',(0,0),(-1,-1),2), ('BOTTOMPADDING',(0,0),(-1,-1),6)]))
story += [H1(3, T('Drucken', 'Printing')),
  P(T('Alle Teile außer dem Bumper aus PETG oder PLA, den Bumper aus TPU. PETG verträgt Wärme besser, etwa im Auto oder auf der Fensterbank.', 'All parts except the bumper in PETG or PLA, the bumper in TPU. PETG handles heat better, for example in a car or on a windowsill.')),
  P(T('Am schnellsten geht es mit den fertigen Druckbetten in <font name="Mono" size="8.3">models/plates/</font>: eine 3MF-Datei pro Farbe, die Teile schon angeordnet, für Betten ab 220 × 220 mm. In PrusaSlicer öffnet man sie als Projekt (Datei › Projekt öffnen), dann sind Schichthöhe, Wände, Füllung, Stützen und die Pause für die Tokens schon eingestellt. Für Bambu Studio liegt dort ein Projekt mit allen fünf Betten, ebenfalls fertig eingestellt. Andere Slicer übernehmen nur die Anordnung. Einzelne Teile liegen als STL in <font name="Mono" size="8.3">models/stl/</font>, benannt wie in der Tabelle und schon so gedreht, wie sie aufs Bett kommen.', 'The quickest way is the ready-made plates in <font name="Mono" size="8.3">models/plates/</font>: one 3MF file per colour, parts already arranged, for beds from 220 × 220 mm. Open them in PrusaSlicer as a project (File › Open Project), and layer height, walls, infill, supports and the pause for the tokens are already set. For Bambu Studio there is a project with all five plates, also ready to go. Other slicers only take the layout. Single parts are in <font name="Mono" size="8.3">models/stl/</font> as STL, named as in the table and already turned the way they go on the bed.')),
  P(T('Einstellungen: 0,2 mm Schichthöhe, 3 Wände, 15–20 % Füllung. Die NFC-Tasche im Token ist auf 0,2 mm Schichthöhe ausgelegt.', 'Settings: 0.2 mm layer height, 3 walls, 15–20% infill. The NFC pocket in the token is designed for 0.2 mm layers.')),
  table(orient, [33*mm, 38*mm, 46*mm, FW-117*mm]), Spacer(1, 8), grid, cap(T('Druckausrichtung, die graue Fläche ist das Druckbett.', 'Print orientation, the grey area is the print bed.')),
  note(T('Den Deckel mit der Oberseite nach oben drucken. Andersherum lägen die gerundeten Kanten (R 7,5) direkt auf dem Bett und würden unsauber. So bleiben Oberseite und Rundungen glatt, die Stützen sitzen innen, wo man sie nicht sieht. Die Bohrung der Kopfhörerbuchse ist oben mit einer 0,2-mm-Schicht verschlossen und bleibt zu, solange keine Buchse eingebaut ist.', 'Print the lid top side up. The other way round, the rounded edges (R 7.5) would sit directly on the bed and come out rough. This way the top and the rounded edges stay smooth, and the supports sit inside where nobody sees them. The hole for the headphone jack is closed at the top by a 0.2 mm layer and stays closed as long as no jack is fitted.'), 'tip'),
  PageBreak()]
tok_steps = [bullets([
  T('Den Token mit der Oberseite (Ø 44 mm) aufs Bett legen. Die Bildfläche wird so glatt, und der breite Rand haftet gut.', 'Put the token on the bed with its top side (Ø 44 mm) down. That makes the picture surface smooth, and the wide rim sticks well.'),
  T('Im Slicer eine Pause vor der letzten Schicht einfügen, bei 0,2 mm Schichthöhe vor <b>Z = 15,0 mm</b>. Dann ist die NFC-Tasche oben offen.', 'In the slicer, add a pause before the last layer, with 0.2 mm layers before <b>Z = 15.0 mm</b>. The NFC pocket is then open at the top.'),
  T('In der Pause den NFC-Tag mit der Klebeseite nach unten in die Tasche legen und andrücken. Weiterdrucken, die letzte Schicht schließt die Tasche. Der Tag liegt dann 0,2 mm unter der Unterseite, direkt über dem RC522.', 'During the pause, put the NFC tag into the pocket, adhesive side down, and press it on. Resume printing, the last layer closes the pocket. The tag then sits 0.2 mm below the bottom face, right above the RC522.'),
  T('Warnt der Slicer vor langen Brücken, ist das die Decke über dem Hohlraum im Token. Damit ist zu rechnen.', 'If the slicer warns about long bridges, that is the ceiling over the cavity in the token. That is expected.'),
  T('Das Bild aus dem Token-Bogen (<font name="Mono" size="8.3">tools/token-sheet.html</font>) grob ausschneiden, zum Beispiel mit doppelseitigem Klebeband auf die Oberseite (Ø 44 mm) kleben und ringsum an der Kante abschneiden.', 'Roughly cut out the picture from the token sheet (<font name="Mono" size="8.3">tools/token-sheet.html</font>), stick it onto the top side (Ø 44 mm), for example with double-sided tape, and trim it along the edge.')])]
two = Table([[tok_steps, LayerDiagram()]], colWidths=[FW-92*mm, 92*mm]); two.setStyle(TableStyle([('VALIGN',(0,0),(-1,-1),'TOP'), ('LEFTPADDING',(0,0),(-1,-1),0), ('RIGHTPADDING',(0,0),(-1,-1),0)]))
story += [P(T('Token mit NFC-Tag', 'Token with NFC tag'), 'h2'),
  P(T('Der Token ist 15 mm hoch und innen hohl. 0,2 mm über der Unterseite liegt eine Tasche (Ø 26 × 0,4 mm) für einen runden NFC-Klebetag: 25 mm, NTAG, höchstens 0,4 mm dick.', 'The token is 15 mm tall and hollow. 0.2 mm above the bottom face there is a pocket (Ø 26 × 0.4 mm) for a round NFC sticker: 25 mm, NTAG, at most 0.4 mm thick.')),
  fig('token_section', [dict(a='top', t=T('Oberseite Ø 44 mm', 'Top side Ø 44 mm'), s=T('liegt auf dem Bett', 'lies on the bed')), dict(a='flange', t=T('Rand', 'Rim'), s=T('liegt im Gehäuse auf dem Deckel', 'rests on the lid in use')),
       dict(a='body', t=T('Körper Ø 39,7 mm', 'Body Ø 39.7 mm'), s=T('passt in die Mulde Ø 40', 'fits the cup Ø 40')), dict(a='void', t=T('Hohlraum', 'Cavity')),
       dict(a='pocket', t=T('NFC-Tasche Ø 26 × 0,4 mm', 'NFC pocket Ø 26 × 0.4 mm'), s=T('0,2 mm unter der Unterseite', '0.2 mm below the bottom face')), dict(a='chamfer', t=T('Fase', 'Chamfer')), dict(a='bed', t=T('Druckbett', 'Print bed'))], frac=0.64),
  cap(T('Token im Schnitt, so wie er gedruckt wird.', 'Token in section, the way it is printed.')), two, Spacer(1, 8),
  note(T('Sondertokens für Bluetooth und Management werden genauso gedruckt. Ihre UIDs kommen in die <font name="Mono" size="8.3">platformio.ini</font>, siehe Kapitel 5.5.', 'Special tokens for Bluetooth and management are printed the same way. Their UIDs go into <font name="Mono" size="8.3">platformio.ini</font>, see chapter 5.5.'), 'info'), PageBreak()]

# 4 Material
story += [H1(4, T('Material und Werkzeug', 'Materials and tools')), P(T('Zukaufteile fürs Gehäuse', 'Bought parts for the case'), 'h2'),
  table([[T('Teil', 'Part'), T('Anzahl', 'Qty'), T('Anmerkung', 'Note')],
   [T('Gewindeeinsatz M3', 'Heat-set insert M3'), '4', T('Für Bohrung Ø 4,0 mm, 4 mm lang. Bis 5,7 mm geht, ragt dann innen 1,2 mm heraus', 'For a Ø 4.0 mm hole, 4 mm long. Up to 5.7 mm works, it then sticks out 1.2 mm on the inside')],
   [T('Zylinderkopfschraube M3 × 5, ISO 4762', 'Socket head screw M3 × 5, ISO 4762'), '4', T('Kopf Ø 5,5 mm versinkt in der Senkung Ø 5,6 mm und im Bumper. Nicht länger als 5 mm: In zwei Ecken steht direkt über dem Einsatz ein Akku', 'The Ø 5.5 mm head sinks into the Ø 5.6 mm counterbore and into the bumper. No longer than 5 mm: in two corners a cell sits right above the insert')],
   [T('18650-Zelle', '18650 cell'), '2', T('Einfache Zellen ohne Schutzschaltung (65 mm) mit Lötfahnen, gleicher Typ und gleiche Spannung. Parallel geschaltet', 'Plain cells without protection circuit (65 mm) with solder tabs, same type and same voltage. Wired in parallel')],
   [T('Stoff', 'Fabric'), T('1 Stück, 8 × 8 cm', '1 piece, 8 × 8 cm'), T('Dünn und dehnbar, zum Beispiel eine Socke', 'Thin and stretchy, for example a sock')],
   [T('NFC-Klebetag, rund, 25 mm', 'NFC sticker, round, 25 mm'), T('je Token', 'per token'), T('NTAG, höchstens 0,4 mm dick', 'NTAG, at most 0.4 mm thick')], [T('Rolladengurt, ca. 15 mm breit', 'Roller shutter strap, about 15 mm wide'), T('ca. 30 cm', 'about 30 cm'), T('Tragegurt durch die Laschen, aus dem Baumarkt', 'Carrying strap through the lugs, from the hardware store')], [T('Kabelbinder, schmal', 'Cable tie, narrow'), '1', T('Um Hauptplatine, Abstandshalter und RC522', 'Around main board, spacer and RC522')], [T('Transparentes Filament, 1,75 mm', 'Clear filament, 1.75 mm'), T('ein paar cm', 'a few cm'), T('Lichtleiter von der Lade-LED nach außen, darf gebogen verlaufen', 'Light pipe from the charge LED to the outside, may run in a bend')]], [55*mm, 26*mm, FW-81*mm]),
  P(T('Kleber und Kleinkram', 'Glue and small stuff'), 'h2'),
  table([[T('Material', 'Material'), T('wofür', 'what for')], [T('Sekundenkleber', 'Super glue'), T('Gehäuseschale, Tasten, Lautsprechergitter, Bumper', 'Case shell, buttons, speaker grill, bumper')], [T('2-Komponenten-Kleber (Epoxid)', '2-part epoxy'), T('Lautsprecher, Ladeschaltung, Step-up', 'Speaker, charging board, step-up')],
   [T('UV-Lack mit UV-Lampe', 'UV resin with UV lamp'), T('LED, USB-C-Buchse, Mikroschalter', 'LED, USB-C socket, microswitch')], [T('Heißkleber', 'Hot glue'), T('Akkus', 'Cells')], [T('Dünnes doppelseitiges Klebeband', 'Thin double-sided tape'), T('Piezos, RC522, Bilder auf den Token', 'Piezos, RC522, pictures on the tokens')],
   [T('Kapton-Band, Schrumpfschlauch', 'Kapton tape, heat-shrink tubing'), T('Akkukontakte und Lötstellen isolieren', 'Insulate cell contacts and solder joints')], [T('Nadel und reißfestes Garn oder Nähmaschine', 'Needle and strong thread or a sewing machine'), T('Tragegurt', 'Carrying strap')], [T('Schleifpapier, Grundierung, Lack', 'Sandpaper, primer, paint'), T('optional, für die Oberfläche', 'optional, for the surface')]], [62*mm, FW-62*mm]),
  P(T('Elektronik im Gehäuse', 'Electronics in the case'), 'h2'), P(T('Die vollständige Teileliste steht in Kapitel 5.1. Diese Teile müssen zum Gehäuse passen:', 'The full parts list is in chapter 5.1. These parts have to fit the case:')),
  table([[T('Teil', 'Part'), T('Ort', 'Where'), T('Passt, wenn …', 'Fits if …')],
   [T('Lautsprecher, 3 W', 'Speaker, 3 W'), T('innen am Gitter', 'inside behind the grill'), T('Ø 50–57 mm liegt mit dem Rand auf dem Ring (Ø 48,6–54,6 mm); kleinere sitzen im Ring direkt am Gitter', 'Ø 50–57 mm rests with its rim on the ring (Ø 48.6–54.6 mm); smaller ones sit inside the ring right at the grill')],
   [T('Piezo-Scheibe, ca. 35 mm', 'Piezo disc, about 35 mm'), T('innen an den Seitenwänden', 'inside on the side walls'), T('gedünnte Fläche 56 × 50 mm', 'thinned area 56 × 50 mm')],
   [T('PL9823, 8 mm', 'PL9823, 8 mm'), T('Deckel vorn links', 'lid, front left'), T('Bohrung Ø 8 mm', 'hole Ø 8 mm')],
   [T('Poti 10 kΩ', 'Potentiometer 10 kΩ'), T('Deckel hinten links', 'lid, back left'), T('Gewinde M7 (Bohrung Ø 7 mm), Achse 6 mm rund. Gewindehülse und Mutter müssen in die 3 mm tiefe Aussparung im Knopf passen', 'M7 thread (hole Ø 7 mm), round 6 mm shaft. Threaded bushing and nut must fit the 3 mm deep recess in the knob')],
   ['RC522', T('unter der Token-Mulde', 'under the token cup'), T('Muldenboden 1 mm', 'cup floor 1 mm')], [T('Mikroschalter', 'Microswitch'), T('Tasche neben der Mulde', 'pocket next to the cup'), T('Hebel reicht durch den Schlitz in die Mulde', 'lever reaches through the slot into the cup')],
   [T('USB-C-Platine', 'USB-C board'), T('Rückwand unten', 'back wall, bottom'), T('Ausschnitt 9,2 × 3,5 mm', 'cutout 9.2 × 3.5 mm')], [T('Power Latch mit TP4056', 'Power latch with TP4056'), T('Wandinnenseite, nahe der USB-C-Buchse', 'inside of the wall, near the USB-C socket'), T('Lichtleiter vom Loch Ø 2 mm bis an die Lade-LED', 'light pipe from the Ø 2 mm hole to the charge LED')], [T('Step-up 5 V', 'Step-up 5 V'), T('freie Stelle an der Wandinnenseite', 'free spot on the inside of the wall'), T('nicht auf den gedünnten Tastenflächen', 'not on the thinned button areas')], [T('Kopfhörerbuchse 3,5 mm', 'Headphone jack 3.5 mm'), T('Deckel hinten rechts, optional', 'lid, back right, optional'), T('Bohrung Ø 6 mm, Tasche 12 × 7 × 2 mm', 'hole Ø 6 mm, pocket 12 × 7 × 2 mm')]],
   [38*mm, 44*mm, FW-82*mm]),
  P(T('Werkzeug', 'Tools'), 'h2'),
  P(T('Lötkolben (für die Einsätze eine Einschmelzspitze oder eine alte Spitze), Innensechskant 2,5 mm, UV-Lampe, Heißklebepistole, Multimeter, Säge oder Seitenschneider für die Poti-Achse, Cutter, Schleifpapier. Für die Elektronik dazu: Lötkolben mit feiner Spitze, Lötzinn, Flussmittel, Entlötlitze, Pinzette, Abisolierzange und ein USB-C-Kabel zum Flashen.', 'Soldering iron (for the inserts a heat-set tip or an old tip), 2.5 mm hex key, UV lamp, hot glue gun, multimeter, saw or side cutter for the potentiometer shaft, utility knife, sandpaper. For the electronics also: soldering iron with a fine tip, solder, flux, desoldering braid, tweezers, wire stripper and a USB-C cable for flashing.')),
  CondPageBreak(160*mm)]

# 5 Zusammenbau

# 5 Elektronik
EB = '#2f5fa8'   # badge colour of the electronics steps
MB = load_board(str(ROOT / 'hardware' / 'mainboard' / 'timon-box.kicad_pcb'))
PL = load_board(str(ROOT / 'hardware' / 'power-latch' / 'power-latch-v2.kicad_pcb'))
G_MB = [('#f0a23b', T('E1 SMD-Teile', 'E1 SMD parts'), ['U2', 'U3', 'Q1', 'Q2', 'D1', 'D2', 'R1', 'R2', 'R3', 'R4', 'R5', 'C1', 'C2', 'C3', 'C4', 'C7', 'C8']),
        ('#6f9ef0', T('E2 ESP32 und Taster', 'E2 ESP32 and buttons'), ['U1', 'SW1', 'SW2']), ('#b48ae0', T('E3 XH-Buchse', 'E3 XH socket'), ['J4']),
        ('#5cc0b5', T('E4 Buchsenleisten', 'E4 female headers'), ['MAX1', 'U4', 'P1'])]
G_PL = [('#f0a23b', T('E7 SMD-Teile', 'E7 SMD parts'), ['Q1', 'Q2', 'Q3', 'Q4', 'D1', 'R1', 'R2', 'R3', 'R4', 'R5', 'R6', 'R7', 'C1', 'C2']),
        ('#6f9ef0', T('E7 D2 zuletzt', 'E7 D2 last'), ['D2']), ('#b48ae0', T('E7 TP4056-Modul', 'E7 TP4056 module'), ['J4']), ('#5cc0b5', T('E10 Kabel an J1', 'E10 wires to J1'), ['J1'])]


def boards(board, k, groups, dnp=(), titles=(T('Vorderseite', 'Front side'), T('Rückseite, von unten gesehen', 'Back side, seen from below'))):
    a = PcbView(board, 'F', k, groups, dnp, titles[0]); b = PcbView(board, 'B', k, groups, dnp, titles[1])
    t = Table([[a, b]], colWidths=[a.width + 10*mm, b.width], hAlign='CENTER')
    t.setStyle(TableStyle([('VALIGN', (0, 0), (-1, -1), 'TOP'), ('LEFTPADDING', (0, 0), (-1, -1), 0), ('RIGHTPADDING', (0, 0), (-1, -1), 0)]))
    return t


story += [CondPageBreak(160*mm), H1(5, T('Elektronik', 'Electronics')),
  P(T('Die Elektronik besteht aus zwei Platinen und ein paar lose verdrahteten Teilen. Die Bestückungspläne sind direkt aus den KiCad-Dateien unter <font name="Mono" size="8.3">hardware/</font> gezeichnet, die Schaltpläne stehen im Anhang.', 'The electronics are two boards and a few loosely wired parts. The assembly drawings come straight from the KiCad files in <font name="Mono" size="8.3">hardware/</font>, the schematics are in the appendix.')),
  bullets([T('<b>Hauptplatine</b> (40,9 × 60,5 mm): ESP32, USB-seriell-Wandler CH340C, 3,3-V-Regler und drei Module auf Buchsenleisten: I²S-Verstärker MAX98357A und SD-Karte oben, RFID-Leser RC522 unten.', '<b>Main board</b> (40.9 × 60.5 mm): ESP32, CH340C USB-serial converter, 3.3 V regulator and three modules on female headers: MAX98357A I²S amplifier and SD card on top, RC522 RFID reader underneath.'),
           T('<b>Power Latch V2</b> (31,3 × 18,3 mm): Lademodul TP4056, Umschaltung zwischen USB und Akku und die Selbsthaltung. Ein Mikroschalter schaltet die Box ein, wenn ein Token aufgelegt wird. Danach hält der ESP32 die Box selbst an. Anders lässt sie sich nicht einschalten, auch nicht über USB.', '<b>Power latch V2</b> (31.3 × 18.3 mm): TP4056 charger module, switching between USB and battery, and the self-holding circuit. A microswitch turns the box on when a token is placed. After that the ESP32 keeps the box on by itself. There is no other way to switch it on, not even through USB.'),
           T('<b>Lose verdrahtet:</b> 18650-Zellen, USB-C-Buchse, Boost-Wandler auf 5 V, zwei 2N7000, Mikroschalter, zwei Piezos, Poti, LED und Lautsprecher.', '<b>Loosely wired:</b> 18650 cells, USB-C socket, 5 V boost converter, two 2N7000, microswitch, two piezos, potentiometer, LED and speaker.')]),
  P(T('Ein 7-poliges Kabel verbindet die Hauptplatine mit der Stromseite. Über dieses Kabel laufen auch die USB-Datenleitungen. Die USB-C-Buchse am Gehäuse lädt also den Akku und dient zugleich zum Flashen.', 'A 7-pin cable connects the main board to the power side. The USB data lines run through this cable as well, so the USB-C socket on the case charges the battery and is also used for flashing.')),
  P(T('5.1  Teileliste', '5.1  Parts list'), 'h2'),
  table([[T('Teil', 'Part'), T('Anzahl', 'Qty'), T('Anmerkung', 'Note')],
   [T('ESP32-WROOM-32, -32D oder -32E', 'ESP32-WROOM-32, -32D or -32E'), '1', T('Ohne PSRAM. Ein WROVER geht nicht, dort belegt der PSRAM GPIO 16 und 17, die SD-Karte braucht sie', 'Without PSRAM. A WROVER does not work, its PSRAM uses GPIO 16 and 17, which the SD card needs')],
   [T('MAX98357A-Modul', 'MAX98357A module'), '1', T('I²S-Verstärker. 7 Pins: LRC, BCLK, DIN, GAIN, SD, GND, VIN', 'I²S amplifier. 7 pins: LRC, BCLK, DIN, GAIN, SD, GND, VIN')],
   [T('microSD-Modul für 3,3 V', 'microSD module for 3.3 V'), '1', T('6 Pins: 3V3, CS, MOSI, CLK, MISO, GND. Die Platine wird gekürzt (Schritt E5)', '6 pins: 3V3, CS, MOSI, CLK, MISO, GND. The board gets shortened (step E5)')],
   [T('RC522-Modul', 'RC522 module'), '1', T('RFID mit 13,56 MHz. 8 Pins, sitzt unter der Hauptplatine', '13.56 MHz RFID. 8 pins, sits under the main board')],
   [T('TP4056-Lademodul mit Schutzschaltung', 'TP4056 charger module with protection'), '1', T('Wird auf die Rückseite der Power Latch gelötet', 'Gets soldered to the back of the power latch')],
   [T('Boost-Modul mit festen 5 V', 'Boost module with fixed 5 V'), '1', T('Manche Module stellt man erst mit Lötbrücken auf 5 V', 'Some modules need solder bridges to be set to 5 V')],
   [T('18650-Zelle', '18650 cell'), '1–2', T('Einfache Li-Ion-Zellen, 65 mm lang. Im gedruckten Gehäuse zwei, parallel. Die Schutzschaltung sitzt auf dem TP4056-Modul', 'Plain Li-ion cells, 65 mm long. Two in parallel in the printed case. The protection circuit sits on the TP4056 module')],
   [T('USB-C-Buchse auf kleiner Platine', 'USB-C socket on a small board'), '1', T('Mit 5,1 kΩ an CC1 und CC2, sonst liefern USB-C-Netzteile keine 5 V. D+ und D− müssen herausgeführt sein', 'With 5.1 kΩ on CC1 and CC2, otherwise USB-C chargers deliver no 5 V. D+ and D− must be broken out')],
   ['2N7000 (TO-92)', '2', T('Umbau an der Power Latch (Schritt E8)', 'Modification on the power latch (step E8)')],
   [T('Mikroschalter 12,8 × 5,8 mm', 'Microswitch 12.8 × 5.8 mm'), '1', T('Wechsler mit C, NO und NC. Der Token drückt ihn', 'Changeover with C, NO and NC. The token presses it')],
   [T('Piezo-Scheibe, etwa 35 mm', 'Piezo disc, about 35 mm'), '2', T('Klopfsensoren für Vor und Zurück', 'Knock sensors for next and previous')],
   [T('Poti 10 kΩ linear', 'Potentiometer 10 kΩ linear'), '1', T('Lautstärke', 'Volume')],
   [T('PL9823, 8 mm', 'PL9823, 8 mm'), '1', T('Bedrahtete RGB-LED mit eingebautem Controller, Farbreihenfolge RGB', 'Through-hole RGB LED with built-in controller, colour order RGB')],
   [T('Lautsprecher, 3 W', 'Speaker, 3 W'), '1', T('Klein, 4 oder 8 Ω', 'Small, 4 or 8 Ω')],
   [T('JST-XH-Buchse 7-polig, stehend, mit Stecker', 'JST XH socket, 7 pins, vertical, with plug'), '1', T('2,5 mm Raster, für das Kabel zur Stromseite', '2.5 mm pitch, for the cable to the power side')],
   [T('Buchsen- und Stiftleisten, 2,54 mm', 'Female and male headers, 2.54 mm'), '', T('7-polig für den Verstärker, 6-polig für die SD-Karte, 8-polig für das RC522', '7 pins for the amplifier, 6 pins for the SD card, 8 pins for the RC522')],
   [T('microSD-Karte', 'microSD card'), '1', 'FAT32'],
   [T('NFC-Klebetags, rund, 25 mm', 'NFC stickers, round, 25 mm'), T('je Token', 'per token'), T('13,56 MHz, NXP NTAG mit 7-Byte-UID. Sie werden beim Druck in den Token eingelegt (Kapitel 3)', '13.56 MHz, NXP NTAG with 7-byte UID. They go into the token during printing (chapter 3)')]],
   [55*mm, 18*mm, FW-73*mm]),
  CondPageBreak(120*mm)]

story += [P(T('5.2  Hauptplatine', '5.2  Main board'), 'h2'),
  table([[T('Bauteil', 'Part'), T('Wert', 'Value'), T('Bauform', 'Package'), T('Aufgabe', 'Job')],
   ['U1', 'ESP32-WROOM-32(D/E)', T('Modul', 'Module'), ''], ['U2', 'AMS1117-3.3', 'SOT-223', T('3,3 V aus den 5 V vom Kabel', '3.3 V from the 5 V on the cable')],
   ['U3', 'CH340C', 'SOIC-16', T('USB-seriell, ohne Quarz', 'USB-serial, no crystal')], ['Q1, Q2', '2N7002', 'SOT-23', T('Auto-Reset: DTR und RTS steuern EN und IO0', 'Auto reset: DTR and RTS drive EN and IO0')],
   ['D1, D2', 'BZT52C3V3', 'SOD-323', T('Zenerdioden, begrenzen die Piezo-Spannung auf 3,3 V', 'Zener diodes, limit the piezo voltage to 3.3 V')],
   ['R1, R2', '100 kΩ', '0805', T('Last parallel zu den Piezos', 'Load in parallel to the piezos')], ['R3', '10 kΩ', '0805', T('Pull-up an EN', 'Pull-up on EN')],
   ['R4, R5', '10 kΩ', '0805', T('Pull-ups für I²C (Reserve)', 'Pull-ups for I²C (spare)')], ['C1, C8', '100 nF', '0805', T('Abblockkondensatoren CH340C und ESP32', 'Decoupling caps for CH340C and ESP32')],
   ['C2', '1 µF', '0805', T('Mit R3 die Startverzögerung an EN', 'With R3 the start-up delay on EN')], ['C3', '10 µF', '0805', T('Eingang AMS1117', 'AMS1117 input')],
   ['C4, C7', '22 µF', '1206', T('3,3 V', '3.3 V')], ['C5, C6', '–', '0805', T('<b>Nicht bestücken</b>', '<b>Do not populate</b>')], ['SW1, SW2', T('Taster 6 × 6 mm', 'Push button 6 × 6 mm'), 'PTS645', T('RST und BOOT', 'RST and BOOT')],
   ['J4', T('JST XH, 7-polig, stehend', 'JST XH, 7 pins, vertical'), '', T('Kabel zur Stromseite, siehe Schritt E3', 'Cable to the power side, see step E3')], ['J1, J5', T('3-polig', '3 pins'), '', T('Poti und LED, Stiftleiste oder Kabel direkt', 'Potentiometer and LED, pin header or wires directly')],
   ['J2, J3', T('2-polig', '2 pins'), '', T('Piezos, Stiftleiste oder Kabel direkt', 'Piezos, pin header or wires directly')], ['J6', T('4-polig', '4 pins'), '', T('I²C, Reserve für Erweiterungen. Bleibt frei', 'I²C, spare for extensions. Stays empty')],
   ['MAX1', T('Buchsenleiste 7-polig', 'Female header, 7 pins'), '', T('MAX98357A-Modul', 'MAX98357A module')], ['U4', T('Buchsenleiste 6-polig', 'Female header, 6 pins'), '', T('SD-Modul', 'SD module')],
   ['P1', T('Buchsenleiste 8-polig, Rückseite', 'Female header, 8 pins, back side'), '', T('RC522-Modul', 'RC522 module')]], [18*mm, 44*mm, 20*mm, FW-82*mm]),
  Spacer(1, 6),
  KeepTogether([boards(MB, 1.75, G_MB, dnp=['C5', 'C6']), Spacer(1, 4),
    ChipRow([(g[1], g[0]) for g in G_MB] + [(T('nicht bestücken', 'do not populate'), '#e05a4f')]),
    cap(T('Bestückungsplan der Hauptplatine aus <font name="Mono" size="7.4">hardware/mainboard/timon-box.kicad_pcb</font>. Die Rückseite ist gespiegelt, so wie man von unten draufschaut; das RC522 deckt sie fast ganz ab.', 'Assembly drawing of the main board from <font name="Mono" size="7.4">hardware/mainboard/timon-box.kicad_pcb</font>. The back side is mirrored, as seen from below; the RC522 covers almost all of it.'))]),
  step('E1', T('SMD-Teile bestücken', 'Populate the SMD parts'), ['LK'], [
    T('CH340C (U3), AMS1117 (U2), die beiden 2N7002 (Q1, Q2), Widerstände, Kondensatoren und Zenerdioden bestücken. Werte in der Tabelle, Lage im Bestückungsplan.', 'Populate the CH340C (U3), the AMS1117 (U2), both 2N7002 (Q1, Q2), resistors, capacitors and zener diodes. Values in the table, positions in the assembly drawing.'),
    T('C5 und C6 bleiben frei.', 'C5 and C6 stay empty.')], color=EB),
  step('E2', T('ESP32 und Taster einlöten', 'Solder in the ESP32 and the buttons'), ['LK'], [
    T('Das ESP32-Modul (U1) und die beiden Taster RST (SW1) und BOOT (SW2) einlöten.', 'Solder in the ESP32 module (U1) and the two buttons RST (SW1) and BOOT (SW2).'),
    T('Nur ein WROOM-Modul ohne PSRAM nehmen. Beim WROVER belegt der PSRAM GPIO 16 und 17, die braucht die SD-Karte.', 'Only use a WROOM module without PSRAM. On a WROVER the PSRAM uses GPIO 16 and 17, which the SD card needs.')], color=EB),
  step('E3', T('XH-Buchse J4 einsetzen', 'Fit the XH socket J4'), ['LK'], [
    T('Der Footprint von J4 ist eine 2,54-mm-Stiftleiste. Eingebaut wird eine JST-XH-Buchse mit 2,5 mm Raster, sie passt in dieselben Löcher.', 'The footprint of J4 is a 2.54 mm pin header. What goes in is a JST XH socket with 2.5 mm pitch, it fits the same holes.'),
    T('Ihr Gehäuse ist tiefer als eine Stiftleiste und stößt an den AMS1117 (U2). Das Gehäuse deshalb an der Unterseite über dem Regler ausklinken, dann einlöten.', 'Its housing is deeper than a pin header and hits the AMS1117 (U2). So notch the housing on the underside above the regulator, then solder it in.'),
    T('Pin 1 (+5 V) liegt an der Platinenkante neben C4, Pin 7 (D−) zum ESP32 hin.', 'Pin 1 (+5 V) is at the board edge next to C4, pin 7 (D−) towards the ESP32.')], color=EB),
  step('E4', T('Buchsenleisten einlöten', 'Solder in the female headers'), ['LK'], [
    T('Oben die Buchsenleisten für den Verstärker (MAX1, 7-polig) und das SD-Modul (U4, 6-polig) einlöten, unten die für das RC522 (P1, 8-polig).', 'On top, solder in the female headers for the amplifier (MAX1, 7 pins) and the SD module (U4, 6 pins), underneath the one for the RC522 (P1, 8 pins).')], color=EB),
  step('E5', T('SD-Modul kürzen', 'Shorten the SD module'), [], [
    T('Die Platine des SD-Moduls ragt über die XH-Buchse. Die Kante zur Buchse hin absägen oder abschleifen, bis das Modul neben der Buchse steckt (Foto unten).', 'The SD module’s board overlaps the XH socket. Saw or sand off the edge facing the socket until the module plugs in next to the socket (photo below).')], color=EB),
  step('E6', T('Module einstecken', 'Plug in the modules'), ['LK'], [
    T('Verstärker und SD-Modul oben einstecken.', 'Plug the amplifier and the SD module in on top.'),
    T('Am RC522 die Stiftleiste so einlöten, dass es unter der Hauptplatine sitzt.', 'Solder the pin header to the RC522 so that it sits under the main board.'),
    T('Genau gegenüber der Stiftleiste sichert der gedruckte Abstandshalter (', 'Directly opposite the pin header, the printed spacer (') + pn('pcb_spacer') + T(') die frei hängende Hauptplatine, damit sie nicht nur an der Stiftleiste hängt. Sie rastet mit ihren Längskanten zwischen seinen Armen ein, sein Fuß steht auf dem RC522. Im Gehäuse kommt noch ein Kabelbinder darum (Kapitel 6, Schritt 11).', ') supports the overhanging main board so it does not hang on the pin header alone. The board clips in between its arms with its long edges, its foot stands on the RC522. In the case a cable tie goes around it as well (chapter 6, step 11).'),
    T('Im Gehäuse klebt das RC522 mit der flachen Rückseite unter der Token-Mulde (Kapitel 6, Schritt 9).', 'In the case the RC522 is stuck with its flat back under the token cup (chapter 6, step 9).')],
    [('info', T('Am MAX98357A liegt GAIN auf GND, das sind 12 dB. SD bleibt offen. Die Firmware schickt ohnehin mono, der Kanal ist also egal.', 'On the MAX98357A, GAIN is tied to GND, which gives 12 dB. SD stays open. The firmware sends mono anyway, so the channel does not matter.'))], color=EB),
  KeepTogether([fig('spacer_stack', [dict(a='spacer', t='pcb_spacer', s=T('Abstandshalter', 'Spacer')), dict(a='arm', t=T('Arme mit Rastnasen', 'arms with snap hooks'), s=T('halten die Hauptplatine', 'hold the main board')),
     dict(a='mainboard', t=T('Hauptplatine', 'Main board')), dict(a='rc522', t='RC522')], frac=0.6),
     cap(T('Der Abstandshalter genau gegenüber der Stiftleiste, zwischen Hauptplatine und RC522. Die Stiftleiste sitzt am anderen Ende, hinten im Bild.', 'The spacer directly opposite the pin header, between main board and RC522. The pin header is at the other end, at the back of the picture.'))]),
  KeepTogether([photos([('hw_overview_top.jpeg', T('Von oben: Verstärker (lila), SD-Modul, darunter die XH-Buchse J4, rechts der ESP32.', 'From above: amplifier (purple), SD module, the XH socket J4 below it, the ESP32 on the right.')),
                        ('hw_overview_buttom.jpeg', T('Von unten: das RC522.', 'From below: the RC522.')),
                        ('hw_cutpcb.jpeg', T('Das gekürzte SD-Modul (oben eingekreist) und die ausgeklinkte XH-Buchse über dem AMS1117 (unten eingekreist).', 'The shortened SD module (circled at the top) and the notched XH socket above the AMS1117 (circled at the bottom).'))], cols=3),
                photos([('hw_overview_side.jpeg', T('Von der Seite: Verstärker und SD-Modul auf Buchsenleisten, das RC522 unter der Hauptplatine.', 'From the side: amplifier and SD module on female headers, the RC522 under the main board.'))], cols=1, width=110*mm)])]

story += [CondPageBreak(140*mm), P(T('5.3  Stromseite', '5.3  Power side'), 'h2'),
  table([[T('Bauteil', 'Part'), T('Wert', 'Value'), T('Bauform', 'Package'), T('Aufgabe', 'Job')],
   ['Q1', 'AO4606', 'SOIC-8', T('N- und P-Kanal in einem Gehäuse: P schaltet Vcc, N hält ihn', 'N and P channel in one package: P switches Vcc, N holds it')], ['Q2', 'BSS138', 'SOT-23', T('TrigOut, Open Drain zum ESP32', 'TrigOut, open drain to the ESP32')],
   ['Q3', 'BSS138', 'SOT-23', T('Einschaltimpuls', 'Switch-on pulse')], ['Q4', 'AO3401A', 'SOT-23', T('Akku-Zweig der Umschaltung USB/Akku', 'Battery branch of the USB/battery switchover')],
   ['D1', 'BAT85', 'SOD-323', T('Klemmt den Einschaltimpuls nach unten', 'Clamps the switch-on pulse downwards')], ['D2', 'BAT85', T('DO-35, bedrahtet', 'DO-35, through-hole'), T('USB-Zweig der Umschaltung', 'USB branch of the switchover')],
   ['R1, R4–R7', '10 kΩ', '0805', T('R4/R5: Spannungsteiler für Sense', 'R4/R5: voltage divider for Sense')], ['R2', '1 MΩ', '0805', T('Pull-down an Hold', 'Pull-down on Hold')],
   ['R3', '100 kΩ', '0805', T('Entlädt C1', 'Discharges C1')], ['C1', '10 µF', '0805', T('Einschaltimpuls', 'Switch-on pulse')], ['C2', '1 µF', '0805', T('Gate des P-Kanals', 'Gate of the P channel')],
   ['J4', T('TP4056-Modul', 'TP4056 module'), '', T('Auf der Rückseite aufgelötet', 'Soldered onto the back')], ['J1', '–', '', T('Hier werden die sieben Kabel direkt eingelötet', 'The seven wires are soldered in directly here')]],
   [18*mm, 30*mm, 30*mm, FW-78*mm]),
  Spacer(1, 6),
  KeepTogether([boards(PL, 2.5, G_PL), Spacer(1, 4), ChipRow([(g[1], g[0]) for g in G_PL]),
    cap(T('Bestückungsplan der Power Latch V2 aus <font name="Mono" size="7.4">hardware/power-latch/power-latch-v2.kicad_pcb</font>. Die Beschriftung von J1 steht auf beiden Seiten.', 'Assembly drawing of the power latch V2 from <font name="Mono" size="7.4">hardware/power-latch/power-latch-v2.kicad_pcb</font>. The labels of J1 are on both sides.'))]),
  P(T('So schaltet die Power Latch', 'How the power latch switches'), 'h3'),
  bullets([T('<b>USB oder Akku:</b> Die 5 V vom USB kommen über D2 zur Last, der Akku über Q4. Steckt USB, sperrt Q4. Die Box läuft dann vom USB, und das TP4056 lädt den Akku.', '<b>USB or battery:</b> The 5 V from USB reach the load through D2, the battery through Q4. With USB plugged in, Q4 blocks. The box then runs from USB, and the TP4056 charges the battery.'),
           T('<b>Einschalten:</b> Ein Token drückt den Mikroschalter, der verbindet ', '<b>Switching on:</b> A token presses the microswitch, which connects ') + code('Trig+') + T(' mit ', ' to ') + code('TrigIn') + T('. Über C1 schaltet Q3 für etwa eine Sekunde durch, genug, damit der ESP32 startet und HOLD setzt.', '. Through C1, Q3 turns on for about a second, long enough for the ESP32 to start and set HOLD.'),
           T('<b>Halten:</b> Liegt ', '<b>Holding:</b> While ') + code('Hold') + T(' hoch, hält der N-Kanal von Q1 die Box an. Sonst zieht R2 ', ' is high, the N channel of Q1 keeps the box on. Otherwise R2 pulls ') + code('Hold') + T(' auf GND.', ' to GND.'),
           T('<b>Token-Meldung:</b> Solange der Schalter geschlossen ist, zieht Q2 ', '<b>Token signal:</b> While the switch is closed, Q2 pulls ') + code('TrigOut') + T(' auf GND. Der ESP32 liest das an GPIO 32.', ' to GND. The ESP32 reads this on GPIO 32.'),
           T('<b>Akkuspannung:</b> R4 und R5 teilen ', '<b>Battery voltage:</b> R4 and R5 divide ') + code('Vcc') + T(' durch 2 und geben das auf ', ' by 2 and put the result on ') + code('Sense') + T('. Der ESP32 misst es an GPIO 39.', '. The ESP32 measures it on GPIO 39.')]),
  step('E7', T('Power Latch bestücken', 'Populate the power latch'), ['LK'], [
    T('Die SMD-Teile bestücken, die bedrahtete Diode D2 zuletzt.', 'Populate the SMD parts, the through-hole diode D2 last.'),
    T('Dann das TP4056-Modul auf die Rückseite auflöten (J4). Seine vier Eckpads IN+, IN−, OUT+ und OUT− sitzen direkt auf dem Footprint.', 'Then solder the TP4056 module onto the back (J4). Its four corner pads IN+, IN−, OUT+ and OUT− sit directly on the footprint.')], color=EB),
  step('E8', T('Zwei 2N7000 nachrüsten', 'Add two 2N7000'), ['LK'], [
    code('Hold') + T(' der Latch hängt nicht direkt am ESP32. Zwei 2N7000 liegen parallel: Drain an ', ' of the latch is not connected to the ESP32 directly. Two 2N7000 sit in parallel: drain to ') + code('Vcc') + T(', Source an ', ', source to ') + code('Hold') + '.',
    T('Gate des einen („ESP“) an HOLD vom ESP32, Kabel Pin 3 (blau). Gate des anderen („USB“) an D− der USB-C-Buchse.', 'Gate of one (“ESP”) to HOLD from the ESP32, cable pin 3 (blue). Gate of the other (“USB”) to D− of the USB-C socket.'),
    T('Die beiden bilden ein logisches Oder: ', 'Together they form a logical OR: ') + code('Hold') + T(' liegt hoch, wenn der ESP32 HOLD setzt oder an D− Spannung liegt. Einschalten kann keiner der beiden, denn ist die Box aus, liegt an ', ' is high when the ESP32 sets HOLD or when there is voltage on D−. Neither of them can switch the box on: when the box is off, ') + code('Vcc') + T(' keine Spannung.', ' carries no voltage.')], color=EB),
  step('E9', T('Boost-Modul prüfen', 'Check the boost module'), [], [
    T('Das Boost-Modul vor dem Anschließen allein und ohne Last messen: Es muss 5 V liefern. Manche Module stellt man erst mit Lötbrücken auf 5 V.', 'Measure the boost module on its own and without load before connecting it: it has to deliver 5 V. Some modules need solder bridges to be set to 5 V.')], color=EB),
  step('E10', T('Stromseite verdrahten', 'Wire up the power side'), ['LK'], [
    T('USB-C-Buchse, Mikroschalter, Boost-Modul, die beiden 2N7000 und die sieben Adern des XH-Kabels nach den Tabellen in 5.4 anlöten. Die Kabel lang genug für den Einbau lassen.', 'Solder on the USB-C socket, microswitch, boost module, both 2N7000 and the seven wires of the XH cable following the tables in 5.4. Leave the wires long enough for fitting.'),
    T('Der NC-Kontakt des Mikroschalters bleibt frei. Die Akkus kommen erst beim Einbau dazu (Kapitel 6, Schritt 11).', 'The NC contact of the microswitch stays free. The cells are added only during assembly (chapter 6, step 11).')], color=EB),
  photos([('hw_bat.jpeg', T('Power Latch V2 mit dem TP4056-Modul auf der Rückseite, rechts die USB-C-Buchse, unten die 18650-Zelle.', 'Power latch V2 with the TP4056 module on the back, the USB-C socket on the right, the 18650 cell at the bottom.')),
          ('hw_hack.jpeg', T('Einer der beiden 2N7000 auf den Pads Hold und Vcc der Latch.', 'One of the two 2N7000 on the Hold and Vcc pads of the latch.')),
          ('hw_peripherals.jpeg', T('Stromseite mit Mikroschalter und Kabel zur Hauptplatine.', 'Power side with microswitch and cable to the main board.'))], cols=3)]

j1 = table([['Pin', T('Beschriftung', 'Label'), T('an', 'to')], ['1', code('Vcc'), T('Boost IN+ und Drain beider 2N7000', 'Boost IN+ and drain of both 2N7000')], ['2', code('Hold'), T('Source beider 2N7000', 'Source of both 2N7000')],
            ['3', code('Sense 50%'), T('Kabel Pin 4 (gelb)', 'Cable pin 4 (yellow)')], ['4', code('Trigger +'), T('Mikroschalter C', 'Microswitch C')], ['5', code('Trigger In'), T('Mikroschalter NO', 'Microswitch NO')],
            ['6', code('Trigger Out'), T('Kabel Pin 5 (orange)', 'Cable pin 5 (orange)')], ['7', code('GND'), 'Boost IN−']], [12*mm, 28*mm, 62*mm])
conn = [photo('hw_connector.jpeg', 58*mm), Paragraph(T('Der XH-Stecker mit den Aderfarben, links Pin 1.', 'The XH plug with its wire colours, pin 1 on the left.'), S['pcap'])]
side = Table([[[P(T('Power Latch, Anschluss J1', 'Power latch, connector J1'), 'h3'), j1, Spacer(1, 3), P(T('Der NC-Kontakt des Mikroschalters bleibt frei.', 'The NC contact of the microswitch stays free.'), 'cap')], conn]], colWidths=[FW-62*mm, 62*mm])
side.setStyle(TableStyle([('VALIGN', (0, 0), (-1, -1), 'TOP'), ('LEFTPADDING', (0, 0), (-1, -1), 0), ('RIGHTPADDING', (0, 0), (0, 0), 6*mm), ('RIGHTPADDING', (1, 0), (1, 0), 0)]))
story += [CondPageBreak(120*mm), P(T('5.4  Verkabelung', '5.4  Wiring'), 'h2'),
  P(T('Jede Zeile nennt beide Enden. Das vollständige Verdrahtungsdiagramm der Stromseite steht im Anhang.', 'Every row names both ends. The full wiring diagram of the power side is in the appendix.')),
  P(T('Kabel zwischen Hauptplatine und Stromseite', 'Cable between main board and power side'), 'h3'),
  P(T('Auf der Hauptplatine steckt das Kabel in J4, auf der Stromseite gehen die Adern einzeln an verschiedene Teile. Pin 1 (+5 V) liegt an der Platinenkante neben C4, Pin 7 (D−) zum ESP32 hin.', 'On the main board the cable plugs into J4, on the power side the wires go to different parts. Pin 1 (+5 V) is at the board edge next to C4, pin 7 (D−) towards the ESP32.')),
  table([[T('Pin J4', 'J4 pin'), 'Signal', T('Ader', 'Wire'), 'ESP32', T('Stromseite', 'Power side')], ['1', '+5V', Wire('rot'), '–', 'Boost OUT+'], ['2', 'GND', Wire('schwarz'), '–', 'Boost OUT−'],
         ['3', 'HOLD', Wire('blau'), 'GPIO 33', T('Gate des 2N7000 „ESP“', 'Gate of the 2N7000 “ESP”')], ['4', 'SENSE', Wire('gelb'), 'GPIO 39', T('Latch J1 Pin 3, ', 'Latch J1 pin 3, ') + code('Sense 50%')],
         ['5', 'SW_TOKEN', Wire('orange'), 'GPIO 32', T('Latch J1 Pin 6, ', 'Latch J1 pin 6, ') + code('Trigger Out')], ['6', 'D+', Wire('grün'), 'CH340C', 'USB-C D+'],
         ['7', 'D−', Wire('weiß'), 'CH340C', T('USB-C D− und Gate des 2N7000 „USB“', 'USB-C D− and gate of the 2N7000 “USB”')]], [16*mm, 24*mm, 26*mm, 22*mm, FW-88*mm]),
  Spacer(1, 4), side,
  P(T('Akku, Laden und USB-C', 'Battery, charging and USB-C'), 'h3'),
  table([[T('von', 'from'), T('an', 'to')], ['USB-C VBUS, GND', T('TP4056 IN+, IN−. Auf der Latch sind das die Pads 5v + und GND', 'TP4056 IN+, IN−. On the latch these are the pads 5v + and GND')], [T('18650 Plus, Minus (beide Zellen parallel)', '18650 plus, minus (both cells in parallel)'), 'TP4056 B+, B−'],
         ['TP4056 OUT+, OUT−', T('Latch Bat+, Bat−. Das Modul ist dort aufgelötet', 'Latch Bat+, Bat−. The module is soldered on there')], ['USB-C D+, D−', T('Kabel Pin 6 und 7, D− zusätzlich an das Gate des 2N7000 „USB“', 'Cable pins 6 and 7, D− also to the gate of the 2N7000 “USB”')]],
        [62*mm, FW-62*mm]),
  P(T('Bedienteile an der Hauptplatine', 'Controls on the main board'), 'h3'),
  table([[T('Anschluss', 'Connector'), 'Pins', 'GPIO', T('an', 'to')], [T('J1 „Volume“', 'J1 “Volume”'), '+3V3, SIG, GND', '36', T('Poti: Enden an +3V3 und GND, Schleifer an SIG', 'Potentiometer: ends to +3V3 and GND, wiper to SIG')],
         [T('J2 „R Btn“', 'J2 “R Btn”'), 'BTN, GND', '34', T('Piezo an der <b>linken</b> Wand, von vorn gesehen: zurück', 'Piezo on the <b>left</b> wall, seen from the front: previous')], [T('J3 „L Btn“', 'J3 “L Btn”'), 'BTN, GND', '35', T('Piezo an der <b>rechten</b> Wand: vor', 'Piezo on the <b>right</b> wall: next')],
         [T('J5 „NeoPixel“', 'J5 “NeoPixel”'), '+5V, DATA, GND', '2', T('PL9823: VDD, DIN, GND. DOUT bleibt frei', 'PL9823: VDD, DIN, GND. DOUT stays free')], ['J6 „I2C“', '+3V3, GND, SDA, SCL', '14, 13', T('Reserve, frei', 'Spare, free')],
         ['MAX98357A', '+, −', '', T('Lautsprecher', 'Speaker')]], [28*mm, 34*mm, 16*mm, FW-78*mm]),
  Spacer(1, 6),
  note(T('Die Beschriftung von J2 und J3 auf der Platine ist seitenverkehrt, in der Firmware ebenso. Wer den Piezo von J2 an die linke Wand klebt, bekommt links zurück und rechts vor.', 'The labels of J2 and J3 on the board are swapped, and so are they in the firmware. Glue the piezo from J2 to the left wall and you get previous on the left and next on the right.'), 'info'),
  Spacer(1, 6),
  note(T('Die Piezos sind rückwärts gepolt: die Messingseite (meist die schwarze Ader) an BTN, die Keramikseite (meist rot) an GND. Löst ein Piezo erst beim Loslassen aus, seine Adern tauschen. Wird die Box beim Drehen nach links lauter statt leiser, die beiden äußeren Drähte am Poti tauschen.', 'The piezos are connected reversed: the brass side (usually the black wire) to BTN, the ceramic side (usually red) to GND. If a piezo only triggers when you let go, swap its wires. If the box gets louder instead of quieter when you turn left, swap the two outer wires on the potentiometer.'), 'tip')]

ENV_EXAMPLE = T(r'''[env:meinebox]
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
    -D KNOCK_MAX_GAP_US=700''', '[env:mybox]\nbuild_flags =\n    ${env.build_flags}\n    -D BOX_NAME=\\"My-Box\\"\n    -D BOX_COLOR=\\"#2f6b5a\\"\n    -D BLUETOOTH_TAG=\\"04c6d4b26e6780\\"\n    -D MANAGEMENT_SERVER_TAG=\\"042041122a6781\\"\n    -D BATTERY_ADC_LOW=1994\n    -D BATTERY_MV_LOW=3685\n    -D BATTERY_ADC_HIGH=2390\n    -D BATTERY_MV_HIGH=4322\n    -D RKS_THRESHOLD=10000\n    -D LKS_THRESHOLD=10000\n    -D KNOCK_ON_LEVEL=100\n    -D KNOCK_MIN_PRESS_MS=25\n    -D KNOCK_MAX_GAP_US=700')
envbox = Table([[codeblock(ENV_EXAMPLE, 84*mm), [P(T('Im Code stehen nur Ersatzwerte. Für eine neue Box eine bestehende Env kopieren, umbenennen und die Werte anpassen. Die Werte von Timon sind ein guter Anfang.', 'The code only holds fallback values. For a new box, copy an existing env, rename it and adjust the values. Timon’s values are a good start.')),
                                                   P(T('Zeichenketten stehen in maskierten Anführungszeichen, wie bei ', 'Strings are in escaped quotes, as with ') + code('BOX_NAME') + T('. Nach jeder Änderung neu flashen.', '. Flash again after every change.'))]]],
               colWidths=[90*mm, FW-90*mm])
envbox.setStyle(TableStyle([('VALIGN', (0, 0), (-1, -1), 'TOP'), ('LEFTPADDING', (0, 0), (-1, -1), 0), ('RIGHTPADDING', (0, 0), (-1, -1), 0)]))
story += [CondPageBreak(110*mm), P(T('5.5  Inbetriebnahme', '5.5  First start'), 'h2'),
  P(T('Firmware, Platinen und diese Anleitung liegen im Repo ', 'Firmware, boards and this guide live in the repo ') + repo() + T('. Gebaut und geflasht wird mit PlatformIO, in VS Code mit der PlatformIO-Erweiterung oder mit ', '. Building and flashing is done with PlatformIO, in VS Code with the PlatformIO extension or with ') + code('pio') + T(' auf der Kommandozeile. Am besten einmal vor dem Einbau mit offen liegender Elektronik in Betrieb nehmen und noch einmal im Gehäuse (Kapitel 6, Schritt 12).', ' on the command line. Best do a first start once before fitting, with the electronics lying open, and once more in the case (chapter 6, step 12).')),
  P(T('Werte pro Box in der platformio.ini', 'Per-box values in platformio.ini'), 'h3'),
  P(T('Alles, was sich von Box zu Box unterscheidet, steht in einer eigenen Env in der ', 'Everything that differs from box to box lives in its own env in ') + code('platformio.ini') + ':'),
  envbox, Spacer(1, 8),
  table([[T('Wert', 'Value'), T('Bedeutung', 'Meaning'), T('So findet man ihn heraus', 'How to find it')],
         [code('BOX_NAME', 7.6), T('Name im WLAN, bei Bluetooth und in der Web-UI', 'Name in Wi-Fi, in Bluetooth and in the web UI'), T('frei wählen', 'your choice')],
         [code('BOX_COLOR', 7.6), T('Farbe des Symbols im Browser-Tab', 'Colour of the icon in the browser tab'), T('frei wählen', 'your choice')],
         [code('BLUETOOTH_TAG', 7.6) + '<br/>' + code('MANAGEMENT_SERVER_TAG', 7.6), T('UIDs der beiden Sondertokens', 'UIDs of the two special tokens'), T('Token auflegen, die Konsole meldet ', 'Place the token, the console reports ') + code('tag: <uid>', 7.6) + T(' (Schritt 4)', ' (step 4)')],
         [code('BATTERY_ADC_LOW', 7.6) + '<br/>' + code('BATTERY_MV_LOW', 7.6) + '<br/>' + code('BATTERY_ADC_HIGH', 7.6) + '<br/>' + code('BATTERY_MV_HIGH', 7.6), T('Zwei Messpunkte für die Akkuanzeige: ADC-Wert und Spannung in mV', 'Two calibration points for the battery gauge: ADC value and voltage in mV'), code('battery:', 7.6) + T(' oder die Management-Seite zeigt den ADC-Wert, ein Multimeter die Spannung (Schritt 6)', ' or the management page shows the ADC value, a multimeter the voltage (step 6)')],
         [code('RKS_THRESHOLD', 7.6) + '<br/>' + code('LKS_THRESHOLD', 7.6) + '<br/>' + code('KNOCK_…', 7.6), T('Klopfsensoren', 'Knock sensors'), code('knockdebug:1', 7.6) + T(' und ', ' and ') + code('knockdebug:2', 7.6) + T(', siehe doc/development.md (Schritt 5)', ', see doc/development.md (step 5)')]],
        [50*mm, 52*mm, FW-102*mm]),
  P(T('Serielle Konsole', 'Serial console'), 'h3'),
  P(T('USB-C-Buchse an den Rechner stecken, die Box mit einem Token einschalten und ', 'Plug the USB-C socket into the computer, switch the box on with a token and start ') + code('pio device monitor -b 115200') + T(' starten, oder den Serial Monitor in VS Code. Befehle mit Doppelpunkt eintippen, etwa ', ', or the Serial Monitor in VS Code. Type commands with a colon, for example ') + code('battery:') + T(', dann Enter.', ', then Enter.')),
  P(T('Schritt für Schritt', 'Step by step'), 'h3'),
  ListFlowable([Paragraph(t, S['li']) for t in [
    T('<b>SD-Karte:</b> FAT32. Ins Wurzelverzeichnis gehören ', '<b>SD card:</b> FAT32. The root directory needs ') + code('tap.mp3') + T(' (Klick beim Klopfen) und ', ' (click when knocking) and ') + code('alert.mp3') + T(' (Warnton bei schwachem Akku), beide aus ', ' (warning sound for low battery), both from ') + code('sdcard/') + T('. Den Aufbau der Karte beschreibt architecture.md, Abschnitt 4.', '. The card layout is described in architecture.md, section 4.'),
    T('<b>Eigene Env:</b> In der ', '<b>Your own env:</b> In ') + code('platformio.ini') + T(' eine bestehende Env wie ', ', copy an existing env such as ') + code('[env:timon]') + T(' kopieren und umbenennen, ', ', rename it and adjust ') + code('BOX_NAME') + T(' und ', ' and ') + code('BOX_COLOR') + T(' anpassen. Die übrigen Werte folgen in den nächsten Schritten.', '. The other values follow in the next steps.'),
    T('<b>Flashen:</b> Die Box mit dem Mikroschalter einschalten, über USB geht sie nicht an. Dann die USB-C-Buchse an den Rechner stecken und ', '<b>Flashing:</b> Switch the box on with the microswitch, USB does not turn it on. Then plug the USB-C socket into the computer and run ') + code('~/.platformio/penv/bin/pio run -e <box> -t upload --upload-port /dev/ttyUSBn') + T(' ausführen.', '.'),
    T('<b>Sondertokens:</b> Ein Token für den Bluetooth-Modus und einer für den Management-Modus. Jeden auflegen, die Konsole meldet ', '<b>Special tokens:</b> One token for Bluetooth mode and one for management mode. Place each of them, the console reports ') + code('tag: <uid>') + T('. Die UIDs als ', '. Enter the UIDs as ') + code('BLUETOOTH_TAG') + T(' und ', ' and ') + code('MANAGEMENT_SERVER_TAG') + T(' in die Env eintragen und neu flashen. Jede andere UID ist ein Album.', ' in the env and flash again. Every other UID is an album.'),
    T('<b>Klopfsensoren abstimmen:</b> ', '<b>Tune the knock sensors:</b> ') + code('knockdebug:1') + T(' zeigt pro Sekunde Pegel und Flächen, ', ' shows levels and areas every second, ') + code('knockdebug:2') + T(' zusätzlich den Verlauf des nächsten Drucks. Die Schwellen in der Env so setzen, dass ein Druck sicher auslöst und laute Musik nicht (doc/development.md). Neu flashen.', ' also traces the next press. Set the thresholds in the env so that a press triggers reliably and loud music does not (doc/development.md). Flash again.'),
    T('<b>Akkuanzeige kalibrieren:</b> ESP32-ADCs streuen. Die Box rechnet den ADC-Wert über zwei Messpunkte in Millivolt um. Jeder Punkt ist ein ADC-Wert und die Spannung, die ein Multimeter im selben Moment zwischen ', '<b>Calibrate the battery gauge:</b> ESP32 ADCs vary. The box converts the ADC value to millivolts using two calibration points. Each point is an ADC value plus the voltage a multimeter measures at the same moment between ') + code('Vcc') + T(' und ', ' and ') + code('GND') + T(' an J1 der Latch misst. ', ' on J1 of the latch. ') +
    T('Am USB zeigt ', 'On USB, ') + code('battery:') + T(' den ADC-Wert, er und die Spannung kommen in ', ' shows the ADC value; it and the voltage go into ') + code('BATTERY_ADC_HIGH') + T(' und ', ' and ') + code('BATTERY_MV_HIGH') + '. ' +
    T('Im Akkubetrieb ohne USB, am besten mit halb leerem Akku, den Management-Token auflegen: Die Management-Seite zeigt unter „Battery“ den ADC-Wert, er und die Spannung kommen in ', 'On battery without USB, ideally with a half-empty battery, place the management token: the management page shows the ADC value under “Battery”; it and the voltage go into ') + code('BATTERY_ADC_LOW') + T(' und ', ' and ') + code('BATTERY_MV_LOW') + '. ' +
    T('Neu flashen. ', 'Flash again. ') + code('battery:') + T(' zeigt danach Spannung und Prozent, die Spannung sollte zum Multimeter passen.', ' then shows voltage and percentage, the voltage should match the multimeter.')]],
    bulletType='1', leftIndent=14, bulletFontName='Plex-SB', bulletFontSize=8.6, bulletColor=ACCENT),
  Spacer(1, 6),
  note(T('Steckt USB, misst die Box die USB-Spannung statt des Akkus, die Akkuanzeige steht dann auf voll. Am USB-Netzteil bleibt die Box außerdem an, auch wenn die Firmware HOLD abschaltet.', 'With USB plugged in, the box measures the USB voltage instead of the battery, so the battery gauge shows full. On a USB charger the box also stays on, even if the firmware switches HOLD off.'), 'info'),
  CondPageBreak(170*mm)]

story += [H1(6, T('Zusammenbau', 'Assembly')),
  P(T('Erst werden die Teile vorbereitet, dann Rahmen und Boden verklebt und alles eingebaut, was an den Wänden sitzt. Der Deckel wird separat bestückt und erst zum Schluss aufgeklebt. Bis dahin kommt man von oben an alles, danach nur noch durch die Bodenklappe.', 'First the parts are prepared, then frame and bottom are glued together and everything that sits on the walls is fitted. The lid is fitted out separately and glued on at the very end. Until then you can reach everything from above, afterwards only through the bottom hatch.')),
  ChipRow(['CA','EP','UV','HG','TB'], label=T('Farben in den Schritten:', 'Colours in the steps:')), Spacer(1, 10)]
story += [KeepTogether([P(T('6.1  Vorbereiten', '6.1  Preparation'), 'h2'), fig('inserts', [dict(a='ins0', t=T('Gewindeeinsatz M3', 'Heat-set insert M3'), s=T('4 Stück', '4 pieces'), side='L'),
     dict(a='hole1', t=T('Bohrung Ø 4,0 mm', 'hole Ø 4.0 mm'), s=T('4,5 mm tief, durchgehend', '4.5 mm deep, through'), side='R'), dict(a='ledge', t=T('Auflage der Klappe', 'seat of the hatch'), side='R')],
     arrows=[('ins0','hole0'),('ins1','hole1'),('ins2','hole2'),('ins3','hole3')], frac=0.6), cap(pn('back') + T(' von unten, die Einsätze kommen von der Seite der Klappe.', ' from below, the inserts go in from the hatch side.'))]),
  step(1, T('Teile vorbereiten', 'Prepare the parts'), ['SL'], [
    T('Stützen entfernen, Löcher entgraten. Gitter, Token und Klappe probehalber einsetzen: Das Gitter muss ohne Kraft in die Öffnung passen.', 'Remove supports, deburr holes. Test-fit grill, token and hatch: the grill has to go into the opening without force.'),
    T('Wer lackieren will, schleift und lackiert ', 'If you want to paint, sand and paint ') + pn('back') + ', ' + pn('frame') + ', ' + pn('front') + T(' und die Tasten jetzt. Die Klebeflächen dabei abkleben und blank lassen, Sekundenkleber hält auf blankem Kunststoff besser als auf Lack.', ' and the buttons now. Mask the glue surfaces and leave them bare, super glue holds better on bare plastic than on paint.')]),
  step(2, T('Gewindeeinsätze einschmelzen', 'Melt in the heat-set inserts'), ['LK'], [
    T('Die vier M3-Einsätze von unten in die Bohrungen in ', 'Put the four M3 inserts into the holes in ') + pn('back') + T(' setzen, also von der Seite, an der später die Klappe anliegt.', ' from below, that is from the side the hatch will rest against later.'),
    T('Mit dem Lötkolben gerade einschmelzen, bis sie bündig mit der Auflagefläche sind. Den Lötkolben etwa auf Drucktemperatur des Materials stellen.', 'Melt them in straight with the soldering iron until they are flush with the seating surface. Set the iron to roughly the printing temperature of the material.')])]
story += [CondPageBreak(120*mm), KeepTogether([P(T('6.2  Unterteil', '6.2  Lower part'), 'h2'), fig('shell', [dict(a='front', t='front', s=T('erst in Schritt 12', 'only in step 12'), side='R'),
     dict(a='frame_top', t=T('Klebekante oben', 'glue edge at the top'), s=T('für den Deckel', 'for the lid'), side='R'), dict(a='frame', t='frame', side='R'), dict(a='back_glue', t=T('Klebefläche back', 'glue surface back'), side='R'),
     dict(a='back', t='back', s=T('Einsätze nach unten', 'inserts facing down'), side='R'), dict(a='prev', t='prev button', s=T('links', 'left'), side='L'), dict(a='next', t='next button', s=T('rechts', 'right'), side='R')],
     arrows=[('prev_from','prev_to')], frac=0.56), cap(T('Klebeflächen orange. Die Tasten kleben außen auf der Seitenwand.', 'Glue surfaces in orange. The buttons are glued to the outside of the side wall.'))]),
  step(3, T('Tasten aufkleben', 'Glue on the buttons'), ['CA'], [
    pn('prev button') + T(' auf die linke, ', ' onto the left, ') + pn('next button') + T(' auf die rechte Seitenwand kleben, von vorn gesehen. Beide sitzen mittig unter der Lasche, die Mitte 31 mm über der Unterkante des Rahmens.', ' onto the right side wall, seen from the front. Both sit centred under the lug, their centre 31 mm above the bottom edge of the frame.'),
    T('Das Symbol waagerecht ausrichten. Kleber sparsam auftragen, er darf nicht aus den Aussparungen des Symbols quellen.', 'Align the symbol horizontally. Apply glue sparingly, it must not ooze out of the symbol’s cutouts.')]),
  step(4, T('Boden an den Rahmen kleben', 'Glue the bottom to the frame'), ['CA'], [
    pn('back') + T(' mit den Einsätzen nach unten an die Unterkante von ', ' with the inserts facing down against the bottom edge of ') + pn('frame') + T(' setzen. Die gewölbte Seite gehört nach hinten, zur USB-C-Buchse.', '. The curved side goes to the back, towards the USB-C socket.'),
    T('Auf einer ebenen Fläche verkleben und die Kanten bündig ausrichten. Den Deckel noch nicht aufkleben.', 'Glue them on a flat surface and align the edges flush. Do not glue on the lid yet.')])]
story += [CondPageBreak(120*mm), KeepTogether([P(T('6.3  Wände', '6.3  Walls'), 'h2'), fig('inside', [dict(a='piezo', t='Piezo', s=T('Messingseite zur Wand', 'brass side to the wall'), side='L'),
     dict(a='wall', t=T('Wand 1 mm', 'wall 1 mm'), s=T('Klebeband', 'Tape'), side='L'), dict(a='usb', t=T('USB-C-Platine', 'USB-C board'), s=T('UV-Lack', 'UV resin'), side='R'),
     dict(a='speaker', t=T('Lautsprecher', 'Speaker'), s=T('2K-Kleber auf dem Ring', '2-part epoxy on the ring'), side='L'), dict(a='grill', t=T('Gitter mit Stoff', 'Grill with fabric'), s=T('Sekundenkleber', 'Super glue'), side='L')], frac=0.6),
     cap(T('Rahmen im Schnitt: was innen an den Wänden sitzt.', 'Frame in section: what sits on the inside of the walls.'))]),
  step(5, T('Lautsprechergitter mit Stoff beziehen', 'Cover the speaker grill with fabric'), ['CA'], [
    T('Ein Stück dünnen Stoff, etwa eine Socke, straff über die Außenseite des Gitters ziehen.', 'Pull a piece of thin fabric, a sock for example, tight over the outside of the grill.'),
    T('Das Gitter von außen in die Öffnung drücken. Der Stoff wird im Spalt zwischen Rand (Ø 54,6 mm) und Öffnung (Ø 55 mm) eingeklemmt.', 'Press the grill into the opening from the outside. The fabric gets clamped in the gap between rim (Ø 54.6 mm) and opening (Ø 55 mm).'),
    T('Von innen mit Sekundenkleber fixieren und den überstehenden Stoff innen abschneiden.', 'Fix it from the inside with super glue and cut off the excess fabric on the inside.')],
    [('tip', T('Kleber nur innen auftragen. Sekundenkleber zieht in Stoff ein, wird hart und hinterlässt helle Flecken.', 'Only apply glue on the inside. Super glue soaks into fabric, hardens and leaves pale stains.'))]),
  step(6, T('Lautsprecher einkleben', 'Glue in the speaker'), ['EP'], [
    T('Die Kabel vorher anlöten, lang genug bis zur Hauptplatine unter dem Deckel.', 'Solder the wires on first, long enough to reach the main board under the lid.'),
    T('Den Lautsprecher von innen mit 2K-Kleber auf das Gitter kleben. Mit 50–57 mm Durchmesser liegt er mit seinem Rand auf dem Ring an der Innenseite des Gitters, ein kleinerer sitzt im Ring direkt am Gitter.', 'Glue the speaker onto the grill from the inside with 2-part epoxy. At 50–57 mm diameter its rim rests on the ring on the inside of the grill, a smaller one sits inside the ring right at the grill.'),
    T('Kleber nur auf den Rand, nicht auf die Membran. Aushärten lassen.', 'Glue only on the rim, not on the cone. Let it cure.')]),
  step(7, T('Piezos ankleben', 'Stick on the piezos'), ['TB'], [
    T('Je eine Piezo-Scheibe mit dünnem doppelseitigem Klebeband von innen auf die gedünnte Wand hinter ', 'Stick one piezo disc each with thin double-sided tape from the inside onto the thinned wall behind ') + pn('prev button') + T(' und ', ' and ') + pn('next button') + T(' kleben, mittig hinter der Taste, Messingseite zur Wand.', ', centred behind the button, brass side to the wall.'),
    T('Die Kabel beschriften: links ist „zurück“ (J2), rechts „vor“ (J3), siehe Kapitel 5.4.', 'Label the wires: left is “previous” (J2), right is “next” (J3), see chapter 5.4.')],
    [('tip', T('Dünnes, festes Klebeband überträgt den Druck besser als dickes Schaumklebeband. Die Schwellen werden in Schritt 12 abgestimmt.', 'Thin, firm tape transfers the press better than thick foam tape. The thresholds get tuned in step 12.'))]),
  step(8, T('USB-C-Buchse einkleben', 'Glue in the USB-C socket'), ['UV'], [
    T('Die Kabel vorher anlöten: VBUS und GND zum TP4056, D+ und D− zum Kabel zur Hauptplatine (Kapitel 5.4).', 'Solder the wires on first: VBUS and GND to the TP4056, D+ and D− to the cable to the main board (chapter 5.4).'),
    T('Die Platine von innen in den Ausschnitt unten in der Rückwand setzen, die Buchse bündig mit der Außenseite.', 'Put the board into the cutout at the bottom of the back wall from the inside, the socket flush with the outside.'),
    T('Ein USB-C-Kabel einstecken, damit die Buchse gerade sitzt. Dann mit UV-Lack von innen fixieren und aushärten. Kein Lack in die Buchse.', 'Plug in a USB-C cable so the socket sits straight. Then fix it from the inside with UV resin and cure it. No resin in the socket.')])]
story += [CondPageBreak(120*mm), KeepTogether([P(T('6.4  Deckel', '6.4  Lid'), 'h2'), fig('lid', [dict(a='led', t=T('LED Ø 8 mm', 'LED Ø 8 mm'), s=T('UV-Lack', 'UV resin'), side='R'),
     dict(a='poti', t=T('Poti Ø 7 mm', 'Potentiometer Ø 7 mm'), s=T('Mutter von außen', 'nut from outside'), side='L'), dict(a='jack', t=T('Kopfhörerbuchse', 'Headphone jack'), s=T('Tasche 12 × 7 mm\nBohrung bleibt zu', 'Pocket 12 × 7 mm\nhole stays closed'), side='L'),
     dict(a='switch', t=T('Mikroschalter', 'Microswitch'), s=T('Hebel durch den Schlitz\nUV-Lack', 'Lever through the slot\nUV resin'), side='L'), dict(a='cup', t=T('RC522 hier aufkleben', 'stick RC522 here'), s=T('Muldenboden 1 mm', 'cup floor 1 mm'), side='R'),
     dict(a='skirt', t=T('Klebekante', 'Glue edge'), s=T('zum Rahmen', 'to the frame'), side='R')], frac=0.6),
     cap(pn('front') + T(' von innen. Blau: Fläche für das RC522.', ' from the inside. Blue: area for the RC522.'))]),
  step(9, T('Deckel bestücken', 'Fit out the lid'), ['UV', 'TB'], [
    T('<b>LED:</b> Die PL9823 von innen bis zum Kragen in die 8-mm-Bohrung vorn links stecken und mit UV-Lack fixieren.', '<b>LED:</b> Push the PL9823 from the inside up to its collar into the 8 mm hole at the front left and fix it with UV resin.'),
    T('<b>Poti:</b> Von innen durch die 7-mm-Bohrung hinten links stecken, außen Scheibe und Mutter. Den Verdrehschutz-Nocken am Poti abbrechen, falls vorhanden; der Deckel hat dafür kein Loch. Die Achse so kürzen, dass sie 8,5 mm über die Deckeloberfläche ragt, dann sitzt der Knopf mit 0,5 mm Luft. Den Knopf ', '<b>Potentiometer:</b> Push it from the inside through the 7 mm hole at the back left, washer and nut on the outside. Break off the anti-rotation tab on the potentiometer if there is one; the lid has no hole for it. Cut the shaft so that it sticks out 8.5 mm above the lid surface, then the knob sits with 0.5 mm clearance. Push on the knob ') + pn('volume') + T(' aufstecken.', '.'),
    T('<b>Mikroschalter:</b> In die Tasche an der Rückseite der Token-Mulde setzen, sodass der Hebel durch den Schlitz in die Mulde ragt. Prüfen, dass ein aufgelegter Token ihn hörbar drückt, dann mit UV-Lack fixieren und aushärten. Lack nur ans Gehäuse des Schalters, nicht an Hebel und Stößel.', '<b>Microswitch:</b> Put it into the pocket at the back of the token cup so that the lever reaches through the slot into the cup. Check that a placed token presses it audibly, then fix it with UV resin and cure. Resin only on the switch body, not on lever and plunger.'),
    T('<b>RC522:</b> Mit dünnem doppelseitigem Klebeband von innen unter die Token-Mulde kleben, flache Rückseite nach oben, Antenne mittig unter der Mulde. Die Stiftleiste zeigt nach unten, dort steckt später die Hauptplatine.', '<b>RC522:</b> Stick it under the token cup from the inside with thin double-sided tape, flat back facing up, antenna centred under the cup. The pin header points down, that is where the main board plugs in later.'),
    T('<b>Kopfhörerbuchse:</b> Bleibt vorerst leer, die Bohrung hinten rechts bleibt verschlossen. Für eine spätere Buchse ist innen eine Tasche; dann die 0,2-mm-Schicht mit 6 mm aufbohren.', '<b>Headphone jack:</b> Stays empty for now, the hole at the back right stays closed. There is a pocket on the inside for a later jack; then drill through the 0.2 mm layer with 6 mm.')]),
  KeepTogether([fig('lid_full', [dict(a='rc522', t='RC522', s=T('flache Rückseite an der Mulde', 'flat back against the cup')), dict(a='header', t=T('Stiftleiste', 'Pin header'), s=T('für die Hauptplatine', 'for the main board')),
     dict(a='led', t='LED', s=T('UV-Lack', 'UV resin')), dict(a='poti', t=T('Poti', 'Potentiometer'), s=T('Mutter von außen', 'nut from outside')), dict(a='switch', t=T('Mikroschalter', 'Microswitch'), s=T('UV-Lack', 'UV resin'))], frac=0.58),
     cap(T('Deckel nach Schritt 9. Die Hauptplatine kommt in Schritt 11 auf die Stiftleiste des RC522.', 'Lid after step 9. The main board goes onto the RC522’s pin header in step 11.'))])]
story += [CondPageBreak(110*mm), KeepTogether([P(T('6.5  Elektronik und Akkus', '6.5  Electronics and cells'), 'h2'), LightPipeDiagram(),
     cap(T('Schnitt durch die Rückwand an der USB-C-Buchse: Das Filament führt das Licht der Lade-LED nach außen und darf gebogen verlaufen.', 'Section through the back wall at the USB-C socket: the filament carries the light of the charge LED to the outside and may run in a bend.'))]),
  step(10, T('Ladeschaltung und Step-up einkleben', 'Glue in the charging board and the step-up'), ['EP'], [
    T('Die Ladeschaltung, also die Power Latch mit dem TP4056-Modul auf der Rückseite, mit 2K-Kleber innen an die Wand kleben, in die Nähe der USB-C-Buchse. Heißkleber reicht hier nicht: Der Laderegler wird beim Laden sehr warm, und der Heißkleber kann sich dann lösen.', 'Glue the charging board, that is the power latch with the TP4056 module on its back, to the inside of the wall with 2-part epoxy, close to the USB-C socket. Hot glue is not enough here: the charge controller gets very warm while charging, and hot glue can come loose then.'),
    T('Schnell härtenden 2K-Kleber nehmen und die Platine halten, bis er anzieht. Kein Kleber auf die Lade-LED.', 'Use fast-curing 2-part epoxy and hold the board until it grabs. No glue on the charge LED.'),
    T('Ein Stück transparentes Filament (1,75 mm) von außen durch das 2-mm-Loch über der USB-C-Buchse stecken und innen bis an die Lade-LED führen. Es darf länger sein und in einem sanften Bogen verlaufen, so ist man bei der Lage der Ladeschaltung frei. Außen bündig abschneiden. Hält es nicht von selbst, mit einem Tropfen UV-Lack sichern.', 'Push a piece of clear filament (1.75 mm) from the outside through the 2 mm hole above the USB-C socket and guide it on the inside up to the charge LED. It may be longer and run in a gentle bend, so you are free in placing the charging board. Cut it flush on the outside. If it does not hold by itself, secure it with a drop of UV resin.'),
    T('Den Step-up-Konverter ebenfalls mit 2K-Kleber an eine freie Stelle der Wandinnenseite kleben, nicht auf die gedünnten Tastenflächen. Auch er wird unter Last warm.', 'Glue the step-up converter to a free spot on the inside of the wall with 2-part epoxy as well, not on the thinned button areas. It gets warm under load too.')],
    [('tip', T('Beim Laden leuchtet das Filament neben der USB-C-Buchse in der Farbe der LED: je nach Modul rot beim Laden, blau oder grün, wenn der Akku voll ist.', 'While charging, the filament next to the USB-C socket glows in the colour of the LED: depending on the module red while charging, blue or green when the battery is full.'))]),
  CondPageBreak(110*mm), KeepTogether([fig('akkus', [
     dict(a='cellA', t=T('18650 vorn links', '18650 front left'), s=T('stehend, Pluspol oben', 'upright, plus at the top')), dict(a='cellB', t=T('18650 hinten rechts', '18650 back right'), s=T('diagonal gegenüber', 'diagonally opposite')),
     dict(a='kapton', t=T('Kapton über der Lötstelle', 'Kapton over the solder joint')), dict(a='red', t=T('Plus, rot', 'Plus, red'), s=T('zu B+ am TP4056', 'to B+ on the TP4056')), dict(a='black', t=T('Minus, schwarz', 'Minus, black'), s=T('zu B− am TP4056', 'to B− on the TP4056')),
     dict(a='window', t=T('Tastenwand', 'Button wall'), s=T('kein Kleber', 'no glue')), dict(a='usb', t='USB-C'), dict(a='latch', t=T('Ladeschaltung', 'Charging board'), s=T('2K-Kleber', '2-part epoxy')), dict(a='boost', t='Step-up', s=T('2K-Kleber', '2-part epoxy'))], frac=0.6),
     cap(T('Akkus stehend in zwei diagonal gegenüberliegenden Ecken, hier vorn links und hinten rechts. An der Rückwand Ladeschaltung und Step-up. Rahmen aufgeschnitten, Deckel noch nicht aufgeklebt.', 'Cells upright in two diagonally opposite corners, here front left and back right. On the back wall the charging board and the step-up. Frame cut open, lid not glued on yet.'))]),
  step(11, T('Elektronik verdrahten und Akkus einkleben', 'Wire up the electronics and glue in the cells'), ['EL', 'HG'], [
    T('Die Hauptplatine von unten auf die Stiftleiste des RC522 stecken und am freien Ende den Abstandshalter einclipsen (Kapitel 5.2, Schritt E6).', 'Plug the main board from below onto the RC522’s pin header and clip the spacer in at the free end (chapter 5.2, step E6).'),
    T('Zur Sicherheit einen Kabelbinder um Hauptplatine, Abstandshalter und RC522 legen, am Ende gegenüber der Stiftleiste. Dort ist zwischen RC522 und Deckel Platz, denn die Token-Mulde trägt das RC522 nur in der Mitte.', 'To be safe, put a cable tie around main board, spacer and RC522, at the end opposite the pin header. There is room between RC522 and lid there, because the token cup only carries the RC522 in the middle.'),
    T('Alles nach den Tabellen in Kapitel 5.4 verdrahten: Kabel zu J4, Lautsprecher an den MAX98357A, Piezos an J2 und J3, Poti an J1, LED an J5, Mikroschalter mit C an Trigger + und NO an Trigger In der Latch.', 'Wire everything following the tables in chapter 5.4: cable to J4, speaker to the MAX98357A, piezos to J2 and J3, potentiometer to J1, LED to J5, microswitch with C to Trigger + and NO to Trigger In of the latch.'),
    T('Die beiden 18650-Zellen stehend in zwei diagonal gegenüberliegende Ecken setzen, Pluspol oben, zum Beispiel vorn links und hinten rechts. Hinten links sitzt direkt darüber das Poti.', 'Stand the two 18650 cells upright in two diagonally opposite corners, plus at the top, for example front left and back right. At the back left the potentiometer sits right above.'),
    T('Die Zellen stehen auf dem Boden direkt über den Gewindeeinsätzen und reichen bis knapp unter den Deckel. Einfache Zellen ohne Schutzschaltung (65 mm) passen, die Schutzschaltung sitzt auf dem TP4056-Modul.', 'The cells stand on the bottom right above the heat-set inserts and reach to just below the lid. Plain cells without protection circuit (65 mm) fit, the protection circuit sits on the TP4056 module.'),
    T('Mit Heißkleber in den Spalten zwischen Zelle und Ecke fixieren. Kein Kleber auf die gedünnten Wandflächen der Tasten, er dämpft sonst die Piezos.', 'Fix them with hot glue in the gaps between cell and corner. No glue on the thinned wall areas of the buttons, it would damp the piezos.'),
    T('Parallel schalten, Plus an Plus und Minus an Minus, und an B+ und B− des TP4056 anschließen. Alle Lötstellen mit Kapton-Band oder Schrumpfschlauch isolieren.', 'Wire them in parallel, plus to plus and minus to minus, and connect them to B+ and B− of the TP4056. Insulate all solder joints with Kapton tape or heat-shrink tubing.')],
    [('warn', T('Vor dem Zusammenschalten beide Zellen auf gleiche Spannung bringen (höchstens etwa 0,1 V Unterschied), sonst fließt ein hoher Ausgleichsstrom. Nur gleiche Zellen verwenden, Zellen mit Lötfahnen nehmen und nicht direkt an die Zelle löten. Nichts Blankes darf Schrauben, Einsätze oder andere Bauteile berühren.', 'Before connecting them, bring both cells to the same voltage (at most about 0.1 V difference), otherwise a high equalising current flows. Only use identical cells, take cells with solder tabs and do not solder to the cell directly. Nothing bare may touch screws, inserts or other parts.'))])]
story += [KeepTogether([fig('lid_stack', [dict(a='tie', t=T('Kabelbinder', 'Cable tie'), s=T('am freien Ende', 'at the free end')), dict(a='spacer', t=T('Abstandshalter', 'Spacer')), dict(a='mainboard', t=T('Hauptplatine', 'Main board')),
     dict(a='rc522', t='RC522', s=T('an der Token-Mulde', 'at the token cup')), dict(a='header', t=T('Stiftleiste', 'Pin header'))], frac=0.56),
     cap(T('Deckel mit Elektronik von innen. Gegenüber der Stiftleiste halten Abstandshalter und Kabelbinder die Hauptplatine.', 'Lid with electronics from the inside. Opposite the pin header, spacer and cable tie hold the main board.'))])]
left = [fig('akku_detail', [dict(a='glue1', t=T('Heißkleber', 'Hot glue'), s=T('in beiden Spalten zur Wand', 'in both gaps to the wall')), dict(a='kapton', t='Kapton'),
        dict(a='cell', t=T('Zelle in der Ecke', 'Cell in the corner')), dict(a='window', t=T('Tastenwand', 'Button wall'), s=T('hier kein Kleber', 'no glue here'))], frac=0.58, width=88*mm, place='left', gap=7),
        cap(T('Ecke vorn links von oben.', 'Front left corner from above.'))]
right = [Spacer(1, 6), ParallelDiagram(), Spacer(1, 4), cap(T('Parallelschaltung: Plus an Plus, Minus an Minus.', 'Parallel wiring: plus to plus, minus to minus.'))]
pair = Table([[left, right]], colWidths=[88*mm, FW-88*mm])
pair.setStyle(TableStyle([('VALIGN',(0,0),(-1,-1),'TOP'), ('LEFTPADDING',(0,0),(-1,-1),0), ('RIGHTPADDING',(0,0),(-1,-1),0)]))
story += [KeepTogether([pair])]
story += [CondPageBreak(120*mm), KeepTogether([P(T('6.6  Abschluss', '6.6  Finishing'), 'h2'), fig('lidopen', [dict(a='mainboard', t=T('Hauptplatine', 'Main board'), s=T('steckt auf dem RC522', 'plugged onto the RC522')),
     dict(a='modules', t=T('Verstärker und SD-Modul', 'Amplifier and SD module')), dict(a='rc522', t='RC522', s=T('unter der Token-Mulde', 'under the token cup')), dict(a='cable', t=T('7-poliges Kabel', '7-pin cable'), s=T('nach innen legen', 'tuck inside')),
     dict(a='latch', t=T('Ladeschaltung', 'Charging board')), dict(a='boost', t='Step-up'), dict(a='cellB', t=T('Akkus', 'Cells'))], frac=0.5),
     cap(T('Vor dem Aufkleben: Der Deckel trägt RC522, Hauptplatine mit Modulen, LED, Poti und Mikroschalter. Das 7-polige Kabel verbindet ihn mit der Stromseite.', 'Before gluing: the lid carries the RC522, the main board with its modules, LED, potentiometer and microswitch. The 7-pin cable connects it to the power side.'))]),
  step(12, T('Testen, dann Deckel aufkleben', 'Test, then glue on the lid'), ['CA'], [
    T('Vor dem Verkleben in Betrieb nehmen, wie in Kapitel 5.5 beschrieben: Token auflegen, Box startet, flashen, Lautstärke, LED und Akkuanzeige prüfen.', 'Start it up before gluing, as described in chapter 5.5: place a token, box starts, flash, check volume, LED and battery gauge.'),
    T('Die Klopfsensoren mit <font name="Mono" size="8.3">knockdebug:1</font> und <font name="Mono" size="8.3">knockdebug:2</font> abstimmen (doc/development.md). Die Schwellen stehen pro Box in der <font name="Mono" size="8.3">platformio.ini</font>, eine neue Box bekommt dort eine eigene Env.', 'Tune the knock sensors with <font name="Mono" size="8.3">knockdebug:1</font> and <font name="Mono" size="8.3">knockdebug:2</font> (doc/development.md). The thresholds are set per box in <font name="Mono" size="8.3">platformio.ini</font>, a new box gets its own env there.'),
    T('Die Kabel nach innen legen, auf der Klebekante darf nichts liegen. Den Deckel mit Sekundenkleber auf den Rahmen kleben, die gewölbte Seite nach hinten. Danach die Klopfsensoren kurz gegenprüfen.', 'Tuck the wires inside, nothing may lie on the glue edge. Glue the lid onto the frame with super glue, the curved side to the back. Then quickly re-check the knock sensors.')]),
  KeepTogether([fig('door', [dict(a='back', t='back', s=T('mit Einschmelzgewinden', 'with heat-set inserts'), side='R'),
     dict(a='door', t='door', s=T('Bodenklappe', 'Bottom hatch'), side='R'), dict(a='bumper_glue', t='bumper (TPU)', s=T('Klebefläche zur Klappe', 'glue surface to the hatch'), side='L'),
     dict(a='scr0', t='4 × M3 × 5', s='ISO 4762', side='L')], arrows=[('scr0','dhole0'),('scr1','dhole1'),('scr3','dhole3')], frac=0.56),
     cap(T('Bodenklappe mit Bumper und Schrauben. Orange: Klebefläche.', 'Bottom hatch with bumper and screws. Orange: glue surface.'))]),
  step(13, T('Bodenklappe anschrauben', 'Screw on the bottom hatch'), ['CA'], [
    T('Den Bumper mit Sekundenkleber unter die Klappe kleben. Zum Ausrichten zwei Schrauben durch Bumper und Klappe stecken.', 'Glue the bumper under the hatch with super glue. To align it, push two screws through bumper and hatch.'),
    T('Die Klappe mit vier Schrauben M3 × 5 anschrauben. Die Köpfe versinken im Bumper. Durch die Klappe kommt man wieder an die Elektronik.', 'Screw the hatch on with four M3 × 5 screws. The heads sink into the bumper. Through the hatch you can reach the electronics again.')],
    [('warn', T('Keine längeren Schrauben als M3 × 5 verwenden. In zwei Ecken steht direkt über dem Einsatz ein Akku.', 'Do not use screws longer than M3 × 5. In two corners a cell sits right above the insert.'))]),
  KeepTogether([fig('strap', [dict(a='apex', t=T('Rolladengurt', 'Roller shutter strap'), s=T('ca. 15 mm breit, ca. 30 cm', 'about 15 mm wide, about 30 cm')), dict(a='fold', t=T('Ende umgeschlagen', 'end folded over'), s=T('und vernäht', 'and sewn')),
     dict(a='loop', t=T('Lasche', 'Lug'), s=T('Öffnung 20 × 3 mm', 'opening 20 × 3 mm'))], frac=0.5), cap(T('Zum Tragen: ein Stück Rolladengurt durch beide Laschen.', 'For carrying: a piece of roller shutter strap through both lugs.'))]),
  step(14, T('Tragegurt anbringen', 'Fit the carrying strap'), [(T('Nadel und Faden', 'Needle and thread'), '#9aa5a0')], [
    T('Etwa 30 cm Rolladengurt aus dem Baumarkt nehmen, ca. 15 mm breit.', 'Take about 30 cm of roller shutter strap from the hardware store, about 15 mm wide.'),
    T('Je ein Ende durch eine Lasche führen, umschlagen und festnähen, zum Beispiel mit einem Kästchen mit Kreuz.', 'Pass each end through a lug, fold it over and sew it down, for example with a box stitch with a cross.'),
    T('Dann lässt sich die Box am Gurt tragen.', 'Now the box can be carried by the strap.')],
    [('tip', T('Die Schnittkanten kurz mit dem Feuerzeug anschmelzen, dann franst der Gurt nicht aus.', 'Briefly melt the cut edges with a lighter, then the strap does not fray.'))]),
  CondPageBreak(150*mm)]

# 6 Maße
story += [H1(7, T('Maße und Schnittstellen', 'Dimensions and interfaces')), P(T('Alle Werte aus dem STEP-Modell, in Millimetern. Pfade beziehen sich auf das Repo ', 'All values from the STEP model, in millimetres. Paths refer to the repo ') + repo() + '.'),
  table([[T('Stelle', 'Location'), T('Maß', 'Dimension')],
   [T('Außen', 'Outside'), T('94 × 90 mm, mit Laschen 102 mm breit; Höhe mit Bumper hinten 94,5 mm, vorn 87 mm', '94 × 90 mm, 102 mm wide with lugs; height with bumper 94.5 mm at the back, 87 mm at the front')],
   [T('Deckel', 'Lid'), T('5° nach vorn geneigt, 3 mm Deckplatte, 8 mm Rand', 'tilted forward by 5°, 3 mm top plate, 8 mm rim')],
   [T('Wände', 'Walls'), T('3 mm, an den Tasten auf einer Fläche von 56 × 50 mm auf 1 mm gedünnt', '3 mm, thinned to 1 mm over an area of 56 × 50 mm at the buttons')],
   [T('Laschen', 'Lugs'), T('Öffnung 20 × 3 mm, für Gurte bis knapp 20 mm Breite', 'opening 20 × 3 mm, for straps up to just under 20 mm wide')],
   [T('Token-Mulde', 'Token cup'), T('Ø 40 mm, 10 mm tief, Boden 1 mm', 'Ø 40 mm, 10 mm deep, floor 1 mm')],
   ['Token', T('Körper Ø 39,7 mm, Rand Ø 44 mm, 15 mm hoch; NFC-Tasche Ø 26 × 0,4 mm, 0,2 mm über der Unterseite', 'body Ø 39.7 mm, rim Ø 44 mm, 15 mm tall; NFC pocket Ø 26 × 0.4 mm, 0.2 mm above the bottom face')],
   ['LED', T('Bohrung Ø 8 mm', 'hole Ø 8 mm')],
   [T('Poti und Knopf', 'Potentiometer and knob'), T('Bohrung Ø 7 mm; Knopf Ø 14 × 9 mm, Achsloch Ø 6,2 mm (8 mm tief ab Unterkante), Aussparung Ø 12 × 3 mm', 'hole Ø 7 mm; knob Ø 14 × 9 mm, shaft hole Ø 6.2 mm (8 mm deep from the bottom edge), recess Ø 12 × 3 mm')],
   [T('Kopfhörerbuchse', 'Headphone jack'), T('Bohrung Ø 6 mm, oben mit 0,2 mm verschlossen; Tasche 12 × 7 × 2 mm', 'hole Ø 6 mm, closed at the top by 0.2 mm; pocket 12 × 7 × 2 mm')],
   ['USB-C', T('Ausschnitt 9,2 × 3,5 mm, Ecken R 1,5; Mitte 5 mm darüber ein Loch Ø 2 mm für den Lichtleiter', 'cutout 9.2 × 3.5 mm, corners R 1.5; 5 mm above its centre a Ø 2 mm hole for the light pipe')],
   [T('Lautsprecher', 'Speaker'), T('Öffnung Ø 55 mm; Gitter mit Rand Ø 54,6 mm, Auflagering Ø 48,6–54,6 mm, steht 3 mm vor', 'opening Ø 55 mm; grill with rim Ø 54.6 mm, seating ring Ø 48.6–54.6 mm, protrudes 3 mm')],
   [T('Tasten', 'Buttons'), T('Ø 20 × 1 mm, Mitte 31 mm über der Unterkante des Rahmens', 'Ø 20 × 1 mm, centre 31 mm above the bottom edge of the frame')],
   [T('Gewindeeinsätze', 'Heat-set inserts'), T('4 Bohrungen Ø 4,0 mm, 4,5 mm tief, durchgehend', '4 holes Ø 4.0 mm, 4.5 mm deep, through')],
   [T('Klappe', 'Hatch'), T('2,9 mm dick, Senkung Ø 5,6 × 2 mm, Durchgang Ø 3,1 mm', '2.9 mm thick, counterbore Ø 5.6 × 2 mm, clearance hole Ø 3.1 mm')],
   ['Bumper', T('2 mm, TPU', '2 mm, TPU')],
   [T('Abstandshalter', 'Spacer'), T('44,2 × 16,8 × 10 mm; zwischen den Armen 41,2 mm breit, Rastnasen auf 39,9 mm', '44.2 × 16.8 × 10 mm; 41.2 mm wide between the arms, snap hooks at 39.9 mm')],
   [T('Platinenstapel', 'Board stack'), T('etwa 11 mm zwischen Hauptplatine und RC522: Buchsenleiste 8,5 mm und Kunststoff der Stiftleiste 2,5 mm', 'about 11 mm between main board and RC522: female header 8.5 mm and the plastic of the pin header 2.5 mm')]], [36*mm, FW-36*mm]),
  P(T('Dateien', 'Files'), 'h2'),
  table([[T('Pfad', 'Path'), T('Inhalt', 'Contents')], ['<font name="Mono" size="7.9">models/box.step</font>', T('CAD-Modell, alle zehn Teile in Einbaulage', 'CAD model, all ten parts in assembled position')],
   ['<font name="Mono" size="7.9">models/pcb_spacer.step</font>', T('Abstandshalter zwischen Hauptplatine und RC522', 'Spacer between main board and RC522')],
   ['<font name="Mono" size="7.9">models/plates/</font>', T('Fertige Druckbetten als 3MF, eins pro Farbe, und ein Bambu-Studio-Projekt mit allen', 'Ready-made print plates as 3MF, one per colour, and a Bambu Studio project with all of them')], ['<font name="Mono" size="7.9">models/stl/</font>', T('Jedes Druckteil als STL, in Druckausrichtung', 'Every printed part as STL, in print orientation')],
   ['<font name="Mono" size="7.9">hardware/</font>', T('KiCad-Projekte, Schaltpläne und Gerber der Platinen, Verdrahtung der Stromseite (WireViz)', 'KiCad projects, schematics and Gerbers of the boards, wiring of the power side (WireViz)')],
   ['<font name="Mono" size="7.9">doc/hardware.md</font>', T('Elektronik: Teileliste, Platinen, Verkabelung, Inbetriebnahme', 'Electronics: parts list, boards, wiring, first start')],
   ['<font name="Mono" size="7.9">README.md</font>', T('Überblick und Schnellstart', 'Overview and quick start')], ['<font name="Mono" size="7.9">doc/usage.md</font>', T('Bedienung', 'How to use the box')], ['<font name="Mono" size="7.9">doc/development.md</font>', T('Klopfsensoren abstimmen, Entwicklung', 'Tuning the knock sensors, development')], ['<font name="Mono" size="7.9">tools/token-sheet.html</font>', T('Token-Bilder als Fotoabzug 10 × 15 cm', 'Token pictures as a 10 × 15 cm photo print')],
   ['<font name="Mono" size="7.9">doc/assembly/</font>', T('Quellen dieser Anleitung: Text, Bilder aus dem Modell, Build-Skript', 'Sources of this guide: text, images from the model, build script')]],
   [52*mm, FW-52*mm])]

# 8 Anhang
AH = 218*mm
story += [PageBreak(), H1(8, T('Anhang', 'Appendix')), P(T('Schaltpläne und Verdrahtung aus <font name="Mono" size="8.3">hardware/</font>, unverändert eingebunden. Am Bildschirm lassen sie sich beliebig vergrößern.', 'Schematics and wiring from <font name="Mono" size="8.3">hardware/</font>, included unchanged. On screen they can be zoomed in as far as needed.')),
  P(T('Schaltplan Hauptplatine', 'Schematic of the main board'), 'h2'), PdfPage(ROOT / 'hardware' / 'mainboard' / 'timon-box-schematic.pdf', FW, 196*mm), PageBreak(),
  P(T('Schaltplan Power Latch V2', 'Schematic of the power latch V2'), 'h2'), PdfPage(ROOT / 'hardware' / 'power-latch' / 'power-latch-v2-schematic.pdf', FW, AH), PageBreak(),
  P(T('Verdrahtung der Stromseite', 'Wiring of the power side'), 'h2'), P(T('Aus <font name="Mono" size="8.3">hardware/wiring/power-side.yml</font> mit WireViz erzeugt. Jede Zeile in einem Kabel nennt beide Enden.', 'Generated from <font name="Mono" size="8.3">hardware/wiring/power-side.yml</font> with WireViz. Every row in a cable names both ends.')),
  SvgPage(ROOT / 'hardware' / 'wiring' / ('power-side.svg' if LANG == 'de' else 'power-side-en.svg'), FW, 205*mm)]

out = str(HERE.parent / ('assembly.pdf' if LANG == 'de' else f'assembly-{LANG}.pdf'))
Doc(out, META).multiBuild(story, canvasmaker=NumberedCanvas)
if LANG != 'de':
    # Anything German that would end up in the English guide: T() without English, or text without T() at all.
    for de in dict.fromkeys(MISSING):
        print(f'  missing English: {de[:90]}')
    for f, line, text in check_untranslated([HERE / 'content.py', HERE / 'layout.py'], ignore=WIRES):
        print(f'  not translated, {f}:{line}: {text[:80]}')
    for text in wiring.leftovers():
        print(f'  not translated in {wiring.SVG_EN.name}: {text} (add it to WIRING_EN in wiring.py)')
print('written', out)
