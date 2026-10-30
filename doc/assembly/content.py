"""Text and structure of doc/assembly.pdf (German). Build with ./build.sh, layout lives in layout.py."""
from layout import *

META = dict(
    title='Timon Box – Gehäuse: Druck und Zusammenbau', subject='Hardware-Doku: Druckteile, Material, Zusammenbau',
    kicker='HARDWARE-DOKU · GEHÄUSE', name='Timon Box', subtitle='Druck und Zusammenbau',
    intro='Druckteile, Material und eine Schritt-für-Schritt-Anleitung für das 3D-gedruckte Gehäuse der Box.',
    facts=['Modell: models/box.step', 'Elektronik: doc/hardware.md', 'Stand: Oktober 2026'],
    header='Timon Box · Gehäuse', footer='Stand Oktober 2026 · Modell models/box.step')


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
  P('Vorn ist die Lautsprecherseite. Von vorn gesehen liegt die Taste „zurück“ links und „vor“ rechts, passend zu J2 und J3 in hardware.md. Der Deckel ist um 5° nach vorn geneigt, die Rückseite mit der USB-C-Buchse ist leicht gewölbt.'), Spacer(1, 4)]
story += [fig('overview', [
    dict(a='a_token', t='Token-Mulde', s='RC522 darunter', side='L'), dict(a='a_knob', t='Lautstärke', side='L'),
    dict(a='a_led', t='LED', side='L'), dict(a='a_handle', t='Griff', side='L'), dict(a='a_prev', t='Taste „zurück“', s='Piezo innen', side='L'),
    dict(a='a_grill', t='Lautsprecher', side='L', dy=-6),
    dict(a='b_jack', t='Kopfhörerbuchse', s='noch verschlossen', side='R'), dict(a='b_next', t='Taste „vor“', side='R'),
    dict(a='b_curved', t='Rückseite', s='leicht gewölbt', side='R'), dict(a='b_usb', t='USB-C', s='Laden und Flashen', side='R'),
    dict(a='b_bumper', t='Bumper (TPU)', side='R')], frac=0.70, gap=6),
  cap('Links von vorn, rechts von hinten.'),
  table([['Maß', 'Wert'], ['Grundfläche', '94 × 90 mm, mit Griffen 102 mm breit'], ['Höhe mit Bumper', 'hinten 94,5 mm, vorn 87 mm'],
         ['Darüber', 'Token 5 mm, Drehknopf 9 mm über der Deckeloberfläche']], [42*mm, FW-42*mm]), Spacer(1, 10),
  P('Auf einen Blick', 'h2'), glance(), Spacer(1, 10),
  note('Diese Anleitung beschreibt nur das Gehäuse. Teileliste der Elektronik, Platinen, Verkabelung und Inbetriebnahme stehen in <b>doc/hardware.md</b>, das Abstimmen der Klopfsensoren im <b>README</b>.', 'info'),
  PageBreak()]

# 2 Druckteile
rows = [['Teil', 'Funktion', 'Material', 'Befestigung'],
 [pn('front'), 'Deckel mit Token-Mulde, LED, Poti, Kopfhörerbuchse und Tasche für den Mikroschalter', 'PETG/PLA', 'Sekundenkleber auf ' + pn('frame')],
 [pn('frame'), 'Rahmen: Seitenwände mit Griffen, Lautsprecheröffnung, USB-C-Ausschnitt. An den Tasten ist die Wand innen auf 1 mm gedünnt', 'PETG/PLA', '–'],
 [pn('back'), 'Boden: Rahmen um die Klappe mit vier Gewindeeinsätzen', 'PETG/PLA', 'Sekundenkleber an ' + pn('frame')],
 [pn('door'), 'Bodenklappe', 'PETG/PLA', '4 × M3 × 5 in ' + pn('back')],
 [pn('bumper'), 'Fuß unter der Klappe', 'TPU', 'Sekundenkleber an ' + pn('door')],
 [pn('speaker grill'), 'Lautsprechergitter, mit Stoff bezogen', 'PETG/PLA', 'eingesetzt, Sekundenkleber'],
 [pn('prev button'), 'Taste „zurück“, Ø 20 × 1 mm', 'PETG/PLA', 'Sekundenkleber, außen auf ' + pn('frame')],
 [pn('next button'), 'Taste „vor“, Ø 20 × 1 mm', 'PETG/PLA', 'Sekundenkleber, außen auf ' + pn('frame')],
 [pn('volume'), 'Drehknopf', 'PETG/PLA', 'auf die Poti-Achse gesteckt'],
 [pn('token'), 'Token mit eingedrucktem NFC-Tag, einer je Album', 'PETG/PLA', '–']]
story += [H1(2, 'Druckteile'), P('Die Teilenamen sind die aus dem STEP-Modell <font name="Mono" size="8.3">models/box.step</font> (Onshape-Export „julius_box“). Dort liegen alle Teile in Einbaulage.'),
  table(rows, [27*mm, 82*mm, 20*mm, FW-129*mm]), Spacer(1, 8),
  fig('exploded', [dict(a='token', t='token'), dict(a='volume', t='volume', s='Drehknopf'), dict(a='front', t='front', s='Deckel'),
     dict(a='frame', t='frame', s='Rahmen'), dict(a='speaker grill', t='speaker grill', s='Lautsprechergitter'),
     dict(a='prev button', t='prev button', s='Taste „zurück“'), dict(a='next button', t='next button', s='Taste „vor“'),
     dict(a='back', t='back', s='Boden'), dict(a='door', t='door', s='Bodenklappe'), dict(a='bumper', t='bumper', s='Fuß, TPU')], frac=0.54),
  cap('Explosionszeichnung. Unten die Klappe mit Bumper, seitlich die Tasten, vorn das Lautsprechergitter.'), PageBreak()]

# 3 Drucken
orient = [['Teil', 'Aufs Druckbett', 'Stützen', 'Hinweis'],
 [pn('frame'), 'Unterkante', 'im Lautsprecherloch und unter den Griffen', 'Das Gitter hat nur 0,2 mm Spiel: Loch entgraten'],
 [pn('front'), 'Unterkante, Oberseite nach oben', 'innen unter der Decke, nur vom Druckbett (Baumstützen)', 'Taschen für Mikroschalter und Buchse freimachen'],
 [pn('back'), 'Seite zum Rahmen (kopfüber)', 'keine', ''],
 [pn('door'), 'Innenseite', 'keine', 'Senkungen zeigen nach oben'],
 [pn('bumper'), 'Klebeseite', 'keine', 'TPU, langsam drucken'],
 [pn('speaker grill'), 'Außenseite', 'keine', ''],
 [pn('prev button') + ', ' + pn('next button'), 'Sichtseite', 'keine', '1 mm dünn'],
 [pn('volume'), 'Oberseite', 'keine', ''],
 [pn('token'), 'Oberseite (Bildseite)', 'keine', 'Pause vor der letzten Schicht für den NFC-Tag, siehe unten']]
tiles = [('frame','Unterkante'), ('front','Unterkante'), ('back','kopfüber'), ('door','Innenseite'), ('bumper','Klebeseite, TPU'),
         ('speaker grill','Außenseite'), ('prev button','Sichtseite'), ('next button','Sichtseite'), ('volume','Oberseite'), ('token','Oberseite')]
cw = FW/5; cells = []
for name, bed in tiles:
    im = PILImage.open(FIG+'tile_'+name.replace(' ','_')+'.jpg'); iw = cw-4; ih = iw*im.height/im.width
    cells.append([Image(FIG+'tile_'+name.replace(' ','_')+'.jpg', iw, ih), Paragraph(f'{pn(name, 7.4)}<br/>{bed}', S['tile'])])
grid = Table([cells[:5], cells[5:]], colWidths=[cw]*5)
grid.setStyle(TableStyle([('ALIGN',(0,0),(-1,-1),'CENTER'), ('VALIGN',(0,0),(-1,-1),'TOP'), ('LEFTPADDING',(0,0),(-1,-1),2), ('RIGHTPADDING',(0,0),(-1,-1),2), ('BOTTOMPADDING',(0,0),(-1,-1),6)]))
story += [H1(3, 'Drucken'),
  P('Alle Teile außer dem Bumper aus PETG oder PLA, den Bumper aus TPU. PETG verträgt Wärme besser, etwa im Auto oder auf der Fensterbank.'),
  P('PrusaSlicer, OrcaSlicer und Bambu Studio öffnen die STEP-Datei direkt. Das Modell dort in Einzelteile zerlegen und jedes Teil wie in der Tabelle auf das Bett legen. Wer lieber STL nimmt, exportiert die Teile einzeln aus Onshape.'),
  P('Einstellungen: 0,2 mm Schichthöhe, 3 Wände, 15–20 % Füllung. Die NFC-Tasche im Token ist auf 0,2 mm Schichthöhe ausgelegt.'),
  table(orient, [33*mm, 38*mm, 46*mm, FW-117*mm]), Spacer(1, 8), grid, cap('Druckausrichtung, die graue Fläche ist das Druckbett.'),
  note('Den Deckel mit der Oberseite nach oben drucken. Andersherum lägen die gerundeten Kanten (R 7,5) direkt auf dem Bett und würden unsauber. So bleiben Oberseite und Rundungen glatt, die Stützen sitzen innen, wo man sie nicht sieht. Die Bohrung der Kopfhörerbuchse ist oben mit einer 0,2-mm-Schicht verschlossen und bleibt zu, solange keine Buchse eingebaut ist.', 'tip'),
  PageBreak()]
tok_steps = [bullets([
  'Den Token mit der Oberseite (Ø 44 mm) aufs Bett legen. Die Bildfläche wird so glatt, und der breite Rand haftet gut.',
  'Im Slicer eine Pause vor der letzten Schicht einfügen, bei 0,2 mm Schichthöhe vor <b>Z = 15,0 mm</b>. Dann ist die NFC-Tasche oben offen.',
  'In der Pause den NFC-Tag mit der Klebeseite nach unten in die Tasche legen und andrücken. Weiterdrucken, die letzte Schicht schließt die Tasche. Der Tag liegt dann 0,2 mm unter der Unterseite, direkt über dem RC522.',
  'Das Bild aus dem Token-Bogen (<font name="Mono" size="8.3">tools/token-sheet.html</font>) grob ausschneiden, zum Beispiel mit doppelseitigem Klebeband auf die Oberseite (Ø 44 mm) kleben und ringsum an der Kante abschneiden.'])]
two = Table([[tok_steps, LayerDiagram()]], colWidths=[FW-92*mm, 92*mm]); two.setStyle(TableStyle([('VALIGN',(0,0),(-1,-1),'TOP'), ('LEFTPADDING',(0,0),(-1,-1),0), ('RIGHTPADDING',(0,0),(-1,-1),0)]))
story += [P('Token mit NFC-Tag', 'h2'),
  P('Der Token ist 15 mm hoch und innen hohl. 0,2 mm über der Unterseite liegt eine Tasche (Ø 26 × 0,4 mm) für einen runden NFC-Klebetag: 25 mm, NTAG, höchstens 0,4 mm dick.'),
  fig('token_section', [dict(a='top', t='Oberseite Ø 44 mm', s='liegt auf dem Bett'), dict(a='flange', t='Rand', s='liegt im Gehäuse auf dem Deckel'),
       dict(a='body', t='Körper Ø 39,7 mm', s='passt in die Mulde Ø 40'), dict(a='void', t='Hohlraum'),
       dict(a='pocket', t='NFC-Tasche Ø 26 × 0,4 mm', s='0,2 mm unter der Unterseite'), dict(a='chamfer', t='Fase'), dict(a='bed', t='Druckbett')], frac=0.64),
  cap('Token im Schnitt, so wie er gedruckt wird.'), two, Spacer(1, 8),
  note('Sondertokens für Bluetooth und Management werden genauso gedruckt. Ihre UIDs stehen in <font name="Mono" size="8.3">src/main.ino</font>, siehe hardware.md, Inbetriebnahme.', 'info'), PageBreak()]

# 4 Material
story += [H1(4, 'Material und Werkzeug'), P('Zukaufteile fürs Gehäuse', 'h2'),
  table([['Teil', 'Anzahl', 'Anmerkung'],
   ['Gewindeeinsatz M3', '4', 'Für Bohrung Ø 4,0 mm, 4 mm lang. Bis 5,7 mm geht, ragt dann innen 1,2 mm heraus'],
   ['Zylinderkopfschraube M3 × 5, ISO 4762', '4', 'Kopf Ø 5,5 mm versinkt in der Senkung Ø 5,6 mm und im Bumper. Nicht länger als 5 mm: In zwei Ecken steht direkt über dem Einsatz ein Akku'],
   ['18650-Zelle', '2', 'Einfache Zellen ohne Schutzschaltung (65 mm) mit Lötfahnen, gleicher Typ und gleiche Spannung. Parallel geschaltet'],
   ['Stoff', '1 Stück, 8 × 8 cm', 'Dünn und dehnbar, zum Beispiel eine Socke'],
   ['NFC-Klebetag, rund, 25 mm', 'je Token', 'NTAG, höchstens 0,4 mm dick']], [55*mm, 26*mm, FW-81*mm]),
  P('Kleber und Kleinkram', 'h2'),
  table([['Material', 'wofür'], ['Sekundenkleber', 'Gehäuseschale, Tasten, Lautsprechergitter, Bumper'], ['2-Komponenten-Kleber (Epoxid)', 'Lautsprecher'],
   ['UV-Lack mit UV-Lampe', 'LED, USB-C-Buchse, Mikroschalter'], ['Heißkleber', 'Akkus'], ['Dünnes doppelseitiges Klebeband', 'Piezos, RC522, Bilder auf den Token'],
   ['Kapton-Band, Schrumpfschlauch', 'Akkukontakte und Lötstellen isolieren'], ['Schleifpapier, Grundierung, Lack', 'optional, für die Oberfläche']], [62*mm, FW-62*mm]),
  P('Elektronik im Gehäuse', 'h2'), P('Die vollständige Teileliste steht in hardware.md. Diese Teile müssen zum Gehäuse passen:'),
  table([['Teil', 'Ort', 'Passt, wenn …'],
   ['Lautsprecher, 3 W', 'innen am Gitter', 'Ø 50–57 mm liegt mit dem Rand auf dem Ring (Ø 48,6–54,6 mm); kleinere sitzen im Ring direkt am Gitter'],
   ['Piezo-Scheibe, ca. 35 mm', 'innen an den Seitenwänden', 'gedünnte Fläche 56 × 50 mm'],
   ['PL9823, 8 mm', 'Deckel vorn links', 'Bohrung Ø 8 mm'],
   ['Poti 10 kΩ', 'Deckel hinten links', 'Gewinde M7 (Bohrung Ø 7 mm), Achse 6 mm rund. Gewindehülse und Mutter müssen in die 3 mm tiefe Aussparung im Knopf passen'],
   ['RC522', 'unter der Token-Mulde', 'Muldenboden 1 mm'], ['Mikroschalter', 'Tasche neben der Mulde', 'Hebel reicht durch den Schlitz in die Mulde'],
   ['USB-C-Platine', 'Rückwand unten', 'Ausschnitt 9,2 × 3,5 mm'], ['Kopfhörerbuchse 3,5 mm', 'Deckel hinten rechts, optional', 'Bohrung Ø 6 mm, Tasche 12 × 7 × 2 mm']],
   [38*mm, 44*mm, FW-82*mm]),
  P('Werkzeug', 'h2'),
  P('Lötkolben (für die Einsätze eine Einschmelzspitze oder eine alte Spitze), Innensechskant 2,5 mm, UV-Lampe, Heißklebepistole, Multimeter, Säge oder Seitenschneider für die Poti-Achse, Cutter, Schleifpapier.'),
  PageBreak()]

# 5 Zusammenbau
story += [H1(5, 'Zusammenbau'),
  P('Erst werden die Teile vorbereitet, dann Rahmen und Boden verklebt und alles eingebaut, was an den Wänden sitzt. Der Deckel wird separat bestückt und erst zum Schluss aufgeklebt. Bis dahin kommt man von oben an alles, danach nur noch durch die Bodenklappe.'),
  ChipRow(['CA','EP','UV','HG','TB'], label='Farben in den Schritten:'), Spacer(1, 10)]
story += [KeepTogether([P('5.1  Vorbereiten', 'h2'), fig('inserts', [dict(a='ins0', t='Gewindeeinsatz M3', s='4 Stück', side='L'),
     dict(a='hole1', t='Bohrung Ø 4,0 mm', s='4,5 mm tief, durchgehend', side='R'), dict(a='ledge', t='Auflage der Klappe', side='R')],
     arrows=[('ins0','hole0'),('ins1','hole1'),('ins2','hole2'),('ins3','hole3')], frac=0.6), cap(pn('back') + ' von unten, die Einsätze kommen von der Seite der Klappe.')]),
  step(1, 'Teile vorbereiten', ['SL'], [
    'Stützen entfernen, Löcher entgraten. Gitter, Token und Klappe probehalber einsetzen: Das Gitter muss ohne Kraft in die Öffnung passen.',
    'Wer lackieren will, schleift und lackiert ' + pn('back') + ', ' + pn('frame') + ', ' + pn('front') + ' und die Tasten jetzt. Die Klebeflächen dabei abkleben und blank lassen, Sekundenkleber hält auf blankem Kunststoff besser als auf Lack.']),
  step(2, 'Gewindeeinsätze einschmelzen', ['LK'], [
    'Die vier M3-Einsätze von unten in die Bohrungen in ' + pn('back') + ' setzen, also von der Seite, an der später die Klappe anliegt.',
    'Mit dem Lötkolben gerade einschmelzen, bis sie bündig mit der Auflagefläche sind. Den Lötkolben etwa auf Drucktemperatur des Materials stellen.'])]
story += [CondPageBreak(120*mm), KeepTogether([P('5.2  Unterteil', 'h2'), fig('shell', [dict(a='front', t='front', s='erst in Schritt 11', side='R'),
     dict(a='frame_top', t='Klebekante oben', s='für den Deckel', side='R'), dict(a='frame', t='frame', side='R'), dict(a='back_glue', t='Klebefläche back', side='R'),
     dict(a='back', t='back', s='Einsätze nach unten', side='R'), dict(a='prev', t='prev button', s='links', side='L'), dict(a='next', t='next button', s='rechts', side='R')],
     arrows=[('prev_from','prev_to')], frac=0.56), cap('Klebeflächen orange. Die Tasten kleben außen auf der Seitenwand.')]),
  step(3, 'Tasten aufkleben', ['CA'], [
    pn('prev button') + ' auf die linke, ' + pn('next button') + ' auf die rechte Seitenwand kleben, von vorn gesehen. Beide sitzen mittig unter dem Griff, die Mitte 31 mm über der Unterkante des Rahmens.',
    'Das Symbol waagerecht ausrichten. Kleber sparsam auftragen, er darf nicht aus den Aussparungen des Symbols quellen.']),
  step(4, 'Boden an den Rahmen kleben', ['CA'], [
    pn('back') + ' mit den Einsätzen nach unten an die Unterkante von ' + pn('frame') + ' setzen. Die gewölbte Seite gehört nach hinten, zur USB-C-Buchse.',
    'Auf einer ebenen Fläche verkleben und die Kanten bündig ausrichten. Den Deckel noch nicht aufkleben.'])]
story += [CondPageBreak(120*mm), KeepTogether([P('5.3  Wände', 'h2'), fig('inside', [dict(a='piezo', t='Piezo', s='Messingseite zur Wand', side='L'),
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
    'Die Kabel beschriften: links ist „zurück“ (J2), rechts „vor“ (J3), siehe hardware.md.'],
    [('tip', 'Dünnes, festes Klebeband überträgt den Druck besser als dickes Schaumklebeband. Die Schwellen werden in Schritt 11 abgestimmt.')]),
  step(8, 'USB-C-Buchse einkleben', ['UV'], [
    'Die Kabel vorher anlöten: VBUS und GND zum TP4056, D+ und D− zum Kabel zur Hauptplatine (hardware.md, Verkabelung).',
    'Die Platine von innen in den Ausschnitt unten in der Rückwand setzen, die Buchse bündig mit der Außenseite.',
    'Ein USB-C-Kabel einstecken, damit die Buchse gerade sitzt. Dann mit UV-Lack von innen fixieren und aushärten. Kein Lack in die Buchse.'])]
story += [CondPageBreak(120*mm), KeepTogether([P('5.4  Deckel', 'h2'), fig('lid', [dict(a='led', t='LED Ø 8 mm', s='UV-Lack', side='R'),
     dict(a='poti', t='Poti Ø 7 mm', s='Mutter von außen', side='L'), dict(a='jack', t='Kopfhörerbuchse', s='Tasche 12 × 7 mm\nBohrung bleibt zu', side='L'),
     dict(a='switch', t='Mikroschalter', s='Hebel durch den Schlitz\nUV-Lack', side='L'), dict(a='cup', t='RC522 hier aufkleben', s='Muldenboden 1 mm', side='R'),
     dict(a='skirt', t='Klebekante', s='zum Rahmen', side='R')], frac=0.6),
     cap(pn('front') + ' von innen. Blau: Fläche für das RC522.')]),
  step(9, 'Deckel bestücken', ['UV', 'TB'], [
    '<b>LED:</b> Die PL9823 von innen bis zum Kragen in die 8-mm-Bohrung vorn links stecken und mit UV-Lack fixieren.',
    '<b>Poti:</b> Von innen durch die 7-mm-Bohrung hinten links stecken, außen Scheibe und Mutter. Den Verdrehschutz-Nocken am Poti abbrechen, falls vorhanden; der Deckel hat dafür kein Loch. Die Achse so kürzen, dass sie 8,5 mm über die Deckeloberfläche ragt, dann sitzt der Knopf mit 0,5 mm Luft. Den Knopf ' + pn('volume') + ' aufstecken.',
    '<b>Mikroschalter:</b> In die Tasche an der Rückseite der Token-Mulde setzen, sodass der Hebel durch den Schlitz in die Mulde ragt. Prüfen, dass ein aufgelegter Token ihn hörbar drückt, dann mit UV-Lack fixieren und aushärten. Lack nur ans Gehäuse des Schalters, nicht an Hebel und Stößel.',
    '<b>RC522:</b> Mit dünnem doppelseitigem Klebeband von innen unter die Token-Mulde kleben, flache Rückseite nach oben, Antenne mittig unter der Mulde. Die Stiftleiste zeigt nach unten, dort steckt später die Hauptplatine.',
    '<b>Kopfhörerbuchse:</b> Bleibt vorerst leer, die Bohrung hinten rechts bleibt verschlossen. Für eine spätere Buchse ist innen eine Tasche; dann die 0,2-mm-Schicht mit 6 mm aufbohren.'])]
story += [CondPageBreak(120*mm), KeepTogether([P('5.5  Elektronik und Akkus', 'h2'), fig('akkus', [
     dict(a='cellA', t='18650 vorn links', s='stehend, Pluspol oben'), dict(a='cellB', t='18650 hinten rechts', s='diagonal gegenüber'),
     dict(a='kapton', t='Kapton über der Lötstelle'), dict(a='red', t='Plus, rot', s='zu B+ am TP4056'), dict(a='black', t='Minus, schwarz', s='zu B− am TP4056'),
     dict(a='window', t='Tastenwand', s='kein Kleber'), dict(a='usb', t='USB-C')], frac=0.6),
     cap('Akkus stehend in zwei diagonal gegenüberliegenden Ecken, hier vorn links und hinten rechts. Rahmen aufgeschnitten, Deckel noch nicht aufgeklebt.')]),
  step(10, 'Elektronik einbauen und Akkus einkleben', ['EL', 'HG'], [
    'Die Hauptplatine von unten auf die Stiftleiste des RC522 stecken.',
    'Stromseite einsetzen (Power Latch mit TP4056, Boost-Modul, zwei 2N7000) und nach den Tabellen in hardware.md verdrahten: Kabel zu J4, Lautsprecher an den MAX98357A, Piezos an J2 und J3, Poti an J1, LED an J5, Mikroschalter mit C an Trigger + und NO an Trigger In der Latch.',
    'Die beiden 18650-Zellen stehend in zwei diagonal gegenüberliegende Ecken setzen, Pluspol oben, zum Beispiel vorn links und hinten rechts. Hinten links sitzt direkt darüber das Poti.',
    'Die Zellen stehen auf dem Boden direkt über den Gewindeeinsätzen und reichen bis knapp unter den Deckel. Einfache Zellen ohne Schutzschaltung (65 mm) passen, die Schutzschaltung sitzt auf dem TP4056-Modul.',
    'Mit Heißkleber in den Spalten zwischen Zelle und Ecke fixieren. Kein Kleber auf die gedünnten Wandflächen der Tasten, er dämpft sonst die Piezos.',
    'Parallel schalten, Plus an Plus und Minus an Minus, und an B+ und B− des TP4056 anschließen. Alle Lötstellen mit Kapton-Band oder Schrumpfschlauch isolieren.'],
    [('warn', 'Vor dem Zusammenschalten beide Zellen auf gleiche Spannung bringen (höchstens etwa 0,1 V Unterschied), sonst fließt ein hoher Ausgleichsstrom. Nur gleiche Zellen verwenden, Zellen mit Lötfahnen nehmen und nicht direkt an die Zelle löten. Nichts Blankes darf Schrauben, Einsätze oder andere Bauteile berühren.')])]
left = [fig('akku_detail', [dict(a='glue1', t='Heißkleber', s='in beiden Spalten zur Wand'), dict(a='kapton', t='Kapton'),
        dict(a='cell', t='Zelle in der Ecke'), dict(a='window', t='Tastenwand', s='hier kein Kleber')], frac=0.58, width=88*mm, place='left', gap=7),
        cap('Ecke vorn links von oben.')]
right = [Spacer(1, 6), ParallelDiagram(), Spacer(1, 4), cap('Parallelschaltung: Plus an Plus, Minus an Minus.')]
pair = Table([[left, right]], colWidths=[88*mm, FW-88*mm])
pair.setStyle(TableStyle([('VALIGN',(0,0),(-1,-1),'TOP'), ('LEFTPADDING',(0,0),(-1,-1),0), ('RIGHTPADDING',(0,0),(-1,-1),0)]))
story += [KeepTogether([pair])]
story += [CondPageBreak(120*mm), KeepTogether([P('5.6  Abschluss', 'h2'), fig('door', [dict(a='back', t='back', s='mit Einschmelzgewinden', side='R'),
     dict(a='door', t='door', s='Bodenklappe', side='R'), dict(a='bumper_glue', t='bumper (TPU)', s='Klebefläche zur Klappe', side='L'),
     dict(a='scr0', t='4 × M3 × 5', s='ISO 4762', side='L')], arrows=[('scr0','dhole0'),('scr1','dhole1'),('scr3','dhole3')], frac=0.56),
     cap('Bodenklappe mit Bumper und Schrauben. Orange: Klebefläche.')]),
  step(11, 'Testen, dann Deckel aufkleben', ['CA'], [
    'Vor dem Verkleben in Betrieb nehmen, wie in hardware.md unter Inbetriebnahme beschrieben: Token auflegen, Box startet, flashen, Lautstärke, LED und Akkuanzeige prüfen.',
    'Die Klopfsensoren mit <font name="Mono" size="8.3">knockdebug:1</font> und <font name="Mono" size="8.3">knockdebug:2</font> abstimmen (README). Die Schwellen stehen pro Box in der <font name="Mono" size="8.3">platformio.ini</font>, eine neue Box bekommt dort eine eigene Env.',
    'Die Kabel nach innen legen, auf der Klebekante darf nichts liegen. Den Deckel mit Sekundenkleber auf den Rahmen kleben, die gewölbte Seite nach hinten. Danach die Klopfsensoren kurz gegenprüfen.']),
  step(12, 'Bodenklappe anschrauben', ['CA'], [
    'Den Bumper mit Sekundenkleber unter die Klappe kleben. Zum Ausrichten zwei Schrauben durch Bumper und Klappe stecken.',
    'Die Klappe mit vier Schrauben M3 × 5 anschrauben. Die Köpfe versinken im Bumper. Durch die Klappe kommt man wieder an die Elektronik.'],
    [('warn', 'Keine längeren Schrauben als M3 × 5 verwenden. In zwei Ecken steht direkt über dem Einsatz ein Akku.')]),
  CondPageBreak(150*mm)]

# 6 Maße
story += [H1(6, 'Maße und Schnittstellen'), P('Alle Werte aus dem STEP-Modell, in Millimetern.'),
  table([['Stelle', 'Maß'],
   ['Außen', '94 × 90 mm, mit Griffen 102 mm breit; Höhe mit Bumper hinten 94,5 mm, vorn 87 mm'],
   ['Deckel', '5° nach vorn geneigt, 3 mm Deckplatte, 8 mm Rand'],
   ['Wände', '3 mm, an den Tasten auf einer Fläche von 56 × 50 mm auf 1 mm gedünnt'],
   ['Token-Mulde', 'Ø 40 mm, 10 mm tief, Boden 1 mm'],
   ['Token', 'Körper Ø 39,7 mm, Rand Ø 44 mm, 15 mm hoch; NFC-Tasche Ø 26 × 0,4 mm, 0,2 mm über der Unterseite'],
   ['LED', 'Bohrung Ø 8 mm'],
   ['Poti und Knopf', 'Bohrung Ø 7 mm; Knopf Ø 14 × 9 mm, Achsloch Ø 6,2 mm (8 mm tief ab Unterkante), Aussparung Ø 12 × 3 mm'],
   ['Kopfhörerbuchse', 'Bohrung Ø 6 mm, oben mit 0,2 mm verschlossen; Tasche 12 × 7 × 2 mm'],
   ['USB-C', 'Ausschnitt 9,2 × 3,5 mm, Ecken R 1,5'],
   ['Lautsprecher', 'Öffnung Ø 55 mm; Gitter mit Rand Ø 54,6 mm, Auflagering Ø 48,6–54,6 mm, steht 3 mm vor'],
   ['Tasten', 'Ø 20 × 1 mm, Mitte 31 mm über der Unterkante des Rahmens'],
   ['Gewindeeinsätze', '4 Bohrungen Ø 4,0 mm, 4,5 mm tief, durchgehend'],
   ['Klappe', '2,9 mm dick, Senkung Ø 5,6 × 2 mm, Durchgang Ø 3,1 mm'],
   ['Bumper', '2 mm, TPU']], [36*mm, FW-36*mm]),
  P('Dateien', 'h2'),
  table([['Pfad', 'Inhalt'], ['<font name="Mono" size="7.9">models/box.step</font>', 'CAD-Modell (Onshape-Export), alle zehn Teile in Einbaulage'],
   ['<font name="Mono" size="7.9">doc/hardware.md</font>', 'Elektronik: Teileliste, Platinen, Verkabelung, Inbetriebnahme'],
   ['<font name="Mono" size="7.9">README.md</font>', 'Klopfsensoren abstimmen'], ['<font name="Mono" size="7.9">tools/token-sheet.html</font>', 'Token-Bilder als Fotoabzug 10 × 15 cm'],
   ['<font name="Mono" size="7.9">doc/assembly/</font>', 'Quellen dieser Anleitung: Text, Bilder aus dem Modell, Build-Skript']],
   [52*mm, FW-52*mm])]

out = str(HERE.parent / 'assembly.pdf')
Doc(out, META).multiBuild(story, canvasmaker=NumberedCanvas)
print('written', out)
