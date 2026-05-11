# Gotchas — Stolperfallen beim Arbeiten in add2go-empfaenger-pio

## Core-Version pinnen

`platform = espressif32 @ ~6.5.0` ist **gepinnt** in [platformio.ini](platformio.ini).

ESP32-Arduino-Core ~3.20014 (entspricht Core 2.0.x, IDF 4.4.x) ist die ZWINGENDE Version weil:

- **Watchdog-API**: Der Original-Sketch v1.0.2 nutzt `esp_task_wdt_config_t`-struct-API (IDF 5+). Auf Core 2.0.x existiert die struct-API NICHT — wir nutzen die alte 2-Arg-API `esp_task_wdt_init(WDT_TIMEOUT_SEC, true)` in [src/main.cpp](src/main.cpp). Bei automatischem Bump auf Core 3.x würde **die struct-API erzwungen** (siehe add2flow `GOTCHAS.md`).
- **LEDC-API** (für eventuelle spätere PWM-Erweiterung): gleiches Risiko wie bei add2flow — Core 3.x bricht die Kanal-API.

Wenn `pio platform update` automatisch auf Core 3.x bumpt: **NICHT mergen**. Die `~6.5.0`-Constraint im env greift.

## TFT_eSPI User_Setup.h überschreiben

```ini
build_flags =
    -DUSER_SETUP_LOADED=1
    -DILI9341_DRIVER=1
    -DTFT_WIDTH=240
    -DTFT_HEIGHT=320
    -DTFT_MISO=19 -DTFT_MOSI=23 -DTFT_SCLK=18
    -DTFT_CS=5 -DTFT_DC=4 -DTFT_RST=2
    -DTOUCH_CS=15
    -DLOAD_GLCD -DLOAD_FONT2 ... -DLOAD_GFXFF -DSMOOTH_FONT
```

`USER_SETUP_LOADED=1` schaltet die globale `User_Setup.h`-Inkludierung der
TFT_eSPI-Lib aus, sodass die `build_flags`-Defines greifen. Damit können
add2flow (ST7796 320×480) und add2go-empfaenger-pio (ILI9341 240×320)
parallel auf demselben Rechner gebaut werden ohne sich die globale Config
gegenseitig kaputtzumachen.

**Falle**: Wenn man `User_Setup.h` per Hand aufmacht und dort etwas
ändert, denkt man der Build benutzt die globale Config. Tut er nicht.

## ADS1115-Watchdog (aus Sketch v1.0.1)

Im [src/scale.cpp](src/scale.cpp) sind zwei Schichten:

1. **Fehler-Counter** (`adsFehlerZaehler`): `ads.readADC_SingleEnded` returnt
   `-1` bei I²C-Fehler. Nach 10 Fehlern (= 1 s bei 10 Hz Polling) wird der
   ADS als „nicht vorhanden" markiert.
2. **Stuck-Detection** (`adsGleicheWerteZaehler`): Wenn der Sensor 50×
   identisch denselben Wert liefert (= ~5 s), markiert ihn der Watchdog
   ebenfalls als ausgefallen.

Beide setzen `adsVorhanden = false`, was Display und Logik wissen lässt
„Sensor-Fehler" anzuzeigen statt grottige Werte.

**Falle**: Wenn ein Logic-Bug verhindert dass `adsVorhanden` wieder auf
true gesetzt wird, hilft nur Reboot. Aktuell macht das Sketch und PIO
**nicht** — bewusste Entscheidung, war im Sketch v1.0.2 auch so.

## SPI-Buffer-Konflikt Display vs Touch

Display und Touch teilen den SPI-Bus (MOSI/SCK/MISO). Mit `setRotation(1)`
ist das Display in Landscape. SPI-Frequenz Display: 27 MHz. SPI-Frequenz
Touch: 2.5 MHz (Touch ist langsamer und hat eigenen `SPI_TOUCH_FREQUENCY`).

**Falle**: Bei manchen Boards muss zwischen Display- und Touch-Reads
explizit `SPI.endTransaction()` aufgerufen werden. TFT_eSPI + XPT2046_Touchscreen
machen das aber transparent. Falls Touch-Reads beginnen zu „glitchen":
SPI-Frequenz prüfen.

## Reed-Kontakt nur input-only

GPIO36 ist **input-only** (kein Pull-Up möglich!). Der externe Pull-Up
muss auf der PCB sitzen. Wenn man den ADS abzieht aber den Reed liest,
muss man sicherstellen dass der Pull-Up noch versorgt wird.

## Boot-Loop bei add2flow nicht verfügbar (Phase-3c-spezifisch)

Beim Tab-Switch zu Flow wird `WiFi.disconnect()` + `WiFi.begin("add2flow")`
ausgeführt. Wenn add2flow nicht da ist:

- `WiFi.begin()` ist **non-blocking** — kein Boot-Loop.
- Aber: `WiFi.status()` bleibt `WL_DISCONNECTED`.
- [src/flow_tab.cpp](src/flow_tab.cpp) misst `millis() - s_staSwitchStart`.
  Bei `> WIFI_STA_SWITCH_TIMEOUT_MS` (5 s) → `staSwitchTimedOut() == true`.
- Display zeigt Fallback-Screen mit ADD2-Button rechts oben (zurück zu
  Schaufel-Tab).

**Falle**: Wenn der Spinner unendlich hängt obwohl add2flow nicht da ist,
prüfe ob `displayNeedsFullRedraw` beim Übergang Spinner→Fallback getriggert
wird. In [src/flow_tab.cpp](src/flow_tab.cpp) `loop()` ist eine
Transition-Detection eingebaut die das `displayNeedsFullRedraw = true`
setzt sobald `staSwitchTimedOut()` true wird.

## Auto-Push-Hook und Path-Scoped Stage

Der Auto-Push-Hook in [.claude/auto-push.ps1](.claude/auto-push.ps1) macht
`git -C D:\Add2go add -A add2go_empfaenger_pio/`. **Das `-A`-Flag ist
WICHTIG** für Deletions — bei Modul-Umbenennungen würde sonst die gelöschte
Datei im Remote weiterhin existieren. Das `add2go_empfaenger_pio/` am Ende
verhindert dass parallele Änderungen an Sender-Sketch / KiCad / STL
versehentlich mitcommitted werden.

**Falle**: Wenn jemand `git add -A` ohne Pfad nutzt, werden ALLE
Repo-Änderungen gestaged — auch die Sender-Sketch-Modifikationen. Immer
path-scoped staged.

## WebSocketsClient (Phase 3c) vs WebSocketsServer (add2flow)

`links2004/WebSockets@^2.6.1` — gleiche Lib für beide Seiten. add2flow
nutzt `WebSocketsServer`, der Empfänger im Flow-Tab nutzt
`WebSocketsClient`. Beide haben dasselbe Event-Handler-Pattern.

**Falle**: Nicht beide gleichzeitig in einer Binary instanzieren — das
kostet ~50 kB extra Heap. Im Empfänger nur Client, im add2flow nur Server.

## ESP32-AP-Default-IP-Verwirrung

`192.168.4.1` ist die **Standard-AP-IP** für ESP32-SoftAP. Beide add2flow
**und** add2go-Sender nutzen diese IP für ihren eigenen AP. Heißt: Der
Empfänger, wenn er sich mit „ADD2go" verbindet, bekommt typisch `192.168.4.2`
und routet UDP zum Sender. Wenn er sich mit „add2flow" verbindet, bekommt
er ebenfalls `192.168.4.2` und WS zum add2flow.

**Falle**: Im Serial-Log sieht „IP: 192.168.4.2" beide Male gleich aus —
unterschieden wird nur über die SSID. Bei Debugging immer auch die SSID
loggen.
