# Architektur der Firmware

Diese Datei beschreibt, wie die Firmware funktioniert und was man vor einer Änderung wissen muss. Die Kurzfassung mit
Regeln und Arbeitsablauf steht in [CLAUDE.md](../CLAUDE.md). Für die Bedienung, den Klopfsensor-Hintergrund und den
Token-Bogen siehe [README.md](../README.md). Die Zustände als Diagramm zeigt [state-machine.html](state-machine.html).
Wer Zustände oder Timeouts ändert, aktualisiert das Diagramm mit.

Stand: Oktober 2026, Branch `podcast`.

## 1. Hardware und Pins

ESP32-WROOM ohne PSRAM, 4 MB Flash mit Partition `huge_app.csv`. Arduino-ESP32 2.0.17 auf ESP-IDF 4.4, Plattform
`espressif32@^6.11.0`. Platinen, Teile und Verkabelung beschreibt [hardware.md](hardware.md).

| Funktion | Pins | Anmerkung |
|---|---|---|
| RFID-Leser MFRC522 | CS 23, SCK 22, MOSI 21, MISO 19, RST 18 | Standard-`SPI` |
| SD-Karte | CS 5, SCK 16, MOSI 17, MISO 4 | eigener `HSPI`, 27 MHz |
| I²S-Verstärker | BCLK 27, LRC 26, DOUT 25 | Ausgabe mono (`forceMono`) |
| Token-Schalter | 32 | `LOW` heißt, ein Token liegt auf |
| Lautstärke-Poti | 36 | ADC |
| Akkuspannung | 39 | ADC, gefiltert in `updateBatteryAdc()` |
| Klopfsensoren | 34 (`RKS_PIN`), 35 (`LKS_PIN`) | ADC1, siehe Abschnitt 9 |
| NeoPixel | 2 | eine RGB-LED, Helligkeit 20 |
| Selbsthaltung | 33 (`POWER_SUPPLY_PIN`) | `HIGH` hält die Box an, `LOW` schaltet aus |

Der Watchdog steht auf 200 s mit Panic. Nur die Hauptschleife meldet sich bei ihm.

## 2. Betriebsmodi und Zustände

Der Zustandsautomat steht in `loop()` in `main.ino`.

| Zustand | LED | Bedeutung |
|---|---|---|
| `paused` | orange | kein Token oder pausiert; nach `MAX_PAUSE_TIME_IN_S` (10 min) aus |
| `playing` | grün | Album oder Podcast spielt; nach `MAX_PLAY_TIME_IN_S` (120 min) aus |
| `podcastLoading` | grün, pulsierend | Podcast-Token: Feed prüfen, neueste Folge laden |
| `bluetoothPaused` / `bluetoothPlaying` | indigo / blau | Bluetooth-Lautsprecher |
| `managementServerRunning` | orange | Management-Modus mit eigenem Hotspot |
| `managementServerConnecting` | weiß | Management-Modus, verbindet sich mit dem Heimnetz |
| `managementServerLan` | cyan | Management-Modus im Heimnetz |

Bei Akku ≤ 20 % (`LOW_BATTERY_THRESHOLD`) leuchtet die LED rot. Den ADC-Wert rechnet `getBattery()` über zwei
Kalibrierpunkte pro Box in Millivolt um (`BATTERY_ADC_*`, `BATTERY_MV_*` in `platformio.ini`), der Befehl `battery:`
zeigt Rohwert und Ergebnis. Während die andere Box Titel abholt, pulsiert die
Zustandsfarbe (`updateTransferLed()`).

**Token:** `getTag()` liest das RFID nur, wenn der Token-Schalter einen Token meldet und noch keine Kennung bekannt ist.
Die Kennung ist die UID als Hex-String. Zwei Kennungen stehen pro Box in der `platformio.ini`: `BLUETOOTH_TAG` und
`MANAGEMENT_SERVER_TAG`, Ersatzwerte in `box-config.h`.
Jede andere Kennung ist ein Album.

**Auflegen im Zustand `paused`:**
- Derselbe Token wie zuvor, und es lief schon etwas: `resume()`.
- Ein neuer Token: Akku prüfen, `stopSong()`, `loadPlaylist(tag)`. Bei einem Podcast-Album startet `podcastStart()`,
  und der Zustand wird `podcastLoading`. Ist der Akku zu schwach, spielt gleich die neueste Folge auf der Karte. Ein
  normales Album startet mit `startOrContinueAlbum()` am gemerkten Titel und an der gemerkten Stelle.
- `fileWhenTagInserted` merkt sich den ersten Titel. Kommt das Album wieder bei ihm an, geht die Box aus. So endet ein
  Album nach einem Durchlauf, auch wenn es mitten im Album begonnen hat.

**Abnehmen im Zustand `playing`:** Pause, Position speichern, Zustand `paused`, dann 3 s `delay`. Beim
Wiederauflegen geht es nahtlos weiter.

**Klopfen:** Rechts ist `next()`, links ist `previous()`. Beide innerhalb von 750 ms springen zum ersten Titel. Vor
jeder Aktion spielt `playClick()` den Klick `tap.mp3`. Der Aufruf blockiert 300 ms.

**Ausschalten:** `shutdownBox()` stoppt den Podcast-Task, löscht die LED und setzt `POWER_SUPPLY_PIN` auf `LOW`. Am
USB-Netzteil bleibt die Box trotzdem an. Die Schleife läuft weiter und ruft `shutdownBox()` immer wieder auf. Deshalb
meldet sie das Ausschalten nur einmal. `shuttingDown` unterdrückt Folgemeldungen.

**SD-Fehler:** Ist `player.sdConnected` falsch, startet die Box im Zustand `playing` oder `paused` neu.

## 3. Code-Struktur

### Übersetzungseinheit

```
main.ino
├── knock.cpp                 (nur Klasse KnockSensor)
└── playerI2S.h
    ├── player.cpp            (nur Klasse Player) ── LList.h, podcast-feed.h
    ├── Audio.h               (ESP32-audioI2S)
    ├── gated-fs.h
    ├── management-server.h
    │   ├── web-ui.h
    │   ├── wifi-setup.h ── web-ui.h
    │   ├── podcast.h
    │   └── box-sync.h        (mitten in management-server.h eingebunden, nutzt dessen Helfer)
    └── podcast.h ── ff.h, gated-fs.h, podcast-feed.h, wifi-setup.h
```

Alles hängt an `main.ino`. Die Header definieren globale Variablen und normale Funktionen, das geht nur, weil sie in
genau einer Übersetzungseinheit landen. **Ausnahme:** PlatformIO übersetzt jede `.cpp`-Datei in `src/` auch einzeln.
`knock.cpp` und `player.cpp` sind in Wahrheit Header. Sie und alles, was sie einbinden (`LList.h`,
`podcast-feed.h`), dürfen nur Klassen und `inline`- oder `static`-Funktionen enthalten. Sonst gibt es doppelte
Symbole beim Linken.

Neuer Code kommt in einen neuen Header, eingebunden dort, wo er gebraucht wird. Reine Logik ohne Arduino-Abhängigkeit
gehört nach dem Vorbild von `podcast-feed.h` in einen inline-Header, mit Tests unter `test/host/`.

### Bibliotheken

| Bibliothek | Version | Zweck |
|---|---|---|
| ESP32-audioI2S (schreibfaul1) | `#3.0.0` | MP3-Decoder, I²S-Ausgabe |
| ESP32-A2DP (pschatzmann) | `#v1.8.7` | Bluetooth-Lautsprecher |
| ESPAsyncWebServer, AsyncTCP (ESP32Async) | neueste | Web-Server mit Middleware |
| MFRC522 | `^1.4.10` | RFID |
| Adafruit NeoPixel | `^1.15.1` | LED |

ESPAsyncWebServer ist der Fork von ESP32Async, Version 3.x. Er hat `addMiddleware()`, `AsyncCorsMiddleware` und
`addHeader(name, value, replaceExisting = true)`. Ein `send()` im Handler legt die Antwort nur ab. Middleware kann nach
`next()` noch Header ergänzen.

## 4. SD-Karte

### Aufbau

```
/tap.mp3, /alert.mp3       Klick beim Klopfen, Warnton bei schwachem Akku
/<Name>_<tag>/             Albumordner; ohne Namen nur /<tag>
    *.mp3                  Titel, Reihenfolge nach Dateiname
    index.txt              Playlist-Cache mit vollen Pfaden
    last_played.txt        Zeile 1: Pfad des Titels, Zeile 2: Byte-Position (optional)
    podcast.txt            Zeile 1: Feed-URL, optional "keep=N". Macht das Album zum Podcast
    podcast_state.txt      was der Podcast-Task zuletzt herausfand (key=value)
    podcast_kept.txt       von Hand geladene Folgen, eine pro Zeile: das Aufräumen lässt sie stehen
/temp_<n>                  laufender Upload
/temp_sync0, /temp_sync1   laufende Kopien von der anderen Box
/spike_<hash>.mp3          Testdownloads von dl:, weg mit dlclean:
/temp_episodes.txt         Folgenliste des zuletzt gelesenen Feeds (Karte „Episodes“)
/temp_episode              Folge, die gerade von Hand geladen wird
```

`determineAlbumDir(tag)` nimmt den ersten Ordner, dessen Name auf die Kennung endet. Gibt es keinen, legt sie
`/<tag>` an. Die Web-UI blendet die Kennung aus und hängt sie beim Umbenennen wieder an. Steht `vol[N]` im Pfad, also
im Ordner- oder Dateinamen, begrenzt das die Lautstärke auf N (`handleParams()`).

`index.txt` entsteht beim ersten Laden aus dem Ordner, sortiert nach Name. Uploads, Löschen und der Podcast-Task
löschen den Index, damit er neu entsteht. Fehlt ein Titel aus dem Index, baut `playTrack()` die Playlist neu.

### Einbindung und FATFS

`SD.begin(SD_CS, spiSD, 27 MHz, "/sd", 5)`. Höchstens 5 Dateien sind über das VFS gleichzeitig offen. Wichtige
FATFS-Einstellungen dieses Frameworks:
- `FF_MAX_SS = 4096`, weil der Flash-Wear-Levelling-Treiber 4-kB-Sektoren hat. Jede Datei (`FIL`) hat einen eigenen
  Sektorpuffer, also 4 kB. Jeder VFS-Platz belegt diese 4 kB dauerhaft.
- `FF_FS_LOCK = 0`: Nichts verhindert, dass man eine offene Datei löscht, umbenennt oder kürzt. Das muss der Code
  selbst ausschließen.
- `FF_FS_REENTRANT = 1`: FATFS sperrt das Volume pro Aufruf. Verschiedene `FIL`-Objekte aus verschiedenen Tasks sind
  sicher.
- Lange Dateinamen in UTF-8. Der Laufwerkspfad für direkte FATFS-Aufrufe ist `"0:/..."`, siehe `fatPath()` in
  `podcast.h`.

### Regeln für parallelen Zugriff

1. **VFS und stdio nur aus einem Task gleichzeitig.** Mit `File`, `SD.open` und `fopen` haben sich bei einem
   Titelwechsel während eines Downloads die Handles von Wiedergabe und Download vermischt: EBADF beim Schreiben,
   EACCES beim Lesen. Ein zweiter Task benutzt deshalb FATFS direkt mit eigenem `FIL` auf dem Heap, wie der
   Podcast-Task (`f_open`, `f_write`, `f_sync`, `f_stat`, `f_unlink`, `f_rename`, `f_opendir`).
2. **`sdGate`** (`gated-fs.h`) ist ein Mutex für die Karte, solange etwas neben der Wiedergabe schreibt. Der Schreiber
   nimmt ihn blockierend um jeden Zugriff. Der Leser der Wiedergabe versucht ihn nur und liest bei belegter Karte aus
   seinem Puffer weiter. Die Hauptschleife speichert die Position nur mit `sdGateTake(0)`.
3. **`filesMutex`** (`management-server.h`) schützt im Management-Modus den Albumordner und die Dateiliste. Die
   Hauptschleife wechselt das Album, die Web-Handler laufen im AsyncTCP-Task.
4. Eine Datei, die gerade spielt, wird nie gelöscht, umbenannt oder gekürzt. Die Ausnahme ist der Neustart eines
   Downloads, der über die Generationszählung abgesichert ist (Abschnitt 6).

### Treiber-Eigenheit: falsche Schreibfehler

`sdWriteSectors()` im SD-Treiber des Arduino-Cores fragt nach jedem Mehrsektor-Schreiben den Kartenstatus ab (CMD13).
Ist dort irgendein Bit gesetzt, meldet er einen Fehler, obwohl die Daten angekommen sind. FATFS macht daraus
`FR_DISK_ERR`, also EIO, und lässt das `FIL` danach nicht mehr schreiben. Auf beiden Karten passiert das alle paar
hundert Kilobyte. Der Podcast-Download schließt die Datei deshalb, öffnet sie mit `FA_OPEN_APPEND` neu, prüft die
Größe und schreibt den Rest des Blocks noch einmal (`reopenAt()` und `writeBlock()`). Er gibt erst nach fünf
Fehlern ohne 64 kB Fortschritt dazwischen auf. Die MD5-Summen geladener Folgen stimmen mit dem Server überein. Der
Upload-Schreiber im Management-Modus wiederholt noch nicht.

## 5. Wiedergabe

### Player und PlayerI2S

`Player` in `player.cpp` verwaltet Ordner, Playlist und `last_played.txt`. `PlayerI2S` in `playerI2S.h` kapselt die
Audio-Bibliothek.

- `next()` springt ans Ende und wieder an den Anfang. `previous()` geht in den ersten 5 s eines Titels einen Titel
  zurück, sonst an den Anfang des aktuellen.
- `playTrack(path, pos)` startet einen Titel über `connecttoFS(card(), path, pos)`. `card()` liefert `gatedFS`,
  solange der Podcast-Task läuft, sonst `SD`. Scheitert der Start, prüft `startFile()`, ob die Karte noch antwortet.
  Ein Neustart gibt es nur bei einer toten Karte. Zu wenig RAM für den Decoder, etwa 23 kB, ist kein Grund dafür.
- **Fortsetzen:** `resumePosition()` ist `getFilePos() − inBufferFilled()`, abzüglich `RESUME_REWIND_S` (5 s) mal
  Bitrate, nie innerhalb des ID3-Tags. `savePlayingState()` schreibt Pfad und Position. Das passiert beim Abnehmen
  des Tokens, jede Minute während der Wiedergabe und vor dem Ausschalten am Albumende. „Play next“ in der Web-UI
  schreibt nur den Pfad, der Titel beginnt dann von vorn. Eine Position ab der Dateigröße wird verworfen.
- **Lautstärke:** Das Poti wird auf 2 bis `maxVolume` abgebildet. `maxVolume` ist 17 (sicher) oder 21 (laut, serieller
  Befehl `maxVolume:unsafe`), begrenzt durch `vol[N]`.
- **Klang:** `low`, `mid` und `high` liegen im NVS-Namensraum `settings` und gehen an `audio.setTone()`.
- `playClick()` und `playAlert()` stoppen den Titel und spielen `tap.mp3` bzw. `alert.mp3` in einer blockierenden
  Schleife. `audio_eof_mp3()` ignoriert das Ende dieser beiden Dateien.

### Audio-Bibliothek ESP32-audioI2S 3.0.0

- Titel nur mit `connecttoFS(fs, path, resumePos)` starten. `setFilePos()` und `setTimeOffset()` können die Mutex der
  Bibliothek verklemmen. Zum Fortsetzen deshalb neu starten, wie `playat:` es tut.
- Die Bibliothek fragt vor jedem Lesen die Datei ab (`if (audiofile)`). Liefert `operator bool` einmal `false`, liest
  sie diese Datei nie wieder. `GatedFile` meldet deshalb „offen“, bis die Bibliothek sie schließt.
- Vor dem Lesen des MP3-Kopfs gibt es keine Prüfung auf das Dateiende. Ein Lesefehler ganz am Anfang hängt also für
  immer, wenn niemand neu öffnet.
- Das Dateiende kommt, wenn die gelesenen Bytes `audiofile.size()` erreichen. Die Größe wird also laufend neu
  abgefragt. Darauf baut die Wiedergabe der wachsenden Datei auf.
- Große ID3-Tags überspringt sie nur 256 Bytes pro Schleifendurchlauf. Der Podcast-Download lässt sie deshalb weg.
- Die Bibliothek installiert I²S schon im Konstruktor des globalen `Audio`-Objekts, mit 16 × 512 Frames, also 32 kB.
  `startAudio()` installiert den Treiber mit 8 × 512 Frames neu, also 16 kB und etwa 90 ms. Im Bluetooth-Modus nutzt
  A2DP den Treiber aus dem Konstruktor weiter. Die Meldung „i2s_driver_install failed“ dort ist harmlos.
- `audio_eof_mp3(info)` bekommt nur den Dateinamen ohne Ordner.

## 6. Podcast-Alben

### Ablauf

1. **Auflegen:** Liegt im Albumordner eine `podcast.txt`, ruft `main.ino` `podcastStart(dir)` auf und wechselt in den
   Zustand `podcastLoading`. Ist für denselben Ordner schon ein Task aktiv, etwa weil der Token wieder aufgelegt
   wurde, läuft dieser einfach weiter.
2. **Podcast-Task** (`podcastRun()`, Core 0, Priorität 1, 12 kB Stack):
   1. Zustand und `podcast.txt` lesen.
   2. Aufräumen: Alles über die neuesten `keep` Folgen hinaus wird gelöscht, die aktuelle Folge bleibt. Das passiert,
      bevor etwas spielt.
   3. WLAN mit den Zugangsdaten aus NVS (`readSavedCredentials()`), Zeitlimit 10 s. Ohne WLAN: Phase `POD_OFFLINE`.
   4. Feed lesen, die neueste Folge bestimmen. Ist sie kein MP3: `POD_UNSUPPORTED`.
   5. Liegt die Folge vollständig auf der Karte, also gleicher Name, `complete` und gleiche Größe: `POD_UP_TO_DATE`,
      ohne die Episoden-URL anzufassen.
   6. Sonst den Zustand schreiben, `POD_DOWNLOADING`, `downloadEpisode()`.
   7. Am Ende WLAN aus, Task beendet sich.
3. **Wiedergabe:** `podcastLoading` wartet, bis 64 kB auf der Karte sind (`podcastPlayable()`), der Task fertig ist
   oder 20 s vergangen sind (`PODCAST_LOADING_TIMEOUT_MS`). Dann entscheidet `startPodcastPlayback()`:
   - Hat der Task gerade den Download einer Folge begonnen, die noch gar nicht auf der Karte lag
     (`podcastNewDownload`), spielt sie von vorn.
   - Sonst geht es mit dem zuletzt gehörten Titel an der gemerkten Stelle weiter, egal welcher es ist. Die neueste
     Folge hat Timon dann vermutlich schon gehört.
   - Ist nichts gemerkt, beginnt die Reihe von vorn, also bei der neuesten Folge.
   Bei einer wachsenden Datei bleibt die Startposition deutlich hinter dem Geladenen.
4. **Reihenfolge und Ende:** Die Folgen laufen von neu nach alt, danach die übrigen Dateien des Ordners nach Namen,
   etwa Uploads (`podcastOrderNext()` in `podcast-feed.h`, `Player::podcastNextIndex()`). Nach der letzten speichert
   die Box den Anfang der Reihe an Position 0 und geht aus. Das nächste Auflegen beginnt so wieder bei der neuesten.
   Klopfen geht wie im Album einen Titel vor oder zurück.

`podcastStop(wait, timeoutMs)` setzt `podcastCancel` und wartet auf das Ende des Tasks. Das passiert vor dem
Ausschalten, vor dem Bluetooth-Neustart und vor dem Management-Modus, dort mit bis zu 20 s, weil dessen WLAN erst
starten darf, wenn das des Tasks aus ist. Ein TLS-Handshake kann bis zu 15 s brauchen, bis er aufgibt.

### Feed (`podcast-feed.h`)

- `FeedParser` liest RSS 2.0 mit iTunes-Erweiterungen streamend mit festem Speicher. Er nimmt die neueste Folge nach
  `pubDate`, überspringt Trailer (`itunes:episodeType`) und kennt Entities, CDATA, RFC 822, ISO-Daten und deutsche
  Monatsnamen.
- Er hört nach 5 Items auf, außer die Daten steigen an. Dann liest er den ganzen Feed, höchstens 8 MB.
  `itunes:new-feed-url` zeigt die Web-UI als Hinweis an, die URL wird nicht überschrieben.
- Ob eine Folge MP3 ist (`isMp3()`): Ein ausdrücklich anderer MIME-Typ schlägt die Endung `.mp3`. Die Endung zählt nur
  bei leerem oder generischem Typ.
- **Dateiname:** `YYYY-MM-DD_HHMM_<Titel>_<id>.mp3`, höchstens 100 Zeichen. `<id>` sind 4 Hex-Zeichen aus
  `episodeIdHash()` (FNV-1a) über die GUID, ersatzweise über die URL. Ohne Datum steht dort `0000-00-00_0000`. Die
  Namen sortieren nach Datum, die neueste Folge ist also die letzte. `isEpisodeFileName()` erkennt das Muster.
- `planResume()` entscheidet aus lokaler Größe, Range-Unterstützung, Servergröße und ID3-Größe, ob die Folge
  vollständig ist, weitergeladen wird oder neu beginnt. Sie bindet das Weiterladen an die beim Start gespeicherte
  Servergröße. Hoster mit eingefügter Werbung wie Podigee liefern sonst eine andere Datei unter derselben URL.

### Download (`podcast.h`)

- `HttpStream`: Weiterleitungen von Hand, höchstens 10, auch zwischen http und https. HTTP/1.0, damit Server nicht
  chunked antworten, mit Chunked-Decoder als Rückfall. TLS ohne Zertifikatsprüfung (`setInsecure`), Handshake-Limit
  15 s, Lese-Timeout 10 s, eigener User-Agent. `close()` stoppt die Clients selbst. Das verhindert einen Absturz durch
  einen hängenden TLS-Client im `HTTPClient`.
- **Probe:** `Range: bytes=0-9`, zuerst über die http-Variante der URL, denn nur eine echte 206-Antwort zählt. Sie
  liefert ID3-Größe und Gesamtgröße. Danach kommt der Körper bevorzugt per http vom End-Host der Weiterleitungen, aber
  nur, wenn dort dieselbe Datei liegt (`sameFile()`). Das spart rund 45 kB RAM für TLS während der Wiedergabe.
  `EpisodeSource` merkt sich das Ziel für Wiederholungen, die dann ohne neue Probe und ohne TLS auskommen.
- **Schreiben:** 4-kB-Blöcke. Ein `f_sync` bei 64 kB, dann alle 128 kB und früher, wenn der Leser hungert (16 kB).
  Erst `f_sync` aktualisiert den Verzeichniseintrag, also die Größe, die ein neuer Leser sieht.
- **Wiederholungen:** 4 Netzversuche mit 2, 5 und 10 s Pause, jeweils per Range fortgesetzt. Schreibfehler werden an
  Ort und Stelle wiederholt (Abschnitt 4). Eine Verbindung unter 8 kB/s über 15 s wird ersetzt, bis zu 20-mal.
- **`podcast_state.txt`:** `file`, `guid`, `title`, `date`, `total`, `skip`, `size`, `complete`, `result`, `error`,
  `checked` (Datum des Servers), `feedTitle`, `newFeedUrl`. Geschrieben über eine Temp-Datei und Umbenennen. Vor dem
  ersten Byte speichert der Task Servergröße und Sprung, damit ein Stromausfall nicht zu falschem Weiterladen führt.

### Wachsende Datei abspielen (`gated-fs.h`)

`GatedFSImpl` und `GatedFile` sind ein eigenes `fs::FS` für die Audio-Bibliothek, nur zum Lesen. Der Downloader
beschreibt die Datei über `growingStart()`, `growingSynced()` und `growingDone()`. Dazu gehören Pfad, gesicherte
Bytes, Endgröße, `done`, `hungry` und eine Generation.
- `size()` meldet die erwartete Endgröße, bei unbekannter Größe `0x7FFFFFFF`. `read()` gibt nur gesicherte Bytes
  heraus. FATFS friert die Größe beim Öffnen ein. Ist dieser Stand aufgebraucht, öffnet `GatedFile` die echte Datei
  neu.
- Holt die Wiedergabe den Download ein, liefert `read()` 0 Bytes, setzt `hungry` und wartet auf 32 kB Vorsprung
  (`RESUME_LEAD`). Die Bibliothek wartet still.
- Bricht der Download ab, sinkt die Größe auf das Gesicherte. Die Wiedergabe endet dann sauber.
- **Generation:** Jedes Öffnen durch den Schreiber zählt sie hoch, im selben `sdGate`-Abschnitt wie das Öffnen. Das
  Öffnen beim Neustart kürzt die Datei. Ein Leser vergleicht die Generation unter dem Gate und öffnet neu, bevor er
  weiterliest. So liest er nie über ein Handle aus der Zeit vor dem Kürzen in freigegebene Cluster.
- **Lesefehler:** Datei schließen, die bekannte Größe behalten, nach 200 ms neu öffnen (`RETRY_MS`). `operator bool`
  bleibt `true`, siehe Abschnitt 5.

### RAM

Im Player-Modus gibt `setup()` den nie genutzten Bluetooth-Speicher frei (`esp_bt_mem_release`, etwa 49 kB). TLS
läuft nur für den Feed, bevor etwas spielt. Die Folge kommt bevorzugt per http, und die I²S-DMA-Puffer sind halbiert.
Messwerte stehen in Abschnitt 10.

### Testen

`dl:<url>` lädt eine URL über denselben Download-Code nach `/spike_<hash>.mp3`. `dlplay:` spielt die Datei, auch
während sie wächst, `dlstop:` bricht ab, `dlclean:` löscht die Testdateien. `podcasturl:<url> <keep>` setzt den Feed
des aufgelegten Albums. `dltls:1` erzwingt https für Folgen. `tools/podcast-test-server.py` stellt schwierige Server
nach. Bewährte Prüfungen nach Änderungen am Download- oder Lesepfad:
- mehrere Titelwechsel per `play:<i>` während eines Downloads, danach `audio:`;
- `dlplay:` während des Downloads;
- Testserver mit `--no-range --drop-after 2000000`, also Neustart beim Abspielen;
- MD5 gegen `curl`.

## 7. Management-Modus

### Ein- und Ausstieg

Der Management-Token startet `startManagementServer()`. Die Methode ruft die Destruktoren des `Audio`-Objekts und der
Playlist auf, um RAM für WLAN und Web-Server frei zu machen. Danach spielt nichts mehr. Verlassen wird der Modus nur
über einen Neustart. Der Nutzer tauscht den Management-Token gegen einen Album-Token. Dessen Ordner bearbeitet die
Web-UI dann (`selectAlbumForManagement()`). Nimmt er den Album-Token ab, geht die Box aus und startet neu. Liegt nach
dem Management-Token 60 s lang kein Token auf, ebenso.

### Netz (`wifi-setup.h`)

- Zustände `NET_CONNECTING`, `NET_STATION` und `NET_ACCESS_POINT`. Die Zugangsdaten liegen im eigenen NVS-Namensraum
  `wifi`, mit `WiFi.persistent(false)`. Kein Netz gespeichert oder 15 s ohne Verbindung: offener Hotspot `BOX_NAME`
  mit Captive-Portal-DNS. 30 s Verbindungsverlust im Heimnetz führen ebenfalls zum Hotspot. Danach versucht die Box
  das Heimnetz nicht wieder.
- Web-Handler fassen das Funkmodul nie an. Sie legen Aktionen ab (`pendingAction`), und `netLoop()` in der
  Hauptschleife führt sie aus, mit 400 ms Verzögerung, damit die Antwort noch ankommt.
- **mDNS:** Hostname ist `BOX_NAME` in Kleinbuchstaben, also `timon-box` oder `julius-box`. Dazu der Dienst
  `_http._tcp` mit TXT `box=<BOX_NAME>`, an dem die andere Box sie erkennt. Im Heimnetz des Nutzers kommen
  Multicast-Anfragen nie bei der Box an. Unicast-Anfragen an Port 5353 beantwortet sie, eigene Ankündigungen erreichen
  die Rechner. `announceMdnsPeriodically()` kündigt deshalb jede Minute neu an, indem es den Port des Dienstes neu
  setzt. Das hält den Namen in den Caches der Rechner, die ihn sonst nach 120 s vergessen.
- `directAddress()` liefert die IP im Heimnetz, im Hotspot leer. Die Seiten zeigen sie im Fuß und nutzen sie als
  Rückfall (unten).
- `/speedtest` liefert 4 MB Füllbytes zum Messen des WLAN-Durchsatzes.

### Web-Server (`management-server.h`)

Die Handler laufen im AsyncTCP-Task. SD-Zugriffe und der Albumzustand gehen über `filesLock()`. Kommt der Lock nicht,
antwortet `sendBusy()` mit 503. Seiten werden in einen `PageBuffer` geschrieben und mit `sendBuffered()` gesendet. So
braucht keine Seite einen großen zusammenhängenden Block, und es geht nur ein TCP-Segment pro Aufruf raus. Ein großer
`tcp_write` scheiterte auf zerstückeltem Heap still und verschluckte den Anfang der Seite.

Middleware: zuerst das Anfragelog (`[http] GET /... from ..., heap ...`), dann CORS. CORS lässt nur den eigenen
`.local`-Ursprung herein, beantwortet `OPTIONS` mit 204 und überschreibt keine Header, die ein Handler selbst gesetzt
hat. `/wifi-status` hat für die Hotspot-Seite sein eigenes `*`.

| Route | Zweck |
|---|---|
| `GET /` | Titelliste mit „Play next“ und Löschen, 10 pro Seite |
| `GET /home` | Status, Upload, Ordner umbenennen, Album löschen |
| `GET /podcast` | Podcast-Status, Feed-Suche, Feed und `keep` setzen |
| `GET /copy` | Album von der anderen Box kopieren |
| `GET /wifi` | WLAN wählen, testen, vergessen |
| `GET /css`, `GET /app.js` | Stylesheet und Seiten-Skript aus dem Flash, mit Inhalts-Hash als `?v=` |
| `GET /album` | JSON: gewählter Ordner, Titelzahl, für den „Which album?“-Hinweis |
| `POST /upload?name=` | Upload als roher Body (Abschnitt unten) |
| `POST /select`, `/delete` | „Play next“ (schreibt `last_played.txt` ohne Position), Titel löschen |
| `POST /setFolderPath`, `/deleteAlbum` | Ordner umbenennen, Album samt Ordner löschen |
| `POST /setPodcast` | `podcast.txt` schreiben, mit `remove=1` löschen. URL prüfen: http(s), ≤ 500 Zeichen |
| `POST /episodes-load`, `GET /episodes?offset=&limit=` | Feed auf der Box lesen, eine Seite Folgen mit „On the box“ |
| `POST /episodes-download`, `GET /episodes-status`, `POST /episodes-cancel` | Ausgewählte Folgen laden, Fortschritt, Abbruch |
| `GET /wifi-scan`, `/wifi-status`, `POST /wifi-connect`, `/wifi-forget` | WLAN-Seite |
| `GET /peer-info`, `/peer-album`, `/peer-file` | Serverseite der Kopierfunktion |
| `GET /copy-status`, `/copy-scan`, `POST /copy-list`, `/copy-start`, `/copy-cancel` | Kopierseite |

Unbekannte Pfade leiten auf `/home` um.

**Upload:** Der Browser lädt zwei Dateien gleichzeitig hoch (`data-par="2"`), jede als roher Body. Der AsyncTCP-Task
kopiert die Daten nur in einen Pool aus 4 Puffern zu 4 kB. Der Schreib-Task `sdwriter` (Core 1, Priorität 3, 6 kB
Stack) schreibt sie nach `/temp_<n>` und benennt die Datei am Ende in den Albumordner um. Ein gleichnamiger Titel
wird dabei überschrieben, und `index.txt` wird gelöscht. Das Empfangs-Timeout ist 60 s. Bricht der Client ab, räumt
`UP_ABORT` die Temp-Datei weg. `uploadsActive` zählt laufende Uploads. Während die Box einen Feed per TLS liest, leiht
sie dem TLS die 16 kB des Pools (`uploadPoolRelease()` und `uploadPoolRefill()`) und weist Uploads mit einer Meldung
ab.

### Folgen von Hand laden (`podcast-episodes.h`)

Die Karte „Episodes“ auf der Podcast-Seite zeigt die Folgen eines Feeds und lädt ausgewählte in den Albumordner.
Den Feed liest die Box, denn die meisten Podcast-Server senden keine CORS-Header. MDR erlaubt es, BR und WDR nicht.
- Ein Worker-Task mit 12 kB Stack startet bei Bedarf, für jeweils einen Auftrag.
  - **Liste:** `fetchFeed()` mit einem `FeedParser`, der jede Folge an `onItem` meldet und den ganzen Feed liest,
    höchstens 8 MB. Jede MP3-Folge kommt als Zeile nach `/temp_episodes.txt`: Datum, Datum als Text, Größe laut Feed,
    Dateiname aus `episodeFileName()`, URL, Titel. Dazu ein Index im RAM, sortiert neu nach alt, höchstens 1000
    Folgen. Der 1,1 MB große MDR-Feed mit 462 Folgen dauert etwa 16 s.
  - **Download:** Die Folgen nacheinander mit `downloadEpisode()` nach `/temp_episode` laden. Dann unter `filesLock()`
    in den Albumordner umbenennen, `filesInDir` aktualisieren, `index.txt` löschen und den Namen in
    `podcast_kept.txt` eintragen. Den Fortschritt liefert `growingGet()`, Abbrechen geht über `podcastCancel`.
- Kein Start, während eine Kopie, ein Upload oder schon ein Auftrag läuft, und nur im Heimnetz. Während eines Auftrags
  lehnen Kopie, Umbenennen und Löschen des Albums mit 409 ab.
- Die Größenangaben im Feed sind oft falsch, bei MDR zu groß. Die Platzprüfung der Seite ist dadurch nur vorsichtiger.
  Nach dem Download zeigt der Status die echte Größe.
- **Speicher:** TLS braucht im Management-Modus rund 50 kB von etwa 85 kB freiem Heap. Ohne Entlastung sank der freie
  Heap beim Lesen des MDR-Feeds auf 12,5 kB, und eine gleichzeitig angeforderte Seite scheiterte („Failed to
  allocate“). Deshalb leiht der Worker dem TLS die Upload-Puffer, und die Seite fragt beim Lesen nur alle 2 s nach.
  Die Downloads selbst laufen meist per http mit rund 60 kB frei.

### Web-UI (`web-ui.h`)

- Ein neuer Reiter ist ein Eintrag in `NAV_ITEMS`. Seiten beginnen mit `pageHead(res, request, "<key>")` und enden
  mit `pageFoot(res)`. Einhängepunkte: `pageSubtitle`, `navItemVisible`, `pageNotice`, `pageDirectAddress`.
- `PAGE_CSS` und `PAGE_JS` sind Raw-String-Literale. `PAGE_JS` ist ein einziges Skript für alle Seiten. Jeder Teil
  schaltet sich über die IDs seiner Elemente ein. Texte sind Englisch, der Stil folgt Apples Human Interface
  Guidelines, hell und dunkel per Cookie `theme`.
- **IP-Rückfall:** `boxFetch(path)` und `boxUrl(path)` versuchen zuerst den Namen. Scheitert eine Anfrage, gehen sie
  und alle weiteren an `http://<BOX_IP>`. `BOX_IP` steht im Kopf jeder Seite. Ein Upload, der mit einem Netzwerkfehler
  abbricht, startet einmal über die IP neu. Normale Links und Formulare kann das Skript nicht umleiten. Neue
  Abfragen per Skript sollten `boxFetch` benutzen.
- Die Podcast-Suche fragt die iTunes-Suche per JSONP, weil Apples API keine CORS-Header sendet.
- HTML immer mit `htmlEscape()`, JSON mit `jsonEscape()` maskieren.
- **Favicon:** eine Token-Scheibe mit weißem Play-Zeichen, drumherum transparent, als SVG aus dem Flash
  (`FAVICON_SVG`, auch unter `/favicon.ico`). Die Farbe kommt aus `BOX_COLOR` in `platformio.ini`: Timon kupfer,
  Julius petrol, damit sich die Tabs unterscheiden.
- In einem Podcast-Album sortiert die Dateiliste wie die Wiedergabe: Folgen neu nach alt, dann die übrigen Dateien.
- **Größe und Dauer** stehen bei jedem Titel: in der Dateiliste, beim Kopieren, bei den Podcast-Folgen und vor dem
  Upload. Die Dauer liest `mp3DurationMs()` aus dem Kopf der Datei: ID3-Header lesen, hinter den Tag springen, 4 kB
  lesen, dann `mp3Info()` aus `mp3-info.h`. Mit Xing/Info- oder VBRI-Header ist sie exakt, sonst aus Größe und Bitrate
  berechnet. Bei Folgen, die noch nicht auf der Karte liegen, kommen Dauer und Größe aus dem Feed (`itunes:duration`,
  `length`); die Größe zeigt die Seite dort als ungefähr an. Vor dem Upload bestimmt der Browser die Dauer, eine Datei
  nach der anderen. `fmtD()` und `formatDuration()` schreiben sie als `26:11` oder `1:02:33`.

### Kopieren zwischen den Boxen (`box-sync.h`)

Die liefernde Box braucht nur ihren Management-Token: `/peer-album` und `/peer-file` finden das Album über die
Kennung in der Anfrage. `/peer-album` liefert pro Titel eine Zeile `<Größe>,<Dauer in ms>\t<Name>`. Ältere Firmware
liest die Größe mit `strtoul` und hört am Komma auf, versteht die Zeilen also weiterhin. Die holende Box startet bei Bedarf einen Worker-Task mit 6 kB Stack. Er lädt die Titelliste,
ermittelt, was lokal fehlt, und holt zwei Dateien parallel, jede über eine eigene Temp-Datei (`/temp_sync0`,
`/temp_sync1`). Die Kopierseite stellt nur Aufträge ein
und fragt den Status ab. Die Übertragung überlebt also ein Neuladen der Seite. Die unvollständige Podcast-Folge wird
nicht angeboten, `podcast.txt` wird nicht mitkopiert. Die Gegenstelle findet die Box über eine mDNS-Dienstabfrage.
Das ist Multicast und scheitert im Heimnetz des Nutzers wahrscheinlich. Man kann den Namen oder die IP auch eintippen,
etwa `julius-box`, `192.168.0.157` oder `host:8080`.

## 8. Bluetooth-Modus

Der Bluetooth-Token setzt `bluetoothMode` (`RTC_NOINIT_ATTR`, überlebt `ESP.restart()`), stoppt den Podcast-Task und
startet neu. `setup()` glaubt dem Flag nur nach einem Software-Reset. Es ruft `startBluetooth()` auf: A2DP-Senke mit
Namen `BOX_NAME`, automatische Wiederverbindung. Klopfen schickt dort Vor und Zurück an das Handy. Abnehmen setzt das
Flag zurück und startet neu. Im Player-Modus ist der Bluetooth-Speicher freigegeben. Bluetooth geht dort also nur nach
einem Neustart. Ton funktioniert, die I²S-Fehlermeldung beim Start ist harmlos (Abschnitt 5).

## 9. Klopfsensoren (`knock.cpp`)

Das Prinzip steht im README. Ein Druck ist ein einzelner ununterbrochener Buckel im ADC-Signal. Er zählt, wenn er
lange genug dauert (`KNOCK_MIN_PRESS_MS`) oder seine Fläche groß genug ist (`RKS_THRESHOLD` und `LKS_THRESHOLD`).
Musik erzeugt dagegen viele kurze Halbwellen mit Pausen.

- Abgetastet wird mit 2 kHz von einem `esp_timer` (`ESP_TIMER_TASK`, `skip_unhandled_events`). Der Timer liest
  `adc1_get_raw()` direkt, weil `analogRead()` jedes Mal 100 µs Pin-Setup kostet. Die Hauptschleife käme in der
  Wiedergabe nur alle 9 ms vorbei.
- Fehlen mehr als 2 ms Abtastungen (`maxBlindUs`), etwa durch WLAN oder Flash-Zugriffe, beginnt der Buckel neu. Das
  verhindert Fehlauslösungen bei Downloads.
- Nach einem Druck folgen 800 ms Sperre. Ein Druck, der älter als 200 ms ist, wird nicht nachgeholt.
- `loop()` führt die Aktion in der Hauptschleife aus und meldet `knock <pin>: press after <ms>, peak, area`.
- Die Schwellen stehen pro Box in `platformio.ini`. Julius hat dünnere Wände, höhere Pegel und kürzere Lücken.
  `knockdebug:1` zeigt pro Sekunde Statistiken, `knockdebug:2` zusätzlich den Rohverlauf des nächsten Buckels.
- Die Namen sind vertauscht: `leftKnockSensor` liest `RKS_PIN` (34) und ruft `previous()` auf. `rightKnockSensor`
  liest `LKS_PIN` (35) und ruft `next()` auf. Auf der Platine ebenso: GPIO 34 hängt an J2 „R Btn“, dessen Piezo an
  der linken Wand klebt.

## 10. Tasks, Kerne und Speicher

| Task | Kern, Priorität | Stack | Aufgabe |
|---|---|---|---|
| Arduino `loopTask` | 1 | 8 kB | Zustandsautomat, `audio.loop()`, Netz-Zustandsautomat |
| `podcast` | 0, 1 | 12 kB | Feed, Download, auch für `dl:` |
| `esp_timer` | 0 | | Abtastung der Klopfsensoren |
| `sdwriter` | 1, 3 | 6 kB | Upload auf die Karte |
| `episodes` | 0, 1, bei Bedarf | 12 kB | Feed lesen und Folgen von Hand laden |
| Sync-Worker | bei Bedarf | 6 kB | Kopie von der anderen Box |
| AsyncTCP, WLAN | | | Web-Server, Funk |

Gemessene Werte für den freien Heap (Oktober 2026, nach der Umstellung auf 5 VFS-Dateien):

| Situation | frei | größter Block |
|---|---|---|
| nach dem Start, vor der BT-Freigabe | 150 kB | |
| nach der BT-Freigabe | 198 kB | 110 kB |
| Podcast-Task startet | 186 kB | 86 kB |
| Feed per TLS, Minimum | 60–67 kB | |
| Download per http und Wiedergabe | 65–70 kB | 36–45 kB |
| Management-Modus mit WLAN | ~100 kB | 65 kB |

TLS neben der Wiedergabe war vor der Umstellung mit etwa 16 kB frei und 9 kB größtem Block sehr knapp. Danach wurde es
nicht neu gemessen. `heap:` zeigt den aktuellen Stand, `heap:<s>` loggt periodisch.

## 11. Testen und Messen

- **Host:** `tools/host-test.sh` übersetzt `test/host/feed_test.cpp` mit g++ und prüft es gegen echte Feeds in
  `test/host/fixtures/`: WDR, MDR, Kakadu, BR, Simplecast, Podlove, Podigee und Anchor. `expected.txt` erzeugt
  `make_expected.py` unabhängig vom Parser. Neue Fälle gehören hierher.
- **Box:** Mit `tools/serial-bridge.py` einen Logger starten und Befehle über die FIFO schicken. Das Log zählt ab dem
  Start des Loggers, nicht ab dem Start der Box. Nach dem Flashen den Logger sofort starten, sonst fehlt das
  Startprotokoll.
- **Netz:** Unicast gegen Multicast mit `dig @<ip> -p 5353 <name>.local` und `dig @224.0.0.251 -p 5353 <name>.local`.
  Erreichbarkeit mit `ping`. Eine Box im Netz findet man über die Titelseite: `curl http://192.168.0.x/` enthält
  `<title>Timon-Box`.
- Der Nutzer hilft gern: Klopftests, Bluetooth, Uploads im Browser, Token auflegen. Sag ihm genau, was er wann tun
  soll, und warne ihn, bevor Flashen oder Auslesen der MAC die Box neu startet.

## 12. Offene Punkte und Ideen

- Der Upload-Schreiber wiederholt Schreibfehler nicht wie der Podcast-Download.
- Nach 30 s Verbindungsverlust bleibt der Management-Modus im Hotspot, ohne das Heimnetz erneut zu versuchen.
- Die Kopierfunktion findet die andere Box über mDNS-Multicast. Das klappt im Heimnetz des Nutzers wohl nicht.
- `podcast.txt` wird beim Kopieren nicht mitgenommen. M4A- und AAC-Podcasts werden nicht unterstützt.
- Die Schreibfehler-Prüfung des SD-Treibers ließe sich mit einer lokalen Kopie der SD-Bibliothek unter `lib/`
  entschärfen.
- SD-Schreibzugriffe dauern gelegentlich 100–360 ms. Ob das hörbare Aussetzer gibt, ist nicht bestätigt.
- WLAN schon beim Booten zu starten, würde beim Podcast 2–4 s bis zum ersten Ton sparen.
