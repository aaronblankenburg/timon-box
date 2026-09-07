# Timon Box
## Hardware Bring up instructions

### Architecture
```mermaid
graph TD;
    A["ESP32"]
    B["USB2Serial"]
    C["Programming Interface"]
    D["Audio MAX"]
    E["RFID Reader"]
    F["BMS"]
    G["Battery"]
    H["Vol BTN"]
    I["On/Off BTN"]

    pwr_h["5V"]
    pwr_l["3.3V"]

    pwr_h ---> D
    pwr_l ---> B
    pwr_l ---> A
    pwr_l ---> E
    F ---> pwr_h
    F ---> pwr_l 
    A --| RX/TX |--> B 
    B --| D+/D- |--> C
    A --| SPI |--> E
    A --| I²S |--> D
    G ---> F
    H ---> A
    I ---> F
    
    classDef pwr fill:#ffcccc,stroke:#ff0000,stroke-width:2px;
    classDef btn fill:#ffffcc,stroke:#ff0000,stroke-width:2px;
    class pwr_l,pwr_h,G pwr
    class H,I btn

    linkStyle 0,1,2,3 stroke:#f00,stroke-width:2px
```
### Fotos
![HW_Bat](assets/hw_bat.jpeg)
![hw_connector](assets/hw_connector.jpeg)
![hw_connector](assets/hw_cutpcb.jpeg)
![hw_connector](assets/hw_hack.jpeg)
![hw_connector](assets/hw_overview_buttom.jpeg)
![hw_connector](assets/hw_overview_side.jpeg)
![hw_connector](assets/hw_overview_top.jpeg)
![hw_connector](assets/hw_peripherals.jpeg)

## Software Instruction
### IDE
Visual Studio Code with Platform IO Extension (platformio.platformio-ide)

### Zustandsautomat
[doc/state-machine.html](doc/state-machine.html) zeigt die Zustände der Firmware (Wiedergabe, Bluetooth, Management-Modus mit Netzmodus und Token-Regeln) als Mermaid-Diagramme in den LED-Farben, dazu alle Timeouts mit ihren Konstanten. Im Browser öffnen; Mermaid wird vom CDN geladen.

### Token-Bogen
[tools/token-sheet.html](tools/token-sheet.html) erzeugt den 10 × 15 cm Fotoabzug mit sechs runden Token-Bildern, gleiches Layout wie `src/prepare_tokens.py` (1772 × 1181 px, Kreise mit 515 px). Die Seite ist eine einzelne HTML-Datei ohne Server und ohne Abhängigkeiten, im Look der Management-Web-UI, und führt in drei Schritten durch den Ablauf: Bilder sammeln (Fotos, Kamera, Web-Links, Einfügen, Drag & Drop), jedes Bild im Kreis zuschneiden (Verschieben, Pinch-Zoom, Drehen), fertigen Bogen teilen oder als JPEG speichern. Die Bilder verlassen das Gerät nicht; eine angefangene Sitzung bleibt im Browser gespeichert.

Web-Links funktionieren nur, wenn die Seite den direkten Zugriff erlaubt (CORS); sonst das Bild im Browser kopieren oder speichern und als Datei hinzufügen.

#### Fürs Handy: die Android-App

[android/](android/) packt genau diese Datei in eine App. Sie läuft offline und überall, ohne Server und ohne Heimnetz.

```bash
cd android
./setup-toolchain.sh   # einmalig: JDK und Android-SDK
./deploy.sh            # bauen, per USB installieren, starten
```

`deploy.sh` kennt außerdem `--no-build` (nur neu installieren) und `--run-only` (nur neu starten). `tools/token-sheet.html` bleibt die einzige Quelle der Seite; Gradle kopiert sie beim Bauen ins APK. Wer nur die Seite ändert, ruft trotzdem `./deploy.sh` auf, denn ohne Server gibt es keinen anderen Weg aufs Telefon.

Die App ist eine Hülle um einen WebView und reicht drei Dinge nach, die dort fehlen: **Teilen**, **Speichern** und **Bilder empfangen**. Die Android-WebView kennt `navigator.share` nicht, und ein `<a download>` bleibt dort wirkungslos. Die App nimmt das fertige JPEG deshalb über eine native Brücke entgegen, öffnet damit den echten Teilen-Dialog und legt den Bogen unter *Bilder / Token Sheet* in der Galerie ab. Ohne App verhält sich `tools/token-sheet.html` unverändert wie vorher und lässt sich weiterhin einfach im Browser öffnen.

Umgekehrt ist die App auch ein **Empfänger für Bilder**: in der Galerie, im Browser oder sonstwo auf Teilen tippen, *Token Sheet* wählen, und das Bild landet im nächsten freien Platz. Mehrere auf einmal gehen auch, sie füllen die nächsten freien Plätze der Reihe nach. Läuft die App gerade beim Zuschneiden oder beim fertigen Bogen, springt sie zurück auf die Bilderübersicht. Ist der Bogen voll, sagt sie das und ändert nichts.

Die hereingereichten Bilder gehen nicht durch die Brücke, sondern werden in den Cache kopiert und der Seite unter derselben Herkunft ausgeliefert, unter der auch die Seite selbst läuft. Ein Handyfoto als `data:`-URL wären sonst schnell acht Megabyte Base64 quer durch `evaluateJavascript`.

Berechtigungen: nur Internet, für die Bild-Links. Die Kamera läuft über die Kamera-App, das Speichern über den MediaStore. Auf einem iPhone bleibt nur die Datei im Browser, dort ohne Teilen-Knopf.

### MAC Fix

Error with ESP Async Webserver can be fixed with changing the platformio.ini to:

lib_deps = 
	https://github.com/schreibfaul1/ESP32-audioI2S.git
	miguelbalboa/MFRC522@^1.4.10
	https://github.com/pschatzmann/ESP32-A2DP.git
	;me-no-dev/ESP Async WebServer @ 1.2.4
    https://github.com/me-no-dev/ESPAsyncWebServer.git
    https://github.com/me-no-dev/AsyncTCP.git

Compiling .pio/build/esp32dev/libf11/SPI/SPI.cpp.o
In file included from /Users/dennis/Documents/Development/TimonBox/timon-box/src/main.ino:9:
.pio/libdeps/esp32dev/ESP32-A2DP/src/BluetoothA2DPSink.h:23:2: warning: #warning "AudioTools library is not included first or installed" [-Wcpp]
 #warning "AudioTools library is not included first or installed"

lib_deps = 
    + https://github.com/pschatzmann/ESP32-A2DP.git
    + https://github.com/pschatzmann/arduino-audio-tools.git



