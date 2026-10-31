# Bedienung

## Im Alltag

- **Einschalten:** Einen Token auflegen. Die Box startet und spielt das Album.
- **Pause:** Den Token abnehmen. Wieder aufgelegt, spielt die Box an derselben Stelle weiter, ein paar Sekunden davor.
  Das klappt auch nach dem Ausschalten.
- **Vor und zurück:** Auf die rechte Seite drücken springt zum nächsten Titel, auf die linke zum vorigen. Ein kurzer,
  kräftiger Druck reicht.
- **Lautstärke:** Am Drehknopf.
- **Ausschalten:** Das macht die Box selbst: zehn Minuten nach dem Abnehmen, nach zwei Stunden Wiedergabe und wenn ein
  Album einmal ganz durchgelaufen ist.
- **Laden:** Über die USB-C-Buchse hinten. Das kleine Licht daneben zeigt, ob der Akku lädt.

## Was die LED zeigt

| Farbe | Bedeutung |
|---|---|
| grün | spielt |
| grün, pulsierend | Podcast: holt gerade die neueste Folge |
| orange | Pause, oder Management-Modus mit eigenem WLAN |
| blau / indigo | Bluetooth: spielt / wartet |
| weiß | Management-Modus verbindet sich mit dem Heimnetz |
| cyan | Management-Modus im Heimnetz |
| rot | Akku fast leer, 20 % oder weniger |

Pulsiert die Farbe, holt gerade die andere Box Titel ab.

## Musik aufspielen

Die Musik liegt auf der SD-Karte, ein Ordner pro Token. Am bequemsten kommt sie über die Web-Oberfläche dorthin:

1. Den **Management-Token** auflegen. Hat die Box ein Heimnetz gespeichert, verbindet sie sich damit. Sonst öffnet sie
   ein eigenes WLAN mit ihrem Namen.
2. Mit Handy oder Rechner verbinden. Im WLAN der Box öffnet sich die Seite von selbst. Im Heimnetz ist die Box unter
   `http://<name>.local` erreichbar, etwa `http://timon-box.local`, und unter ihrer IP-Adresse, die unten auf jeder
   Seite steht.
3. Den Management-Token gegen den **Token des Albums** tauschen, das befüllt werden soll. Die Seite zeigt jetzt dessen
   Ordner. Dort lädt man Titel hoch, benennt das Album, löscht Titel oder legt fest, was als Nächstes spielt.
4. Den Token abnehmen. Die Box geht aus und spielt beim nächsten Auflegen wie gewohnt.

Auf der Seite **Wi-Fi** speichert man das Heimnetz. Dann verbindet sich die Box beim nächsten Mal dort, und
Podcasts funktionieren.

## Podcasts

Jedes Album kann einem Podcast folgen. Auf der Seite **Podcast** sucht man ihn im Apple-Podcast-Verzeichnis oder
trägt die Feed-Adresse direkt ein.

Beim Auflegen pulsiert die LED grün: Die Box verbindet sich mit dem Heimnetz, liest den Feed und spielt die neueste
Folge. Ist sie neu, beginnt sie, sobald die ersten Sekunden geladen sind, der Rest kommt im Hintergrund. Sonst geht es
an der gemerkten Stelle weiter. Ohne WLAN spielt die Box, was schon auf der Karte liegt. Die neuesten 30 Folgen
bleiben, ältere löscht sie selbst. Unterstützt werden MP3-Podcasts, also fast alle.

Unter **Episodes** lassen sich auch ältere Folgen einzeln laden, ebenso auf einen Token, der kein Podcast ist. Diese
Folgen bleiben, bis man sie selbst löscht.

## Zwei Boxen

Gibt es eine zweite Box im selben Netz, kopiert man auf der Seite **Copy** Alben von ihr herüber. Die andere Box
braucht dafür nur ihren Management-Token. Findet die Seite sie nicht von selbst, tippt man ihren Namen oder ihre
IP-Adresse ein.

## Bluetooth

Den **Bluetooth-Token** auflegen. Die Box startet neu und meldet sich als Bluetooth-Lautsprecher mit ihrem Namen. Am
Handy verbinden, und die Musik kommt aus der Box. Drücken auf die Seiten schickt vor und zurück ans Handy. Den Token
abnehmen, und die Box ist wieder eine Hörbox.

## Eigene Tokens

Neue Tokens druckt man selbst, der NFC-Tag kommt während des Drucks hinein (Bauanleitung, Kapitel 3). Die Bilder
entstehen mit [tools/token-sheet.html](../tools/token-sheet.html). Ein neuer Token ist sofort ein leeres Album: einmal
auflegen, dann im Management-Modus befüllen.
