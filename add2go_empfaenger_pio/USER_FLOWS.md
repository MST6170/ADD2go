# User Flows — add2go-empfaenger-pio

ASCII-Diagramme der Bedienung. Referenz für jede UI-Änderung — Touch-Pfade
müssen sich konsistent zu diesem Dokument verhalten.

## Top-Level: Tab-System (Phase 3c)

```
                    ┌────────────────────────────────┐
                    │  Boot                          │
                    │  - I2C + ADS + NVS + Display   │
                    │  - Touch                       │
                    │  - Watchdog 5 s                │
                    │  - WiFi STA -> ADD2go          │
                    └──────────┬─────────────────────┘
                               ▼
                    ┌─────────────────┐
              ┌────►│  TAB_SCHAUFEL   │◄─── Default beim Boot
              │     │  (UDP zu Sender) │
              │     └──────┬──────────┘
              │            │ Tap auf Wassertropfen-Button (oben rechts)
              │            ▼
              │     ┌─────────────────┐
              │     │  STA-Switch     │  Spinner waehrend WiFi connectet
              │     │  zu add2flow    │  Max 5 s timeout
              │     └──┬──────────────┘
              │        │
              │  ┌─────┴────────┐
              │  │ STA verbunden│
              │  ▼              ▼ Timeout
              │ ┌──────────┐  ┌───────────────────┐
              │ │ WS-Client│  │ Fallback-Screen   │
              │ │  aktiv   │  │ "add2flow nicht   │
              │ └────┬─────┘  │  erreichbar"      │
              │      │        └────────┬──────────┘
              │      │                 │ Tap ADD2-Button
              │      │ Tap ADD2-Button │
              │      ▼                 ▼
              └──────┴─────────────────┘
                   (zurueck zu Schaufel)
```

Tab-Switch-Button (quadratisch, ~35×35, oben rechts unter Signal-Bars):

- **TAB_SCHAUFEL aktiv** → Symbol dunkelblauer **Wassertropfen** (Ziel-Symbol = Wechsel zu Flow)
- **TAB_FLOW aktiv** → Text **"ADD" / "2"** rubinrot zweizeilig (Ziel = zurück zu Schaufel)

Klick toggelt + triggert STA-Reconnect via `network::onTabChanged()`.

## Schaufel-Tab — Menü-Tree (1:1 wie v1.0.2)

```
HAUPTBILDSCHIRM (MENU_NONE)
├── Schaufel-Button oben → MENU_SCHAUFEL
├── TOTAL-Button → add2Offset reset, ZeroModus off
└── ZERO-Button  → add2Offset = add2Gewicht, ZeroModus on

MENU_SCHAUFEL "WAAGE SETUP"
├── TARA → MENU_CONFIRM_TARA   (nur wenn schaufelInPosition)
├── KALIBRIEREN → MENU_KALIBRIEREN  (nur wenn schaufelInPosition)
├── FILTER → MENU_FILTER
└── ZURUECK → MENU_NONE

MENU_KALIBRIEREN
├── -10 / +10 → kalibierGewicht +- 10
├── ESC → MENU_SCHAUFEL
└── SAVE → schaufelFaktor = kalibierGewicht / spannung, NVS, resetFilter, MENU_NONE

MENU_FILTER
├── Anzahl -5 / +5 → tempFilterAnzahl
├── Schwelle -1 / +1 → tempFilterSchwelle
├── ESC → MENU_SCHAUFEL
└── SAVE → NVS persist (filterAnz, filterSchw), MENU_NONE

MENU_CONFIRM_TARA
├── ESC → MENU_SCHAUFEL
└── SAVE → schaufelTara = schaufelRohwert, NVS, resetFilter, MENU_NONE
```

**Position-Loss-Protection**: Wenn `schaufelInPosition == false` während
`MENU_KALIBRIEREN` oder `MENU_CONFIRM_TARA`, springt
[src/ui.cpp](src/ui.cpp) `handleMenuLiveUpdate()` zurück nach `MENU_SCHAUFEL`
und triggert Full-Redraw.

## Flow-Tab (Phase 3c, Schritt 5)

```
TAB_FLOW (nach STA-Switch verbunden)
├── Liter gross zentriert: "doneL / targetL"
├── L/min klein
├── State-Badge (Farben nach add2flow State):
│   - IDLE/SETTINGS: grau "BEREIT" / "EINSTELLUNG"
│   - FILLING: orange "BEFUELLT"
│   - PAUSED: gelb "PAUSE"
│   - DONE: gruen "FERTIG"
│   - ERROR: rot "FEHLER"
│   - RESUME_PROMPT: blau "FORTSETZEN?"
│   - CALIBRATING: grau "KALIBRIERUNG"
├── 3 Preset-Buttons (p1, p2, p3 aus WS-Broadcast) → Confirm-Modal
└── STOP-Button → sendet "STOP" via WS

CONFIRM-MODAL (Vollbild-Overlay)
├── "Sind Sie sicher?" + "Tank pruefen!"
├── "Preset N starten?"
├── Ja-Button gruen → sendet "START:N" via WS, Modal weg
└── Abbruch-Button rot → Modal weg, kein WS-Command
```

## Wire-Protokoll (Server -> Client, 10 Hz Broadcast)

Pipe-delimited, **erstes Feld ist Protokoll-Version**:

```
v1|STATE|done|target|lpm|preset|err|rssi|p1|p2|p3
```

Empfänger ignoriert Frames wenn `parts[0] != "v1"`. Bei Phase 3d würde
`v2|...` neue Felder einführen, der Empfänger könnte auf v2-Parser
dispatchen ohne Phase 3c-Build zu brechen.

Felder:
- **STATE**: `IDLE` / `RESUME_PROMPT` / `FILLING` / `PAUSED` / `DONE` / `ABORTED` / `ERROR` / `SETTINGS` / `SETTINGS_ADV` / `CALIBRATING`
- **done/target**: `uint16` Liter
- **lpm**: `float` (1 Nachkommastelle)
- **preset**: `0/1/2/3`
- **err**: leer-String oder Fehlermeldung
- **rssi**: `-128` = STA nicht verbunden, sonst dBm
- **p1/p2/p3**: aktuelle Preset-Mengen aus add2flow-NVS

## Wire-Protokoll (Client -> Server, Commands)

Phase 3c sendet nur diese vom Flow-Tab:

| Command       | Bedingung                |
|---------------|--------------------------|
| `START:1`     | Confirm-Modal Ja, Preset 1 |
| `START:2`     | Confirm-Modal Ja, Preset 2 |
| `START:3`     | Confirm-Modal Ja, Preset 3 |
| `STOP`        | STOP-Button direkt        |

Nicht implementiert in Phase 3c (Phone-only über add2flow-WebApp):
`ADJUST:+10` / `ADJUST:-10`, `RESUME:YES` / `RESUME:NO`, `ERROR_DISMISS`,
`DONE_DISMISS`.

## Offline-UX

Wenn `millis() - lastFrameMs > 5000` (kein WS-Frame mehr eingegangen):

- Alle Liter/L/min/Badge-Texte werden **grau** dargestellt
- Hinweiszeile „Verbindung verloren" in rot unter dem State-Badge
- Buttons (Preset/STOP) bleiben sichtbar aber Touch wird ignoriert in
  [src/flow_tab.cpp](src/flow_tab.cpp) `handleTouch()` (early return)

Bei wiederhergestelltem WS-Frame: Greying weg, Buttons wieder aktiv.
Transition triggert `displayNeedsFullRedraw=true` für Re-Render.
