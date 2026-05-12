# add2go-empfaenger-pio

PlatformIO-Migration des **ADD2go-Empfängers v1.0.2** mit Erweiterung um einen
**Flow-Tab** (Phase 3c), der sich per WebSocket-Client mit
[add2flow](https://github.com/MST6170/add2flow) verbindet.

Original-Sketch
[`add2go_empfaenger_v1_0_2.ino`](../add2go_empfaenger_v1_0_2/add2go_empfaenger_v1_0_2.ino)
bleibt im Repo-Root nebendran als **Stable-Backup** unangetastet.

## Hardware

Läuft auf der bestehenden **ADD2go-Empfänger-PCB** v1.0.2 — gleiche Bestückung
wie für den Sketch, kein PCB-Re-Spin.

| Komponente       | Anschluss                | Notiz                                    |
|------------------|--------------------------|------------------------------------------|
| MCU              | ESP32-WROOM-32U          | mit externer Antenne                     |
| Display          | ILI9341 SPI 240×320      | CS=5 / RST=2 / DC=4 / MOSI=23 / SCK=18 / MISO=19 |
| Touch            | XPT2046                  | T_CS=15 / T_IRQ=14                       |
| ADC              | ADS1115 I²C              | SDA=21 / SCL=22 / Addr 0x48              |
| Reed-Kontakt     | GPIO36                   | LOW = Schaufel in Wiegeposition          |

Pin-Map zentral in [include/config.h](include/config.h), Display-Pins zusätzlich
in `platformio.ini` build_flags (TFT_eSPI braucht beide Stellen synchron).

## Build

PlatformIO (`pio`) liegt unter `C:\Users\MST\.platformio\penv\Scripts\`.

```bash
pio run -t upload                                  # default env = main
pio device monitor                                 # 115200, esp32_exception_decoder
pio run -e main -e wifi_test -e display_test -e flow_tab_test   # alle 4 Envs
```

## Phase-Tabelle

| Phase | Komponente | Status |
|-------|------------|--------|
| 3c-1  | Repo-Init + Auto-Push-Hook + gitignore-Setup | ✅ done |
| 3c-2  | PlatformIO-Skelett + 4 Env-Stubs | ✅ done |
| 3c-3  | Sketch-Migration in Module (Spaghetti → Module) | ✅ Code komplett, Display + NVS verifiziert am 2026-05-11 |
| 3c-4  | Tab-Switch-Button (rechts oben, Wassertropfen ↔ ADD2) | ✅ visuell verifiziert |
| 3c-5  | Flow-Tab mit WebSocketsClient + Confirm-Modal + Offline-Greying | ✅ Code komplett + Build grün, mehrere E2E-Befüllungs-Cycles stabil (2026-05-12) |
| 3c-6  | Doku finalisieren | ✅ done |
| 3c-7  | Hotfix: ADD2-Button bleibt sichtbar während FILLING (Liter-fillRect bis x=280 statt 320) | ✅ geflasht 2026-05-12 |

## Doku-Files

| Datei | Zweck |
|-------|-------|
| [CLAUDE.md](CLAUDE.md) | Architektur-Übersicht für Code-Sessions, Migration-Map |
| [HARDWARE.md](HARDWARE.md) | Pin-Map (entsteht in Schritt 6) |
| [GOTCHAS.md](GOTCHAS.md) | Stolperfallen aus Migration (entsteht in Schritt 6) |
| [USER_FLOWS.md](USER_FLOWS.md) | Menü-Tree + Flow-Tab-State (entsteht in Schritt 6) |
| [REGRESSION_CHECKLIST.md](REGRESSION_CHECKLIST.md) | v1.0.2-Verhaltens-Checkliste, vor jedem Refactor abhaken |

## Repo-Struktur

Dieser PlatformIO-Empfänger lebt als **Unterordner** im bestehenden
`MST6170/ADD2go`-Repo (alles auf `main`). Begründung: alle ADD2go-Artefakte
(Original-Sketch, Sender-Sketch, KiCad-PCB-Daten, 3D-Druck-STL-Cases,
PIO-Migration) bleiben in **einer Git-History**. Versionierung von Hardware
(PCB-Rev) und Firmware (Sketch v1.0.2 → PIO v1.1) wandert mit denselben
Commits.
