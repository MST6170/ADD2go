# Hardware — add2go-empfaenger-pio

1:1 die add2go-Empfänger-PCB v1.0.2. Kein PCB-Re-Spin in Phase 3c.

## ESP32-Modul

**ESP32-WROOM-32U** mit externer Antenne (für besseren Empfang im
Feld-Einsatz unter Blechdach o.ä.). 4 MB Flash, 320 KB RAM.

Bootet auf `esp32dev`-Variante von PlatformIO mit Core 2.0.x (Arduino-Core
Version 3.20014.231204, IDF 4.4.x). Platform gepinnt:
`platform = espressif32 @ ~6.5.0` — siehe [GOTCHAS.md](GOTCHAS.md#core-version-pinnen).

## Pin-Map

| Funktion           | GPIO  | Notiz                                        |
|--------------------|-------|----------------------------------------------|
| Reed-Sensor        | 36    | Input-only, LOW = Schaufel in Wiegeposition  |
| I²C SDA            | 21    | ADS1115 + spätere I²C-Devices                |
| I²C SCL            | 22    | ADS1115 + spätere I²C-Devices                |
| Display CS         | 5     | ILI9341 (240×320)                            |
| Display RST        | 2     | ILI9341                                      |
| Display DC         | 4     | ILI9341                                      |
| SPI MOSI           | 23    | Display + Touch (shared)                     |
| SPI SCK            | 18    | Display + Touch (shared)                     |
| SPI MISO           | 19    | Display + Touch (shared)                     |
| Touch CS           | 15    | XPT2046                                      |
| Touch IRQ          | 14    | XPT2046 (PEN-Down)                           |
| USB-UART           | 1/3   | CP210x via USB-C (TX/RX)                     |

Pin-Defines zentral in [include/config.h](include/config.h), Display-Pins
zusätzlich in `platformio.ini` build_flags (TFT_eSPI braucht beide Stellen
synchron).

## Sensoren / Peripherie

### ADS1115 (ADC, 16 bit)

I²C-Adresse **0x48** (ADDR-Pin auf GND). Gain `GAIN_ONE` = ±4.096 V FSR.
Single-ended Read auf Kanal 0. Konvertiert die analoge Druckmessdose der
Schaufel.

Watchdog-Logik in [src/scale.cpp](src/scale.cpp):
- 10 aufeinanderfolgende `readADC_SingleEnded == -1` → ADS als ausgefallen
  markiert, Display zeigt „SENSOR-FEHLER!"
- 50 aufeinanderfolgende identische Werte (~5 s) → stuck-Detection,
  ebenfalls SENSOR-FEHLER

### Reed-Kontakt (Position)

GPIO36 (input-only). Reed-Kontakt schliesst gegen GND wenn die Schaufel
in Wiegeposition ist (LOW). Im Loop alle ~10 ms gelesen, sehr
schnelle Reaktion. Bei verlust der Position wird:
- ADC-Sampling pausiert (`schaufelInPosition == false`)
- Wenn gerade im Kalibrieren-/Tara-Confirm-Menü: Rückkehr zum WAAGE-SETUP

### ILI9341 4" SPI TFT 240×320 (Landscape via `setRotation(1)` → 320×240)

TFT_eSPI Library mit `USER_SETUP_LOADED=1`-Override in build_flags (umgeht
globale `User_Setup.h` — kann mit add2flow ST7796 parallel auf demselben
Rechner gebaut werden).

SPI mit 27 MHz, Touch-SPI mit 2.5 MHz. Loaded Fonts: GLCD, 2, 4, 6, 7, 8,
GFXFF (FreeFonts), SmoothFont.

### XPT2046 Resistive Touch (4-wire)

Library: `paulstoffregen/XPT2046_Touchscreen`. Touch-Koordinaten via
`map(p.x, 3900, 300, 0, 320)` und `map(p.y, 3900, 300, 0, 240)` —
Kalibrierungs-Werte aus Sketch v1.0.2 übernommen. Touch-Cal nicht in NVS
persistiert (würde Reset-Knopf brauchen — out of scope für Phase 3c).

## Stromversorgung

USB-C über CP210x-USB-UART → 5 V → 3.3 V LDO am ESP-Modul. ADS1115 und
Display laufen ebenfalls auf 3.3 V. Reed-Kontakt: Pin-INPUT mit internem
Pull-Up.

## Connectoren / Steckplätze

PCB-Schematic siehe [`D:\Add2go\add2go_pcb\add2go_pcb.kicad_sch`](file:///D:/Add2go/add2go_pcb/add2go_pcb.kicad_sch).
KiCad-Daten liegen im Repo-Root des bestehenden ADD2go-Repos
(`MST6170/ADD2go`).

## WiFi-Topologie

Der Empfänger nutzt **zwei** AP-Verbindungen, **eine zur Zeit** (Phase 3c):

| Tab        | STA-AP       | Protokoll                | Use-Case              |
|------------|--------------|--------------------------|-----------------------|
| Schaufel   | `ADD2go`     | UDP Port 5005 (RX-only)  | Sender-Gewichtsdaten  |
| Flow       | `add2flow`   | WebSocket Port 81 (RX+TX)| add2flow-Befüllung    |

Beim Tab-Switch (`network::onTabChanged()` in [src/network.cpp](src/network.cpp)):
1. `WiFi.disconnect(true)`
2. `WiFi.begin(neuer-AP)`
3. Spinner für max 5 s
4. Bei Timeout → Fallback-Screen mit ADD2-Rück-Button

Restzeit der Verbindung ist non-blocking (`network::handleWiFi()`-Reconnect
alle 5 s).
