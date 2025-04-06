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



