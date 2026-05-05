# CLAUDE.md

## Projektübersicht

ADD2go ist eine DIY Funkbrücke für die Dinamica-Generale-ADD2-Waage am
Siloking-Mischwagen. Zwei ESP32-Module: ein **Sender** im Mischwagen liest die
Waage per RS232 aus, ein **Empfänger** im Radlader zeigt das Gewicht auf einem
TFT-Display an. Kommunikation per WLAN (UDP-Broadcast für die Hardware,
WebSocket für eine eingebaute WebApp). Privates Hobby-Projekt, Lizenz CC
BY-NC-SA 4.0.

## Aktuelle Versionen

- **Sender:** v1.2.4 (Sketch-Ordner: `add2go_sender_v1_2_4_1/`)
- **Empfänger:** v1.0.1 (Sketch-Ordner: `add2go_empfaenger_v1_0_1/`)

## Hardware

### Sender (Mischwagen)
- ESP32-WROOM-32U mit externer 2.4-GHz-Antenne (U.FL)
- MAX3232CPE für RS232↔TTL-Pegelwandlung
- K7805M Schaltregler (12 V → 5 V, pin-kompatibel zu 7805), 1N5822 Verpolschutz, 1.5KE18A TVS
- Anschluss an Waage: Binder M16 5-pol Stecker (Bestellnr. 09 0313 00 05)
- Pins: RS232-RX = GPIO16 (RXD2), TX = GPIO17 (ungenutzt)

### Empfänger (Radlader)
- ESP32-WROOM-32U mit externer Antenne
- ILI9341 3.2" TFT 320×240 mit XPT2046 Touch
- ADS1115 16-bit ADC (I²C 0x48) für optionalen Schaufel-Drucksensor
- Reed-Kontakt zur Erkennung der Wiegeposition
- Pins: TFT CS=5 / RST=2 / DC=4, SPI MOSI=23 SCK=18 MISO=19,
  Touch CS=15 IRQ=14, I²C SDA=21 SCL=22, Reed=GPIO36

## Kommunikation

- WLAN-AP `ADD2go` (offen) auf 192.168.4.1, Sender ist APSTA → optional
  zusätzlich im Heimnetz unter `add2go.local`
- UDP-Broadcast auf 192.168.4.255:5005 (Sender → Empfänger), 10 Hz, ASCII-Gewicht oder `OFF`
- WebSocket Port 81 (Sender ↔ Browser), Format `Gewicht|RSSI`
- HTTP Port 80 (eingebaute WebApp + WLAN-Setup)

## Libraries

**Sender:**
- WebSockets by Markus Sattler (Arduino Library Manager)
- Rest ist im ESP32-Core enthalten (WiFi, WebServer, ESPmDNS, Preferences,
  esp_task_wdt, esp_wifi)

**Empfänger:**
- TFT_eSPI by Bodmer
- XPT2046_Touchscreen by Paul Stoffregen
- Adafruit ADS1X15 by Adafruit

## Kompilieren / Flashen

Beide Sketches mit der **Arduino IDE** (1.8.19 oder neuer, ESP32 Board Package
v3.x) bauen.

- Board: **ESP32 Dev Module**
- Upload-Baudrate: 115200
- Sketch öffnen: jeweils die `.ino` im gleichnamigen Ordner
- Sender: `add2go_sender_v1_2_4_1/add2go_sender_v1_2_4_1.ino`
- Empfänger: `add2go_empfaenger_v1_0_1/add2go_empfaenger_v1_0_1.ino`

Serial Monitor zum Debuggen ebenfalls auf 115200.

## ⚠️ TFT_eSPI - WICHTIG vor jedem Flash des Empfängers

Die **TFT_eSPI**-Library hat eine **globale** Konfigurationsdatei
`Arduino/libraries/TFT_eSPI/User_Setup.h`, die für *alle* Projekte gilt, die
diese Library verwenden. Wenn die Pins darin nicht zu ADD2go passen, hängt der
Empfänger beim Boot in `tft.init()` ohne Fehlermeldung.

Vor dem Flashen muss `User_Setup.h` enthalten:

```cpp
#define ILI9341_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320
#define TFT_CS   5
#define TFT_DC   4
#define TFT_RST  2
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_MISO 19
#define SPI_FREQUENCY 40000000
#define TOUCH_CS 15
```

Im Repo-Root liegt eine fertige `User_Setup.h` als Vorlage - die kann direkt in
den TFT_eSPI-Library-Ordner kopiert werden.

## Projektstruktur

```
ADD2go/
├── add2go_sender_v1_2_4_1/      # Sender-Sketch (Arduino-Ordner)
├── add2go_empfaenger_v1_0_1/    # Empfänger-Sketch (Arduino-Ordner)
├── add2go_pcb/                  # KiCad-Projekt für die Sender-Platine
├── case/                        # 3D-Druck-STLs für Empfänger-Gehäuse
├── images/                      # Pinout-Bilder & Foto-Doku
├── User_Setup.h                 # Vorlage für TFT_eSPI (in Library kopieren)
├── README.md                    # Ausführliche Doku
└── LICENSE                      # CC BY-NC-SA 4.0
```

Sender-Gehäuse ist gekauft (Industrie-Gehäuse 100×68×50 mm, Link in der README).
Nur das Empfänger-Gehäuse ist 3D-gedruckt.
