# CLAUDE.md

Guidance für Claude Code beim Arbeiten in `add2go_empfaenger_pio/`.

## Project

ESP32-Empfänger-Firmware (PlatformIO + Arduino framework) für die
ADD2go-Wireless-Waage. Empfängt Gewichtsdaten vom Sender (UDP Port 5005),
zeigt sie auf 240×320 ILI9341 TFT, hat Touch-Bedienung. Phase 3c erweitert
um einen **Flow-Tab**, der sich per WebSocket-Client mit
[add2flow](https://github.com/MST6170/add2flow) verbindet.

**Aktuelles Ziel:** Migration des 1397-Zeilen-Arduino-Sketches
[`add2go_empfaenger_v1_0_2.ino`](../add2go_empfaenger_v1_0_2/add2go_empfaenger_v1_0_2.ino)
auf PlatformIO mit Modul-Struktur, dann Flow-Tab-Erweiterung. Doku auf Deutsch.

## Wo dieser Ordner lebt — Repo-Struktur

Dieser PlatformIO-Empfänger lebt als **Unterordner** im bestehenden
`MST6170/ADD2go`-Repo (alles auf `main`). Kein separater GitHub-Repo.

**Echte Begründung:** Alle ADD2go-Artefakte (Original-Sketch, Sender-Sketch,
KiCad-PCB-Daten, 3D-Druck-STL-Cases, PIO-Migration) bleiben in **einer
Git-History** unter `MST6170/ADD2go`. Versionierung von Hardware (PCB-Rev)
und Firmware (Sketch v1.0.2 → PIO v1.1) wandert mit denselben Commits.
Issue-Tracker und PRs an einem Ort. — `gh`-CLI ist sekundär nicht installiert,
das ist Nebeneffekt nicht Begründung.

Konsequenzen:
- Git-Repo-Root ist `D:\Add2go\`, nicht dieser Unterordner.
- Auto-Push-Hook in `.claude\auto-push.ps1` operiert via `git -C D:\Add2go` und
  staged path-scoped `git add -A add2go_empfaenger_pio/` (touched keine
  parallelen KiCad/STL/Sender-Änderungen mit).
- `.gitignore` im Repo-Root `D:\Add2go\.gitignore` hat PIO-spezifische
  Patterns + Re-Include für `add2go_empfaenger_pio/.claude/` (sonst würde
  `auto-push.ps1` und `settings.json` vom globalen `.claude/`-Blanket
  gitignored).

## Doku-Files — bei jeder Session zuerst lesen

| Datei | Zweck |
|-------|-------|
| [README.md](README.md) | Project-Overview, Hardware, Build-Anleitung, Phase-Tabelle |
| [HARDWARE.md](HARDWARE.md) | Pin-Map, PCB-Connector-Map (Phase 3c-6) |
| [GOTCHAS.md](GOTCHAS.md) | Stolperfallen aus Migration (Phase 3c-6) |
| [USER_FLOWS.md](USER_FLOWS.md) | Menü-Tree + Flow-Tab-State (Phase 3c-6) |
| [REGRESSION_CHECKLIST.md](REGRESSION_CHECKLIST.md) | v1.0.2-Verhaltens-Checkliste — vor jedem Refactor abhaken |

## Build & Flash

```bash
pio run -t upload                                              # default env = main
pio device monitor                                             # 115200
pio run -e main -e wifi_test -e display_test -e flow_tab_test  # Build-Gate, alle 4 Envs
```

## Phase 3c-Sequenz (alle abgeschlossen)

1. **3c-1** ✅ Repo-Init + Auto-Push-Hook + gitignore (3 Sub-Schritte 1a/1b/1c)
2. **3c-2** ✅ PlatformIO-Skelett + 4 Env-Stubs (`main`, `wifi_test`, `display_test`, `flow_tab_test`)
3. **3c-3** ✅ Sketch-Migration in Module — Hardware-Display/NVS verifiziert, volle Regression-Test pending
4. **3c-4** ✅ Tab-Switch-Button (Wassertropfen ↔ "ADD2") rechts oben
5. **3c-5** ✅ Flow-Tab mit WebSocketsClient + STA-Switch + Confirm-Modal + Offline-Greying + Fallback-Screen
6. **3c-6** ✅ Doku finalisieren

**E2E-Test pending**: Vollständiger Test add2flow ↔ Empfänger ist offen
bis beide Geräte gleichzeitig laufen + REGRESSION_CHECKLIST.md durchgespielt
ist (siehe Schritt 3 + Validation der Flow-Tab-Funktionen).

Detail-Plan: siehe `C:\Users\MST\.claude\plans\abstract-hugging-raven.md`
(Phase-3c-Plan, freigegeben 2026-05-11).

## Architektur

| Modul | Verantwortung |
|-------|---------------|
| [include/config.h](include/config.h) | Pin-Defines, Konstanten zentral, WIFI_AP_*, STA-Switch-Timeout |
| [src/storage.{h,cpp}](src/storage.cpp) | NVS-Wrapper (Tara, Faktor, Filter) — Namespace `"schaufel"` |
| [src/state.{h,cpp}](src/state.cpp) | Globals als extern (Definitionen + init in state.cpp) inkl. `currentTab` |
| [src/network.{h,cpp}](src/network.cpp) | WiFi STA + UDP-Empfang vom Sender, **SSID-Wahl abhängig vom currentTab**, `onTabChanged()` |
| [src/scale.{h,cpp}](src/scale.cpp) | ADS1115 + Reed + Filter + ADS-Watchdog (10 + 50-Stuck) |
| [src/display.{h,cpp}](src/display.cpp) | TFT-Init + Render-Funktionen (`zeichne*`). Phase 3c: `zeichneTabSwitchButton`, `zeichneFlowTab`, `zeichneFlowConfirmModal`, `zeichneFlowFallback`, `zeichneFlowSpinner`. |
| [src/touch.{h,cpp}](src/touch.cpp) | Touch-Routing pro Menü. Tab-Switch-Button-Check first. Im Flow-Tab Forwarding an `flow::handleTouch`. |
| [src/ui.{h,cpp}](src/ui.cpp) | MenuState Live-Update (Position-Loss-Protection) |
| [src/flow_tab.{h,cpp}](src/flow_tab.cpp) | **Nur** WS-Logik + Parser + State-Cache. **Kein TFT-Code** — Rendering in `display::zeichneFlowTab()`. Confirm-Modal-State und Touch-Logik hier. |
| [src/main.cpp](src/main.cpp) | Orchestrierung: setup-Sequenz, loop-Schleife mit `scale::handleReed/ADC`, `network::handleWiFi/UDP/RSSI`, `flow::loop`, `touch::handle`, `display::handle`, `ui::handleMenuLiveUpdate` |

## Hardware-Pin-Map

Volle Tabelle in [include/config.h](include/config.h) und (nach Schritt 6) in
[HARDWARE.md](HARDWARE.md). Wichtigste Pins:

| Funktion | GPIO |
|---|---|
| Reed-Sensor | 36 (input-only) |
| I²C SDA/SCL (ADS1115) | 21 / 22 |
| Display SPI | CS=5 / RST=2 / DC=4 / MOSI=23 / SCK=18 / MISO=19 |
| Touch | T_CS=15 / T_IRQ=14 |

## TFT_eSPI Configuration

Wie add2flow: Globale `User_Setup.h` wird via
`-DUSER_SETUP_LOADED=1 -DILI9341_DRIVER ...` in `platformio.ini` build_flags
umgangen. Damit können add2flow (ST7796 320×480) und dieser Empfänger
(ILI9341 240×320) parallel auf demselben Rechner gebaut werden, ohne sich zu
stören. Stolperfallen siehe (Phase 3c-6) [GOTCHAS.md](GOTCHAS.md).

## Auto-Push Workflow

Stop-Hook in `.claude/settings.local.json` läuft nach jeder Assistant-Turn:

1. Wenn nichts im PIO-Unterordner geändert: still beenden.
2. Build-Gate: `pio run -e main -e wifi_test -e display_test -e flow_tab_test`.
3. Bei Erfolg: `git -C D:\Add2go add -A add2go_empfaenger_pio/`, Commit, Push.
4. Bei Build-Fail: nicht pushen, in `.claude/last-build.log` loggen.

Pushed nach `MST6170/ADD2go` main (gemeinsamer Repo mit dem Original-Sketch).
Path-scoped Stage verhindert Mit-Committen von paralleler KiCad/STL/Sender-
Arbeit im selben Repo.

## Memory-Verzeichnis

Erwartet `~/.claude/projects/d--Add2go--add2go_empfaenger_pio\memory\` (Case-
Preserved-Pattern wie bestehendes `d--ADD2go\` und `d--add2flow\`). **Bei
erster Memory-Operation verifizieren** — falls anderer Pfad, hier
aktualisieren.
