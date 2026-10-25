# Timon-Box: Kontext für Claude

Hörbox für Kinder auf einem ESP32 (WROOM, **ohne PSRAM**). Ein RFID-Token wählt ein Album auf der SD-Karte, die Box
spielt MP3 über I²S. Dazu kommen Podcast-Alben, ein Bluetooth-Lautsprechermodus, ein Management-Modus mit Web-UI im
WLAN und zwei Piezo-„Klopftasten“ für Vor und Zurück. Es gibt zwei Boxen, Timon und Julius. Sie sind gleich aufgebaut,
haben aber unterschiedlich empfindliche Gehäusewände: PlatformIO-Envs `timon` und `julius`.

Ausführlich, Modul für Modul: **[doc/architecture.md](doc/architecture.md)**. Diese Datei hier ist das Minimum, um
loszulegen. Lies den passenden Abschnitt dort, bevor du ein Teilsystem änderst. Platinen, Teile und Verkabelung:
**[doc/hardware.md](doc/hardware.md)**, die KiCad-Projekte liegen unter `hardware/`.

## Regeln

- **Sprache:** Code-Kommentare und Commit-Messages Englisch. README und `doc/` Deutsch. Texte der Web-UI Englisch, im
  Look von `src/web-ui.h` (Karten, Pills, gleiche CSS-Klassen).
- **Branches:** Neue Features auf einem eigenen Branch, nicht auf `main`. Übernahme nach `main` nur, wenn der Nutzer
  das sagt. Aktueller Feature-Branch: `podcast`.
- **Commits:** Dateien immer einzeln mit Pfad stagen, nie `git commit -a`. `.vscode/extensions.json` und `t.sh` sind
  lokale Änderungen des Nutzers und werden nie committet.
- **Bauen:** Immer beide Envs. `pio` liegt nicht im PATH:
  `~/.platformio/penv/bin/pio run -e timon -e julius`. Den Exit-Code von pio prüfen, nicht den von `| tail`.
- **Flashen:** `~/.platformio/penv/bin/pio run -e <box> -t upload --upload-port /dev/ttyUSBn`. Vorher prüfen, welche
  Box am Port hängt: `~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py --port
  /dev/ttyUSBn read_mac`. Das setzt die Box zurück. Die falsche Env flasht den falschen Namen und falsche
  Klopf-Schwellen. Flashen startet die Box neu und wirft sie aus dem Management-Modus: vorher Bescheid sagen.
- **Kein passwortloses sudo** auf diesem Rechner (zazu). Kein tcpdump, kein `ip neigh replace`.
- **Doku pflegen:** Wer ein Teilsystem ändert, passt den Abschnitt in `doc/architecture.md` im selben Commit an. Neue
  Fallstricke kommen hier unten dazu, neue Zustände oder Timeouts auch in `doc/state-machine.html`.

## Testen

- **Host-Tests** für Feed-Parser, Dateinamen und Download-Planung: `tools/host-test.sh`, braucht nur g++.
- **Serielle Konsole:** `tools/serial-bridge.py` hält den Port, schreibt `box.log` und nimmt Befehle über eine FIFO
  an. Der Kopfkommentar des Skripts zeigt die Benutzung. Logs und FIFO gehören ins Scratchpad, nicht ins Repo. Befehle
  immer als `befehl:` mit Doppelpunkt schicken, mit ein paar Sekunden Abstand. Der Parser in `main.ino` liest bis
  `:` und verschluckt danach eine Sekunde lang alles.
- **Wichtige Befehle:** `audio:`, `podcast:`, `heap:[s]`, `list:`, `play:<i>`, `knockdebug:<0|1|2>`, `md5:<pfad>`,
  `dl:<url>`, `dlplay:`, `dlstop:`, `dlclean:`, `podcasturl:<url> [keep]`, `playat:<byte>`, `restart:`. Die volle
  Liste gibt ein unbekannter Befehl aus.
- **Podcast-Testserver:** `tools/podcast-test-server.py --dir <mp3-ordner>` liefert einen Feed mit Schaltern für
  schwierige Server: `--no-range`, `--no-length`, `--chunked`, `--rate`, `--redirects`, `--drop-after`, `--id3`.
- **Downloads prüfen:** `md5:<pfad>` auf der Box mit `curl -r <id3>- <url> | md5sum` vergleichen.
- Der Nutzer testet gern mit und sagt, wenn eine Box angesteckt ist. Prüfe, mit welcher Box er gerade arbeitet: Der
  Browser kann eine andere Box ansprechen als die am USB-Port.

## Aufbau in einer Minute

Alles landet in **einer Übersetzungseinheit**: `main.ino` bindet `knock.cpp` und `playerI2S.h` ein, und von dort
kommt der Rest. Die meisten Header definieren globale Variablen und Funktionen. **Aber:** PlatformIO übersetzt
`knock.cpp` und `player.cpp` zusätzlich einzeln. Was die beiden einbinden, darf nur Klassen und
`inline`/`static`-Funktionen enthalten. `podcast-feed.h` ist deshalb komplett inline und auch auf dem Host testbar.

| Datei | Aufgabe |
|---|---|
| `main.ino` | Zustandsautomat, Token-Erkennung, LED, Lautstärke, Akku, serielle Befehle, Ein/Aus |
| `player.cpp` | Basisklasse `Player`: Albumordner, Playlist (`index.txt`), `last_played.txt` mit Position |
| `playerI2S.h` | `PlayerI2S`: Audio-Bibliothek, I²S, Start eines Titels, Klick und Alarm |
| `knock.cpp` | Klopfsensor: ADC per `esp_timer` mit 2 kHz, erkennt einen Druck an der Buckelform |
| `podcast-feed.h` | Reines C++: RSS-Parser, Datumsformate, Episoden-Dateinamen, Fortsetzungsplan |
| `mp3-info.h` | Reines C++: Dauer einer MP3 aus Frame-Kopf und Xing/Info- oder VBRI-Header |
| `podcast.h` | Podcast-Task: WLAN, Feed, HTTP-Download mit Range, Zustandsdateien, Aufräumen |
| `podcast-episodes.h` | Karte „Episodes“: Feed auf der Box lesen, einzelne Folgen von Hand laden |
| `gated-fs.h` | `sdGate` und `GatedFS`: spielt eine Datei, während sie noch geladen wird |
| `management-server.h` | Web-Server im Management-Modus: Seiten, Upload, Löschen, Umbenennen, Podcast-Reiter |
| `web-ui.h` | CSS, Seiten-Skript, Kopf, Navigation, `PageBuffer` und `sendBuffered` |
| `wifi-setup.h` | Netz im Management-Modus: Heimnetz oder Hotspot, WLAN-Seite, mDNS |
| `box-sync.h` | Alben zwischen den beiden Boxen kopieren |
| `LList.h` | Verkettete Liste für die Playlist |

## Fallstricke, die schon Zeit gekostet haben

1. **RAM** ist der Engpass. Mit Download und Wiedergabe bleiben etwa 65 kB frei, mit TLS deutlich weniger. Große
   zusammenhängende Blöcke sind knapp: Seiten deshalb in 1-kB-Blöcken senden, mit `PageBuffer`.
2. **Audio-Bibliothek** (ESP32-audioI2S 3.0.0): Titel nur mit `connecttoFS(fs, path, pos)` starten. Nie
   `setFilePos` oder `setTimeOffset` benutzen, die können verklemmen. Liefert `operator bool` der Datei `false`,
   liest die Bibliothek sie nie wieder. Das Dateiende erkennt sie erst nach dem Lesen des MP3-Kopfs.
3. **SD-Karte, Treiber:** Er meldet Schreibfehler, obwohl die Daten angekommen sind. Wer schreibt, muss einen Block
   wiederholen können. Der Podcast-Download tut das. Der Upload-Schreiber wiederholt nicht. Ob er betroffen ist, ist
   offen: Ein 22-MB-Upload lief ohne Fehler.
4. **SD-Karte, Zugriff:** Zwei Tasks dürfen nicht gleichzeitig über VFS und stdio zugreifen, also über `File` und
   `SD.open`, sonst vertauschen sich Datei-Handles. Der Podcast-Task nutzt deshalb FATFS direkt. Jede offene Datei
   kostet 4 kB, denn `FF_MAX_SS` ist 4096. Das VFS ist auf 5 Dateien begrenzt.
5. **sdGate:** Wer neben der Wiedergabe schreibt, nimmt das Gate. Der Leser versucht es nur und wartet nie.
6. **mDNS:** Im Heimnetz des Nutzers empfangen die Boxen keine Multicast-Anfragen. Deshalb kündigt sich die Box jede
   Minute an, und die Seiten fallen auf die IP zurück. Ein `.local`-Name, der nicht auflöst, ist kein Absturz der
   Box. Gegenprobe: `dig @<ip> -p 5353 <name>.local` als Unicast und `dig @224.0.0.251 -p 5353 ...` als Multicast.
7. **Bluetooth:** Läuft nur nach `ESP.restart()` mit gesetztem RTC-Flag. Im Player-Modus wird der BT-Speicher
   freigegeben. Die Meldung „i2s_driver_install failed“ im BT-Modus ist harmlos, der Ton funktioniert.
8. **Klopfsensoren:** `leftKnockSensor` hängt an `RKS_PIN` und `rightKnockSensor` an `LKS_PIN`, die Namen sind
   vertauscht. Auch die Platine ist seitenverkehrt beschriftet: J2 „R Btn“ gehört zum Piezo an der linken Wand. Die
   Schwellen stehen pro Box in `platformio.ini`.
9. **Seriell:** Ein schlichtes Öffnen des Ports setzte die Box nicht zurück. Schaltet man mit pyserial vor dem Öffnen
   DTR und RTS ab, startet sie neu. Steckt man die Box ab, solange der Port noch offen ist, bekommt sie beim
   Wiederanstecken die nächste ttyUSB-Nummer.
10. **Ausschalten:** Am Netzteil bleibt die Box an. `shutdownBox()` zieht nur die Selbsthaltung
    (`POWER_SUPPLY_PIN`), danach läuft die Schleife weiter.
